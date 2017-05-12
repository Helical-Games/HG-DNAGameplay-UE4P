// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DNADebuggerAddonManager.h"
#include "DNADebuggerAddonBase.h"
#include "DNADebuggerCategoryReplicator.h"
#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#endif

AActor* FDNADebuggerAddonBase::FindLocalDebugActor() const
{
	ADNADebuggerCategoryReplicator* RepOwnerOb = RepOwner.Get();
	return RepOwnerOb ? RepOwnerOb->GetDebugActor() : nullptr;
}

ADNADebuggerCategoryReplicator* FDNADebuggerAddonBase::GetReplicator() const
{
	return RepOwner.Get();
}

FString FDNADebuggerAddonBase::GetInputHandlerDescription(int32 HandlerId) const
{
	return InputHandlers.IsValidIndex(HandlerId) ? InputHandlers[HandlerId].ToString() : FString();
}

void FDNADebuggerAddonBase::OnDNADebuggerActivated()
{
	// empty in base class
}

void FDNADebuggerAddonBase::OnDNADebuggerDeactivated()
{
	// empty in base class
}

bool FDNADebuggerAddonBase::IsSimulateInEditor()
{
#if WITH_EDITOR
	extern UNREALED_API UEditorEngine* GEditor;
	return GIsEditor && (GEditor->bIsSimulateInEditorQueued || GEditor->bIsSimulatingInEditor);
#endif
	return false;
}
