// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/Tasks/AbilityTask_WaitCancel.h"
#include "AbilitySystemComponent.h"

UDNAAbilityTask_WaitCancel::UDNAAbilityTask_WaitCancel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RegisteredCallbacks = false;

}

void UDNAAbilityTask_WaitCancel::OnCancelCallback()
{
	if (DNAAbilitySystemComponent)
	{
		DNAAbilitySystemComponent->ConsumeGenericReplicatedEvent(EAbilityGenericReplicatedEvent::GenericCancel, GetAbilitySpecHandle(), GetActivationPredictionKey());
		OnCancel.Broadcast();
		EndTask();
	}
}

void UDNAAbilityTask_WaitCancel::OnLocalCancelCallback()
{
	FScopedPredictionWindow ScopedPrediction(DNAAbilitySystemComponent, IsPredictingClient());

	if (DNAAbilitySystemComponent && IsPredictingClient())
	{
		DNAAbilitySystemComponent->ServerSetReplicatedEvent(EAbilityGenericReplicatedEvent::GenericCancel, GetAbilitySpecHandle(), GetActivationPredictionKey() ,DNAAbilitySystemComponent->ScopedPredictionKey);
	}
	OnCancelCallback();
}

UDNAAbilityTask_WaitCancel* UDNAAbilityTask_WaitCancel::WaitCancel(UDNAAbility* OwningAbility)
{
	return NewDNAAbilityTask<UDNAAbilityTask_WaitCancel>(OwningAbility);
}

void UDNAAbilityTask_WaitCancel::Activate()
{
	if (DNAAbilitySystemComponent)
	{
		const FDNAAbilityActorInfo* Info = Ability->GetCurrentActorInfo();

		
		if (Info->IsLocallyControlled())
		{
			// We have to wait for the callback from the DNAAbilitySystemComponent.
			DNAAbilitySystemComponent->GenericLocalCancelCallbacks.AddDynamic(this, &UDNAAbilityTask_WaitCancel::OnLocalCancelCallback);	// Tell me if the cancel input is pressed

			RegisteredCallbacks = true;
		}
		else
		{
			if (CallOrAddReplicatedDelegate(EAbilityGenericReplicatedEvent::GenericCancel,  FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &UDNAAbilityTask_WaitCancel::OnCancelCallback)) )
			{
				// GenericCancel was already received from the client and we just called OnCancelCallback. The task is done.
				return;
			}
		}
	}
}

void UDNAAbilityTask_WaitCancel::OnDestroy(bool AbilityEnding)
{
	if (RegisteredCallbacks && DNAAbilitySystemComponent)
	{
		DNAAbilitySystemComponent->GenericLocalCancelCallbacks.RemoveDynamic(this, &UDNAAbilityTask_WaitCancel::OnLocalCancelCallback);
	}

	Super::OnDestroy(AbilityEnding);
}
