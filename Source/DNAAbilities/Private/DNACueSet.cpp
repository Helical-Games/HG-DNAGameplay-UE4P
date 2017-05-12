// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNACueSet.h"
#include "DNATagsManager.h"
#include "DNATagsModule.h"
#include "AbilitySystemGlobals.h"
#include "DNACueNotify_Actor.h"
#include "DNACueNotify_Static.h"
#include "DNACueManager.h"

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	UDNACueSet
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

UDNACueSet::UDNACueSet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

bool UDNACueSet::HandleDNACue(AActor* TargetActor, FDNATag DNACueTag, EDNACueEvent::Type EventType, const FDNACueParameters& Parameters)
{
	// DNACueTags could have been removed from the dictionary but not content. When the content is resaved the old tag will be cleaned up, but it could still come through here
	// at runtime. Since we only populate the map with dictionary DNAcue tags, we may not find it here.
	int32* Ptr = DNACueDataMap.Find(DNACueTag);
	if (Ptr)
	{
		int32 DataIdx = *Ptr;

		// TODO - resolve internal handler modifying params before passing them on with new const-ref params.
		FDNACueParameters writableParameters = Parameters;
		return HandleDNACueNotify_Internal(TargetActor, DataIdx, EventType, writableParameters);
	}

	return false;
}

void UDNACueSet::AddCues(const TArray<FDNACueReferencePair>& CuesToAdd)
{
	if (CuesToAdd.Num() > 0)
	{
		for (const FDNACueReferencePair& CueRefPair : CuesToAdd)
		{
			const FDNATag& DNACueTag = CueRefPair.DNACueTag;
			const FStringAssetReference& StringRef = CueRefPair.StringRef;

			// Check for duplicates: we may want to remove this eventually (allow multiple GC notifies to handle same event)
			bool bDupe = false;
			for (FDNACueNotifyData& Data : DNACueData)
			{
				if (Data.DNACueTag == DNACueTag)
				{
					if (StringRef.ToString() != Data.DNACueNotifyObj.ToString())
					{
						ABILITY_LOG(Warning, TEXT("AddDNACueData_Internal called for [%s,%s] when it already existed [%s,%s]. Skipping."), *DNACueTag.ToString(), *StringRef.ToString(), *Data.DNACueTag.ToString(), *Data.DNACueNotifyObj.ToString());
					}
					bDupe = true;
					break;
				}
			}

			if (bDupe)
			{
				continue;
			}

			FDNACueNotifyData NewData;
			NewData.DNACueNotifyObj = StringRef;
			NewData.DNACueTag = DNACueTag;

			DNACueData.Add(NewData);
		}

		BuildAccelerationMap_Internal();
	}
}

void UDNACueSet::RemoveCuesByTags(const FDNATagContainer& TagsToRemove)
{
}

void UDNACueSet::RemoveCuesByStringRefs(const TArray<FStringAssetReference>& CuesToRemove)
{
	for (const FStringAssetReference& StringRefToRemove : CuesToRemove)
	{
		for (int32 idx = 0; idx < DNACueData.Num(); ++idx)
		{
			if (DNACueData[idx].DNACueNotifyObj == StringRefToRemove)
			{
				DNACueData.RemoveAt(idx);
				BuildAccelerationMap_Internal();
				break;
			}
		}
	}
}

void UDNACueSet::RemoveLoadedClass(UClass* Class)
{
	for (int32 idx = 0; idx < DNACueData.Num(); ++idx)
	{
		if (DNACueData[idx].LoadedDNACueClass == Class)
		{
			DNACueData[idx].LoadedDNACueClass = nullptr;
		}
	}
}

void UDNACueSet::GetFilenames(TArray<FString>& Filenames) const
{
	Filenames.Reserve(DNACueData.Num());
	for (const FDNACueNotifyData& Data : DNACueData)
	{
		Filenames.Add(Data.DNACueNotifyObj.GetLongPackageName());
	}
}

void UDNACueSet::GetStringAssetReferences(TArray<FStringAssetReference>& List) const
{
	List.Reserve(DNACueData.Num());
	for (const FDNACueNotifyData& Data : DNACueData)
	{
		List.Add(Data.DNACueNotifyObj);
	}
}

