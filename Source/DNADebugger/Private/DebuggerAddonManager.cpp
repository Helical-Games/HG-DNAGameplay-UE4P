// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DNADebuggerAddonManager.h"
#include "Engine/World.h"
#include "DNADebuggerCategoryReplicator.h"
#include "DNADebuggerExtension.h"
#include "DNADebuggerConfig.h"

FDNADebuggerAddonManager::FDNADebuggerAddonManager()
{
	NumVisibleCategories = 0;
}

void FDNADebuggerAddonManager::RegisterCategory(FName CategoryName, IDNADebugger::FOnGetCategory MakeInstanceDelegate, EDNADebuggerCategoryState CategoryState, int32 SlotIdx)
{
	FDNADebuggerCategoryInfo NewInfo;
	NewInfo.MakeInstanceDelegate = MakeInstanceDelegate;
	NewInfo.DefaultCategoryState = CategoryState;

	UDNADebuggerConfig* MutableToolConfig = UDNADebuggerConfig::StaticClass()->GetDefaultObject<UDNADebuggerConfig>();
	uint8 NewCategoryState = (uint8)CategoryState;
	MutableToolConfig->UpdateCategoryConfig(CategoryName, SlotIdx, NewCategoryState);

	NewInfo.CategoryState = (EDNADebuggerCategoryState)NewCategoryState;
	NewInfo.SlotIdx = SlotIdx;

	CategoryMap.Add(CategoryName, NewInfo);

	// create and destroy single instance to handle input configurators
	FDNADebuggerInputHandlerConfig::CurrentCategoryName = CategoryName;
	TSharedRef<FDNADebuggerCategory> DummyRef = MakeInstanceDelegate.Execute();
	FDNADebuggerInputHandlerConfig::CurrentCategoryName = NAME_None;
}

void FDNADebuggerAddonManager::UnregisterCategory(FName CategoryName)
{
	CategoryMap.Remove(CategoryName);
}

void FDNADebuggerAddonManager::NotifyCategoriesChanged()
{
	struct FSlotInfo
	{
		FName CategoryName;
		int32 CategoryId;
		int32 SlotIdx;

		bool operator<(const FSlotInfo& Other) const { return (SlotIdx == Other.SlotIdx) ? (CategoryName < Other.CategoryName) : (SlotIdx < Other.SlotIdx); }
	};

	TArray<FSlotInfo> AssignList;
	TSet<int32> OccupiedSlots;
	int32 CategoryId = 0;
	for (auto It : CategoryMap)
	{
		if (It.Value.CategoryState == EDNADebuggerCategoryState::Hidden)
		{
			continue;
		}

		const int32 SanitizedSlotIdx = (It.Value.SlotIdx < 0) ? INDEX_NONE : FMath::Min(100, It.Value.SlotIdx);

		FSlotInfo SlotInfo;
		SlotInfo.CategoryId = CategoryId;
		SlotInfo.CategoryName = It.Key;
		SlotInfo.SlotIdx = SanitizedSlotIdx;
		CategoryId++;

		AssignList.Add(SlotInfo);

		if (SanitizedSlotIdx >= 0)
		{
			OccupiedSlots.Add(SanitizedSlotIdx);
		}
	}

	NumVisibleCategories = AssignList.Num();
	AssignList.Sort();

	int32 MaxSlotIdx = 0;
	for (int32 Idx = 0; Idx < AssignList.Num(); Idx++)
	{
		FSlotInfo& SlotInfo = AssignList[Idx];
		if (SlotInfo.SlotIdx == INDEX_NONE)
		{
			int32 FreeSlotIdx = 0;
			while (OccupiedSlots.Contains(FreeSlotIdx))
			{
				FreeSlotIdx++;
			}

			SlotInfo.SlotIdx = FreeSlotIdx;
			OccupiedSlots.Add(FreeSlotIdx);
		}

		MaxSlotIdx = FMath::Max(MaxSlotIdx, SlotInfo.SlotIdx);
	}

	SlotMap.Reset();
	SlotNames.Reset();
	SlotMap.AddDefaulted(MaxSlotIdx + 1);
	SlotNames.AddDefaulted(MaxSlotIdx + 1);

	for (int32 Idx = 0; Idx < AssignList.Num(); Idx++)
	{
		FSlotInfo& SlotInfo = AssignList[Idx];

		if (SlotNames[SlotInfo.SlotIdx].Len())
		{
			SlotNames[SlotInfo.SlotIdx] += TEXT('+');
		}

		SlotNames[SlotInfo.SlotIdx] += SlotInfo.CategoryName.ToString();
		SlotMap[SlotInfo.SlotIdx].Add(SlotInfo.CategoryId);
	}

	OnCategoriesChanged.Broadcast();
}

