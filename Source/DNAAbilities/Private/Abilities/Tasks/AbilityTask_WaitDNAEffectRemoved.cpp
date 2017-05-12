// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/Tasks/AbilityTask_WaitDNAEffectRemoved.h"
#include "AbilitySystemComponent.h"

UDNAAbilityTask_WaitDNAEffectRemoved::UDNAAbilityTask_WaitDNAEffectRemoved(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Registered = false;
}

UDNAAbilityTask_WaitDNAEffectRemoved* UDNAAbilityTask_WaitDNAEffectRemoved::WaitForDNAEffectRemoved(UDNAAbility* OwningAbility, FActiveDNAEffectHandle InHandle)
{
	auto MyObj = NewDNAAbilityTask<UDNAAbilityTask_WaitDNAEffectRemoved>(OwningAbility);
	MyObj->Handle = InHandle;

	return MyObj;
}

void UDNAAbilityTask_WaitDNAEffectRemoved::Activate()
{
	if (Handle.IsValid() == false)
	{
		InvalidHandle.Broadcast();
		EndTask();
		return;;
	}

	UDNAAbilitySystemComponent* EffectOwningDNAAbilitySystemComponent = Handle.GetOwningDNAAbilitySystemComponent();

	if (EffectOwningDNAAbilitySystemComponent)
	{
		FOnActiveDNAEffectRemoved* DelPtr = EffectOwningDNAAbilitySystemComponent->OnDNAEffectRemovedDelegate(Handle);
		if (DelPtr)
		{
			OnDNAEffectRemovedDelegateHandle = DelPtr->AddUObject(this, &UDNAAbilityTask_WaitDNAEffectRemoved::OnDNAEffectRemoved);
			Registered = true;
		}
	}

	if (!Registered)
	{
		// DNAEffect was already removed, treat this as a warning? Could be cases of immunity or chained DNA rules that would instant remove something
		OnDNAEffectRemoved();
	}
}

void UDNAAbilityTask_WaitDNAEffectRemoved::OnDestroy(bool AbilityIsEnding)
{
	UDNAAbilitySystemComponent* EffectOwningDNAAbilitySystemComponent = Handle.GetOwningDNAAbilitySystemComponent();
	if (EffectOwningDNAAbilitySystemComponent)
	{
		FOnActiveDNAEffectRemoved* DelPtr = EffectOwningDNAAbilitySystemComponent->OnDNAEffectRemovedDelegate(Handle);
		if (DelPtr)
		{
			DelPtr->Remove(OnDNAEffectRemovedDelegateHandle);
		}
	}

	Super::OnDestroy(AbilityIsEnding);
}

void UDNAAbilityTask_WaitDNAEffectRemoved::OnDNAEffectRemoved()
{
	OnRemoved.Broadcast();
	EndTask();
}
