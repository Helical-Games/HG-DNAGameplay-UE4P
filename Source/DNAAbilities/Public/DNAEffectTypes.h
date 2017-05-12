// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "Engine.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Templates/SubclassOf.h"
#include "Engine/NetSerialization.h"
#include "DNATagContainer.h"
#include "DNATagAssetInterface.h"
#include "AttributeSet.h"
#include "DNAPrediction.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "AbilitySystemLog.h"
#include "DNAEffectTypes.generated.h"

#define SKILL_SYSTEM_AGGREGATOR_DEBUG 1

#if SKILL_SYSTEM_AGGREGATOR_DEBUG
	#define SKILL_AGG_DEBUG( Format, ... ) *FString::Printf(Format, ##__VA_ARGS__)
#else
	#define SKILL_AGG_DEBUG( Format, ... ) NULL
#endif

class Error;
class UDNAAbilitySystemComponent;
class UDNAAbility;
struct FActiveDNAEffect;
struct FDNAEffectModCallbackData;
struct FDNAEffectSpec;
struct FDNAAttribute;

DNAABILITIES_API FString EDNAModOpToString(int32 Type);

DNAABILITIES_API FString EDNAModToString(int32 Type);

DNAABILITIES_API FString EDNAModEffectToString(int32 Type);

DNAABILITIES_API FString EDNACueEventToString(int32 Type);

/** Valid DNA modifier evaluation channels; Displayed and renamed via game-specific aliases and options */
UENUM()
enum class EDNAModEvaluationChannel : uint8
{
	Channel0 UMETA(Hidden),
	Channel1 UMETA(Hidden),
	Channel2 UMETA(Hidden),
	Channel3 UMETA(Hidden),
	Channel4 UMETA(Hidden),
	Channel5 UMETA(Hidden),
	Channel6 UMETA(Hidden),
	Channel7 UMETA(Hidden),
	Channel8 UMETA(Hidden),
	Channel9 UMETA(Hidden),

	// Always keep last
	Channel_MAX UMETA(Hidden)
};

/** Struct representing evaluation channel settings for a DNA modifier */
USTRUCT()
struct DNAABILITIES_API FDNAModEvaluationChannelSettings
{
	GENERATED_USTRUCT_BODY()
	
	/** Constructor */
	FDNAModEvaluationChannelSettings();

	/**
	 * Get the modifier evaluation channel to use
	 * 
	 * @return	Either the channel directly specified within the settings, if valid, or Channel0 in the event of a game not using modifier
	 *			channels or in the case of an invalid channel being specified within the settings
	 */
	EDNAModEvaluationChannel GetEvaluationChannel() const;

	/** Editor-only constants to aid in hiding evaluation channel settings when appropriate */
#if WITH_EDITORONLY_DATA
	static const FName ForceHideMetadataKey;
	static const FString ForceHideMetadataEnabledValue;
#endif // #if WITH_EDITORONLY_DATA

protected:

	/** Channel the settings would prefer to use, if possible/valid */
	UPROPERTY(EditDefaultsOnly, Category=EvaluationChannel)
	EDNAModEvaluationChannel Channel;

	// Allow the details customization as a friend so it can handle custom display of the struct
	friend class FDNAModEvaluationChannelSettingsDetails;
};

UENUM(BlueprintType)
namespace EDNAModOp
{
	enum Type
	{		
		/** Numeric. */
		Additive = 0		UMETA(DisplayName="Add"),
		/** Numeric. */
		Multiplicitive		UMETA(DisplayName = "Multiply"),
		/** Numeric. */
		Division			UMETA(DisplayName = "Divide"),

		/** Other. */
		Override 			UMETA(DisplayName="Override"),	// This should always be the first non numeric ModOp

		// This must always be at the end.
		Max					UMETA(DisplayName="Invalid")
	};
}

namespace DNAEffectUtilities
{
	/**
	 * Helper function to retrieve the modifier bias based upon modifier operation
	 * 
	 * @param ModOp	Modifier operation to retrieve the modifier bias for
	 * 
	 * @return Modifier bias for the specified operation
	 */
	DNAABILITIES_API float GetModifierBiasByModifierOp(EDNAModOp::Type ModOp);

	/**
	 * Helper function to compute the stacked modifier magnitude from a base magnitude, given a stack count and modifier operation
	 * 
	 * @param BaseComputedMagnitude	Base magnitude to compute from
	 * @param StackCount			Stack count to use for the calculation
	 * @param ModOp					Modifier operation to use
	 * 
	 * @return Computed modifier magnitude with stack count factored in
	 */
	DNAABILITIES_API float ComputeStackedModifierMagnitude(float BaseComputedMagnitude, int32 StackCount, EDNAModOp::Type ModOp);
}


/** Enumeration for options of where to capture DNA attributes from for DNA effects. */
UENUM()
enum class EDNAEffectAttributeCaptureSource : uint8
{
	/** Source (caster) of the DNA effect. */
	Source,	
	/** Target (recipient) of the DNA effect. */
	Target	
};

/** Enumeration for ways a single DNAEffect asset can stack. */
UENUM()
enum class EDNAEffectStackingType : uint8
{
	/** No stacking. Multiple applications of this DNAEffect are treated as separate instances. */
	None,
	/** Each caster has its own stack. */
	AggregateBySource,
	/** Each target has its own stack. */
	AggregateByTarget,
};

