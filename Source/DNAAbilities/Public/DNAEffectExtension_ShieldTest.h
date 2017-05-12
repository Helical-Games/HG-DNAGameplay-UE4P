// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "DNAEffectExtension.h"
#include "DNAEffectExtension_ShieldTest.generated.h"

class UDNAEffect;
struct FDNAModifierEvaluatedData;

UCLASS(BlueprintType)
class DNAABILITIES_API UDNAEffectExtension_ShieldTest : public UDNAEffectExtension
{
	GENERATED_UCLASS_BODY()

public:

	void PreDNAEffectExecute(const FDNAModifierEvaluatedData &SelfData, FDNAEffectModCallbackData &Data) const override;
	void PostDNAEffectExecute(const FDNAModifierEvaluatedData &SelfData, const FDNAEffectModCallbackData &Data) const override;

	UPROPERTY(EditDefaultsOnly, Category = Lifesteal)
	UDNAEffect * ShieldRemoveDNAEffect;
};
