// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DNADebuggerAddonManager.h"
#include "DNADebuggerExtension_HUD.h"
#include "InputCoreTypes.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "EngineGlobals.h"
#include "GameFramework/HUD.h"

FDNADebuggerExtension_HUD::FDNADebuggerExtension_HUD()
{
	bWantsHUDEnabled = false;
	bIsGameHUDEnabled = false;
	bAreDebugMessagesEnabled = false;
	bPrevDebugMessagesEnabled = GEngine && GEngine->bEnableOnScreenDebugMessages;
	bIsCachedDescriptionValid = false;

	const FDNADebuggerInputHandlerConfig HUDKeyConfig(TEXT("ToggleHUD"), EKeys::Tilde.GetFName(), FDNADebuggerInputModifier::Ctrl);
	const FDNADebuggerInputHandlerConfig MessagesKeyConfig(TEXT("ToggleMessages"), EKeys::Tab.GetFName(), FDNADebuggerInputModifier::Ctrl);

	const bool bHasHUDBinding = BindKeyPress(HUDKeyConfig, this, &FDNADebuggerExtension_HUD::ToggleGameHUD);
	HudBindingIdx = bHasHUDBinding ? (GetNumInputHandlers() - 1) : INDEX_NONE;

	const bool bHasMessagesBinding = BindKeyPress(MessagesKeyConfig, this, &FDNADebuggerExtension_HUD::ToggleDebugMessages);
	MessagesBindingIdx = bHasMessagesBinding ? (GetNumInputHandlers() - 1) : INDEX_NONE;
}

TSharedRef<FDNADebuggerExtension> FDNADebuggerExtension_HUD::MakeInstance()
{
	return MakeShareable(new FDNADebuggerExtension_HUD());
}

void FDNADebuggerExtension_HUD::OnActivated()
{
	SetGameHUDEnabled(bWantsHUDEnabled);
	SetDebugMessagesEnabled(false);
}

void FDNADebuggerExtension_HUD::OnDeactivated()
{
	SetGameHUDEnabled(true);
	SetDebugMessagesEnabled(bPrevDebugMessagesEnabled);
}

FString FDNADebuggerExtension_HUD::GetDescription() const
{
	if (!bIsCachedDescriptionValid)
	{
		FString Description;

		if (HudBindingIdx != INDEX_NONE)
		{
			Description = FString::Printf(TEXT("{%s}%s:{%s}HUD"),
				*FDNADebuggerCanvasStrings::ColorNameInput,
				*GetInputHandlerDescription(HudBindingIdx),
				bIsGameHUDEnabled ? *FDNADebuggerCanvasStrings::ColorNameEnabled : *FDNADebuggerCanvasStrings::ColorNameDisabled);
		}

		if (MessagesBindingIdx != INDEX_NONE)
		{
			if (Description.Len() > 0)
			{
				Description += FDNADebuggerCanvasStrings::SeparatorSpace;
			}

			Description += FString::Printf(TEXT("{%s}%s:{%s}DebugMessages"),
				*FDNADebuggerCanvasStrings::ColorNameInput,
				*GetInputHandlerDescription(MessagesBindingIdx),
				bAreDebugMessagesEnabled ? *FDNADebuggerCanvasStrings::ColorNameEnabled : *FDNADebuggerCanvasStrings::ColorNameDisabled);
		}

		CachedDescription = Description;
		bIsCachedDescriptionValid = true;
	}

	return CachedDescription;
}

void FDNADebuggerExtension_HUD::SetGameHUDEnabled(bool bEnable)
{
	APlayerController* OwnerPC = GetPlayerController();
	AHUD* GameHUD = OwnerPC ? OwnerPC->GetHUD() : nullptr;
	if (GameHUD)
	{
		GameHUD->bShowHUD = bEnable;
	}

	bIsGameHUDEnabled = bEnable;
	bIsCachedDescriptionValid = false;
}

void FDNADebuggerExtension_HUD::SetDebugMessagesEnabled(bool bEnable)
{
	if (GEngine)
	{
		GEngine->bEnableOnScreenDebugMessages = bEnable;
	}

	bAreDebugMessagesEnabled = bEnable;
	bIsCachedDescriptionValid = false;
}

void FDNADebuggerExtension_HUD::ToggleGameHUD()
{
	const bool bEnable = bWantsHUDEnabled = !bIsGameHUDEnabled;
	SetGameHUDEnabled(bEnable);
}

void FDNADebuggerExtension_HUD::ToggleDebugMessages()
{
	SetDebugMessagesEnabled(!bAreDebugMessagesEnabled);
}
