// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "Templates/SubclassOf.h"
#include "Engine/NetSerialization.h"
#include "AttributeSet.h"
#include "DNAEffectTypes.h"
#include "DNAPrediction.h"
#include "DNAAbilitySpec.generated.h"

class UDNAAbilitySystemComponent;
class UDNAAbility;

/**
 *	This file exists in addition so that DNAEffect.h can use FDNAAbilitySpec without having to include DNAAbilityTypes.h which has depancies on
 *	DNAEffect.h
 */

USTRUCT(BlueprintType)
struct FDNAAbilitySpecHandle
{
	GENERATED_USTRUCT_BODY()

	FDNAAbilitySpecHandle()
	: Handle(INDEX_NONE)
	{

	}

	bool IsValid() const
	{
		return Handle != INDEX_NONE;
	}

	void GenerateNewHandle()
	{
		static int32 GHandle = 1;
		Handle = GHandle++;
	}

	bool operator==(const FDNAAbilitySpecHandle& Other) const
	{
		return Handle == Other.Handle;
	}

	bool operator!=(const FDNAAbilitySpecHandle& Other) const
	{
		return Handle != Other.Handle;
	}

	friend uint32 GetTypeHash(const FDNAAbilitySpecHandle& SpecHandle)
	{
		return ::GetTypeHash(SpecHandle.Handle);
	}

	FString ToString() const
	{
		return IsValid() ? FString::FromInt(Handle) : TEXT("Invalid");
	}

private:

	UPROPERTY()
	int32 Handle;
};

UENUM(BlueprintType)
namespace EDNAAbilityActivationMode
{
	enum Type
	{
		// We are the authority activating this ability
		Authority,

		// We are not the authority but aren't predicting yet. This is a mostly invalid state to be in.
		NonAuthority,

		// We are predicting the activation of this ability
		Predicting,

		// We are not the authority, but the authority has confirmed this activation
		Confirmed,

		// We tried to activate it, and server told us we couldn't (even though we thought we could)
		Rejected,
	};
}

/** Describes what happens when a DNAEffect, that is granting an active ability, is removed from its owner. */
UENUM()
enum class EDNAEffectGrantedAbilityRemovePolicy : uint8
{
	/** Active abilities are immediately canceled and the ability is removed. */
	CancelAbilityImmediately,
	/** Active abilities are allowed to finish, and then removed. */
	RemoveAbilityOnEnd,
	/** Granted abilties are left lone when the granting DNAEffect is removed. */
	DoNothing,
};

/** This is data that can be used to create an FDNAAbilitySpec. Has some data that is only relevant when granted by a DNAEffect */
USTRUCT(BlueprintType)
struct FDNAAbilitySpecDef
{
	FDNAAbilitySpecDef()
		: Level(INDEX_NONE)
		, InputID(INDEX_NONE)
		, RemovalPolicy(EDNAEffectGrantedAbilityRemovePolicy::CancelAbilityImmediately)
		, SourceObject(nullptr)
	{
		LevelScalableFloat.SetValue(1.f);
	}

	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditDefaultsOnly, Category="Ability Definition", NotReplicated)
	TSubclassOf<UDNAAbility> Ability;
	
	// Deprecated for LevelScalableFloat
	UPROPERTY(NotReplicated)
	int32 Level; 

	UPROPERTY(EditDefaultsOnly, Category="Ability Definition", NotReplicated, DisplayName="Level")
	FScalableFloat LevelScalableFloat; 
	
	UPROPERTY(EditDefaultsOnly, Category="Ability Definition", NotReplicated)
	int32 InputID;

	UPROPERTY(EditDefaultsOnly, Category="Ability Definition", NotReplicated)
	EDNAEffectGrantedAbilityRemovePolicy RemovalPolicy;
	
	UPROPERTY(NotReplicated)
	UObject* SourceObject;

	/** This handle can be set if the SpecDef is used to create a real FDNAbilitySpec */
	UPROPERTY()
	FDNAAbilitySpecHandle	AssignedHandle;
};

