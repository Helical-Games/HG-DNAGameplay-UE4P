// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToActorForce.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/RootMotionSource.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Net/UnrealNetwork.h"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
int32 DebugMoveToActorForce = 0;
static FAutoConsoleVariableRef CVarDebugMoveToActorForce(
TEXT("DNAAbilitySystem.DebugMoveToActorForce"),
	DebugMoveToActorForce,
	TEXT("Show debug info for MoveToActorForce"),
	ECVF_Default
	);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

UDNAAbilityTask_ApplyRootMotionMoveToActorForce::UDNAAbilityTask_ApplyRootMotionMoveToActorForce(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bDisableDestinationReachedInterrupt = false;
	bSetNewMovementMode = false;
	NewMovementMode = EMovementMode::MOVE_Walking;
	PreviousMovementMode = EMovementMode::MOVE_None;
	TargetLocationOffset = FVector::ZeroVector;
	OffsetAlignment = ERootMotionMoveToActorTargetOffsetType::AlignFromTargetToSource;
	bRestrictSpeedToExpected = false;
	PathOffsetCurve = nullptr;
	TimeMappingCurve = nullptr;
	TargetLerpSpeedHorizontalCurve = nullptr;
	TargetLerpSpeedVerticalCurve = nullptr;
	VelocityOnFinishMode = ERootMotionFinishVelocityMode::MaintainLastRootMotionVelocity;
	SetVelocityOnFinish = FVector::ZeroVector;
	ClampVelocityOnFinish = 0.0f;
}

UDNAAbilityTask_ApplyRootMotionMoveToActorForce* UDNAAbilityTask_ApplyRootMotionMoveToActorForce::ApplyRootMotionMoveToActorForce(UDNAAbility* OwningAbility, FName TaskInstanceName, AActor* TargetActor, FVector TargetLocationOffset, ERootMotionMoveToActorTargetOffsetType OffsetAlignment, float Duration, UCurveFloat* TargetLerpSpeedHorizontal, UCurveFloat* TargetLerpSpeedVertical, bool bSetNewMovementMode, EMovementMode MovementMode, bool bRestrictSpeedToExpected, UCurveVector* PathOffsetCurve, UCurveFloat* TimeMappingCurve, ERootMotionFinishVelocityMode VelocityOnFinishMode, FVector SetVelocityOnFinish, float ClampVelocityOnFinish, bool bDisableDestinationReachedInterrupt)
{
	auto MyTask = NewDNAAbilityTask<UDNAAbilityTask_ApplyRootMotionMoveToActorForce>(OwningAbility, TaskInstanceName);

	UDNAAbilitySystemGlobals::NonShipping_ApplyGlobalAbilityScaler_Duration(Duration);

	MyTask->ForceName = TaskInstanceName;
	MyTask->TargetActor = TargetActor;
	MyTask->TargetLocationOffset = TargetLocationOffset;
	MyTask->OffsetAlignment = OffsetAlignment;
	MyTask->Duration = FMath::Max(Duration, KINDA_SMALL_NUMBER); // Avoid negative or divide-by-zero cases
	MyTask->bDisableDestinationReachedInterrupt = bDisableDestinationReachedInterrupt;
	MyTask->TargetLerpSpeedHorizontalCurve = TargetLerpSpeedHorizontal;
	MyTask->TargetLerpSpeedVerticalCurve = TargetLerpSpeedVertical;
	MyTask->bSetNewMovementMode = bSetNewMovementMode;
	MyTask->NewMovementMode = MovementMode;
	MyTask->bRestrictSpeedToExpected = bRestrictSpeedToExpected;
	MyTask->PathOffsetCurve = PathOffsetCurve;
	MyTask->TimeMappingCurve = TimeMappingCurve;
	MyTask->VelocityOnFinishMode = VelocityOnFinishMode;
	MyTask->SetVelocityOnFinish = SetVelocityOnFinish;
	MyTask->ClampVelocityOnFinish = ClampVelocityOnFinish;
	if (MyTask->GetAvatarActor() != nullptr)
	{
		MyTask->StartLocation = MyTask->GetAvatarActor()->GetActorLocation();
	}
	else
	{
		checkf(false, TEXT("UDNAAbilityTask_ApplyRootMotionMoveToActorForce called without valid avatar actor to get start location from."));
		MyTask->StartLocation = TargetActor ? TargetActor->GetActorLocation() : FVector(0.f);
	}
	MyTask->SharedInitAndApply();

	return MyTask;
}

void UDNAAbilityTask_ApplyRootMotionMoveToActorForce::OnRep_TargetLocation()
{
	if (bIsSimulating)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		// Draw debug
		if (DebugMoveToActorForce > 0)
		{
			DrawDebugSphere(GetWorld(), TargetLocation, 50.f, 10, FColor::Green, false, 15.f);
		}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

		SetRootMotionTargetLocation(TargetLocation);
	}
}

