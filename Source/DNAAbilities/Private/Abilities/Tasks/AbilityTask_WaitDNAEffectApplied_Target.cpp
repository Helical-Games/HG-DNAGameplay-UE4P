// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/Tasks/AbilityTask_WaitDNAEffectApplied_Target.h"
#include "AbilitySystemComponent.h"

UDNAAbilityTask_WaitDNAEffectApplied_Target::UDNAAbilityTask_WaitDNAEffectApplied_Target(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UDNAAbilityTask_WaitDNAEffectApplied_Target* UDNAAbilityTask_WaitDNAEffectApplied_Target::WaitDNAEffectAppliedToTarget(UDNAAbility* OwningAbility, const FDNATargetDataFilterHandle InFilter, FDNATagRequirements InSourceTagRequirements, FDNATagRequirements InTargetTagRequirements, bool InTriggerOnce, AActor* OptionalExternalOwner, bool InListenForPeriodicEffect)
{
	auto MyObj = NewDNAAbilityTask<UDNAAbilityTask_WaitDNAEffectApplied_Target>(OwningAbility);
	MyObj->Filter = InFilter;
	MyObj->SourceTagRequirements = InSourceTagRequirements;
	MyObj->TargetTagRequirements = InTargetTagRequirements;
	MyObj->TriggerOnce = InTriggerOnce;
	MyObj->SetExternalActor(OptionalExternalOwner);
	MyObj->ListenForPeriodicEffects = InListenForPeriodicEffect;
	return MyObj;
}

void UDNAAbilityTask_WaitDNAEffectApplied_Target::BroadcastDelegate(AActor* Avatar, FDNAEffectSpecHandle SpecHandle, FActiveDNAEffectHandle ActiveHandle)
{
	OnApplied.Broadcast(Avatar, SpecHandle, ActiveHandle);
}

void UDNAAbilityTask_WaitDNAEffectApplied_Target::RegisterDelegate()
{
	OnApplyDNAEffectCallbackDelegateHandle = GetASC()->OnDNAEffectAppliedDelegateToTarget.AddUObject(this, &UDNAAbilityTask_WaitDNAEffectApplied::OnApplyDNAEffectCallback);
	if (ListenForPeriodicEffects)
	{
		OnPeriodicDNAEffectExecuteCallbackDelegateHandle = GetASC()->OnPeriodicDNAEffectExecuteDelegateOnTarget.AddUObject(this, &UDNAAbilityTask_WaitDNAEffectApplied::OnApplyDNAEffectCallback);
	}
}

void UDNAAbilityTask_WaitDNAEffectApplied_Target::RemoveDelegate()
{
	GetASC()->OnDNAEffectAppliedDelegateToTarget.Remove(OnApplyDNAEffectCallbackDelegateHandle);
	if (OnPeriodicDNAEffectExecuteCallbackDelegateHandle.IsValid())
	{
		GetASC()->OnDNAEffectAppliedDelegateToTarget.Remove(OnPeriodicDNAEffectExecuteCallbackDelegateHandle);
	}
}
