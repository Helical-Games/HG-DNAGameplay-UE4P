// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNAAbilitySet.h"
#include "AbilitySystemComponent.h"

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	UDNAAbilitySet
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

UDNAAbilitySet::UDNAAbilitySet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void UDNAAbilitySet::GiveAbilities(UDNAAbilitySystemComponent* DNAAbilitySystemComponent) const
{
	for (const FDNAAbilityBindInfo& BindInfo : Abilities)
	{
		if (BindInfo.DNAAbilityClass)
		{
			DNAAbilitySystemComponent->GiveAbility(FDNAAbilitySpec(BindInfo.DNAAbilityClass->GetDefaultObject<UDNAAbility>(), 1, (int32)BindInfo.Command));
		}
	}
}