void FDNADebuggerAddonManager::CreateCategories(ADNADebuggerCategoryReplicator& Owner, TArray<TSharedRef<FDNADebuggerCategory> >& CategoryObjects)
{
	UWorld* World = Owner.GetWorld();
	const ENetMode NetMode = World->GetNetMode();
	const bool bHasAuthority = (NetMode != NM_Client);
	const bool bIsLocal = (NetMode != NM_DedicatedServer);
	const bool bIsSimulate = FDNADebuggerAddonBase::IsSimulateInEditor();

	TArray<TSharedRef<FDNADebuggerCategory> > UnsortedCategories;
	for (auto It : CategoryMap)
	{
		FDNADebuggerInputHandlerConfig::CurrentCategoryName = It.Key;
		if (It.Value.CategoryState == EDNADebuggerCategoryState::Hidden)
		{
			continue;
		}

		TSharedRef<FDNADebuggerCategory> CategoryObjectRef = It.Value.MakeInstanceDelegate.Execute();
		FDNADebuggerCategory& CategoryObject = CategoryObjectRef.Get();
		CategoryObject.RepOwner = &Owner;
		CategoryObject.CategoryId = CategoryObjects.Num();
		CategoryObject.CategoryName = It.Key;
		CategoryObject.bHasAuthority = bHasAuthority;
		CategoryObject.bIsLocal = bIsLocal;
		CategoryObject.bIsEnabled =
			(It.Value.CategoryState == EDNADebuggerCategoryState::EnabledInGameAndSimulate) ||
			(It.Value.CategoryState == EDNADebuggerCategoryState::EnabledInGame && !bIsSimulate) ||
			(It.Value.CategoryState == EDNADebuggerCategoryState::EnabledInSimulate && bIsSimulate);

		UnsortedCategories.Add(CategoryObjectRef);
	}

	FDNADebuggerInputHandlerConfig::CurrentCategoryName = NAME_None;

	// sort by slots for drawing order
	CategoryObjects.Reset();
	for (int32 SlotIdx = 0; SlotIdx < SlotMap.Num(); SlotIdx++)
	{
		for (int32 Idx = 0; Idx < SlotMap[SlotIdx].Num(); Idx++)
		{
			const int32 CategoryId = SlotMap[SlotIdx][Idx];
			CategoryObjects.Add(UnsortedCategories[CategoryId]);
		}
	}
}

void FDNADebuggerAddonManager::RegisterExtension(FName ExtensionName, IDNADebugger::FOnGetExtension MakeInstanceDelegate)
{
	FDNADebuggerExtensionInfo NewInfo;
	NewInfo.MakeInstanceDelegate = MakeInstanceDelegate;
	NewInfo.bDefaultEnabled = true;

	uint8 UseExtension = NewInfo.bDefaultEnabled ? 1 : 0;
	UDNADebuggerConfig* MutableToolConfig = UDNADebuggerConfig::StaticClass()->GetDefaultObject<UDNADebuggerConfig>();
	MutableToolConfig->UpdateExtensionConfig(ExtensionName, UseExtension);

	NewInfo.bEnabled = UseExtension > 0;

	ExtensionMap.Add(ExtensionName, NewInfo);

	// create and destroy single instance to handle input configurators
	FDNADebuggerInputHandlerConfig::CurrentExtensionName = ExtensionName;
	TSharedRef<FDNADebuggerExtension> DummyRef = MakeInstanceDelegate.Execute();
	FDNADebuggerInputHandlerConfig::CurrentExtensionName = NAME_None;
}

void FDNADebuggerAddonManager::UnregisterExtension(FName ExtensionName)
{
	ExtensionMap.Remove(ExtensionName);
}

void FDNADebuggerAddonManager::NotifyExtensionsChanged()
{
	OnExtensionsChanged.Broadcast();
}