/**
 *	FDNAAbilityActivationInfo
 *
 *	Data tied to a specific activation of an ability.
 *		-Tell us whether we are the authority, if we are predicting, confirmed, etc.
 *		-Holds current and previous PredictionKey
 *		-Generally not meant to be subclassed in projects.
 *		-Passed around by value since the struct is small.
 */
USTRUCT(BlueprintType)
struct DNAABILITIES_API FDNAAbilityActivationInfo
{
	GENERATED_USTRUCT_BODY()

	FDNAAbilityActivationInfo()
		: ActivationMode(EDNAAbilityActivationMode::Authority)
		, bCanBeEndedByOtherInstance(false)
	{

	}

	FDNAAbilityActivationInfo(AActor* InActor)
		: bCanBeEndedByOtherInstance(false)	
	{
		// On Init, we are either Authority or NonAuthority. We haven't been given a PredictionKey and we haven't been confirmed.
		// NonAuthority essentially means 'I'm not sure what how I'm going to do this yet'.
		ActivationMode = (InActor->Role == ROLE_Authority ? EDNAAbilityActivationMode::Authority : EDNAAbilityActivationMode::NonAuthority);
	}

	FDNAAbilityActivationInfo(EDNAAbilityActivationMode::Type InType)
		: ActivationMode(InType)
		, bCanBeEndedByOtherInstance(false)
	{
	}	

	UPROPERTY(BlueprintReadOnly, Category = "ActorInfo")
	mutable TEnumAsByte<EDNAAbilityActivationMode::Type>	ActivationMode;

	/** An ability that runs on multiple game instances can be canceled by a remote instance, but only if that remote instance has already confirmed starting it. */
	UPROPERTY()
	uint8 bCanBeEndedByOtherInstance:1;

	void SetActivationConfirmed();

	void SetActivationRejected();

	/** Called on client to set this as a predicted ability */
	void SetPredicting(FPredictionKey PredictionKey);

	/** Called on the server to set the key used by the client to predict this ability */
	void ServerSetActivationPredictionKey(FPredictionKey PredictionKey);

	const FPredictionKey& GetActivationPredictionKey() const { return PredictionKeyWhenActivated; }

private:

	// This was the prediction key used to activate this ability. It does not get updated
	// if new prediction keys are generated over the course of the ability.
	UPROPERTY()
	FPredictionKey PredictionKeyWhenActivated;
	
};


/** An activatable ability spec, hosted on the ability system component. This defines both what the ability is (what class, what level, input binding etc)
 *  and also holds runtime state that must be kept outside of the ability being instanced/activated.
 */
USTRUCT()
struct DNAABILITIES_API FDNAAbilitySpec : public FFastArraySerializerItem
{
	GENERATED_USTRUCT_BODY()

	FDNAAbilitySpec()
	: Ability(nullptr), Level(1), InputID(INDEX_NONE), SourceObject(nullptr), ActiveCount(0), InputPressed(false), RemoveAfterActivation(false), PendingRemove(false)
	{
		
	}

	FDNAAbilitySpec(UDNAAbility* InAbility, int32 InLevel=1, int32 InInputID=INDEX_NONE, UObject* InSourceObject=nullptr)
		: Ability(InAbility), Level(InLevel), InputID(InInputID), SourceObject(InSourceObject), ActiveCount(0), InputPressed(false), RemoveAfterActivation(false), PendingRemove(false)
	{
		Handle.GenerateNewHandle();
	}
	
	FDNAAbilitySpec(FDNAAbilitySpecDef& InDef, int32 InDNAEffectLevel, FActiveDNAEffectHandle InDNAEffectHandle = FActiveDNAEffectHandle());

	/** Handle for outside sources to refer to this spec by */
	UPROPERTY()
	FDNAAbilitySpecHandle Handle;
	
	/** Ability of the spec (Always the CDO. This should be const but too many things modify it currently) */
	UPROPERTY()
	UDNAAbility* Ability;
	
	/** Level of Ability */
	UPROPERTY()
	int32	Level;

	/** InputID, if bound */
	UPROPERTY()
	int32	InputID;

