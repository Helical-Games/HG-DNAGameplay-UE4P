// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/Tasks/AbilityTask_WaitConfirmCancel.h"
#include "AbilitySystemComponent.h"

UDNAAbilityTask_WaitConfirmCancel::UDNAAbilityTask_WaitConfirmCancel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RegisteredCallbacks = false;

}

void UDNAAbilityTask_WaitConfirmCancel::OnConfirmCallback()
{
	
	if (DNAAbilitySystemComponent)
	{
		DNAAbilitySystemComponent->ConsumeGenericReplicatedEvent(EAbilityGenericReplicatedEvent::GenericConfirm, GetAbilitySpecHandle(), GetActivationPredictionKey());
		OnConfirm.Broadcast();
		EndTask();
	}
}

void UDNAAbilityTask_WaitConfirmCancel::OnCancelCallback()
{
	if (DNAAbilitySystemComponent)
	{
		DNAAbilitySystemComponent->ConsumeGenericReplicatedEvent(EAbilityGenericReplicatedEvent::GenericCancel, GetAbilitySpecHandle(), GetActivationPredictionKey());
		OnCancel.Broadcast();
		EndTask();
	}
}

void UDNAAbilityTask_WaitConfirmCancel::OnLocalConfirmCallback()
{
	FScopedPredictionWindow ScopedPrediction(DNAAbilitySystemComponent, IsPredictingClient());

	if (DNAAbilitySystemComponent && IsPredictingClient())
	{
		DNAAbilitySystemComponent->ServerSetReplicatedEvent(EAbilityGenericReplicatedEvent::GenericConfirm, GetAbilitySpecHandle(), GetActivationPredictionKey() ,DNAAbilitySystemComponent->ScopedPredictionKey);
	}
	OnConfirmCallback();
}

void UDNAAbilityTask_WaitConfirmCancel::OnLocalCancelCallback()
{
	FScopedPredictionWindow ScopedPrediction(DNAAbilitySystemComponent, IsPredictingClient());

	if (DNAAbilitySystemComponent && IsPredictingClient())
	{
		DNAAbilitySystemComponent->ServerSetReplicatedEvent(EAbilityGenericReplicatedEvent::GenericCancel, GetAbilitySpecHandle(), GetActivationPredictionKey() ,DNAAbilitySystemComponent->ScopedPredictionKey);
	}
	OnCancelCallback();
}

UDNAAbilityTask_WaitConfirmCancel* UDNAAbilityTask_WaitConfirmCancel::WaitConfirmCancel(UDNAAbility* OwningAbility)
{
	return NewDNAAbilityTask<UDNAAbilityTask_WaitConfirmCancel>(OwningAbility);
}

void UDNAAbilityTask_WaitConfirmCancel::Activate()
{
	if (DNAAbilitySystemComponent && Ability)
	{
		const FDNAAbilityActorInfo* Info = Ability->GetCurrentActorInfo();

		
		if (Info->IsLocallyControlled())
		{
			// We have to wait for the callback from the DNAAbilitySystemComponent.
			DNAAbilitySystemComponent->GenericLocalConfirmCallbacks.AddDynamic(this, &UDNAAbilityTask_WaitConfirmCancel::OnLocalConfirmCallback);	// Tell me if the confirm input is pressed
			DNAAbilitySystemComponent->GenericLocalCancelCallbacks.AddDynamic(this, &UDNAAbilityTask_WaitConfirmCancel::OnLocalCancelCallback);	// Tell me if the cancel input is pressed

			RegisteredCallbacks = true;
		}
		else
		{
			if (CallOrAddReplicatedDelegate(EAbilityGenericReplicatedEvent::GenericConfirm,  FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &UDNAAbilityTask_WaitConfirmCancel::OnConfirmCallback)) )
			{
				// GenericConfirm was already received from the client and we just called OnConfirmCallback. The task is done.
				return;
			}
			if (CallOrAddReplicatedDelegate(EAbilityGenericReplicatedEvent::GenericCancel,  FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &UDNAAbilityTask_WaitConfirmCancel::OnCancelCallback)) )
			{
				// GenericCancel was already received from the client and we just called OnCancelCallback. The task is done.
				return;
			}
		}
	}
}

void UDNAAbilityTask_WaitConfirmCancel::OnDestroy(bool AbilityEnding)
{
	if (RegisteredCallbacks && DNAAbilitySystemComponent)
	{
		DNAAbilitySystemComponent->GenericLocalConfirmCallbacks.RemoveDynamic(this, &UDNAAbilityTask_WaitConfirmCancel::OnLocalConfirmCallback);
		DNAAbilitySystemComponent->GenericLocalCancelCallbacks.RemoveDynamic(this, &UDNAAbilityTask_WaitConfirmCancel::OnLocalCancelCallback);
	}

	Super::OnDestroy(AbilityEnding);
}
