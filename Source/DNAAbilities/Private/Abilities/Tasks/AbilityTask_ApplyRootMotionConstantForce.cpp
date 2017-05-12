// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "GameFramework/RootMotionSource.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Net/UnrealNetwork.h"

UDNAAbilityTask_ApplyRootMotionConstantForce::UDNAAbilityTask_ApplyRootMotionConstantForce(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	StrengthOverTime = nullptr;
}

UDNAAbilityTask_ApplyRootMotionConstantForce* UDNAAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(UDNAAbility* OwningAbility, FName TaskInstanceName, FVector WorldDirection, float Strength, float Duration, bool bIsAdditive, bool bDisableImpartingVelocityOnRemoval, UCurveFloat* StrengthOverTime)
{
	UDNAAbilitySystemGlobals::NonShipping_ApplyGlobalAbilityScaler_Duration(Duration);

	auto MyTask = NewDNAAbilityTask<UDNAAbilityTask_ApplyRootMotionConstantForce>(OwningAbility, TaskInstanceName);

	MyTask->ForceName = TaskInstanceName;
	MyTask->WorldDirection = WorldDirection.GetSafeNormal();
	MyTask->Strength = Strength;
	MyTask->Duration = Duration;
	MyTask->bIsAdditive = bIsAdditive;
	MyTask->bDisableImpartingVelocityOnRemoval = bDisableImpartingVelocityOnRemoval;
	MyTask->StrengthOverTime = StrengthOverTime;
	MyTask->SharedInitAndApply();

	return MyTask;
}

void UDNAAbilityTask_ApplyRootMotionConstantForce::SharedInitAndApply()
{
	if (DNAAbilitySystemComponent->AbilityActorInfo->MovementComponent.IsValid())
	{
		MovementComponent = Cast<UCharacterMovementComponent>(DNAAbilitySystemComponent->AbilityActorInfo->MovementComponent.Get());
		StartTime = GetWorld()->GetTimeSeconds();
		EndTime = StartTime + Duration;

		if (MovementComponent)
		{
			ForceName = ForceName.IsNone() ? FName("DNAAbilityTaskApplyRootMotionConstantForce"): ForceName;
			FRootMotionSource_ConstantForce* ConstantForce = new FRootMotionSource_ConstantForce();
			ConstantForce->InstanceName = ForceName;
			ConstantForce->AccumulateMode = bIsAdditive ? ERootMotionAccumulateMode::Additive : ERootMotionAccumulateMode::Override;
			if (bDisableImpartingVelocityOnRemoval)
			{
				ConstantForce->bImpartsVelocityOnRemoval = false;
			}
			ConstantForce->Priority = 5;
			ConstantForce->Force = WorldDirection * Strength;
			ConstantForce->Duration = Duration;
			ConstantForce->StrengthOverTime = StrengthOverTime;
			RootMotionSourceID = MovementComponent->ApplyRootMotionSource(ConstantForce);

			if (Ability)
			{
				Ability->SetMovementSyncPoint(ForceName);
			}
		}
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UDNAAbilityTask_ApplyRootMotionConstantForce called in Ability %s with null MovementComponent; Task Instance Name %s."), 
			Ability ? *Ability->GetName() : TEXT("NULL"), 
			*InstanceName.ToString());
	}
}

void UDNAAbilityTask_ApplyRootMotionConstantForce::TickTask(float DeltaTime)
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

void UDNAAbilityTask_ApplyRootMotionConstantForce::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionConstantForce, WorldDirection);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionConstantForce, Strength);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionConstantForce, Duration);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionConstantForce, bIsAdditive);
	DOREPLIFETIME(UDNAAbilityTask_ApplyRootMotionConstantForce, StrengthOverTime);
}

void UDNAAbilityTask_ApplyRootMotionConstantForce::PreDestroyFromReplication()
{
	bIsFinished = true;
	EndTask();
}

void UDNAAbilityTask_ApplyRootMotionConstantForce::OnDestroy(bool AbilityIsEnding)
{
	if (MovementComponent)
	{
		MovementComponent->RemoveRootMotionSourceByID(RootMotionSourceID);
	}

	Super::OnDestroy(AbilityIsEnding);
}
