// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/Tasks/AbilityTask_Repeat.h"
#include "TimerManager.h"

UDNAAbilityTask_Repeat::UDNAAbilityTask_Repeat(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

void UDNAAbilityTask_Repeat::PerformAction()
{		
	OnPerformAction.Broadcast(ActionCounter);
	++ActionCounter;
	if (ActionCounter >= ActionPerformancesDesired)		//Should we allow negative values of ActionPerformancesDesired to mean "unlimited"?
	{
		OnFinished.Broadcast(-1);
		EndTask();
	}
}

UDNAAbilityTask_Repeat* UDNAAbilityTask_Repeat::RepeatAction(UDNAAbility* OwningAbility, float InTimeBetweenActions, int32 TotalActionCount)
{
	auto MyObj = NewDNAAbilityTask<UDNAAbilityTask_Repeat>(OwningAbility);

	//TODO Validate/fix TimeBetweenActions and TotalActionCount values as needed
	MyObj->ActionPerformancesDesired = TotalActionCount;
	MyObj->TimeBetweenActions = InTimeBetweenActions;
	MyObj->ActionCounter = 0;

	return MyObj;
}

void UDNAAbilityTask_Repeat::Activate()
{
	if (ActionCounter < ActionPerformancesDesired)
	{
		PerformAction();
		if (ActionCounter < ActionPerformancesDesired)
		{
			GetWorld()->GetTimerManager().SetTimer(TimerHandle_PerformAction, this, &UDNAAbilityTask_Repeat::PerformAction, TimeBetweenActions, true);
		}
	}
	else
	{
		OnFinished.Broadcast(-1);
		EndTask();
	}
}

void UDNAAbilityTask_Repeat::OnDestroy(bool AbilityIsEnding)
{
	GetWorld()->GetTimerManager().ClearTimer(TimerHandle_PerformAction);

	Super::OnDestroy(AbilityIsEnding);
}

FString UDNAAbilityTask_Repeat::GetDebugString() const
{
	return FString::Printf(TEXT("RepeatAction. TimeBetweenActions: %.2f. ActionCounter: %d"), TimeBetweenActions, ActionCounter);
}
