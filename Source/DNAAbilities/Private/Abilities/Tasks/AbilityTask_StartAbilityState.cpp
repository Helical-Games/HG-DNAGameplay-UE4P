// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/Tasks/AbilityTask_StartAbilityState.h"

UDNAAbilityTask_StartAbilityState::UDNAAbilityTask_StartAbilityState(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bEndCurrentState = true;
	bWasEnded = false;
	bWasInterrupted = false;
}

UDNAAbilityTask_StartAbilityState* UDNAAbilityTask_StartAbilityState::StartAbilityState(UDNAAbility* OwningAbility, FName StateName, bool bEndCurrentState)
{
	auto Task = NewDNAAbilityTask<UDNAAbilityTask_StartAbilityState>(OwningAbility, StateName);
	Task->bEndCurrentState = bEndCurrentState;
	return Task;
}

void UDNAAbilityTask_StartAbilityState::Activate()
{
	if (Ability)
	{
		if (bEndCurrentState && Ability->OnDNAAbilityStateEnded.IsBound())
		{
			Ability->OnDNAAbilityStateEnded.Broadcast(NAME_None);
		}

		EndStateHandle = Ability->OnDNAAbilityStateEnded.AddUObject(this, &UDNAAbilityTask_StartAbilityState::OnEndState);
		InterruptStateHandle = Ability->OnDNAAbilityCancelled.AddUObject(this, &UDNAAbilityTask_StartAbilityState::OnInterruptState);
	}
}

void UDNAAbilityTask_StartAbilityState::OnDestroy(bool AbilityEnded)
{
	Super::OnDestroy(AbilityEnded);

	if (bWasInterrupted && OnStateInterrupted.IsBound())
	{
		OnStateInterrupted.Broadcast();
	}
	else if ((bWasEnded || AbilityEnded) && OnStateEnded.IsBound())
	{
		OnStateEnded.Broadcast();
	}

	if (Ability)
	{
		Ability->OnDNAAbilityCancelled.Remove(InterruptStateHandle);
		Ability->OnDNAAbilityStateEnded.Remove(EndStateHandle);
	}
}

void UDNAAbilityTask_StartAbilityState::OnEndState(FName StateNameToEnd)
{
	// All states end if 'NAME_None' is passed to this delegate
	if (StateNameToEnd.IsNone() || StateNameToEnd == InstanceName)
	{
		bWasEnded = true;

		EndTask();
	}
}

void UDNAAbilityTask_StartAbilityState::OnInterruptState()
{
	bWasInterrupted = true;
}

void UDNAAbilityTask_StartAbilityState::ExternalCancel()
{
	bWasInterrupted = true;

	Super::ExternalCancel();
}

FString UDNAAbilityTask_StartAbilityState::GetDebugString() const
{
	return FString::Printf(TEXT("%s (AbilityState)"), *InstanceName.ToString());
}
