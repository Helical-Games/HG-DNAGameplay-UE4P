// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "AttributeSet.h"
#include "DNAEffectExtension.generated.h"

class UDNAAbilitySystemComponent;
struct FDNAEffectSpec;
struct FDNAModifierEvaluatedData;

struct FDNAEffectModCallbackData
{
	FDNAEffectModCallbackData(const FDNAEffectSpec& InEffectSpec, FDNAModifierEvaluatedData& InEvaluatedData, UDNAAbilitySystemComponent& InTarget)
		: EffectSpec(InEffectSpec)
		, EvaluatedData(InEvaluatedData)
		, Target(InTarget)
	{

	}

	const struct FDNAEffectSpec&		EffectSpec;		// The spec that the mod came from
	struct FDNAModifierEvaluatedData&	EvaluatedData;	// The 'flat'/computed data to be applied to the target

	class UDNAAbilitySystemComponent &Target;		// Target we intend to apply to
};

UCLASS(BlueprintType)
class DNAABILITIES_API UDNAEffectExtension: public UObject
{
	GENERATED_UCLASS_BODY()

	/** Attributes on the source instigator that are relevant to calculating modifications using this extension */
	UPROPERTY(EditDefaultsOnly, Category=Calculation)
	TArray<FDNAAttribute> RelevantSourceAttributes;

	/** Attributes on the target that are relevant to calculating modifications using this extension */
	UPROPERTY(EditDefaultsOnly, Category=Calculation)
	TArray<FDNAAttribute> RelevantTargetAttributes;

public:

	virtual void PreDNAEffectExecute(const FDNAModifierEvaluatedData &SelfData, FDNAEffectModCallbackData &Data) const
	{

	}

	virtual void PostDNAEffectExecute(const FDNAModifierEvaluatedData &SelfData, const FDNAEffectModCallbackData &Data) const
	{

	}
};