/**
 * This handle is required for things outside of FActiveDNAEffectsContainer to refer to a specific active DNAEffect
 *	For example if a skill needs to create an active effect and then destroy that specific effect that it created, it has to do so
 *	through a handle. a pointer or index into the active list is not sufficient.
 */
USTRUCT(BlueprintType)
struct DNAABILITIES_API FActiveDNAEffectHandle
{
	GENERATED_USTRUCT_BODY()

	FActiveDNAEffectHandle()
		: Handle(INDEX_NONE),
		bPassedFiltersAndWasExecuted(false)
	{

	}

	FActiveDNAEffectHandle(int32 InHandle)
		: Handle(InHandle),
		bPassedFiltersAndWasExecuted(true)
	{

	}

	bool IsValid() const
	{
		return Handle != INDEX_NONE;
	}

	bool WasSuccessfullyApplied() const
	{
		return bPassedFiltersAndWasExecuted;
	}

	static FActiveDNAEffectHandle GenerateNewHandle(UDNAAbilitySystemComponent* OwningComponent);

	static void ResetGlobalHandleMap();

	UDNAAbilitySystemComponent* GetOwningDNAAbilitySystemComponent();
	const UDNAAbilitySystemComponent* GetOwningDNAAbilitySystemComponent() const;

	void RemoveFromGlobalMap();

	bool operator==(const FActiveDNAEffectHandle& Other) const
	{
		return Handle == Other.Handle;
	}

	bool operator!=(const FActiveDNAEffectHandle& Other) const
	{
		return Handle != Other.Handle;
	}

	friend uint32 GetTypeHash(const FActiveDNAEffectHandle& InHandle)
	{
		return InHandle.Handle;
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("%d"), Handle);
	}

	void Invalidate()
	{
		Handle = INDEX_NONE;
	}

private:

	UPROPERTY()
	int32 Handle;

	UPROPERTY()
	bool bPassedFiltersAndWasExecuted;
};

USTRUCT()
struct DNAABILITIES_API FDNAModifierEvaluatedData
{
	GENERATED_USTRUCT_BODY()

	FDNAModifierEvaluatedData()
		: Attribute()
		, ModifierOp(EDNAModOp::Additive)
		, Magnitude(0.f)
		, IsValid(false)
	{
	}

	FDNAModifierEvaluatedData(const FDNAAttribute& InAttribute, TEnumAsByte<EDNAModOp::Type> InModOp, float InMagnitude, FActiveDNAEffectHandle InHandle = FActiveDNAEffectHandle())
		: Attribute(InAttribute)
		, ModifierOp(InModOp)
		, Magnitude(InMagnitude)
		, Handle(InHandle)
		, IsValid(true)
	{
	}

	UPROPERTY()
	FDNAAttribute  Attribute;

	/** The numeric operation of this modifier: Override, Add, Multiply, etc  */
	UPROPERTY()
	TEnumAsByte<EDNAModOp::Type> ModifierOp;

	UPROPERTY()
	float Magnitude;

	/** Handle of the active DNA effect that originated us. Will be invalid in many cases */
	UPROPERTY()
	FActiveDNAEffectHandle	Handle;

	UPROPERTY()
	bool IsValid;

	FString ToSimpleString() const
	{
		return FString::Printf(TEXT("%s %s EvalMag: %f"), *Attribute.GetName(), *EDNAModOpToString(ModifierOp), Magnitude);
	}
};

/** Struct defining DNA attribute capture options for DNA effects */
USTRUCT()
struct DNAABILITIES_API FDNAEffectAttributeCaptureDefinition
{
	GENERATED_USTRUCT_BODY()

	FDNAEffectAttributeCaptureDefinition()
	{
		AttributeSource = EDNAEffectAttributeCaptureSource::Source;
		bSnapshot = false;
	}

	FDNAEffectAttributeCaptureDefinition(FDNAAttribute InAttribute, EDNAEffectAttributeCaptureSource InSource, bool InSnapshot)
		: AttributeToCapture(InAttribute), AttributeSource(InSource), bSnapshot(InSnapshot)
	{

	}

	/** DNA attribute to capture */
	UPROPERTY(EditDefaultsOnly, Category=Capture)
	FDNAAttribute AttributeToCapture;

	/** Source of the DNA attribute */
	UPROPERTY(EditDefaultsOnly, Category=Capture)
	EDNAEffectAttributeCaptureSource AttributeSource;

	/** Whether the attribute should be snapshotted or not */
	UPROPERTY(EditDefaultsOnly, Category=Capture)
	bool bSnapshot;

	/** Equality/Inequality operators */
	bool operator==(const FDNAEffectAttributeCaptureDefinition& Other) const;
	bool operator!=(const FDNAEffectAttributeCaptureDefinition& Other) const;

	/**
	 * Get type hash for the capture definition; Implemented to allow usage in TMap
	 *
	 * @param CaptureDef Capture definition to get the type hash of
	 */
	friend uint32 GetTypeHash(const FDNAEffectAttributeCaptureDefinition& CaptureDef)
	{
		uint32 Hash = 0;
		Hash = HashCombine(Hash, GetTypeHash(CaptureDef.AttributeToCapture));
		Hash = HashCombine(Hash, GetTypeHash(static_cast<uint8>(CaptureDef.AttributeSource)));
		Hash = HashCombine(Hash, GetTypeHash(CaptureDef.bSnapshot));
		return Hash;
	}

