// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DNADebuggerAddonManager.h"
#include "DNADebuggerConfig.h"
#include "DNADebugger.h"
#include "DNADebuggerAddonManager.h"

UDNADebuggerConfig::UDNADebuggerConfig(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ActivationKey = EKeys::Apostrophe;

	CategoryRowNextKey = EKeys::Add;
	CategoryRowPrevKey = EKeys::Subtract;

	CategorySlot0 = EKeys::NumPadZero;
	CategorySlot1 = EKeys::NumPadOne;
	CategorySlot2 = EKeys::NumPadTwo;
	CategorySlot3 = EKeys::NumPadThree;
	CategorySlot4 = EKeys::NumPadFour;
	CategorySlot5 = EKeys::NumPadFive;
	CategorySlot6 = EKeys::NumPadSix;
	CategorySlot7 = EKeys::NumPadSeven;
	CategorySlot8 = EKeys::NumPadEight;
	CategorySlot9 = EKeys::NumPadNine;

	DebugCanvasPaddingLeft = 10.0f;
	DebugCanvasPaddingRight = 10.0f;
	DebugCanvasPaddingTop = 10.0f;
	DebugCanvasPaddingBottom = 10.0f;
}

void UDNADebuggerConfig::UpdateCategoryConfig(const FName CategoryName, int32& SlotIdx, uint8& CategoryState)
{
	KnownCategoryNames.AddUnique(CategoryName);

	int32 FoundIdx = INDEX_NONE;
	for (int32 Idx = 0; Idx < Categories.Num(); Idx++)
	{
		if (*Categories[Idx].CategoryName == CategoryName)
		{
			FoundIdx = Idx;
			break;
		}
	}

	if (FoundIdx == INDEX_NONE)
	{
		FDNADebuggerCategoryConfig NewConfigData;
		NewConfigData.CategoryName = CategoryName.ToString();

		FoundIdx = Categories.Add(NewConfigData);
	}

	const EDNADebuggerCategoryState EnumState = (EDNADebuggerCategoryState)CategoryState;
	if (Categories.IsValidIndex(FoundIdx))
	{
		FDNADebuggerCategoryConfig& ConfigData = Categories[FoundIdx];
		if (ConfigData.bOverrideSlotIdx)
		{
			SlotIdx = ConfigData.SlotIdx;
		}
		else
		{
			ConfigData.SlotIdx = SlotIdx;
		}
		
		const bool bDefaultActiveInGame = (EnumState == EDNADebuggerCategoryState::EnabledInGame) || (EnumState == EDNADebuggerCategoryState::EnabledInGameAndSimulate);
		const bool bDefaultActiveInSimulate = (EnumState == EDNADebuggerCategoryState::EnabledInSimulate) || (EnumState == EDNADebuggerCategoryState::EnabledInGameAndSimulate);
		const bool bDefaultHidden = (EnumState == EDNADebuggerCategoryState::Hidden);
		
		const bool bActiveInGame = (ConfigData.ActiveInGame == EDNADebuggerOverrideMode::UseDefault) ? bDefaultActiveInGame : (ConfigData.ActiveInGame == EDNADebuggerOverrideMode::Enable);
		const bool bActiveInSimulate = (ConfigData.ActiveInSimulate == EDNADebuggerOverrideMode::UseDefault) ? bDefaultActiveInSimulate : (ConfigData.ActiveInSimulate == EDNADebuggerOverrideMode::Enable);
		const bool bIsHidden = (ConfigData.Hidden == EDNADebuggerOverrideMode::UseDefault) ? bDefaultHidden : (ConfigData.Hidden == EDNADebuggerOverrideMode::Enable);

		CategoryState = (uint8)(
			bIsHidden ? EDNADebuggerCategoryState::Hidden :
			bActiveInGame && bActiveInSimulate ? EDNADebuggerCategoryState::EnabledInGameAndSimulate :
			bActiveInGame ? EDNADebuggerCategoryState::EnabledInGame :
			bActiveInSimulate ? EDNADebuggerCategoryState::EnabledInSimulate :
			EDNADebuggerCategoryState::Disabled);
	}
}

