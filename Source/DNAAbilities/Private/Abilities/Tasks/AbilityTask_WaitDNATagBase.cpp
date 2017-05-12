// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/Tasks/AbilityTask_WaitDNATagBase.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"

UDNAAbilityTask_WaitDNATag::UDNAAbilityTask_WaitDNATag(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RegisteredCallback = false;
	UseExternalTarget = false;
	OnlyTriggerOnce = false;
}

void UDNAAbilityTask_WaitDNATag::Activate()
{
	UDNAAbilitySystemComponent* ASC = GetTargetASC();
	if (ASC)
	{
		DelegateHandle = ASC->RegisterDNATagEvent(Tag).AddUObject(this, &UDNAAbilityTask_WaitDNATag::DNATagCallback);
		RegisteredCallback = true;
	}
}

void UDNAAbilityTask_WaitDNATag::DNATagCallback(const FDNATag InTag, int32 NewCount)
{
}

void UDNAAbilityTask_WaitDNATag::OnDestroy(bool AbilityIsEnding)
{
	UDNAAbilitySystemComponent* ASC = GetTargetASC();
	if (RegisteredCallback && ASC)
	{
		ASC->RegisterDNATagEvent(Tag).Remove(DelegateHandle);
	}

	Super::OnDestroy(AbilityIsEnding);
}

UDNAAbilitySystemComponent* UDNAAbilityTask_WaitDNATag::GetTargetASC()
{
	if (UseExternalTarget)
	{
		return OptionalExternalTarget;
	}

	return DNAAbilitySystemComponent;
}

void UDNAAbilityTask_WaitDNATag::SetExternalTarget(AActor* Actor)
{
	if (Actor)
	{
		UseExternalTarget = true;
		OptionalExternalTarget = UDNAAbilitySystemGlobals::GetDNAAbilitySystemComponentFromActor(Actor);
	}
}