	FString ToSimpleString() const;
};

/**
 * FDNAEffectContext
 *	Data struct for an instigator and related data. This is still being fleshed out. We will want to track actors but also be able to provide some level of tracking for actors that are destroyed.
 *	We may need to store some positional information as well.
 */
USTRUCT()
struct DNAABILITIES_API FDNAEffectContext
{
	GENERATED_USTRUCT_BODY()

	FDNAEffectContext()
	: Ability(nullptr)
	, AbilityLevel(1)
	, bHasWorldOrigin(false)
	{
	}

	FDNAEffectContext(AActor* InInstigator, AActor* InEffectCauser)
		: Ability(nullptr)
		, AbilityLevel(1)
		, bHasWorldOrigin(false)
	{
		AddInstigator(InInstigator, InEffectCauser);
	}

	virtual ~FDNAEffectContext()
	{
	}

	/** Returns the list of DNA tags applicable to this effect, defaults to the owner's tags */
	virtual void GetOwnedDNATags(OUT FDNATagContainer& ActorTagContainer, OUT FDNATagContainer& SpecTagContainer) const;

	/** Sets the instigator and effect causer. Instigator is who owns the ability that spawned this, EffectCauser is the actor that is the physical source of the effect, such as a weapon. They can be the same. */
	virtual void AddInstigator(class AActor *InInstigator, class AActor *InEffectCauser);

	/** Sets the ability that was used to spawn this */
	virtual void SetAbility(const UDNAAbility* InDNAAbility);

	/** Returns the immediate instigator that applied this effect */
	virtual AActor* GetInstigator() const
	{
		return Instigator.Get();
	}

	/** returns the CDO of the ability used to instigate this context */
	const UDNAAbility* GetAbility() const;

	int32 GetAbilityLevel() const
	{
		return AbilityLevel;
	}

	/** Returns the ability system component of the instigator of this effect */
	virtual UDNAAbilitySystemComponent* GetInstigatorDNAAbilitySystemComponent() const
	{
		return InstigatorDNAAbilitySystemComponent.Get();
	}

	/** Returns the physical actor tied to the application of this effect */
	virtual AActor* GetEffectCauser() const
	{
		return EffectCauser.Get();
	}

	void SetEffectCauser(AActor* InEffectCauser)
	{
		EffectCauser = InEffectCauser;
	}

	/** Should always return the original instigator that started the whole chain. Subclasses can override what this does */
	virtual AActor* GetOriginalInstigator() const
	{
		return Instigator.Get();
	}

	/** Returns the ability system component of the instigator that started the whole chain */
	virtual UDNAAbilitySystemComponent* GetOriginalInstigatorDNAAbilitySystemComponent() const
	{
		return InstigatorDNAAbilitySystemComponent.Get();
	}

	/** Sets the object this effect was created from. */
	virtual void AddSourceObject(const UObject* NewSourceObject)
	{
		SourceObject = NewSourceObject;
	}

	/** Returns the object this effect was created from. */
	virtual UObject* GetSourceObject() const
	{
		return SourceObject.Get();
	}

	virtual void AddActors(const TArray<TWeakObjectPtr<AActor>>& IActor, bool bReset = false);

	virtual void AddHitResult(const FHitResult& InHitResult, bool bReset = false);

	virtual const TArray<TWeakObjectPtr<AActor>>& GetActors() const
	{
		return Actors;
	}

	virtual const FHitResult* GetHitResult() const
	{
		if (HitResult.IsValid())
		{
			return HitResult.Get();
		}
		return NULL;
	}

	virtual void AddOrigin(FVector InOrigin);

	virtual const FVector& GetOrigin() const
	{
		return WorldOrigin;
	}

	virtual bool HasOrigin() const
	{
		return bHasWorldOrigin;
	}

	virtual FString ToString() const
	{
		return Instigator.IsValid() ? Instigator->GetName() : FString(TEXT("NONE"));
	}

	virtual UScriptStruct* GetScriptStruct() const
	{
		return FDNAEffectContext::StaticStruct();
	}

	/** Creates a copy of this context, used to duplicate for later modifications */
	virtual FDNAEffectContext* Duplicate() const
	{
		FDNAEffectContext* NewContext = new FDNAEffectContext();
		*NewContext = *this;
		NewContext->AddActors(Actors);
		if (GetHitResult())
		{
			// Does a deep copy of the hit result
			NewContext->AddHitResult(*GetHitResult(), true);
		}
		return NewContext;
	}

	virtual bool IsLocallyControlled() const;

	virtual bool IsLocallyControlledPlayer() const;

	virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

protected:
	// The object pointers here have to be weak because contexts aren't necessarily tracked by GC in all cases

	/** Instigator actor, the actor that owns the ability system component */
	UPROPERTY()
	TWeakObjectPtr<AActor> Instigator;

	/** The physical actor that actually did the damage, can be a weapon or projectile */
	UPROPERTY()
	TWeakObjectPtr<AActor> EffectCauser;

	/** the ability that is responsible for this effect context */
	UPROPERTY()
	TSubclassOf<UDNAAbility> Ability;

	UPROPERTY()
	int32 AbilityLevel;

	/** Object this effect was created from, can be an actor or static object. Useful to bind an effect to a DNA object */
	UPROPERTY()
	TWeakObjectPtr<UObject> SourceObject;