void UDNAAbilityTask_ApplyRootMotionMoveToActorForce::SharedInitAndApply()
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

			// Set initial target location
			if (TargetActor)
			{
				TargetLocation = CalculateTargetOffset();
			}

			ForceName = ForceName.IsNone() ? FName("DNAAbilityTaskApplyRootMotionMoveToActorForce") : ForceName;
			FRootMotionSource_MoveToDynamicForce* MoveToActorForce = new FRootMotionSource_MoveToDynamicForce();
			MoveToActorForce->InstanceName = ForceName;
			MoveToActorForce->AccumulateMode = ERootMotionAccumulateMode::Override;
			MoveToActorForce->Settings.SetFlag(ERootMotionSourceSettingsFlags::UseSensitiveLiftoffCheck);
			MoveToActorForce->Priority = 900;
			MoveToActorForce->InitialTargetLocation = TargetLocation;
			MoveToActorForce->TargetLocation = TargetLocation;
			MoveToActorForce->StartLocation = StartLocation;
			MoveToActorForce->Duration = FMath::Max(Duration, KINDA_SMALL_NUMBER);
			MoveToActorForce->bRestrictSpeedToExpected = bRestrictSpeedToExpected;
			MoveToActorForce->PathOffsetCurve = PathOffsetCurve;
			MoveToActorForce->TimeMappingCurve = TimeMappingCurve;
			RootMotionSourceID = MovementComponent->ApplyRootMotionSource(MoveToActorForce);

			if (Ability)
			{
				Ability->SetMovementSyncPoint(ForceName);
			}
		}
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UDNAAbilityTask_ApplyRootMotionMoveToActorForce called in Ability %s with null MovementComponent; Task Instance Name %s."), 
			Ability ? *Ability->GetName() : TEXT("NULL"), 
			*InstanceName.ToString());
	}
}

FVector UDNAAbilityTask_ApplyRootMotionMoveToActorForce::CalculateTargetOffset() const
{
	check(TargetActor != nullptr);

	const FVector TargetActorLocation = TargetActor->GetActorLocation();
	FVector CalculatedTargetLocation = TargetActorLocation;
	
	if (OffsetAlignment == ERootMotionMoveToActorTargetOffsetType::AlignFromTargetToSource)
	{
		if (MovementComponent)
		{
			FVector ToSource = MovementComponent->GetActorLocation() - TargetActorLocation;
			ToSource.Z = 0.f;
			CalculatedTargetLocation += ToSource.ToOrientationQuat().RotateVector(TargetLocationOffset);
		}

	}
	else if (OffsetAlignment == ERootMotionMoveToActorTargetOffsetType::AlignToTargetForward)
	{
		CalculatedTargetLocation += TargetActor->GetActorQuat().RotateVector(TargetLocationOffset);
	}
	else if (OffsetAlignment == ERootMotionMoveToActorTargetOffsetType::AlignToWorldSpace)
	{
		CalculatedTargetLocation += TargetLocationOffset;
	}
	
	return CalculatedTargetLocation;
}

bool UDNAAbilityTask_ApplyRootMotionMoveToActorForce::UpdateTargetLocation(float DeltaTime)
{
	if (TargetActor && GetWorld())
	{
		const FVector PreviousTargetLocation = TargetLocation;
		FVector ExactTargetLocation = CalculateTargetOffset();

		const float CurrentTime = GetWorld()->GetTimeSeconds();
		const float CompletionPercent = (CurrentTime - StartTime) / Duration;

		const float TargetLerpSpeedHorizontal = TargetLerpSpeedHorizontalCurve ? TargetLerpSpeedHorizontalCurve->GetFloatValue(CompletionPercent) : 1000.f;
		const float TargetLerpSpeedVertical = TargetLerpSpeedVerticalCurve ? TargetLerpSpeedVerticalCurve->GetFloatValue(CompletionPercent) : 500.f;

		const float MaxHorizontalChange = FMath::Max(0.f, TargetLerpSpeedHorizontal * DeltaTime);
		const float MaxVerticalChange = FMath::Max(0.f, TargetLerpSpeedVertical * DeltaTime);

		FVector ToExactLocation = ExactTargetLocation - PreviousTargetLocation;
		FVector TargetLocationDelta = ToExactLocation;

		// Cap vertical lerp
		if (FMath::Abs(ToExactLocation.Z) > MaxVerticalChange)
		{
			if (ToExactLocation.Z >= 0.f)
			{
				TargetLocationDelta.Z = MaxVerticalChange;
			}
			else
			{
				TargetLocationDelta.Z = -MaxVerticalChange;
			}
		}

		// Cap horizontal lerp
		if (FMath::Abs(ToExactLocation.SizeSquared2D()) > MaxHorizontalChange*MaxHorizontalChange)
		{
			FVector ToExactLocationHorizontal(ToExactLocation.X, ToExactLocation.Y, 0.f);
			ToExactLocationHorizontal.Normalize();
			ToExactLocationHorizontal *= MaxHorizontalChange;

			TargetLocationDelta.X = ToExactLocationHorizontal.X;
			TargetLocationDelta.Y = ToExactLocationHorizontal.Y;
		}

		TargetLocation += TargetLocationDelta;

		return true;
	}

	return false;
}

