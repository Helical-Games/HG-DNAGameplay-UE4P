// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "Misc/StringAssetReference.h"
#include "DNATagContainer.h"
#include "Engine/DataAsset.h"
#include "DNAEffectTypes.h"
#include "DNACueSet.generated.h"

USTRUCT()
struct FDNACueNotifyData
{
	GENERATED_USTRUCT_BODY()

	FDNACueNotifyData()
	: LoadedDNACueClass(nullptr)
	, ParentDataIdx( INDEX_NONE )
	{
	}

	UPROPERTY(EditAnywhere, Category=DNACue)
	FDNATag DNACueTag;

	UPROPERTY(EditAnywhere, Category=DNACue, meta=(AllowedClasses="DNACueNotify"))
	FStringAssetReference DNACueNotifyObj;

	UPROPERTY(transient)
	UClass* LoadedDNACueClass;

	int32 ParentDataIdx;
};

struct FDNACueReferencePair
{
	FDNATag DNACueTag;
	FStringAssetReference StringRef;

	FDNACueReferencePair(const FDNATag& InDNACueTag, const FStringAssetReference& InStringRef)
		: DNACueTag(InDNACueTag)
		, StringRef(InStringRef)
	{}
};

/**
 *	A set of DNA cue actors to handle DNA cue events
 */
UCLASS()
class DNAABILITIES_API UDNACueSet : public UDataAsset
{
	GENERATED_UCLASS_BODY()

	/** Handles the cue event by spawning the cue actor. Returns true if the event was handled. */
	virtual bool HandleDNACue(AActor* TargetActor, FDNATag DNACueTag, EDNACueEvent::Type EventType, const FDNACueParameters& Parameters);

	/** Adds a list of cues to the set */
	virtual void AddCues(const TArray<FDNACueReferencePair>& CuesToAdd);

	/** Removes all cues from the set matching any of the supplied tags */
	virtual void RemoveCuesByTags(const FDNATagContainer& TagsToRemove);

	/** Removes all cues from the set matching the supplied string refs */
	virtual void RemoveCuesByStringRefs(const TArray<FStringAssetReference>& CuesToRemove);

	/** Nulls reference to the loaded class. Note this doesn't remove the entire cue from the internal data structure, just the hard ref to the loaded class */
	virtual void RemoveLoadedClass(UClass* Class);

	/** Returns filenames of everything we know about (loaded or not) */
	virtual void GetFilenames(TArray<FString>& Filenames) const;

	virtual void GetStringAssetReferences(TArray<FStringAssetReference>& List) const;

#if WITH_EDITOR

	void CopyCueDataToSetForEditorPreview(FDNATag Tag, UDNACueSet* DestinationSet);

	/** Updates an existing cue */
	virtual void UpdateCueByStringRefs(const FStringAssetReference& CueToRemove, FString NewPath);

#endif

	/** Removes all cues from the set */
	virtual void Empty();

	virtual void PrintCues() const;
	
	UPROPERTY(EditAnywhere, Category=CueSet)
	TArray<FDNACueNotifyData> DNACueData;

	/** Maps DNACue Tag to index into above DNACues array. */
	TMap<FDNATag, int32> DNACueDataMap;

	static FDNATag	BaseDNACueTag();

protected:
	virtual bool HandleDNACueNotify_Internal(AActor* TargetActor, int32 DataIdx, EDNACueEvent::Type EventType, FDNACueParameters& Parameters);
	virtual void BuildAccelerationMap_Internal();
};