	/** The ability system component that's bound to instigator */
	UPROPERTY(NotReplicated)
	TWeakObjectPtr<UDNAAbilitySystemComponent> InstigatorDNAAbilitySystemComponent;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> Actors;

	/** Trace information - may be NULL in many cases */
	TSharedPtr<FHitResult>	HitResult;

	UPROPERTY()
	FVector	WorldOrigin;

	UPROPERTY()
	bool bHasWorldOrigin;
};

template<>
struct TStructOpsTypeTraits< FDNAEffectContext > : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true		// Necessary so that TSharedPtr<FHitResult> Data is copied around
	};
};

/**
 * Handle that wraps a FDNAEffectContext or subclass, to allow it to be polymorphic and replicate properly
 */
USTRUCT(BlueprintType)
struct FDNAEffectContextHandle
{
	GENERATED_USTRUCT_BODY()

	FDNAEffectContextHandle()
	{
	}

	virtual ~FDNAEffectContextHandle()
	{
	}

	/** Constructs from an existing context, should be allocated by new */
	explicit FDNAEffectContextHandle(FDNAEffectContext* DataPtr)
	{
		Data = TSharedPtr<FDNAEffectContext>(DataPtr);
	}

	/** Sets from an existing context, should be allocated by new */
	void operator=(FDNAEffectContext* DataPtr)
	{
		Data = TSharedPtr<FDNAEffectContext>(DataPtr);
	}

	void Clear()
	{
		Data.Reset();
	}

	bool IsValid() const
	{
		return Data.IsValid();
	}

	FDNAEffectContext* Get()
	{
		return IsValid() ? Data.Get() : NULL;
	}

	const FDNAEffectContext* Get() const
	{
		return IsValid() ? Data.Get() : NULL;
	}

	/** Returns the list of DNA tags applicable to this effect, defaults to the owner's tags */
	void GetOwnedDNATags(OUT FDNATagContainer& ActorTagContainer, OUT FDNATagContainer& SpecTagContainer) const
	{
		if (IsValid())
		{
			Data->GetOwnedDNATags(ActorTagContainer, SpecTagContainer);
		}
	}

	/** Sets the instigator and effect causer. Instigator is who owns the ability that spawned this, EffectCauser is the actor that is the physical source of the effect, such as a weapon. They can be the same. */
	void AddInstigator(class AActor *InInstigator, class AActor *InEffectCauser)
	{
		if (IsValid())
		{
			Data->AddInstigator(InInstigator, InEffectCauser);
		}
	}

	void SetAbility(const UDNAAbility* InDNAAbility)
	{
		if (IsValid())
		{
			Data->SetAbility(InDNAAbility);
		}
	}

	/** Returns the immediate instigator that applied this effect */
	virtual AActor* GetInstigator() const
	{
		if (IsValid())
		{
			return Data->GetInstigator();
		}
		return NULL;
	}

	/** Returns the Ability CDO */
	const UDNAAbility* GetAbility() const
	{
		if (IsValid())
		{
			return Data->GetAbility();
		}
		return nullptr;
	}

	int32 GetAbilityLevel() const
	{
		if (IsValid())
		{
			return Data->GetAbilityLevel();
		}
		return 1;
	}

	/** Returns the ability system component of the instigator of this effect */
	virtual UDNAAbilitySystemComponent* GetInstigatorDNAAbilitySystemComponent() const
	{
		if (IsValid())
		{
			return Data->GetInstigatorDNAAbilitySystemComponent();
		}
		return NULL;
	}

	/** Returns the physical actor tied to the application of this effect */
	virtual AActor* GetEffectCauser() const
	{
		if (IsValid())
		{
			return Data->GetEffectCauser();
		}
		return NULL;
	}

	/** Should always return the original instigator that started the whole chain. Subclasses can override what this does */
	AActor* GetOriginalInstigator() const
	{
		if (IsValid())
		{
			return Data->GetOriginalInstigator();
		}
		return NULL;
	}

	/** Returns the ability system component of the instigator that started the whole chain */
	UDNAAbilitySystemComponent* GetOriginalInstigatorDNAAbilitySystemComponent() const
	{
		if (IsValid())
		{
			return Data->GetOriginalInstigatorDNAAbilitySystemComponent();
		}
		return NULL;
	}

	/** Sets the object this effect was created from. */
	void AddSourceObject(const UObject* NewSourceObject)
	{
		if (IsValid())
		{
			Data->AddSourceObject(NewSourceObject);
		}
	}

	/** Returns the object this effect was created from. */
	UObject* GetSourceObject() const
	{
		if (IsValid())
		{
			return Data->GetSourceObject();
		}
		return nullptr;
	}

	/** Returns if the instigator is locally controlled */
	bool IsLocallyControlled() const
	{
		if (IsValid())
		{
			return Data->IsLocallyControlled();
		}
		return false;
	}

	bool IsLocallyControlledPlayer() const
	{
		if (IsValid())
		{
			return Data->IsLocallyControlledPlayer();
		}
		return false;
	}

	void AddActors(const TArray<TWeakObjectPtr<AActor>>& InActors, bool bReset = false)
	{
		if (IsValid())
		{
			Data->AddActors(InActors, bReset);
		}
	}

