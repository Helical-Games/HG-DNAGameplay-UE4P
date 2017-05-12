// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/HUD.h"
#include "AbilitySystemDebugHUD.generated.h"

class APlayerController;
class UDNAAbilitySystemComponent;
class UCanvas;
class UFont;

namespace EAlignHorizontal
{
	enum Type
	{
		Left,
		Center,
		Right,
	};
}

namespace EAlignVertical
{
	enum Type
	{
		Top,
		Center,
		Bottom,
	};
}

UCLASS()
class ADNAAbilitySystemDebugHUD : public AHUD
{
	GENERATED_UCLASS_BODY()

	/** main HUD update loop */
	void DrawWithBackground(UFont* InFont, const FString& Text, const FColor& TextColor, EAlignHorizontal::Type HAlign, float& OffsetX, EAlignVertical::Type VAlign, float& OffsetY, float Alpha=1.f);

	void DrawDebugHUD(UCanvas* Canvas, APlayerController* PC);

private:

	void DrawDebugDNAAbilitySystemComponent(UDNAAbilitySystemComponent *Component);

};
