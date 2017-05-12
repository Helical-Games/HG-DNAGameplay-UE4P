// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "Engine/NetSerialization.h"
#include "UObject/Interface.h"
#include "DNATagContainer.h"
#include "DNAEffectTypes.h"
#include "DNAPrediction.h"
#include "DNACueInterface.generated.h"

/** Interface for actors that wish to handle DNACue events from DNAEffects. Native only because blueprints can't implement interfaces with native functions */
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UDNACueInterface: public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class DNAABILITIES_API IDNACueInterface
{
	GENERATED_IINTERFACE_BODY()

	virtual void HandleDNACue(AActor *Self, FDNATag DNACueTag, EDNACueEvent::Type EventType, FDNACueParameters Parameters);

	virtual void HandleDNACues(AActor *Self, const FDNATagContainer& DNACueTags, EDNACueEvent::Type EventType, FDNACueParameters Parameters);

	/** Returns true if the actor can currently accept DNA cues associated with the given tag. Returns true by default. Allows actors to opt out of cues in cases such as pending death */
	virtual bool ShouldAcceptDNACue(AActor *Self, FDNATag DNACueTag, EDNACueEvent::Type EventType, FDNACueParameters Parameters);

	/** Return the cue sets used by this object. This is optional and it is possible to leave this list empty. */
	virtual void GetDNACueSets(TArray<class UDNACueSet*>& OutSets) const {}

	/** Default native handler, called if no tag matches found */
	virtual void DNACueDefaultHandler(EDNACueEvent::Type EventType, FDNACueParameters Parameters);

	/** Internal function to map ufunctions directly to DNAcue tags */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = DNACue, meta = (BlueprintInternalUseOnly = "true"))
	void BlueprintCustomHandler(EDNACueEvent::Type EventType, FDNACueParameters Parameters);

	/** Call from a Cue handler event to continue checking for additional, more generic handlers. Called from the ability system blueprint library */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Ability|DNACue")
	virtual void ForwardDNACueToParent();

	static void DispatchBlueprintCustomHandler(AActor* Actor, UFunction* Func, EDNACueEvent::Type EventType, FDNACueParameters Parameters);

	static void ClearTagToFunctionMap();

	IDNACueInterface() : bForwardToParent(false) {}

private:
	/** If true, keep checking for additional handlers */
	bool bForwardToParent;

};


/**
 *	This is meant to provide another way of using DNACues without having to go through DNAEffects.
 *	E.g., it is convenient if DNAAbilities can issue replicated DNACues without having to create
 *	a DNAEffect.
 *	
 *	Essentially provides bare necessities to replicate DNACue Tags.
 */


struct FActiveDNACueContainer;

USTRUCT(BlueprintType)
struct FActiveDNACue : public FFastArraySerializerItem
{
	GENERATED_USTRUCT_BODY()

	FActiveDNACue()	
	{
		bPredictivelyRemoved = false;
	}

	UPROPERTY()
	FDNATag DNACueTag;

	UPROPERTY()
	FPredictionKey PredictionKey;

	UPROPERTY()
	FDNACueParameters Parameters;

	/** Has this been predictively removed on the client? */
	UPROPERTY(NotReplicated)
	bool bPredictivelyRemoved;

	void PreReplicatedRemove(const struct FActiveDNACueContainer &InArray);
	void PostReplicatedAdd(const struct FActiveDNACueContainer &InArray);
	void PostReplicatedChange(const struct FActiveDNACueContainer &InArray) { }
};

USTRUCT(BlueprintType)
struct FActiveDNACueContainer : public FFastArraySerializer
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray< FActiveDNACue >	DNACues;

	UPROPERTY()
	class UDNAAbilitySystemComponent*	Owner;

	/** Should this container only replicate in minimal replication mode */
	bool bMinimalReplication;

	void AddCue(const FDNATag& Tag, const FPredictionKey& PredictionKey, const FDNACueParameters& Parameters);
	void RemoveCue(const FDNATag& Tag);

	/** Marks as predictively removed so that we dont invoke remove event twice due to onrep */
	void PredictiveRemove(const FDNATag& Tag);

	void PredictiveAdd(const FDNATag& Tag, FPredictionKey& PredictionKey);

	/** Does explicit check for DNA cue tag */
	bool HasCue(const FDNATag& Tag) const;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo & DeltaParms);

private:

	int32 GetGameStateTime(const UWorld* World) const;
};

template<>
struct TStructOpsTypeTraits< FActiveDNACueContainer > : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};


/**
 *	Wrapper struct around a DNAtag with the DNACue category. This also allows for a details customization
 */
USTRUCT(BlueprintType)
struct FDNACueTag
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Categories="DNACue"), Category="DNACue")
	FDNATag DNACueTag;

	bool IsValid() const
	{
		return DNACueTag.IsValid();
	}
};