	void AddHitResult(const FHitResult& InHitResult, bool bReset = false)
	{
		if (IsValid())
		{
			Data->AddHitResult(InHitResult, bReset);
		}
	}

	const TArray<TWeakObjectPtr<AActor>> GetActors()
	{
		return Data->GetActors();
	}

	const FHitResult* GetHitResult() const
	{
		if (IsValid())
		{
			return Data->GetHitResult();
		}
		return nullptr;
	}

	void AddOrigin(FVector InOrigin)
	{
		if (IsValid())
		{
			Data->AddOrigin(InOrigin);
		}
	}

	virtual const FVector& GetOrigin() const
	{
		if (IsValid())
		{
			return Data->GetOrigin();
		}
		return FVector::ZeroVector;
	}

	virtual bool HasOrigin() const
	{
		if (IsValid())
		{
			return Data->HasOrigin();
		}
		return false;
	}

	FString ToString() const
	{
		return IsValid() ? Data->ToString() : FString(TEXT("NONE"));
	}

	/** Creates a deep copy of this handle, used before modifying */
	FDNAEffectContextHandle Duplicate() const
	{
		if (IsValid())
		{
			FDNAEffectContext* NewContext = Data->Duplicate();
			return FDNAEffectContextHandle(NewContext);
		}
		else
		{
			return FDNAEffectContextHandle();
		}
	}

	/** Comparison operator */
	bool operator==(FDNAEffectContextHandle const& Other) const
	{
		if (Data.IsValid() != Other.Data.IsValid())
		{
			return false;
		}
		if (Data.Get() != Other.Data.Get())
		{
			return false;
		}
		return true;
	}

	/** Comparison operator */
	bool operator!=(FDNAEffectContextHandle const& Other) const
	{
		return !(FDNAEffectContextHandle::operator==(Other));
	}

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

private:

	TSharedPtr<FDNAEffectContext> Data;
};

template<>
struct TStructOpsTypeTraits<FDNAEffectContextHandle> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithCopy = true,		// Necessary so that TSharedPtr<FDNAEffectContext> Data is copied around
		WithNetSerializer = true,
		WithIdenticalViaEquality = true,
	};
};

// -----------------------------------------------------------


USTRUCT(BlueprintType)
struct DNAABILITIES_API FDNACueParameters
{
	GENERATED_USTRUCT_BODY()

	FDNACueParameters()
	: NormalizedMagnitude(0.0f)
	, RawMagnitude(0.0f)
	, Location(ForceInitToZero)
	, Normal(ForceInitToZero)
	, DNAEffectLevel(1)
	, AbilityLevel(1)
	{}

	/** Projects can override this via UDNAAbilitySystemGlobals */
	FDNACueParameters(const struct FDNAEffectSpecForRPC &Spec);

	FDNACueParameters(const struct FDNAEffectContextHandle& EffectContext);

	/** Magnitude of source DNA effect, normalzed from 0-1. Use this for "how strong is the DNA effect" (0=min, 1=,max) */
	UPROPERTY(BlueprintReadWrite, Category=DNACue)
	float NormalizedMagnitude;

	/** Raw final magnitude of source DNA effect. Use this is you need to display numbers or for other informational purposes. */
	UPROPERTY(BlueprintReadWrite, Category=DNACue)
	float RawMagnitude;

	/** Effect context, contains information about hit result, etc */
	UPROPERTY(BlueprintReadWrite, Category=DNACue)
	FDNAEffectContextHandle EffectContext;

	/** The tag name that matched this specific DNA cue handler */
	UPROPERTY(BlueprintReadWrite, Category=DNACue, NotReplicated)
	FDNATag MatchedTagName;

	/** The original tag of the DNA cue */
	UPROPERTY(BlueprintReadWrite, Category=DNACue, NotReplicated)
	FDNATag OriginalTag;

	/** The aggregated source tags taken from the effect spec */
	UPROPERTY(BlueprintReadWrite, Category=DNACue)
	FDNATagContainer AggregatedSourceTags;

	/** The aggregated target tags taken from the effect spec */
	UPROPERTY(BlueprintReadWrite, Category=DNACue)
	FDNATagContainer AggregatedTargetTags;

	UPROPERTY(BlueprintReadWrite, Category=DNACue)
	FVector_NetQuantize10 Location;

	UPROPERTY(BlueprintReadWrite, Category=DNACue)
	FVector_NetQuantizeNormal Normal;

	/** Instigator actor, the actor that owns the ability system component */
	UPROPERTY(BlueprintReadWrite, Category=DNACue)
	TWeakObjectPtr<AActor> Instigator;

	/** The physical actor that actually did the damage, can be a weapon or projectile */
	UPROPERTY(BlueprintReadWrite, Category=DNACue)
	TWeakObjectPtr<AActor> EffectCauser;

	/** Object this effect was created from, can be an actor or static object. Useful to bind an effect to a DNA object */
	UPROPERTY(BlueprintReadWrite, Category=DNACue)
	TWeakObjectPtr<const UObject> SourceObject;

	/** PhysMat of the hit, if there was a hit. */
	UPROPERTY(BlueprintReadWrite, Category = DNACue)
	TWeakObjectPtr<const UPhysicalMaterial> PhysicalMaterial;

	/** If originating from a DNAEffect, the level of that DNAEffect */
	UPROPERTY(BlueprintReadWrite, Category = DNACue)
	int32 DNAEffectLevel;

