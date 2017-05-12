// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/Tasks/AbilityTask_WaitVelocityChange.h"
#include "GameFramework/MovementComponent.h"

UDNAAbilityTask_WaitVelocityChange::UDNAAbilityTask_WaitVelocityChange(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
}

void UDNAAbilityTask_WaitVelocityChange::TickTask(float DeltaTime)
{
	if (CachedMovementComponent)
	{
		float dot = FVector::DotProduct(Direction, CachedMovementComponent->Velocity);

		if (dot > MinimumMagnitude)
		{
			OnVelocityChage.Broadcast();
			EndTask();
		}
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UDNAAbilityTask_WaitVelocityChange ticked without a valid movement component. ending."));
		EndTask();
	}
}

UDNAAbilityTask_WaitVelocityChange* UDNAAbilityTask_WaitVelocityChange::CreateWaitVelocityChange(UDNAAbility* OwningAbility, FVector InDirection, float InMinimumMagnitude)
{
	auto MyObj = NewDNAAbilityTask<UDNAAbilityTask_WaitVelocityChange>(OwningAbility);

	MyObj->MinimumMagnitude = InMinimumMagnitude;
	MyObj->Direction = InDirection.GetSafeNormal();
	

	return MyObj;
}

void UDNAAbilityTask_WaitVelocityChange::Activate()
{
	const FDNAAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	CachedMovementComponent = ActorInfo->MovementComponent.Get();
	SetWaitingOnAvatar();
}
