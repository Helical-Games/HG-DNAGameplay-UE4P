// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/Tasks/AbilityTask_WaitDNAEffectStackChange.h"
#include "AbilitySystemComponent.h"

UDNAAbilityTask_WaitDNAEffectStackChange::UDNAAbilityTask_WaitDNAEffectStackChange(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Registered = false;
}

UDNAAbilityTask_WaitDNAEffectStackChange* UDNAAbilityTask_WaitDNAEffectStackChange::WaitForDNAEffectStackChange(UDNAAbility* OwningAbility, FActiveDNAEffectHandle InHandle)
{
	auto MyObj = NewDNAAbilityTask<UDNAAbilityTask_WaitDNAEffectStackChange>(OwningAbility);
	MyObj->Handle = InHandle;
	return MyObj;
}

void UDNAAbilityTask_WaitDNAEffectStackChange::Activate()
{
	if (Handle.IsValid() == false)
	{
		InvalidHandle.Broadcast(Handle, 0, 0);
		EndTask();
		return;;
	}

	UDNAAbilitySystemComponent* EffectOwningDNAAbilitySystemComponent = Handle.GetOwningDNAAbilitySystemComponent();

	if (EffectOwningDNAAbilitySystemComponent)
	{
		FOnActiveDNAEffectStackChange* DelPtr = EffectOwningDNAAbilitySystemComponent->OnDNAEffectStackChangeDelegate(Handle);
		if (DelPtr)
		{
			OnDNAEffectStackChangeDelegateHandle = DelPtr->AddUObject(this, &UDNAAbilityTask_WaitDNAEffectStackChange::OnDNAEffectStackChange);
			Registered = true;
		}
	}
}

void UDNAAbilityTask_WaitDNAEffectStackChange::OnDestroy(bool AbilityIsEnding)
{
	UDNAAbilitySystemComponent* EffectOwningDNAAbilitySystemComponent = Handle.GetOwningDNAAbilitySystemComponent();
	if (EffectOwningDNAAbilitySystemComponent && OnDNAEffectStackChangeDelegateHandle.IsValid())
	{
		FOnActiveDNAEffectRemoved* DelPtr = EffectOwningDNAAbilitySystemComponent->OnDNAEffectRemovedDelegate(Handle);
		if (DelPtr)
		{
			DelPtr->Remove(OnDNAEffectStackChangeDelegateHandle);
		}
	}

	Super::OnDestroy(AbilityIsEnding);
}

void UDNAAbilityTask_WaitDNAEffectStackChange::OnDNAEffectStackChange(FActiveDNAEffectHandle InHandle, int32 NewCount, int32 OldCount)
{
	OnChange.Broadcast(InHandle, NewCount, OldCount);
}
