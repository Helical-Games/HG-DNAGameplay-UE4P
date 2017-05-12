// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToForce.h"
#include "GameFramework/RootMotionSource.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Net/UnrealNetwork.h"

UDNAAbilityTask_ApplyRootMotionMoveToForce::UDNAAbilityTask_ApplyRootMotionMoveToForce(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bSetNewMovementMode = false;
	NewMovementMode = EMovementMode::MOVE_Walking;
	PreviousMovementMode = EMovementMode::MOVE_None;
	bRestrictSpeedToExpected = false;
	PathOffsetCurve = nullptr;
	VelocityOnFinishMode = ERootMotionFinishVelocityMode::MaintainLastRootMotionVelocity;
	SetVelocityOnFinish = FVector::ZeroVector;
	ClampVelocityOnFinish = 0.0f;
}

UDNAAbilityTask_ApplyRootMotionMoveToForce* UDNAAbilityTask_ApplyRootMotionMoveToForce::ApplyRootMotionMoveToForce(UDNAAbility* OwningAbility, FName TaskInstanceName, FVector TargetLocation, float Duration, bool bSetNewMovementMode, EMovementMode MovementMode, bool bRestrictSpeedToExpected, UCurveVector* PathOffsetCurve, ERootMotionFinishVelocityMode VelocityOnFinishMode, FVector SetVelocityOnFinish, float ClampVelocityOnFinish)
{
	UDNAAbilitySystemGlobals::NonShipping_ApplyGlobalAbilityScaler_Duration(Duration);

	auto MyTask = NewDNAAbilityTask<UDNAAbilityTask_ApplyRootMotionMoveToForce>(OwningAbility, TaskInstanceName);

	MyTask->ForceName = TaskInstanceName;
	MyTask->TargetLocation = TargetLocation;
	MyTask->Duration = FMath::Max(Duration, KINDA_SMALL_NUMBER); // Avoid negative or divide-by-zero cases
	MyTask->bSetNewMovementMode = bSetNewMovementMode;
	MyTask->NewMovementMode = MovementMode;
	MyTask->bRestrictSpeedToExpected = bRestrictSpeedToExpected;
	MyTask->PathOffsetCurve = PathOffsetCurve;
	MyTask->VelocityOnFinishMode = VelocityOnFinishMode;
	MyTask->SetVelocityOnFinish = SetVelocityOnFinish;
	MyTask->ClampVelocityOnFinish = ClampVelocityOnFinish;
	if (MyTask->GetAvatarActor() != nullptr)
	{
		MyTask->StartLocation = MyTask->GetAvatarActor()->GetActorLocation();
	}
	else
	{
		checkf(false, TEXT("UDNAAbilityTask_ApplyRootMotionMoveToForce called without valid avatar actor to get start location from."));
		MyTask->StartLocation = TargetLocation;
	}
	MyTask->SharedInitAndApply();

	return MyTask;
}

void UDNAAbilityTask_ApplyRootMotionMoveToForce::SharedInitAndApply()
{
	if (DNAAbilitySystemComponent->AbilityActorInfo->MovementComponent.IsValid())
	{
		MovementComponent = Cast<UCharacterMovementComponent>(DNAAbilitySystemComponent->AbilityActorInfo->MovementComponent.Get());
		StartTime = GetWorld()->GetTimeSeconds();
		EndTime = StartTime + Duration;

		if (MovementComponent)
		{
			if (bSetNewMovementMode)
			{
				PreviousMovementMode = MovementComponent->MovementMode;
				MovementComponent->SetMovementMode(NewMovementMode);
			}

			ForceName = ForceName.IsNone() ? FName("DNAAbilityTaskApplyRootMotionMoveToForce") : ForceName;
			FRootMotionSource_MoveToForce* MoveToForce = new FRootMotionSource_MoveToForce();
			MoveToForce->InstanceName = ForceName;
			MoveToForce->AccumulateMode = ERootMotionAccumulateMode::Override;
			MoveToForce->Settings.SetFlag(ERootMotionSourceSettingsFlags::UseSensitiveLiftoffCheck);
			MoveToForce->Priority = 1000;
			MoveToForce->TargetLocation = TargetLocation;
			MoveToForce->StartLocation = StartLocation;
			MoveToForce->Duration = Duration;
			MoveToForce->bRestrictSpeedToExpected = bRestrictSpeedToExpected;
			MoveToForce->PathOffsetCurve = PathOffsetCurve;
			RootMotionSourceID = MovementComponent->ApplyRootMotionSource(MoveToForce);

			if (Ability)
			{
				Ability->SetMovementSyncPoint(ForceName);
			}
		}
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UDNAAbilityTask_ApplyRootMotionMoveToForce called in Ability %s with null MovementComponent; Task Instance Name %s."), 
			Ability ? *Ability->GetName() : TEXT("NULL"), 
			*InstanceName.ToString());
	}
}

void UDNAAbilityTask_ApplyRootMotionMoveToForce::TickTask(float DeltaTime)
{
	if (bIsFinished)
	{
		return;
	}

	Super::TickTask(DeltaTime);

	AActor* MyActor = GetAvatarActor();
	if (MyActor)
	{
		float CurrentTime = GetWorld()->GetTimeSeconds();
		const bool bTimedOut = CurrentTime >= EndTime;

		const float ReachedDestinationDistanceSqr = 50.f * 50.f;
		const bool bReachedDestination = FVector::DistSquared(TargetLocation, MyActor->GetActorLocation()) < ReachedDestinationDistanceSqr;

		if (bTimedOut)
		{
			// Task has finished
			bIsFinished = true;
			if (!bIsSimulating)
			{
				MyActor->ForceNetUpdate();
				if (bReachedDestination)
				{
					OnTimedOutAndDestinationReached.Broadcast();
				}
				else
				{
					OnTimedOut.Broadcast();
				}
				EndTask();
			}
		}
	}
	else
	{
		bIsFinished = true;
		EndTask();
	}
}

void UDNAAbilityTask_ApplyRootMotionMoveToForce::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionMoveToForce, StartLocation);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionMoveToForce, TargetLocation);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionMoveToForce, Duration);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionMoveToForce, bSetNewMovementMode);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionMoveToForce, NewMovementMode);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionMoveToForce, bRestrictSpeedToExpected);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionMoveToForce, PathOffsetCurve);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionMoveToForce, VelocityOnFinishMode);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionMoveToForce, SetVelocityOnFinish);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionMoveToForce, ClampVelocityOnFinish);
}

void UDNAAbilityTask_ApplyRootMotionMoveToForce::PreDestroyFromReplication()
{
	bIsFinished = true;
	EndTask();
}

void UDNAAbilityTask_ApplyRootMotionMoveToForce::OnDestroy(bool AbilityIsEnding)
{
	if (MovementComponent)
	{
		MovementComponent->RemoveRootMotionSourceByID(RootMotionSourceID);

		if (bSetNewMovementMode)
		{
			MovementComponent->SetMovementMode(NewMovementMode);
		}

		if (VelocityOnFinishMode == ERootMotionFinishVelocityMode::SetVelocity)
		{
			SetFinishVelocity(FName("DNAAbilityTaskApplyRootMotionMoveToForce_EndForce"), SetVelocityOnFinish);
		}
		else if (VelocityOnFinishMode == ERootMotionFinishVelocityMode::ClampVelocity)
		{
			ClampFinishVelocity(FName("DNAAbilityTaskApplyRootMotionMoveToForce_VelocityClamp"), ClampVelocityOnFinish);
		}
	}

	Super::OnDestroy(AbilityIsEnding);
}
