// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "DNAEffect.h"
#include "DNAEffectCustomApplicationRequirement.generated.h"

class UDNAAbilitySystemComponent;

/** Class used to perform custom DNA effect modifier calculations, either via blueprint or native code */ 
UCLASS(BlueprintType, Blueprintable, Abstract)
class DNAABILITIES_API UDNAEffectCustomApplicationRequirement : public UObject
{

public:
	GENERATED_UCLASS_BODY()
	
	/** Return whether the DNA effect should be applied or not */
	UFUNCTION(BlueprintNativeEvent, Category="Calculation")
	bool CanApplyDNAEffect(const UDNAEffect* DNAEffect, const FDNAEffectSpec& Spec, UDNAAbilitySystemComponent* ASC) const;
};
