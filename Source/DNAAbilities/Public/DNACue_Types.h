// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "DNATagContainer.h"
#include "DNAEffectTypes.h"
#include "DNAPrediction.h"
#include "UObject/ObjectKey.h"
#include "DNAEffect.h"
#include "DNACue_Types.generated.h"

class ADNACueNotify_Actor;
class UDNAAbilitySystemComponent;
class UDNACueSet;

UENUM()
enum class EDNACuePayloadType : uint8
{
	EffectContext,
	CueParameters,
	FromSpec,
};

/** Structure to keep track of pending DNA cues that haven't been applied yet. */
USTRUCT()
struct FDNACuePendingExecute
{
	GENERATED_USTRUCT_BODY()

	FDNACuePendingExecute()
	: PayloadType(EDNACuePayloadType::EffectContext)
	, OwningComponent(NULL)
	{
	}
	
	TArray<FDNATag, TInlineAllocator<1> > DNACueTags;
	
	/** Prediction key that spawned this cue */
	UPROPERTY()
	FPredictionKey PredictionKey;

	/** What type of payload is attached to this cue */
	UPROPERTY()
	EDNACuePayloadType PayloadType;

	/** What component to send the cue on */
	UPROPERTY()
	UDNAAbilitySystemComponent* OwningComponent;

	/** If this cue is from a spec, here's the copy of that spec */
	UPROPERTY()
	FDNAEffectSpecForRPC FromSpec;

	/** Store the full cue parameters or just the effect context depending on type */
	UPROPERTY()
	FDNACueParameters CueParameters;
};

/** Struct for pooling and preallocating DNAcuenotify_actor classes. This data is per world and used to track what actors are available to recycle and which classes need to preallocate instances of those actors */
USTRUCT()
struct FPreallocationInfo
{
	GENERATED_USTRUCT_BODY()

	TMap<UClass*, TArray<ADNACueNotify_Actor*> >	PreallocatedInstances;

	UPROPERTY(transient)
	TArray<ADNACueNotify_Actor*>	ClassesNeedingPreallocation;

	FObjectKey OwningWorldKey;
};

/** Struct that is used by the DNAcue manager to tie an instanced DNAcue to the calling gamecode. Usually this is just the target actor, but can also be unique per instigator/sourceobject */
struct FGCNotifyActorKey
{
	FGCNotifyActorKey()
	{

	}

	FGCNotifyActorKey(AActor* InTargetActor, UClass* InCueClass, AActor* InInstigatorActor=nullptr, const UObject* InSourceObj=nullptr)
	{
		TargetActor = FObjectKey(InTargetActor);
		OptionalInstigatorActor = FObjectKey(InInstigatorActor);
		OptionalSourceObject = FObjectKey(InSourceObj);
		CueClass = FObjectKey(InCueClass);
	}

	

	FObjectKey	TargetActor;
	FObjectKey	OptionalInstigatorActor;
	FObjectKey	OptionalSourceObject;
	FObjectKey	CueClass;

	FORCEINLINE bool operator==(const FGCNotifyActorKey& Other) const
	{
		return TargetActor == Other.TargetActor && CueClass == Other.CueClass &&
				OptionalInstigatorActor == Other.OptionalInstigatorActor && OptionalSourceObject == Other.OptionalSourceObject;
	}
};

FORCEINLINE uint32 GetTypeHash(const FGCNotifyActorKey& Key)
{
	return GetTypeHash(Key.TargetActor)	^
			GetTypeHash(Key.OptionalInstigatorActor) ^
			GetTypeHash(Key.OptionalSourceObject) ^
			GetTypeHash(Key.CueClass);
}

/**
 *	FScopedDNACueSendContext
 *	Add this around code that sends multiple DNA cues to allow grouping them into a smalkler number of cues for more efficient networking
 */
struct DNAABILITIES_API FScopedDNACueSendContext
{
	FScopedDNACueSendContext();
	~FScopedDNACueSendContext();
};

/** Delegate for when GC notifies are added or removed from manager */
DECLARE_MULTICAST_DELEGATE(FOnDNACueNotifyChange);