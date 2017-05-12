// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "DNAEffectUIData.h"
#include "DNAEffectUIData_TextOnly.generated.h"

/**
 * UI data that contains only text. This is mostly used as an example of a subclass of UDNAEffectUIData.
 * If your game needs only text, this is a reasonable class to use. To include more data, make a custom subclass of UDNAEffectUIData.
 */
UCLASS()
class DNAABILITIES_API UDNAEffectUIData_TextOnly : public UDNAEffectUIData
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Data, meta = (MultiLine = "true"))
	FText Description;
};