void UDNADebuggerConfig::UpdateExtensionConfig(const FName ExtensionName, uint8& UseExtension)
{
	KnownExtensionNames.AddUnique(ExtensionName);

	int32 FoundIdx = INDEX_NONE;
	for (int32 Idx = 0; Idx < Extensions.Num(); Idx++)
	{
		if (*Extensions[Idx].ExtensionName == ExtensionName)
		{
			FoundIdx = Idx;
			break;
		}
	}

	if (FoundIdx == INDEX_NONE)
	{
		FDNADebuggerExtensionConfig NewConfigData;
		NewConfigData.ExtensionName = ExtensionName.ToString();

		FoundIdx = Extensions.Add(NewConfigData);
	}

	if (Extensions.IsValidIndex(FoundIdx))
	{
		FDNADebuggerExtensionConfig& ConfigData = Extensions[FoundIdx];

		const bool bDefaultUseExtension = (UseExtension != 0);
		const bool bUseExtension = (ConfigData.UseExtension == EDNADebuggerOverrideMode::UseDefault) ? bDefaultUseExtension : (ConfigData.UseExtension == EDNADebuggerOverrideMode::Enable);

		UseExtension = bUseExtension ? 1 : 0;
	}
}

void UDNADebuggerConfig::UpdateCategoryInputConfig(const FName CategoryName, const FName InputName, FName& KeyName, FDNADebuggerInputModifier& KeyModifier)
{
	int32 FoundCategoryIdx = INDEX_NONE;
	for (int32 Idx = 0; Idx < Categories.Num(); Idx++)
	{
		if (*Categories[Idx].CategoryName == CategoryName)
		{
			FoundCategoryIdx = Idx;
			break;
		}
	}

	if (!Categories.IsValidIndex(FoundCategoryIdx))
	{
		return;
	}

	FDNADebuggerCategoryConfig& OuterConfigData = Categories[FoundCategoryIdx];
	KnownCategoryInputNames.Add(CategoryName, InputName);

	int32 FoundIdx = INDEX_NONE;
	for (int32 Idx = 0; Idx < OuterConfigData.InputHandlers.Num(); Idx++)
	{
		if (*OuterConfigData.InputHandlers[Idx].ConfigName == InputName)
		{
			FDNADebuggerInputConfig& ConfigData = OuterConfigData.InputHandlers[Idx];
			KeyName = ConfigData.Key.GetFName();
			KeyModifier.bShift = ConfigData.bModShift;
			KeyModifier.bCtrl = ConfigData.bModCtrl;
			KeyModifier.bAlt = ConfigData.bModAlt;
			KeyModifier.bCmd = ConfigData.bModCmd;

			FoundIdx = Idx;
			break;
		}
	}

	if (FoundIdx == INDEX_NONE)
	{
		FDNADebuggerInputConfig NewConfigData;
		NewConfigData.ConfigName = InputName.ToString();
		NewConfigData.Key = FKey(KeyName);
		NewConfigData.bModShift = KeyModifier.bShift;
		NewConfigData.bModCtrl = KeyModifier.bCtrl;
		NewConfigData.bModAlt = KeyModifier.bAlt;
		NewConfigData.bModCmd = KeyModifier.bCmd;

		OuterConfigData.InputHandlers.Add(NewConfigData);
	}
}