	/** If originating from an ability, this will be the level of that ability */
	UPROPERTY(BlueprintReadWrite, Category = DNACue)
	int32 AbilityLevel;

	/** Could be used to say "attach FX to this component always" */
	UPROPERTY(BlueprintReadWrite, Category = DNACue)
	TWeakObjectPtr<USceneComponent> TargetAttachComponent;

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	bool IsInstigatorLocallyControlled() const;

	// Fallback actor is used if the parameters have nullptr for instigator and effect causer
	bool IsInstigatorLocallyControlledPlayer(AActor* FallbackActor=nullptr) const;

	AActor* GetInstigator() const;

	AActor* GetEffectCauser() const;

	const UObject* GetSourceObject() const;
};

template<>
struct TStructOpsTypeTraits<FDNACueParameters> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithNetSerializer = true		
	};
};

UENUM(BlueprintType)
namespace EDNACueEvent
{
	enum Type
	{
		OnActive,		// Called when DNACue is activated.
		WhileActive,	// Called when DNACue is active, even if it wasn't actually just applied (Join in progress, etc)
		Executed,		// Called when a DNACue is executed: instant effects or periodic tick
		Removed			// Called when DNACue is removed
	};
}

DECLARE_DELEGATE_OneParam(FOnDNAAttributeEffectExecuted, struct FDNAModifierEvaluatedData&);

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDNAEffectTagCountChanged, const FDNATag, int32);

DECLARE_MULTICAST_DELEGATE(FOnActiveDNAEffectRemoved);

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGivenActiveDNAEffectRemoved, const FActiveDNAEffect&);

DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnActiveDNAEffectStackChange, FActiveDNAEffectHandle, int32, int32);

/** FActiveDNAEffectHandle that is being effect, the start time, duration of the effect */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnActiveDNAEffectTimeChange, FActiveDNAEffectHandle, float, float);

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDNAAttributeChange, float, const FDNAEffectModCallbackData*);

DECLARE_DELEGATE_RetVal(FDNATagContainer, FGetDNATags);

DECLARE_DELEGATE_RetVal_OneParam(FOnDNAEffectTagCountChanged&, FRegisterDNATagChangeDelegate, FDNATag);

// -----------------------------------------------------------


UENUM(BlueprintType)
namespace EDNATagEventType
{
	enum Type
	{		
		/** Event only happens when tag is new or completely removed */
		NewOrRemoved,

		/** Event happens any time tag "count" changes */
		AnyCountChange		
	};
}

/**
 * Struct that tracks the number/count of tag applications within it. Explicitly tracks the tags added or removed,
 * while simultaneously tracking the count of parent tags as well. Events/delegates are fired whenever the tag counts
 * of any tag (explicit or parent) are modified.
 */

struct DNAABILITIES_API FDNATagCountContainer
{	
	FDNATagCountContainer()
	{}

	/**
	 * Check if the count container has a DNA tag that matches against the specified tag (expands to include parents of asset tags)
	 * 
	 * @param TagToCheck	Tag to check for a match
	 * 
	 * @return True if the count container has a DNA tag that matches, false if not
	 */
	FORCEINLINE bool HasMatchingDNATag(FDNATag TagToCheck) const
	{
		return DNATagCountMap.FindRef(TagToCheck) > 0;
	}

	/**
	 * Check if the count container has DNA tags that matches against all of the specified tags (expands to include parents of asset tags)
	 * 
	 * @param TagContainer			Tag container to check for a match. If empty will return true
	 * 
	 * @return True if the count container matches all of the DNA tags
	 */
	FORCEINLINE bool HasAllMatchingDNATags(const FDNATagContainer& TagContainer) const
	{
		// if the TagContainer count is 0 return bCountEmptyAsMatch;
		if (TagContainer.Num() == 0)
		{
			return true;
		}

		bool AllMatch = true;
		for (const FDNATag& Tag : TagContainer)
		{
			if (DNATagCountMap.FindRef(Tag) <= 0)
			{
				AllMatch = false;
				break;
			}
		}		
		return AllMatch;
	}
	
	/**
	 * Check if the count container has DNA tags that matches against any of the specified tags (expands to include parents of asset tags)
	 * 
	 * @param TagContainer			Tag container to check for a match. If empty will return false
	 * 
	 * @return True if the count container matches any of the DNA tags
	 */
	FORCEINLINE bool HasAnyMatchingDNATags(const FDNATagContainer& TagContainer) const
	{
		if (TagContainer.Num() == 0)
		{
			return false;
		}

		bool AnyMatch = false;
		for (const FDNATag& Tag : TagContainer)
		{
			if (DNATagCountMap.FindRef(Tag) > 0)
			{
				AnyMatch = true;
				break;
			}
		}
		return AnyMatch;
	}
	
	/**
	 * Update the specified container of tags by the specified delta, potentially causing an additional or removal from the explicit tag list
	 * 
	 * @param Container		Container of tags to update
	 * @param CountDelta	Delta of the tag count to apply
	 */
	FORCEINLINE void UpdateTagCount(const FDNATagContainer& Container, int32 CountDelta)
	{
		if (CountDelta != 0)
		{
			for (auto TagIt = Container.CreateConstIterator(); TagIt; ++TagIt)
			{
				UpdateTagMap_Internal(*TagIt, CountDelta);
			}
		}
	}
	
