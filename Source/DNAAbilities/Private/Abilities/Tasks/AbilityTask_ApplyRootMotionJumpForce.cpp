// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionJumpForce.h"
#include "GameFramework/RootMotionSource.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Net/UnrealNetwork.h"

UDNAAbilityTask_ApplyRootMotionJumpForce::UDNAAbilityTask_ApplyRootMotionJumpForce(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	PathOffsetCurve = nullptr;
	TimeMappingCurve = nullptr;
	bHasLanded = false;
}

UDNAAbilityTask_ApplyRootMotionJumpForce* UDNAAbilityTask_ApplyRootMotionJumpForce::ApplyRootMotionJumpForce(UDNAAbility* OwningAbility, FName TaskInstanceName, FRotator Rotation, float Distance, float Height, float Duration, float MinimumLandedTriggerTime, bool bFinishOnLanded, UCurveVector* PathOffsetCurve, UCurveFloat* TimeMappingCurve)
{
	UDNAAbilitySystemGlobals::NonShipping_ApplyGlobalAbilityScaler_Duration(Duration);

	auto MyTask = NewDNAAbilityTask<UDNAAbilityTask_ApplyRootMotionJumpForce>(OwningAbility, TaskInstanceName);

	MyTask->ForceName = TaskInstanceName;
	MyTask->Rotation = Rotation;
	MyTask->Distance = Distance;
	MyTask->Height = Height;
	MyTask->Duration = FMath::Max(Duration, KINDA_SMALL_NUMBER); // No zero duration
	MyTask->MinimumLandedTriggerTime = MinimumLandedTriggerTime * Duration; // MinimumLandedTriggerTime is normalized
	MyTask->bFinishOnLanded = bFinishOnLanded;
	MyTask->PathOffsetCurve = PathOffsetCurve;
	MyTask->TimeMappingCurve = TimeMappingCurve;
	MyTask->SharedInitAndApply();

	return MyTask;
}

void UDNAAbilityTask_ApplyRootMotionJumpForce::Activate()
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActor());
	if (Character)
	{
		Character->LandedDelegate.AddDynamic(this, &UDNAAbilityTask_ApplyRootMotionJumpForce::OnLandedCallback);
	}
	SetWaitingOnAvatar();
}

void UDNAAbilityTask_ApplyRootMotionJumpForce::OnLandedCallback(const FHitResult& Hit)
{
	bHasLanded = true;

	ACharacter* Character = Cast<ACharacter>(GetAvatarActor());
	if (Character && Character->bClientUpdating)
	{
		// If in a move replay, we just mark that we landed so that next tick we trigger landed
	}
	else
	{
		// TriggerLanded immediately if we're past time allowed, otherwise it'll get caught next valid tick
		const float CurrentTime = GetWorld()->GetTimeSeconds();
		if (CurrentTime >= (StartTime+MinimumLandedTriggerTime))
		{
			TriggerLanded();
		}
	}
}

void UDNAAbilityTask_ApplyRootMotionJumpForce::TriggerLanded()
{
	OnLanded.Broadcast();

	if (bFinishOnLanded)
	{
		Finish();
	}
}

void UDNAAbilityTask_ApplyRootMotionJumpForce::SharedInitAndApply()
{
	if (DNAAbilitySystemComponent->AbilityActorInfo->MovementComponent.IsValid())
	{
		MovementComponent = Cast<UCharacterMovementComponent>(DNAAbilitySystemComponent->AbilityActorInfo->MovementComponent.Get());
		StartTime = GetWorld()->GetTimeSeconds();
		EndTime = StartTime + Duration;

		if (MovementComponent)
		{
			ForceName = ForceName.IsNone() ? FName("DNAAbilityTaskApplyRootMotionJumpForce") : ForceName;
			FRootMotionSource_JumpForce* JumpForce = new FRootMotionSource_JumpForce();
			JumpForce->InstanceName = ForceName;
			JumpForce->AccumulateMode = ERootMotionAccumulateMode::Override;
			JumpForce->Priority = 500;
			JumpForce->Duration = Duration;
			JumpForce->Rotation = Rotation;
			JumpForce->Distance = Distance;
			JumpForce->Height = Height;
			JumpForce->Duration = Duration;
			JumpForce->bDisableTimeout = bFinishOnLanded; // If we finish on landed, we need to disable force's timeout
			JumpForce->PathOffsetCurve = PathOffsetCurve;
			JumpForce->TimeMappingCurve = TimeMappingCurve;
			RootMotionSourceID = MovementComponent->ApplyRootMotionSource(JumpForce);

			if (Ability)
			{
				Ability->SetMovementSyncPoint(ForceName);
			}
		}
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UDNAAbilityTask_ApplyRootMotionJumpForce called in Ability %s with null MovementComponent; Task Instance Name %s."), 
			Ability ? *Ability->GetName() : TEXT("NULL"), 
			*InstanceName.ToString());
	}
}

void UDNAAbilityTask_ApplyRootMotionJumpForce::Finish()
{
	bIsFinished = true;

	if (!bIsSimulating)
	{
		AActor* MyActor = GetAvatarActor();
		if (MyActor)
		{
			MyActor->ForceNetUpdate();
			OnFinish.Broadcast();
		}
	}

	EndTask();
}

void UDNAAbilityTask_ApplyRootMotionJumpForce::TickTask(float DeltaTime)
{
	if (bIsFinished)
	{
		return;
	}

	const float CurrentTime = GetWorld()->GetTimeSeconds();

	if (bHasLanded && CurrentTime >= (StartTime+MinimumLandedTriggerTime))
	{
		TriggerLanded();
		return;
	}

	Super::TickTask(DeltaTime);

	AActor* MyActor = GetAvatarActor();
	if (MyActor)
	{
		if (!bFinishOnLanded && CurrentTime >= EndTime)
		{
			// Task has finished
			Finish();
		}
	}
	else
	{
		Finish();
	}
}

void UDNAAbilityTask_ApplyRootMotionJumpForce::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionJumpForce, Rotation);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionJumpForce, Distance);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionJumpForce, Height);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionJumpForce, Duration);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionJumpForce, MinimumLandedTriggerTime);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionJumpForce, bFinishOnLanded);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionJumpForce, PathOffsetCurve);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionJumpForce, TimeMappingCurve);
}

void UDNAAbilityTask_ApplyRootMotionJumpForce::PreDestroyFromReplication()
{
	Finish();
}

void UDNAAbilityTask_ApplyRootMotionJumpForce::OnDestroy(bool AbilityIsEnding)
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActor());
	if (Character)
	{
		Character->LandedDelegate.RemoveDynamic(this, &UDNAAbilityTask_ApplyRootMotionJumpForce::OnLandedCallback);
	}

	if (MovementComponent)
	{
		MovementComponent->RemoveRootMotionSourceByID(RootMotionSourceID);
	}

	Super::OnDestroy(AbilityIsEnding);
}