void FDNADebuggerAddonManager::CreateExtensions(ADNADebuggerCategoryReplicator& Replicator, TArray<TSharedRef<FDNADebuggerExtension> >& ExtensionObjects)
{
	ExtensionObjects.Reset();
	for (auto It : ExtensionMap)
	{
		if (It.Value.bEnabled)
		{
			FDNADebuggerInputHandlerConfig::CurrentExtensionName = It.Key;

			TSharedRef<FDNADebuggerExtension> ExtensionObjectRef = It.Value.MakeInstanceDelegate.Execute();
			FDNADebuggerExtension& ExtensionObject = ExtensionObjectRef.Get();
			ExtensionObject.RepOwner = &Replicator;

			ExtensionObjects.Add(ExtensionObjectRef);
		}
	}

	FDNADebuggerInputHandlerConfig::CurrentExtensionName = NAME_None;
}

void FDNADebuggerAddonManager::UpdateFromConfig()
{
	UDNADebuggerConfig* ToolConfig = UDNADebuggerConfig::StaticClass()->GetDefaultObject<UDNADebuggerConfig>();
	if (ToolConfig == nullptr)
	{
		return;
	}

	bool bCategoriesChanged = false;
	for (auto& It : CategoryMap)
	{
		for (int32 Idx = 0; Idx < ToolConfig->Categories.Num(); Idx++)
		{
			const FDNADebuggerCategoryConfig& ConfigData = ToolConfig->Categories[Idx];
			if (*ConfigData.CategoryName == It.Key)
			{
				const bool bDefaultActiveInGame = (It.Value.DefaultCategoryState == EDNADebuggerCategoryState::EnabledInGame) || (It.Value.DefaultCategoryState == EDNADebuggerCategoryState::EnabledInGameAndSimulate);
				const bool bDefaultActiveInSimulate = (It.Value.DefaultCategoryState == EDNADebuggerCategoryState::EnabledInSimulate) || (It.Value.DefaultCategoryState == EDNADebuggerCategoryState::EnabledInGameAndSimulate);

				const bool bActiveInGame = (ConfigData.ActiveInGame == EDNADebuggerOverrideMode::UseDefault) ? bDefaultActiveInGame : (ConfigData.ActiveInGame == EDNADebuggerOverrideMode::Enable);
				const bool bActiveInSimulate = (ConfigData.ActiveInSimulate == EDNADebuggerOverrideMode::UseDefault) ? bDefaultActiveInSimulate : (ConfigData.ActiveInSimulate == EDNADebuggerOverrideMode::Enable);

				EDNADebuggerCategoryState NewCategoryState =
					bActiveInGame && bActiveInSimulate ? EDNADebuggerCategoryState::EnabledInGameAndSimulate :
					bActiveInGame ? EDNADebuggerCategoryState::EnabledInGame :
					bActiveInSimulate ? EDNADebuggerCategoryState::EnabledInSimulate :
					EDNADebuggerCategoryState::Disabled;

				bCategoriesChanged = bCategoriesChanged || (It.Value.SlotIdx != ConfigData.SlotIdx) || (It.Value.CategoryState != NewCategoryState);
				It.Value.SlotIdx = ConfigData.SlotIdx;
				It.Value.CategoryState = NewCategoryState;
				break;
			}
		}
	}

	bool bExtensionsChanged = false;
	for (auto& It : ExtensionMap)
	{
		for (int32 Idx = 0; Idx < ToolConfig->Extensions.Num(); Idx++)
		{
			const FDNADebuggerExtensionConfig& ConfigData = ToolConfig->Extensions[Idx];
			if (*ConfigData.ExtensionName == It.Key)
			{
				const bool bWantsEnabled = (ConfigData.UseExtension == EDNADebuggerOverrideMode::UseDefault) ? It.Value.bDefaultEnabled : (ConfigData.UseExtension == EDNADebuggerOverrideMode::Enable);
				bExtensionsChanged = bExtensionsChanged || (It.Value.bEnabled != bWantsEnabled);
				It.Value.bEnabled = bWantsEnabled;
				break;
			}
		}
	}

	if (bCategoriesChanged)
	{
		NotifyCategoriesChanged();
	}
	
	if (bExtensionsChanged)
	{
		NotifyExtensionsChanged();
	}
}
