// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "AbilitySystemTestPawn.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemTestAttributeSet.h"

FName  ADNAAbilitySystemTestPawn::DNAAbilitySystemComponentName(TEXT("DNAAbilitySystemComponent0"));

ADNAAbilitySystemTestPawn::ADNAAbilitySystemTestPawn(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	DNAAbilitySystemComponent = CreateDefaultSubobject<UDNAAbilitySystemComponent>(ADNAAbilitySystemTestPawn::DNAAbilitySystemComponentName);
	DNAAbilitySystemComponent->SetIsReplicated(true);

	//DefaultAbilitySet = NULL;
}

void ADNAAbilitySystemTestPawn::PostInitializeComponents()
{	
	static UProperty *DamageProperty = FindFieldChecked<UProperty>(UDNAAbilitySystemTestAttributeSet::StaticClass(), GET_MEMBER_NAME_CHECKED(UDNAAbilitySystemTestAttributeSet, Damage));

	Super::PostInitializeComponents();
	DNAAbilitySystemComponent->InitStats(UDNAAbilitySystemTestAttributeSet::StaticClass(), NULL);

	/*
	if (DefaultAbilitySet != NULL)
	{
		DNAAbilitySystemComponent->InitializeAbilities(DefaultAbilitySet);
	}
	*/
}

UDNAAbilitySystemComponent* ADNAAbilitySystemTestPawn::GetDNAAbilitySystemComponent() const
{
	return FindComponentByClass<UDNAAbilitySystemComponent>();
}

/** Returns DNAAbilitySystemComponent subobject **/
UDNAAbilitySystemComponent* ADNAAbilitySystemTestPawn::GetDNAAbilitySystemComponent() { return DNAAbilitySystemComponent; }