void UDNADebuggerConfig::UpdateExtensionInputConfig(const FName ExtensionName, const FName InputName, FName& KeyName, FDNADebuggerInputModifier& KeyModifier)
{
	int32 FoundExtensionIdx = INDEX_NONE;
	for (int32 Idx = 0; Idx < Extensions.Num(); Idx++)
	{
		if (*Extensions[Idx].ExtensionName == ExtensionName)
		{
			FoundExtensionIdx = Idx;
			break;
		}
	}

	if (!Extensions.IsValidIndex(FoundExtensionIdx))
	{
		return;
	}

	FDNADebuggerExtensionConfig& OuterConfigData = Extensions[FoundExtensionIdx];
	KnownExtensionInputNames.Add(ExtensionName, InputName);

	int32 FoundIdx = INDEX_NONE;
	for (int32 Idx = 0; Idx < OuterConfigData.InputHandlers.Num(); Idx++)
	{
		if (*OuterConfigData.InputHandlers[Idx].ConfigName == InputName)
		{
			FDNADebuggerInputConfig& ConfigData = OuterConfigData.InputHandlers[Idx];
			KeyName = ConfigData.Key.GetFName();
			KeyModifier.bShift = ConfigData.bModShift;
			KeyModifier.bCtrl = ConfigData.bModCtrl;
			KeyModifier.bAlt = ConfigData.bModAlt;
			KeyModifier.bCmd = ConfigData.bModCmd;

			FoundIdx = Idx;
			break;
		}
	}

	if (FoundIdx == INDEX_NONE)
	{
		FDNADebuggerInputConfig NewConfigData;
		NewConfigData.ConfigName = InputName.ToString();
		NewConfigData.Key = FKey(KeyName);
		NewConfigData.bModShift = KeyModifier.bShift;
		NewConfigData.bModCtrl = KeyModifier.bCtrl;
		NewConfigData.bModAlt = KeyModifier.bAlt;
		NewConfigData.bModCmd = KeyModifier.bCmd;

		OuterConfigData.InputHandlers.Add(NewConfigData);
	}
}

void UDNADebuggerConfig::Serialize(FArchive& Ar)
{
	if (Ar.IsSaving())
	{
		RemoveUnknownConfigs();
	}

	Super::Serialize(Ar);
}

void UDNADebuggerConfig::RemoveUnknownConfigs()
{
	for (int32 Idx = Categories.Num() - 1; Idx >= 0; Idx--)
	{
		FDNADebuggerCategoryConfig& ConfigData = Categories[Idx];
		const bool bValid = KnownCategoryNames.Contains(*ConfigData.CategoryName);
		if (!bValid)
		{
			Categories.RemoveAt(Idx, 1, false);
		}
		else
		{
			for (int32 InputIdx = ConfigData.InputHandlers.Num() - 1; InputIdx >= 0; InputIdx--)
			{
				const FDNADebuggerInputConfig& InputConfigData = ConfigData.InputHandlers[InputIdx];
				const FName* FoundDataPtr = KnownCategoryInputNames.FindPair(*ConfigData.CategoryName, *InputConfigData.ConfigName);
				if (FoundDataPtr == nullptr)
				{
					ConfigData.InputHandlers.RemoveAt(InputIdx, 1, false);
				}
			}
		}
	}

	for (int32 Idx = Extensions.Num() - 1; Idx >= 0; Idx--)
	{
		FDNADebuggerExtensionConfig& ConfigData = Extensions[Idx];
		const bool bValid = KnownExtensionNames.Contains(*ConfigData.ExtensionName);
		if (!bValid)
		{
			Extensions.RemoveAt(Idx, 1, false);
		}
		else
		{
			for (int32 InputIdx = ConfigData.InputHandlers.Num() - 1; InputIdx >= 0; InputIdx--)
			{
				const FDNADebuggerInputConfig& InputConfigData = ConfigData.InputHandlers[InputIdx];
				const FName* FoundDataPtr = KnownExtensionInputNames.FindPair(*ConfigData.ExtensionName, *InputConfigData.ConfigName);
				if (FoundDataPtr == nullptr)
				{
					ConfigData.InputHandlers.RemoveAt(InputIdx, 1, false);
				}
			}
		}
	}
}

#if WITH_EDITOR
void UDNADebuggerConfig::PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	FDNADebuggerAddonManager& AddonManager = FDNADebuggerAddonManager::GetCurrent();
	AddonManager.UpdateFromConfig();
}
#endif
