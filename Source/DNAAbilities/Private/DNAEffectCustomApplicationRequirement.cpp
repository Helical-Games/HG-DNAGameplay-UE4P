// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNAEffectCustomApplicationRequirement.h"

UDNAEffectCustomApplicationRequirement::UDNAEffectCustomApplicationRequirement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

bool UDNAEffectCustomApplicationRequirement::CanApplyDNAEffect_Implementation(const UDNAEffect* DNAEffect, const FDNAEffectSpec& Spec, UDNAAbilitySystemComponent* ASC) const
{
	return true;
}
