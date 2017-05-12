// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DNADebuggerAddonManager.h"
#include "DNADebuggerEdMode.h"

#if WITH_EDITOR
#include "DNADebuggerToolkit.h"
#include "Toolkits/ToolkitManager.h"
#include "DNADebuggerPlayerManager.h"
#include "DNADebuggerLocalController.h"

#include "EditorModeManager.h"
#include "Components/InputComponent.h"

const FName FDNADebuggerEdMode::EM_DNADebugger = TEXT("EM_DNADebugger");

void FDNADebuggerEdMode::Enter()
{
	FEdMode::Enter();
	
	if (!Toolkit.IsValid())
	{
		Toolkit = MakeShareable(new FDNADebuggerToolkit(this));
		Toolkit->Init(Owner->GetToolkitHost());
	}

	bPrevGAreScreenMessagesEnabled = GAreScreenMessagesEnabled;
	GAreScreenMessagesEnabled = false;
}

void FDNADebuggerEdMode::Exit()
{
	if (Toolkit.IsValid())
	{
		FToolkitManager::Get().CloseToolkit(Toolkit.ToSharedRef());
		Toolkit.Reset();
	}

	if (FocusedViewport)
	{
		EnableViewportClientFlags(FocusedViewport, false);
		FocusedViewport = nullptr;
	}

	FEdMode::Exit();
	GAreScreenMessagesEnabled = bPrevGAreScreenMessagesEnabled;
}

void FDNADebuggerEdMode::EnableViewportClientFlags(FEditorViewportClient* ViewportClient, bool bEnable)
{
	ViewportClient->bUseNumpadCameraControl = false;
}

bool FDNADebuggerEdMode::ReceivedFocus(FEditorViewportClient* ViewportClient, FViewport* Viewport)
{
	EnableViewportClientFlags(ViewportClient, true);
	FocusedViewport = ViewportClient;
	return false;
}

bool FDNADebuggerEdMode::LostFocus(FEditorViewportClient* ViewportClient, FViewport* Viewport)
{
	EnableViewportClientFlags(ViewportClient, false);
	FocusedViewport = nullptr;
	return false;
}

bool FDNADebuggerEdMode::InputKey(FEditorViewportClient* InViewportClient, FViewport* InViewport, FKey InKey, EInputEvent InEvent)
{
	UWorld* MyWorld = GetWorld();
	APlayerController* LocalPC = GEngine->GetFirstLocalPlayerController(MyWorld);

	if (LocalPC && InViewportClient)
	{
		// process raw input for debugger's input component manually
		// can't use LocalPC->InputKey here, since it will trigger for every bound chord, not only DNA debugger ones
		// and will not work at all when simulation is paused

		ADNADebuggerPlayerManager& PlayerManager = ADNADebuggerPlayerManager::GetCurrent(MyWorld);
		const FDNADebuggerPlayerData* DataPtr = PlayerManager.GetPlayerData(*LocalPC);

		if (DataPtr && DataPtr->InputComponent && DataPtr->Controller && DataPtr->Controller->IsKeyBound(InKey.GetFName()))
		{
			const FInputChord ActiveChord(InKey, InViewportClient->IsShiftPressed(), InViewportClient->IsCtrlPressed(), InViewportClient->IsAltPressed(), InViewportClient->IsCmdPressed());

			// go over all bound actions
			const int32 NumBindings = DataPtr->InputComponent->KeyBindings.Num();
			for (int32 Idx = 0; Idx < NumBindings; Idx++)
			{
				const FInputKeyBinding& KeyBinding = DataPtr->InputComponent->KeyBindings[Idx];
				if (KeyBinding.KeyEvent == InEvent && KeyBinding.Chord == ActiveChord && KeyBinding.KeyDelegate.IsBound())
				{
					KeyBinding.KeyDelegate.Execute(InKey);
				}
			}

			return true;			
		}
	}

	return false;
}

void FDNADebuggerEdMode::Tick(FEditorViewportClient* ViewportClient, float DeltaTime)
{
	if (ViewportClient == nullptr || !ViewportClient->EngineShowFlags.DebugAI)
	{
		Owner->DeactivateMode(FDNADebuggerEdMode::EM_DNADebugger);
	}
}

void FDNADebuggerEdMode::SafeCloseMode()
{
	// this may be called on closing editor during PIE (~viewport -> teardown PIE -> debugger's cleanup on game end)
	//
	// DeactivateMode tries to bring up default mode, but toolkit is already destroyed by that time
	// and editor crashes on check in GLevelEditorModeTools().GetToolkitHost() inside default mode's code

	if (GLevelEditorModeTools().HasToolkitHost())
	{
		GLevelEditorModeTools().DeactivateMode(FDNADebuggerEdMode::EM_DNADebugger);
	}
}

#endif // WITH_EDITOR
