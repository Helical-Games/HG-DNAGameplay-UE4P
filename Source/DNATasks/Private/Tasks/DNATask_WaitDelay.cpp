// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Tasks/DNATask_WaitDelay.h"
#include "Engine/EngineTypes.h"
#include "TimerManager.h"
#include "Engine/World.h"

UDNATask_WaitDelay::UDNATask_WaitDelay(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Time = 0.f;
	TimeStarted = 0.f;
}

UDNATask_WaitDelay* UDNATask_WaitDelay::TaskWaitDelay(TScriptInterface<IDNATaskOwnerInterface> TaskOwner, float Time, const uint8 Priority)
{
	UDNATask_WaitDelay* MyTask = NewTaskUninitialized<UDNATask_WaitDelay>();
	if (MyTask && TaskOwner.GetInterface() != nullptr)
	{
		MyTask->InitTask(*TaskOwner, Priority);
		MyTask->Time = Time;
	}
	return MyTask;
}

UDNATask_WaitDelay* UDNATask_WaitDelay::TaskWaitDelay(IDNATaskOwnerInterface& InTaskOwner, float Time, const uint8 Priority)
{
	if (Time <= 0.f)
	{
		return nullptr;
	}

	UDNATask_WaitDelay* MyTask = NewTaskUninitialized<UDNATask_WaitDelay>();
	if (MyTask)
	{
		MyTask->InitTask(InTaskOwner, Priority);
		MyTask->Time = Time;
	}
	return MyTask;
}

void UDNATask_WaitDelay::Activate()
{
	UWorld* World = GetWorld();
	TimeStarted = World->GetTimeSeconds();

	// Use a dummy timer handle as we don't need to store it for later but we don't need to look for something to clear
	FTimerHandle TimerHandle;
	World->GetTimerManager().SetTimer(TimerHandle, this, &UDNATask_WaitDelay::OnTimeFinish, Time, false);
}

void UDNATask_WaitDelay::OnTimeFinish()
{
	OnFinish.Broadcast();
	EndTask();
}

FString UDNATask_WaitDelay::GetDebugString() const
{
	float TimeLeft = Time - GetWorld()->TimeSince(TimeStarted);
	return FString::Printf(TEXT("WaitDelay. Time: %.2f. TimeLeft: %.2f"), Time, TimeLeft);
}