void UDNAAbilityTask_ApplyRootMotionMoveToActorForce::SetRootMotionTargetLocation(FVector NewTargetLocation)
{
	if (MovementComponent)
	{
		auto RMS = MovementComponent->GetRootMotionSourceByID(RootMotionSourceID);
		if (RMS.IsValid())
		{
			if (RMS->GetScriptStruct() == FRootMotionSource_MoveToDynamicForce::StaticStruct())
			{
				FRootMotionSource_MoveToDynamicForce* MoveToActorForce = static_cast<FRootMotionSource_MoveToDynamicForce*>(RMS.Get());
				if (MoveToActorForce)
				{
					MoveToActorForce->SetTargetLocation(TargetLocation);
				}
			}
		}
	}
}

void UDNAAbilityTask_ApplyRootMotionMoveToActorForce::TickTask(float DeltaTime)
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

		// Update target location
		{
			const FVector PreviousTargetLocation = TargetLocation;
			if (UpdateTargetLocation(DeltaTime))
			{
				SetRootMotionTargetLocation(TargetLocation);
			}
			else
			{
				// TargetLocation not updated - TargetActor not around anymore, continue on to last set TargetLocation
			}
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		// Draw debug
		if (DebugMoveToActorForce > 0)
		{
			DrawDebugSphere(GetWorld(), TargetLocation, 50.f, 10, FColor::Green, false, 15.f);
		}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

		const float ReachedDestinationDistanceSqr = 50.f * 50.f;
		const bool bReachedDestination = FVector::DistSquared(TargetLocation, MyActor->GetActorLocation()) < ReachedDestinationDistanceSqr;

		if (bTimedOut || (bReachedDestination && !bDisableDestinationReachedInterrupt))
		{
			// Task has finished
			bIsFinished = true;
			if (!bIsSimulating)
			{
				MyActor->ForceNetUpdate();
				OnFinished.Broadcast(bReachedDestination, bTimedOut, TargetLocation);
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

void UDNAAbilityTask_ApplyRootMotionMoveToActorForce::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionMoveToActorForce, StartLocation);
	DOREPLIFETIME_CONDITION(UDNAAbilityTask_ApplyRootMotionMoveToActorForce, TargetLocation, COND_SimulatedOnly); // Autonomous and server calculate target location independently
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionMoveToActorForce, TargetActor);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionMoveToActorForce, TargetLocationOffset);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionMoveToActorForce, OffsetAlignment);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionMoveToActorForce, Duration);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionMoveToActorForce, bDisableDestinationReachedInterrupt);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionMoveToActorForce, TargetLerpSpeedHorizontalCurve);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionMoveToActorForce, TargetLerpSpeedVerticalCurve);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionMoveToActorForce, bSetNewMovementMode);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionMoveToActorForce, NewMovementMode);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionMoveToActorForce, bRestrictSpeedToExpected);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionMoveToActorForce, PathOffsetCurve);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionMoveToActorForce, TimeMappingCurve);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionMoveToActorForce, VelocityOnFinishMode);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionMoveToActorForce, SetVelocityOnFinish);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionMoveToActorForce, ClampVelocityOnFinish);
}

void UDNAAbilityTask_ApplyRootMotionMoveToActorForce::PreDestroyFromReplication()
{
	bIsFinished = true;
	EndTask();
}

void UDNAAbilityTask_ApplyRootMotionMoveToActorForce::OnDestroy(bool AbilityIsEnding)
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
			SetFinishVelocity(FName("DNAAbilityTaskApplyRootMotionMoveToActorForce_EndForce"), SetVelocityOnFinish);
		}
		else if (VelocityOnFinishMode == ERootMotionFinishVelocityMode::ClampVelocity)
		{
			ClampFinishVelocity(FName("DNAAbilityTaskApplyRootMotionMoveToActorForce_VelocityClamp"), ClampVelocityOnFinish);
		}
	}

	Super::OnDestroy(AbilityIsEnding);
}
