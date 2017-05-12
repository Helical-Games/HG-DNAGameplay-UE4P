// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "TimerManager.h"
#include "AbilitySystemGlobals.h"

UDNAAbilityTask_WaitDelay::UDNAAbilityTask_WaitDelay(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Time = 0.f;
	TimeStarted = 0.f;
}

UDNAAbilityTask_WaitDelay* UDNAAbilityTask_WaitDelay::WaitDelay(UDNAAbility* OwningAbility, float Time)
{
	UDNAAbilitySystemGlobals::NonShipping_ApplyGlobalAbilityScaler_Duration(Time);

	auto MyObj = NewDNAAbilityTask<UDNAAbilityTask_WaitDelay>(OwningAbility);
	MyObj->Time = Time;
	return MyObj;
}

void UDNAAbilityTask_WaitDelay::Activate()
{
	UWorld* World = GetWorld();
	TimeStarted = World->GetTimeSeconds();

	// Use a dummy timer handle as we don't need to store it for later but we don't need to look for something to clear
	FTimerHandle TimerHandle;
	World->GetTimerManager().SetTimer(TimerHandle, this, &UDNAAbilityTask_WaitDelay::OnTimeFinish, Time, false);
}

void UDNAAbilityTask_WaitDelay::OnDestroy(bool AbilityEnded)
{
	Super::OnDestroy(AbilityEnded);
}

void UDNAAbilityTask_WaitDelay::OnTimeFinish()
{
	OnFinish.Broadcast();
	EndTask();
}

FString UDNAAbilityTask_WaitDelay::GetDebugString() const
{
	float TimeLeft = Time - GetWorld()->TimeSince(TimeStarted);
	return FString::Printf(TEXT("WaitDelay. Time: %.2f. TimeLeft: %.2f"), Time, TimeLeft);
}