	/** Object this ability was created from, can be an actor or static object. Useful to bind an ability to a DNA object */
	UPROPERTY()
	UObject* SourceObject;

	/** A count of the number of times this ability has been activated minus the number of times it has been ended. For instanced abilities this will be the number of currently active instances. Can't replicate until prediction accurately handles this.*/
	UPROPERTY(NotReplicated)
	uint8 ActiveCount;

	/** Is input currently pressed. Set to false when input is released */
	UPROPERTY(NotReplicated)
	uint8 InputPressed:1;

	/** If true, this ability should be removed as soon as it finishes executing */
	UPROPERTY(NotReplicated)
	uint8 RemoveAfterActivation:1;

	/** Pending removal due to scope lock */
	UPROPERTY(NotReplicated)
	uint8 PendingRemove:1;

	/** Activation state of this ability. This is not replicated since it needs to be overwritten locally on clients during prediction. */
	UPROPERTY(NotReplicated)
	FDNAAbilityActivationInfo	ActivationInfo;

	/** Non replicating instances of this ability. */
	UPROPERTY(NotReplicated)
	TArray<UDNAAbility*> NonReplicatedInstances;

	/** Replicated instances of this ability.. */
	UPROPERTY()
	TArray<UDNAAbility*> ReplicatedInstances;

	/** Handle to GE that granted us (usually invalid) */
	UPROPERTY(NotReplicated)
	FActiveDNAEffectHandle	DNAEffectHandle;

	/** Returns the primary instance, used for instance once abilities */
	UDNAAbility* GetPrimaryInstance() const;

	/** Returns all instances, which can include instance per execution abilities */
	TArray<UDNAAbility*> GetAbilityInstances() const
	{
		TArray<UDNAAbility*> Abilities;
		Abilities.Append(ReplicatedInstances);
		Abilities.Append(NonReplicatedInstances);
		return Abilities;
	}

	/** Returns true if this ability is active in any way */
	bool IsActive() const;

	void PreReplicatedRemove(const struct FDNAAbilitySpecContainer& InArraySerializer);
	void PostReplicatedAdd(const struct FDNAAbilitySpecContainer& InArraySerializer);
};


/** Fast serializer wrapper for above struct */
USTRUCT()
struct DNAABILITIES_API FDNAAbilitySpecContainer : public FFastArraySerializer
{
	GENERATED_USTRUCT_BODY()

	/** List of activatable abilities */
	UPROPERTY()
	TArray<FDNAAbilitySpec> Items;

	/** Component that owns this list */
	UDNAAbilitySystemComponent* Owner;

	void RegisterWithOwner(UDNAAbilitySystemComponent* Owner);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo & DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FDNAAbilitySpec, FDNAAbilitySpecContainer>(Items, DeltaParms, *this);
	}
};

template<>
struct TStructOpsTypeTraits< FDNAAbilitySpecContainer > : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};

// ----------------------------------------------------

// Used to stop us from removing abilities from an ability system component while we're iterating through the abilities
struct DNAABILITIES_API FScopedAbilityListLock
{
	FScopedAbilityListLock(UDNAAbilitySystemComponent& InContainer);
	~FScopedAbilityListLock();

private:
	UDNAAbilitySystemComponent& DNAAbilitySystemComponent;
};

#define ABILITYLIST_SCOPE_LOCK()	FScopedAbilityListLock ActiveScopeLock(*this);

// Used to stop us from canceling or ending an ability while we're iterating through its DNA targets
struct DNAABILITIES_API FScopedTargetListLock
{
	FScopedTargetListLock(UDNAAbilitySystemComponent& InDNAAbilitySystemComponent, const UDNAAbility& InAbility);
	~FScopedTargetListLock();

private:
	const UDNAAbility& DNAAbility;

	// we also need to make sure the ability isn't removed while we're in this lock
	FScopedAbilityListLock AbilityLock;
};

#define TARGETLIST_SCOPE_LOCK(ASC)	FScopedTargetListLock ActiveScopeLock(ASC, *this);