#if WITH_EDITOR
void UDNACueSet::UpdateCueByStringRefs(const FStringAssetReference& CueToRemove, FString NewPath)
{
	for (int32 idx = 0; idx < DNACueData.Num(); ++idx)
	{
		if (DNACueData[idx].DNACueNotifyObj == CueToRemove)
		{
			DNACueData[idx].DNACueNotifyObj = NewPath;
			BuildAccelerationMap_Internal();
			break;
		}
	}
}

void UDNACueSet::CopyCueDataToSetForEditorPreview(FDNATag Tag, UDNACueSet* DestinationSet)
{
	const int32 SourceIdx = DNACueData.IndexOfByPredicate([&Tag](const FDNACueNotifyData& Data) { return Data.DNACueTag == Tag; });
	if (SourceIdx == INDEX_NONE)
	{
		// Doesn't exist in source, so nothing to copy
		return;
	}

	int32 DestIdx = DestinationSet->DNACueData.IndexOfByPredicate([&Tag](const FDNACueNotifyData& Data) { return Data.DNACueTag == Tag; });
	if (DestIdx == INDEX_NONE)
	{
		// wholesale copy
		DestIdx = DestinationSet->DNACueData.Num();
		DestinationSet->DNACueData.Add(DNACueData[SourceIdx]);

		DestinationSet->BuildAccelerationMap_Internal();
	}
	else
	{
		// Update only if we need to
		if (DestinationSet->DNACueData[DestIdx].DNACueNotifyObj.IsValid() == false)
		{
			DestinationSet->DNACueData[DestIdx].DNACueNotifyObj = DNACueData[SourceIdx].DNACueNotifyObj;
			DestinationSet->DNACueData[DestIdx].LoadedDNACueClass = DNACueData[SourceIdx].LoadedDNACueClass;
		}

	}

	// Start async load
	UDNACueManager* CueManager = UDNAAbilitySystemGlobals::Get().GetDNACueManager();
	if (ensure(CueManager))
	{
		CueManager->StreamableManager.SimpleAsyncLoad(DestinationSet->DNACueData[DestIdx].DNACueNotifyObj);
	}
}

#endif

void UDNACueSet::Empty()
{
	DNACueData.Empty();
	DNACueDataMap.Empty();
}

void UDNACueSet::PrintCues() const
{
	FDNATagContainer AllDNACueTags = UDNATagsManager::Get().RequestDNATagChildren(BaseDNACueTag());

	for (FDNATag ThisDNACueTag : AllDNACueTags)
	{
		int32 idx = DNACueDataMap.FindChecked(ThisDNACueTag);
		if (idx != INDEX_NONE)
		{
			ABILITY_LOG(Warning, TEXT("   %s -> %d"), *ThisDNACueTag.ToString(), idx);
		}
		else
		{
			ABILITY_LOG(Warning, TEXT("   %s -> unmapped"), *ThisDNACueTag.ToString());
		}
	}
}

