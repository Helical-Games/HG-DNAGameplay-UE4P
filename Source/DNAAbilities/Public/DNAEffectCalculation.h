// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "DNAEffectTypes.h"
#include "DNAEffectCalculation.generated.h"

/** Abstract base for specialized DNA effect calculations; Capable of specifing attribute captures */
UCLASS(BlueprintType, Blueprintable, Abstract)
class DNAABILITIES_API UDNAEffectCalculation : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** Simple accessor to capture definitions for attributes */
	virtual const TArray<FDNAEffectAttributeCaptureDefinition>& GetAttributeCaptureDefinitions() const;

protected:

	/** Attributes to capture that are relevant to the calculation */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Attributes)
	TArray<FDNAEffectAttributeCaptureDefinition> RelevantAttributesToCapture;
};
