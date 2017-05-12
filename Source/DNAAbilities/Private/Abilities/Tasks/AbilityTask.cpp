// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilitySystemComponent.h"

UDNAAbilityTask::UDNAAbilityTask(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WaitStateBitMask = (uint8)EDNAAbilityTaskWaitState::WaitingOnGame;
}

FDNAAbilitySpecHandle UDNAAbilityTask::GetAbilitySpecHandle() const
{
	return Ability ? Ability->GetCurrentAbilitySpecHandle() : FDNAAbilitySpecHandle();
}

void UDNAAbilityTask::SetDNAAbilitySystemComponent(UDNAAbilitySystemComponent* InDNAAbilitySystemComponent)
{
	DNAAbilitySystemComponent = InDNAAbilitySystemComponent;
}

void UDNAAbilityTask::InitSimulatedTask(UDNATasksComponent& InDNATasksComponent)
{
	UDNATask::InitSimulatedTask(InDNATasksComponent);

	SetDNAAbilitySystemComponent(Cast<UDNAAbilitySystemComponent>(TasksComponent.Get()));
}

FPredictionKey UDNAAbilityTask::GetActivationPredictionKey() const
{
	return Ability ? Ability->GetCurrentActivationInfo().GetActivationPredictionKey() : FPredictionKey();
}

bool UDNAAbilityTask::IsPredictingClient() const
{
	return Ability && Ability->IsPredictingClient();
}

bool UDNAAbilityTask::IsForRemoteClient() const
{
	return Ability && Ability->IsForRemoteClient();
}

bool UDNAAbilityTask::IsLocallyControlled() const
{
	return Ability && Ability->IsLocallyControlled();
}

bool UDNAAbilityTask::CallOrAddReplicatedDelegate(EAbilityGenericReplicatedEvent::Type Event, FSimpleMulticastDelegate::FDelegate Delegate)
{
	if (!DNAAbilitySystemComponent->CallOrAddReplicatedDelegate(Event, GetAbilitySpecHandle(), GetActivationPredictionKey(), Delegate))
	{
		SetWaitingOnRemotePlayerData();
		return false;
	}
	return true;
}

void UDNAAbilityTask::SetWaitingOnRemotePlayerData()
{
	if (Ability && IsPendingKill() == false && DNAAbilitySystemComponent)
	{
		WaitStateBitMask |= (uint8)EDNAAbilityTaskWaitState::WaitingOnUser;
		Ability->NotifyDNAAbilityTaskWaitingOnPlayerData(this);
	}
}

void UDNAAbilityTask::ClearWaitingOnRemotePlayerData()
{
	WaitStateBitMask &= ~((uint8)EDNAAbilityTaskWaitState::WaitingOnUser);
}

bool UDNAAbilityTask::IsWaitingOnRemotePlayerdata() const
{
	return (WaitStateBitMask & (uint8)EDNAAbilityTaskWaitState::WaitingOnUser) != 0;
}

void UDNAAbilityTask::SetWaitingOnAvatar()
{
	if (Ability && IsPendingKill() == false && DNAAbilitySystemComponent)
	{
		WaitStateBitMask |= (uint8)EDNAAbilityTaskWaitState::WaitingOnAvatar;
		Ability->NotifyDNAAbilityTaskWaitingOnAvatar(this);
	}
}

void UDNAAbilityTask::ClearWaitingOnAvatar()
{
	WaitStateBitMask &= ~((uint8)EDNAAbilityTaskWaitState::WaitingOnAvatar);
}

bool UDNAAbilityTask::IsWaitingOnAvatar() const
{
	return (WaitStateBitMask & (uint8)EDNAAbilityTaskWaitState::WaitingOnAvatar) != 0;
}
