// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "AbilitySystemComponent.h"

UDNAAbilityTask_WaitInputRelease::UDNAAbilityTask_WaitInputRelease(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	StartTime = 0.f;
	bTestInitialState = false;
}

void UDNAAbilityTask_WaitInputRelease::OnReleaseCallback()
{
	float ElapsedTime = GetWorld()->GetTimeSeconds() - StartTime;

	if (!Ability || !DNAAbilitySystemComponent)
	{
		return;
	}

	DNAAbilitySystemComponent->AbilityReplicatedEventDelegate(EAbilityGenericReplicatedEvent::InputReleased, GetAbilitySpecHandle(), GetActivationPredictionKey()).Remove(DelegateHandle);

	FScopedPredictionWindow ScopedPrediction(DNAAbilitySystemComponent, IsPredictingClient());

	if (IsPredictingClient())
	{
		// Tell the server about this
		DNAAbilitySystemComponent->ServerSetReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, GetAbilitySpecHandle(), GetActivationPredictionKey(), DNAAbilitySystemComponent->ScopedPredictionKey);
	}
	else
	{
		DNAAbilitySystemComponent->ConsumeGenericReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, GetAbilitySpecHandle(), GetActivationPredictionKey());
	}

	// We are done. Kill us so we don't keep getting broadcast messages
	OnRelease.Broadcast(ElapsedTime);
	EndTask();
}

UDNAAbilityTask_WaitInputRelease* UDNAAbilityTask_WaitInputRelease::WaitInputRelease(class UDNAAbility* OwningAbility, bool bTestAlreadyReleased)
{
	UDNAAbilityTask_WaitInputRelease* Task = NewDNAAbilityTask<UDNAAbilityTask_WaitInputRelease>(OwningAbility);
	Task->bTestInitialState = bTestAlreadyReleased;
	return Task;
}

void UDNAAbilityTask_WaitInputRelease::Activate()
{
	StartTime = GetWorld()->GetTimeSeconds();
	if (Ability)
	{
		if (bTestInitialState && IsLocallyControlled())
		{
			FDNAAbilitySpec *Spec = Ability->GetCurrentAbilitySpec();
			if (Spec && !Spec->InputPressed)
			{
				OnReleaseCallback();
				return;
			}
		}

		DelegateHandle = DNAAbilitySystemComponent->AbilityReplicatedEventDelegate(EAbilityGenericReplicatedEvent::InputReleased, GetAbilitySpecHandle(), GetActivationPredictionKey()).AddUObject(this, &UDNAAbilityTask_WaitInputRelease::OnReleaseCallback);
		if (IsForRemoteClient())
		{
			if (!DNAAbilitySystemComponent->CallReplicatedEventDelegateIfSet(EAbilityGenericReplicatedEvent::InputReleased, GetAbilitySpecHandle(), GetActivationPredictionKey()))
			{
				SetWaitingOnRemotePlayerData();
			}
		}
	}
}

void UDNAAbilityTask_WaitInputRelease::OnDestroy(bool AbilityEnded)
{
	Super::OnDestroy(AbilityEnded);
}
