// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/Tasks/AbilityTask_WaitDNAEffectApplied.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"

UDNAAbilityTask_WaitDNAEffectApplied::UDNAAbilityTask_WaitDNAEffectApplied(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Locked = false;
}

void UDNAAbilityTask_WaitDNAEffectApplied::Activate()
{
	if (GetASC())
	{
		RegisterDelegate();
	}
}

void UDNAAbilityTask_WaitDNAEffectApplied::OnApplyDNAEffectCallback(UDNAAbilitySystemComponent* Target, const FDNAEffectSpec& SpecApplied, FActiveDNAEffectHandle ActiveHandle)
{
	bool PassedComparison = false;

	AActor* AvatarActor = Target ? Target->AvatarActor : nullptr;

	if (!Filter.FilterPassesForActor(AvatarActor))
	{
		return;
	}
	if (!SourceTagRequirements.RequirementsMet(*SpecApplied.CapturedSourceTags.GetAggregatedTags()))
	{
		return;
	}
	if (!TargetTagRequirements.RequirementsMet(*SpecApplied.CapturedTargetTags.GetAggregatedTags()))
	{
		return;
	}

	if (Locked)
	{
		ABILITY_LOG(Error, TEXT("WaitDNAEffectApplied recursion detected. Ability: %s. Applied Spec: %s. This could cause an infinite loop! Ignoring"), *GetNameSafe(Ability), *SpecApplied.ToSimpleString());
		return;
	}
	
	FDNAEffectSpecHandle	SpecHandle(new FDNAEffectSpec(SpecApplied));

	{
		TGuardValue<bool> GuardValue(Locked, true);	
		BroadcastDelegate(AvatarActor, SpecHandle, ActiveHandle);
	}

	if (TriggerOnce)
	{
		EndTask();
	}
}

void UDNAAbilityTask_WaitDNAEffectApplied::OnDestroy(bool AbilityEnded)
{
	if (GetASC())
	{
		RemoveDelegate();
	}

	Super::OnDestroy(AbilityEnded);
}

void UDNAAbilityTask_WaitDNAEffectApplied::SetExternalActor(AActor* InActor)
{
	if (InActor)
	{
		UseExternalOwner = true;
		ExternalOwner  = UDNAAbilitySystemGlobals::GetDNAAbilitySystemComponentFromActor(InActor);
	}
}

UDNAAbilitySystemComponent* UDNAAbilityTask_WaitDNAEffectApplied::GetASC()
{
	if (UseExternalOwner)
	{
		return ExternalOwner;
	}
	return DNAAbilitySystemComponent;
}