bool UDNACueSet::HandleDNACueNotify_Internal(AActor* TargetActor, int32 DataIdx, EDNACueEvent::Type EventType, FDNACueParameters& Parameters)
{	
	bool bReturnVal = false;

	if (DataIdx != INDEX_NONE)
	{
		check(DNACueData.IsValidIndex(DataIdx));

		FDNACueNotifyData& CueData = DNACueData[DataIdx];

		Parameters.MatchedTagName = CueData.DNACueTag;

		// If object is not loaded yet
		if (CueData.LoadedDNACueClass == nullptr)
		{
			// Ignore removed events if this wasn't already loaded (only call Removed if we handled OnActive/WhileActive)
			if (EventType == EDNACueEvent::Removed)
			{
				return false;
			}

			// See if the object is loaded but just not hooked up here
			CueData.LoadedDNACueClass = FindObject<UClass>(nullptr, *CueData.DNACueNotifyObj.ToString());
			if (CueData.LoadedDNACueClass == nullptr)
			{
				// Not loaded: start async loading and return
				UDNACueManager* CueManager = UDNAAbilitySystemGlobals::Get().GetDNACueManager();
				if (ensure(CueManager))
				{
					CueManager->StreamableManager.SimpleAsyncLoad(CueData.DNACueNotifyObj);

					ABILITY_LOG(Display, TEXT("DNACueNotify %s was not loaded when DNACue was invoked. Starting async loading."), *CueData.DNACueNotifyObj.ToString());
				}
				return false;
			}
		}

		// Handle the Notify if we found something
		if (UDNACueNotify_Static* NonInstancedCue = Cast<UDNACueNotify_Static>(CueData.LoadedDNACueClass->ClassDefaultObject))
		{
			if (NonInstancedCue->HandlesEvent(EventType))
			{
				NonInstancedCue->HandleDNACue(TargetActor, EventType, Parameters);
				bReturnVal = true;
				if (!NonInstancedCue->IsOverride)
				{
					HandleDNACueNotify_Internal(TargetActor, CueData.ParentDataIdx, EventType, Parameters);
				}
			}
			else
			{
				//Didn't even handle it, so IsOverride should not apply.
				HandleDNACueNotify_Internal(TargetActor, CueData.ParentDataIdx, EventType, Parameters);
			}
		}
		else if (ADNACueNotify_Actor* InstancedCue = Cast<ADNACueNotify_Actor>(CueData.LoadedDNACueClass->ClassDefaultObject))
		{
			if (InstancedCue->HandlesEvent(EventType))
			{
				UDNACueManager* CueManager = UDNAAbilitySystemGlobals::Get().GetDNACueManager();
				if (ensure(CueManager))
				{
					//Get our instance. We should probably have a flag or something to determine if we want to reuse or stack instances. That would mean changing our map to have a list of active instances.
					ADNACueNotify_Actor* SpawnedInstancedCue = CueManager->GetInstancedCueActor(TargetActor, CueData.LoadedDNACueClass, Parameters);
					if (ensure(SpawnedInstancedCue))
					{
						SpawnedInstancedCue->HandleDNACue(TargetActor, EventType, Parameters);
						bReturnVal = true;
						if (!SpawnedInstancedCue->IsOverride)
						{
							HandleDNACueNotify_Internal(TargetActor, CueData.ParentDataIdx, EventType, Parameters);
						}
					}
				}
			}
			else
			{
				//Didn't even handle it, so IsOverride should not apply.
				HandleDNACueNotify_Internal(TargetActor, CueData.ParentDataIdx, EventType, Parameters);
			}
		}
	}

	return bReturnVal;
}

void UDNACueSet::BuildAccelerationMap_Internal()
{
	// ---------------------------------------------------------
	//	Build up the rest of the acceleration map: every DNACue tag should have an entry in the map that points to the index into DNACueData to use when it is invoked.
	//	(or to -1 if no DNACueNotify is associated with that tag)
	// 
	// ---------------------------------------------------------

	DNACueDataMap.Empty();
	DNACueDataMap.Add(BaseDNACueTag()) = INDEX_NONE;

	for (int32 idx = 0; idx < DNACueData.Num(); ++idx)
	{
		DNACueDataMap.FindOrAdd(DNACueData[idx].DNACueTag) = idx;
	}

	FDNATagContainer AllDNACueTags = UDNATagsManager::Get().RequestDNATagChildren(BaseDNACueTag());


	// Create entries for children.
	// E.g., if "a.b" notify exists but "a.b.c" does not, point "a.b.c" entry to "a.b"'s notify.
	for (FDNATag ThisDNACueTag : AllDNACueTags)
	{
		if (DNACueDataMap.Contains(ThisDNACueTag))
		{
			continue;
		}

		FDNATag Parent = ThisDNACueTag.RequestDirectParent();

		int32 ParentValue = DNACueDataMap.FindChecked(Parent);
		DNACueDataMap.Add(ThisDNACueTag, ParentValue);
	}


	// Build up parentIdx on each item in DNACUeData
	for (FDNACueNotifyData& Data : DNACueData)
	{
		FDNATag Parent = Data.DNACueTag.RequestDirectParent();
		while (Parent != BaseDNACueTag() && Parent.IsValid())
		{
			int32* idxPtr = DNACueDataMap.Find(Parent);
			if (idxPtr)
			{
				Data.ParentDataIdx = *idxPtr;
				break;
			}
			Parent = Parent.RequestDirectParent();
			if (Parent.GetTagName() == NAME_None)
			{
				break;
			}
		}
	}

	// PrintDNACueNotifyMap();
}

FDNATag UDNACueSet::BaseDNACueTag()
{
	// Note we should not cache this off as a static variable, since for new projects the DNACue tag will not be found until one is created.
	return FDNATag::RequestDNATag(TEXT("DNACue"), false);
}
