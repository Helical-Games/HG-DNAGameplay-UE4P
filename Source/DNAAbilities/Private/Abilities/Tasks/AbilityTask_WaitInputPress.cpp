// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "AbilitySystemComponent.h"

UDNAAbilityTask_WaitInputPress::UDNAAbilityTask_WaitInputPress(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	StartTime = 0.f;
	bTestInitialState = false;
}

void UDNAAbilityTask_WaitInputPress::OnPressCallback()
{
	float ElapsedTime = GetWorld()->GetTimeSeconds() - StartTime;

	if (!Ability || !DNAAbilitySystemComponent)
	{
		return;
	}

	DNAAbilitySystemComponent->AbilityReplicatedEventDelegate(EAbilityGenericReplicatedEvent::InputPressed, GetAbilitySpecHandle(), GetActivationPredictionKey()).Remove(DelegateHandle);

	FScopedPredictionWindow ScopedPrediction(DNAAbilitySystemComponent, IsPredictingClient());

	if (IsPredictingClient())
	{
		// Tell the server about this
		DNAAbilitySystemComponent->ServerSetReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, GetAbilitySpecHandle(), GetActivationPredictionKey(), DNAAbilitySystemComponent->ScopedPredictionKey);
	}
	else
	{
		DNAAbilitySystemComponent->ConsumeGenericReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, GetAbilitySpecHandle(), GetActivationPredictionKey());
	}

	// We are done. Kill us so we don't keep getting broadcast messages
	OnPress.Broadcast(ElapsedTime);
	EndTask();
}

UDNAAbilityTask_WaitInputPress* UDNAAbilityTask_WaitInputPress::WaitInputPress(class UDNAAbility* OwningAbility, bool bTestAlreadyPressed)
{
	UDNAAbilityTask_WaitInputPress* Task = NewDNAAbilityTask<UDNAAbilityTask_WaitInputPress>(OwningAbility);
	Task->bTestInitialState = bTestAlreadyPressed;
	return Task;
}

void UDNAAbilityTask_WaitInputPress::Activate()
{
	StartTime = GetWorld()->GetTimeSeconds();
	if (Ability)
	{
		if (bTestInitialState && IsLocallyControlled())
		{
			FDNAAbilitySpec *Spec = Ability->GetCurrentAbilitySpec();
			if (Spec && Spec->InputPressed)
			{
				OnPressCallback();
				return;
			}
		}

		DelegateHandle = DNAAbilitySystemComponent->AbilityReplicatedEventDelegate(EAbilityGenericReplicatedEvent::InputPressed, GetAbilitySpecHandle(), GetActivationPredictionKey()).AddUObject(this, &UDNAAbilityTask_WaitInputPress::OnPressCallback);
		if (IsForRemoteClient())
		{
			if (!DNAAbilitySystemComponent->CallReplicatedEventDelegateIfSet(EAbilityGenericReplicatedEvent::InputPressed, GetAbilitySpecHandle(), GetActivationPredictionKey()))
			{
				SetWaitingOnRemotePlayerData();
			}
		}
	}
}

void UDNAAbilityTask_WaitInputPress::OnDestroy(bool AbilityEnded)
{
	Super::OnDestroy(AbilityEnded);
}
