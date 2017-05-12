// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "DNADebuggerExtension.h"

class ADebugCameraController;

class FDNADebuggerExtension_Spectator : public FDNADebuggerExtension
{
public:

	FDNADebuggerExtension_Spectator();

	virtual void OnDeactivated() override;
	virtual FString GetDescription() const override;

	static TSharedRef<FDNADebuggerExtension> MakeInstance();

protected:

	void ToggleSpectatorMode();

	uint32 bHasInputBinding : 1;
	mutable uint32 bIsCachedDescriptionValid : 1;
	mutable FString CachedDescription;
	TWeakObjectPtr<ADebugCameraController> SpectatorController;
};