	/**
	 * Update the specified tag by the specified delta, potentially causing an additional or removal from the explicit tag list
	 * 
	 * @param Tag			Tag to update
	 * @param CountDelta	Delta of the tag count to apply
	 * 
	 * @return True if tag was *either* added or removed. (E.g., we had the tag and now dont. or didnt have the tag and now we do. We didn't just change the count (1 count -> 2 count would return false).
	 */
	FORCEINLINE bool UpdateTagCount(const FDNATag& Tag, int32 CountDelta)
	{
		if (CountDelta != 0)
		{
			return UpdateTagMap_Internal(Tag, CountDelta);
		}

		return false;
	}

	/**
	 * Set the specified tag count to a specific value
	 * 
	 * @param Tag			Tag to update
	 * @param Count			New count of the tag
	 * 
	 * @return True if tag was *either* added or removed. (E.g., we had the tag and now dont. or didnt have the tag and now we do. We didn't just change the count (1 count -> 2 count would return false).
	 */
	FORCEINLINE bool SetTagCount(const FDNATag& Tag, int32 NewCount)
	{
		int32 ExistingCount = 0;
		if (int32* Ptr  = ExplicitTagCountMap.Find(Tag))
		{
			ExistingCount = *Ptr;
		}

		int32 CountDelta = NewCount - ExistingCount;
		if (CountDelta != 0)
		{
			return UpdateTagMap_Internal(Tag, CountDelta);
		}

		return false;
	}

	/**
	* return the count for a specified tag 
	*
	* @param Tag			Tag to update
	*
	* @return the count of the passed in tag
	*/
	FORCEINLINE int32 GetTagCount(const FDNATag& Tag) const
	{
		if (const int32* Ptr = DNATagCountMap.Find(Tag))
		{
			return *Ptr;
		}

		return 0;
	}

	/**
	 *	Broadcasts the AnyChange event for this tag. This is called when the stack count of the backing DNA effect change.
	 *	It is up to the receiver of the broadcasted delegate to decide what to do with this.
	 */
	void Notify_StackCountChange(const FDNATag& Tag);

	/**
	 * Return delegate that can be bound to for when the specific tag's count changes to or off of zero
	 *
	 * @param Tag	Tag to get a delegate for
	 * 
	 * @return Delegate for when the specified tag's count changes to or off of zero
	 */
	FOnDNAEffectTagCountChanged& RegisterDNATagEvent(const FDNATag& Tag, EDNATagEventType::Type EventType=EDNATagEventType::NewOrRemoved);
	
	/**
	 * Return delegate that can be bound to for when the any tag's count changes to or off of zero
	 * 
	 * @return Delegate for when any tag's count changes to or off of zero
	 */
	FOnDNAEffectTagCountChanged& RegisterGenericDNAEvent()
	{
		return OnAnyTagChangeDelegate;
	}

	/** Simple accessor to the explicit DNA tag list */
	const FDNATagContainer& GetExplicitDNATags() const
	{
		return ExplicitTags;
	}

	void Reset();

private:

	struct FDelegateInfo
	{
		FOnDNAEffectTagCountChanged	OnNewOrRemove;
		FOnDNAEffectTagCountChanged	OnAnyChange;
	};

	/** Map of tag to delegate that will be fired when the count for the key tag changes to or away from zero */
	TMap<FDNATag, FDelegateInfo> DNATagEventMap;

	/** Map of tag to active count of that tag */
	TMap<FDNATag, int32> DNATagCountMap;

	/** Map of tag to explicit count of that tag. Cannot share with above map because it's not safe to merge explicit and generic counts */	
	TMap<FDNATag, int32> ExplicitTagCountMap;

	/** Delegate fired whenever any tag's count changes to or away from zero */
	FOnDNAEffectTagCountChanged OnAnyTagChangeDelegate;

	/** Container of tags that were explicitly added */
	FDNATagContainer ExplicitTags;

	/** Internal helper function to adjust the explicit tag list & corresponding maps/delegates/etc. as necessary */
	bool UpdateTagMap_Internal(const FDNATag& Tag, int32 CountDelta);
};


// -----------------------------------------------------------

/** Encapsulate require and ignore tags */
USTRUCT(BlueprintType)
struct DNAABILITIES_API FDNATagRequirements
{
	GENERATED_USTRUCT_BODY()

	/** All of these tags must be present */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = DNAModifier)
	FDNATagContainer RequireTags;

	/** None of these tags may be present */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = DNAModifier)
	FDNATagContainer IgnoreTags;

	bool	RequirementsMet(const FDNATagContainer& Container) const;
	bool	IsEmpty() const;

	static FGetDNATags	SnapshotTags(FGetDNATags TagDelegate);

	FString ToString() const;
};

USTRUCT()
struct DNAABILITIES_API FTagContainerAggregator
{
	GENERATED_USTRUCT_BODY()

	FTagContainerAggregator() : CacheIsValid(false) {}

	FTagContainerAggregator(FTagContainerAggregator&& Other)
		: CapturedActorTags(MoveTemp(Other.CapturedActorTags))
		, CapturedSpecTags(MoveTemp(Other.CapturedSpecTags))
		, ScopedTags(MoveTemp(Other.ScopedTags))
		, CachedAggregator(MoveTemp(Other.CachedAggregator))
		, CacheIsValid(Other.CacheIsValid)
	{
	}

