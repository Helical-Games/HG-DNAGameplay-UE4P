// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionRadialForce.h"
#include "GameFramework/RootMotionSource.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Net/UnrealNetwork.h"

UDNAAbilityTask_ApplyRootMotionRadialForce::UDNAAbilityTask_ApplyRootMotionRadialForce(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	StrengthDistanceFalloff = nullptr;
	StrengthOverTime = nullptr;
	bUseFixedWorldDirection = false;
	VelocityOnFinishMode = ERootMotionFinishVelocityMode::MaintainLastRootMotionVelocity;
	SetVelocityOnFinish = FVector::ZeroVector;
	ClampVelocityOnFinish = 0.0f;
}

UDNAAbilityTask_ApplyRootMotionRadialForce* UDNAAbilityTask_ApplyRootMotionRadialForce::ApplyRootMotionRadialForce(UDNAAbility* OwningAbility, FName TaskInstanceName, FVector Location, AActor* LocationActor, float Strength, float Duration, float Radius, bool bIsPush, bool bIsAdditive, bool bNoZForce, UCurveFloat* StrengthDistanceFalloff, UCurveFloat* StrengthOverTime, bool bUseFixedWorldDirection, FRotator FixedWorldDirection, ERootMotionFinishVelocityMode VelocityOnFinishMode, FVector SetVelocityOnFinish, float ClampVelocityOnFinish)
{
	UDNAAbilitySystemGlobals::NonShipping_ApplyGlobalAbilityScaler_Duration(Duration);

	auto MyTask = NewDNAAbilityTask<UDNAAbilityTask_ApplyRootMotionRadialForce>(OwningAbility, TaskInstanceName);

	MyTask->ForceName = TaskInstanceName;
	MyTask->Location = Location;
	MyTask->LocationActor = LocationActor;
	MyTask->Strength = Strength;
	MyTask->Radius = FMath::Max(Radius, SMALL_NUMBER); // No zero radius
	MyTask->Duration = Duration;
	MyTask->bIsPush = bIsPush;
	MyTask->bIsAdditive = bIsAdditive;
	MyTask->bNoZForce = bNoZForce;
	MyTask->StrengthDistanceFalloff = StrengthDistanceFalloff;
	MyTask->StrengthOverTime = StrengthOverTime;
	MyTask->bUseFixedWorldDirection = bUseFixedWorldDirection;
	MyTask->FixedWorldDirection = FixedWorldDirection;
	MyTask->VelocityOnFinishMode = VelocityOnFinishMode;
	MyTask->SetVelocityOnFinish = SetVelocityOnFinish;
	MyTask->ClampVelocityOnFinish = ClampVelocityOnFinish;
	MyTask->SharedInitAndApply();

	return MyTask;
}

void UDNAAbilityTask_ApplyRootMotionRadialForce::SharedInitAndApply()
{
	if (DNAAbilitySystemComponent->AbilityActorInfo->MovementComponent.IsValid())
	{
		MovementComponent = Cast<UCharacterMovementComponent>(DNAAbilitySystemComponent->AbilityActorInfo->MovementComponent.Get());
		StartTime = GetWorld()->GetTimeSeconds();
		EndTime = StartTime + Duration;

		if (MovementComponent)
		{
			ForceName = ForceName.IsNone() ? FName("DNAAbilityTaskApplyRootMotionRadialForce") : ForceName;
			FRootMotionSource_RadialForce* RadialForce = new FRootMotionSource_RadialForce();
			RadialForce->InstanceName = ForceName;
			RadialForce->AccumulateMode = bIsAdditive ? ERootMotionAccumulateMode::Additive : ERootMotionAccumulateMode::Override;
			RadialForce->Priority = 5;
			RadialForce->Location = Location;
			RadialForce->LocationActor = LocationActor;
			RadialForce->Duration = Duration;
			RadialForce->Radius = Radius;
			RadialForce->Strength = Strength;
			RadialForce->bIsPush = bIsPush;
			RadialForce->bNoZForce = bNoZForce;
			RadialForce->StrengthDistanceFalloff = StrengthDistanceFalloff;
			RadialForce->StrengthOverTime = StrengthOverTime;
			RadialForce->bUseFixedWorldDirection = bUseFixedWorldDirection;
			RadialForce->FixedWorldDirection = FixedWorldDirection;
			RootMotionSourceID = MovementComponent->ApplyRootMotionSource(RadialForce);

			if (Ability)
			{
				Ability->SetMovementSyncPoint(ForceName);
			}
		}
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UDNAAbilityTask_ApplyRootMotionRadialForce called in Ability %s with null MovementComponent; Task Instance Name %s."), 
			Ability ? *Ability->GetName() : TEXT("NULL"), 
			*InstanceName.ToString());
	}
}

void UDNAAbilityTask_ApplyRootMotionRadialForce::TickTask(float DeltaTime)
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
		const bool bIsInfiniteDuration = Duration < 0.f;
		if (!bIsInfiniteDuration && CurrentTime >= EndTime)
		{
			// Task has finished
			bIsFinished = true;
			if (!bIsSimulating)
			{
				MyActor->ForceNetUpdate();
				OnFinish.Broadcast();
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

void UDNAAbilityTask_ApplyRootMotionRadialForce::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionRadialForce, Location);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionRadialForce, LocationActor);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionRadialForce, Radius);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionRadialForce, Strength);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionRadialForce, Duration);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionRadialForce, bIsPush);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionRadialForce, bIsAdditive);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionRadialForce, bNoZForce);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionRadialForce, StrengthDistanceFalloff);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionRadialForce, StrengthOverTime);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionRadialForce, bUseFixedWorldDirection);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionRadialForce, FixedWorldDirection);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionRadialForce, VelocityOnFinishMode);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionRadialForce, SetVelocityOnFinish);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionRadialForce, ClampVelocityOnFinish);
}

void UDNAAbilityTask_ApplyRootMotionRadialForce::PreDestroyFromReplication()
{
	bIsFinished = true;
	EndTask();
}

void UDNAAbilityTask_ApplyRootMotionRadialForce::OnDestroy(bool AbilityIsEnding)
{
	if (MovementComponent)
	{
		MovementComponent->RemoveRootMotionSourceByID(RootMotionSourceID);

		if (VelocityOnFinishMode == ERootMotionFinishVelocityMode::SetVelocity)
		{
			SetFinishVelocity(FName("DNAAbilityTaskApplyRootMotionRadialForce_EndForce"), SetVelocityOnFinish);
		}
		else if (VelocityOnFinishMode == ERootMotionFinishVelocityMode::ClampVelocity)
		{
			ClampFinishVelocity(FName("DNAAbilityTaskApplyRootMotionRadialForce_VelocityClamp"), ClampVelocityOnFinish);
		}
	}

	Super::OnDestroy(AbilityIsEnding);
}
