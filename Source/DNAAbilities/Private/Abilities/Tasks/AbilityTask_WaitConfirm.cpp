// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/Tasks/AbilityTask_WaitConfirm.h"

UDNAAbilityTask_WaitConfirm::UDNAAbilityTask_WaitConfirm(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RegisteredCallback = false;
}

void UDNAAbilityTask_WaitConfirm::OnConfirmCallback(UDNAAbility* InAbility)
{
	DNAAbilityTask_MSG("OnConfirmCallback");
	OnConfirm.Broadcast();

	// We are done. Kill us so we don't keep getting broadcast messages
	EndTask();
}

UDNAAbilityTask_WaitConfirm* UDNAAbilityTask_WaitConfirm::WaitConfirm(class UDNAAbility* OwningAbility)
{
	return NewDNAAbilityTask<UDNAAbilityTask_WaitConfirm>(OwningAbility);
}

void UDNAAbilityTask_WaitConfirm::Activate()
{
	if (Ability)
	{
		if (Ability->GetCurrentActivationInfo().ActivationMode == EDNAAbilityActivationMode::Predicting)
		{
			// Register delegate so that when we are confirmed by the server, we call OnConfirmCallback	
			
			OnConfirmCallbackDelegateHandle = Ability->OnConfirmDelegate.AddUObject(this, &UDNAAbilityTask_WaitConfirm::OnConfirmCallback);
			RegisteredCallback = true;
		}
		else
		{
			// We have already been confirmed, just call the OnConfirm callback
			OnConfirmCallback(Ability);
		}
	}
}

void UDNAAbilityTask_WaitConfirm::OnDestroy(bool AbilityEnded)
{
	if (RegisteredCallback && Ability)
	{
		Ability->OnConfirmDelegate.Remove(OnConfirmCallbackDelegateHandle);
	}

	Super::OnDestroy(AbilityEnded);
}
