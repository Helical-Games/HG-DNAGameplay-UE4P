// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "DNAEffectExtension.h"
#include "DNAEffectExtension_LifestealTest.generated.h"

class UDNAEffect;
struct FDNAModifierEvaluatedData;

UCLASS(BlueprintType)
class DNAABILITIES_API UDNAEffectExtension_LifestealTest : public UDNAEffectExtension
{
	GENERATED_UCLASS_BODY()

public:

	void PreDNAEffectExecute(const FDNAModifierEvaluatedData &SelfData, FDNAEffectModCallbackData &Data) const override;
	void PostDNAEffectExecute(const FDNAModifierEvaluatedData &SelfData, const FDNAEffectModCallbackData &Data) const override;

	/** The DNAEffect to apply when restoring health to the instigator */
	UPROPERTY(EditDefaultsOnly, Category = Lifesteal)
	UDNAEffect *	HealthRestoreDNAEffect;
};