	FTagContainerAggregator(const FTagContainerAggregator& Other)
		: CapturedActorTags(Other.CapturedActorTags)
		, CapturedSpecTags(Other.CapturedSpecTags)
		, ScopedTags(Other.ScopedTags)
		, CachedAggregator(Other.CachedAggregator)
		, CacheIsValid(Other.CacheIsValid)
	{
	}

	FTagContainerAggregator& operator=(FTagContainerAggregator&& Other)
	{
		CapturedActorTags = MoveTemp(Other.CapturedActorTags);
		CapturedSpecTags = MoveTemp(Other.CapturedSpecTags);
		ScopedTags = MoveTemp(Other.ScopedTags);
		CachedAggregator = MoveTemp(Other.CachedAggregator);
		CacheIsValid = Other.CacheIsValid;
		return *this;
	}

	FTagContainerAggregator& operator=(const FTagContainerAggregator& Other)
	{
		CapturedActorTags = Other.CapturedActorTags;
		CapturedSpecTags = Other.CapturedSpecTags;
		ScopedTags = Other.ScopedTags;
		CachedAggregator = Other.CachedAggregator;
		CacheIsValid = Other.CacheIsValid;
		return *this;
	}

	FDNATagContainer& GetActorTags();
	const FDNATagContainer& GetActorTags() const;

	FDNATagContainer& GetSpecTags();
	const FDNATagContainer& GetSpecTags() const;

	const FDNATagContainer* GetAggregatedTags() const;

private:

	UPROPERTY()
	FDNATagContainer CapturedActorTags;

	UPROPERTY()
	FDNATagContainer CapturedSpecTags;

	UPROPERTY()
	FDNATagContainer ScopedTags;

	mutable FDNATagContainer CachedAggregator;
	mutable bool CacheIsValid;
};


/** Allows blueprints to generate a DNAEffectSpec once and then reference it by handle, to apply it multiple times/multiple targets. */
USTRUCT(BlueprintType)
struct DNAABILITIES_API FDNAEffectSpecHandle
{
	GENERATED_USTRUCT_BODY()

	FDNAEffectSpecHandle();
	FDNAEffectSpecHandle(FDNAEffectSpec* DataPtr);

	TSharedPtr<FDNAEffectSpec>	Data;

	bool IsValidCache;

	void Clear()
	{
		Data.Reset();
	}

	bool IsValid() const
	{
		return Data.IsValid();
	}

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
	{
		ABILITY_LOG(Fatal, TEXT("FDNAEffectSpecHandle should not be NetSerialized"));
		return false;
	}

	/** Comparison operator */
	bool operator==(FDNAEffectSpecHandle const& Other) const
	{
		// Both invalid structs or both valid and Pointer compare (???) // deep comparison equality
		bool bBothValid = IsValid() && Other.IsValid();
		bool bBothInvalid = !IsValid() && !Other.IsValid();
		return (bBothInvalid || (bBothValid && (Data.Get() == Other.Data.Get())));
	}

	/** Comparison operator */
	bool operator!=(FDNAEffectSpecHandle const& Other) const
	{
		return !(FDNAEffectSpecHandle::operator==(Other));
	}
};

template<>
struct TStructOpsTypeTraits<FDNAEffectSpecHandle> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithCopy = true,
		WithNetSerializer = true,
		WithIdenticalViaEquality = true,
	};
};

// -----------------------------------------------------------


USTRUCT()
struct DNAABILITIES_API FMinimalReplicationTagCountMap
{
	GENERATED_USTRUCT_BODY()

	FMinimalReplicationTagCountMap()
	{
		MapID = 0;
	}

	void AddTag(const FDNATag& Tag)
	{
		MapID++;
		TagMap.FindOrAdd(Tag)++;
	}

	void RemoveTag(const FDNATag& Tag)
	{
		MapID++;
		int32& Count = TagMap.FindOrAdd(Tag);
		Count--;
		if (Count == 0)
		{
			// Remove from map so that we do not replicate
			TagMap.Remove(Tag);
		}
		else if (Count < 0)
		{
			ABILITY_LOG(Error, TEXT("FMinimapReplicationTagCountMap::RemoveTag called on Tag %s and count is now < 0"), *Tag.ToString());
			Count = 0;
		}
	}

	void AddTags(const FDNATagContainer& Container)
	{
		for (const FDNATag& Tag : Container)
		{
			AddTag(Tag);
		}
	}

	void RemoveTags(const FDNATagContainer& Container)
	{
		for (const FDNATag& Tag : Container)
		{
			RemoveTag(Tag);
		}
	}

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	TMap<FDNATag, int32>	TagMap;

	UPROPERTY()
	class UDNAAbilitySystemComponent* Owner;

	/** Comparison operator */
	bool operator==(FMinimalReplicationTagCountMap const& Other) const
	{
		return (MapID == Other.MapID);
	}

	/** Comparison operator */
	bool operator!=(FMinimalReplicationTagCountMap const& Other) const
	{
		return !(FMinimalReplicationTagCountMap::operator==(Other));
	}

	int32 MapID;
};

template<>
struct TStructOpsTypeTraits<FMinimalReplicationTagCountMap> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithCopy = true,
		WithNetSerializer = true,
		WithIdenticalViaEquality = true,
	};
};

DECLARE_MULTICAST_DELEGATE(FOnExternalDNAModifierDependencyChange);
