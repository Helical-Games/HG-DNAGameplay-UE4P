// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/Tasks/AbilityTask_WaitDNAEffectBlockedImmunity.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"

UDNAAbilityTask_WaitDNAEffectBlockedImmunity::UDNAAbilityTask_WaitDNAEffectBlockedImmunity(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UseExternalOwner = false;
	ExternalOwner = nullptr;
}

UDNAAbilityTask_WaitDNAEffectBlockedImmunity* UDNAAbilityTask_WaitDNAEffectBlockedImmunity::WaitDNAEffectBlockedByImmunity(UDNAAbility* OwningAbility, FDNATagRequirements InSourceTagRequirements, FDNATagRequirements InTargetTagRequirements, AActor* InOptionalExternalTarget, bool InTriggerOnce)
{
	auto MyObj = NewDNAAbilityTask<UDNAAbilityTask_WaitDNAEffectBlockedImmunity>(OwningAbility);
	MyObj->SourceTagRequirements = InSourceTagRequirements;
	MyObj->TargetTagRequirements = InTargetTagRequirements;
	MyObj->TriggerOnce = InTriggerOnce;
	MyObj->SetExternalActor(InOptionalExternalTarget);
	return MyObj;
}

void UDNAAbilityTask_WaitDNAEffectBlockedImmunity::Activate()
{
	if (GetASC())
	{
		RegisterDelegate();
	}
}

void UDNAAbilityTask_WaitDNAEffectBlockedImmunity::ImmunityCallback(const FDNAEffectSpec& BlockedSpec, const FActiveDNAEffect* ImmunityGE)
{
	bool PassedComparison = false;

	if (!SourceTagRequirements.RequirementsMet(*BlockedSpec.CapturedSourceTags.GetAggregatedTags()))
	{
		return;
	}
	if (!TargetTagRequirements.RequirementsMet(*BlockedSpec.CapturedTargetTags.GetAggregatedTags()))
	{
		return;
	}
	
	// We have to copy the spec, since the blocked spec is not ours
	FDNAEffectSpecHandle SpecHandle(new FDNAEffectSpec(BlockedSpec));

	Blocked.Broadcast(SpecHandle, ImmunityGE->Handle);
	
	if (TriggerOnce)
	{
		EndTask();
	}
}

void UDNAAbilityTask_WaitDNAEffectBlockedImmunity::OnDestroy(bool AbilityEnded)
{
	if (GetASC())
	{
		RemoveDelegate();
	}

	Super::OnDestroy(AbilityEnded);
}

void UDNAAbilityTask_WaitDNAEffectBlockedImmunity::SetExternalActor(AActor* InActor)
{
	if (InActor)
	{
		UseExternalOwner = true;
		ExternalOwner  = UDNAAbilitySystemGlobals::GetDNAAbilitySystemComponentFromActor(InActor);
	}
}

UDNAAbilitySystemComponent* UDNAAbilityTask_WaitDNAEffectBlockedImmunity::GetASC()
{
	if (UseExternalOwner)
	{
		return ExternalOwner;
	}
	return DNAAbilitySystemComponent;
}

void UDNAAbilityTask_WaitDNAEffectBlockedImmunity::RegisterDelegate()
{
	if (UDNAAbilitySystemComponent* ASC =  GetASC())
	{
		// Only do this on the server. Clients could mispredict.
		if (ASC->IsNetSimulating() == false)
		{
			DelegateHandle = ASC->OnImmunityBlockDNAEffectDelegate.AddUObject(this, &UDNAAbilityTask_WaitDNAEffectBlockedImmunity::ImmunityCallback);
		}
	}
}

void UDNAAbilityTask_WaitDNAEffectBlockedImmunity::RemoveDelegate()
{
	if (DelegateHandle.IsValid())
	{
		if (UDNAAbilitySystemComponent* ASC =  GetASC())
		{
			ASC->OnImmunityBlockDNAEffectDelegate.Remove(DelegateHandle);
			DelegateHandle.Reset();
		}
	}
}
