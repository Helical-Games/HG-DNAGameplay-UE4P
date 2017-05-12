// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "DNAEffectUIData.generated.h"

/**
 * UDNAEffectUIData
 * Base class to provide game-specific data about how to describe a DNA Effect in the UI. Subclass with data to use in your game.
 */
UCLASS(Blueprintable, Abstract, EditInlineNew, CollapseCategories)
class DNAABILITIES_API UDNAEffectUIData : public UObject
{
	GENERATED_UCLASS_BODY()
};
