// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/Tasks/AbilityTask_WaitDNAEffectApplied_Self.h"
#include "AbilitySystemComponent.h"

UDNAAbilityTask_WaitDNAEffectApplied_Self::UDNAAbilityTask_WaitDNAEffectApplied_Self(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UDNAAbilityTask_WaitDNAEffectApplied_Self* UDNAAbilityTask_WaitDNAEffectApplied_Self::WaitDNAEffectAppliedToSelf(UDNAAbility* OwningAbility, const FDNATargetDataFilterHandle InFilter, FDNATagRequirements InSourceTagRequirements, FDNATagRequirements InTargetTagRequirements, bool InTriggerOnce, AActor* OptionalExternalOwner, bool InListenForPeriodicEffect)
{
	auto MyObj = NewDNAAbilityTask<UDNAAbilityTask_WaitDNAEffectApplied_Self>(OwningAbility);
	MyObj->Filter = InFilter;
	MyObj->SourceTagRequirements = InSourceTagRequirements;
	MyObj->TargetTagRequirements = InTargetTagRequirements;
	MyObj->TriggerOnce = InTriggerOnce;
	MyObj->SetExternalActor(OptionalExternalOwner);
	MyObj->ListenForPeriodicEffects = InListenForPeriodicEffect;
	return MyObj;
}

void UDNAAbilityTask_WaitDNAEffectApplied_Self::BroadcastDelegate(AActor* Avatar, FDNAEffectSpecHandle SpecHandle, FActiveDNAEffectHandle ActiveHandle)
{
	OnApplied.Broadcast(Avatar, SpecHandle, ActiveHandle);
}

void UDNAAbilityTask_WaitDNAEffectApplied_Self::RegisterDelegate()
{
	OnApplyDNAEffectCallbackDelegateHandle = GetASC()->OnDNAEffectAppliedDelegateToSelf.AddUObject(this, &UDNAAbilityTask_WaitDNAEffectApplied::OnApplyDNAEffectCallback);
	if (ListenForPeriodicEffects)
	{
		OnPeriodicDNAEffectExecuteCallbackDelegateHandle = GetASC()->OnPeriodicDNAEffectExecuteDelegateOnSelf.AddUObject(this, &UDNAAbilityTask_WaitDNAEffectApplied::OnApplyDNAEffectCallback);
	}
}

void UDNAAbilityTask_WaitDNAEffectApplied_Self::RemoveDelegate()
{
	GetASC()->OnDNAEffectAppliedDelegateToSelf.Remove(OnApplyDNAEffectCallbackDelegateHandle);
	if (OnPeriodicDNAEffectExecuteCallbackDelegateHandle.IsValid())
	{
		GetASC()->OnDNAEffectAppliedDelegateToTarget.Remove(OnPeriodicDNAEffectExecuteCallbackDelegateHandle);
	}
}
