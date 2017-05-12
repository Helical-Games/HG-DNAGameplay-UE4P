// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/Tasks/AbilityTask_VisualizeTargeting.h"
#include "TimerManager.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "AbilitySystemComponent.h"

UDNAAbilityTask_VisualizeTargeting::UDNAAbilityTask_VisualizeTargeting(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

UDNAAbilityTask_VisualizeTargeting* UDNAAbilityTask_VisualizeTargeting::VisualizeTargeting(UDNAAbility* OwningAbility, TSubclassOf<ADNAAbilityTargetActor> InTargetClass, FName TaskInstanceName, float Duration)
{
	UDNAAbilityTask_VisualizeTargeting* MyObj = NewDNAAbilityTask<UDNAAbilityTask_VisualizeTargeting>(OwningAbility, TaskInstanceName);		//Register for task list here, providing a given FName as a key
	MyObj->TargetClass = InTargetClass;
	MyObj->TargetActor = NULL;
	MyObj->SetDuration(Duration);
	return MyObj;
}

UDNAAbilityTask_VisualizeTargeting* UDNAAbilityTask_VisualizeTargeting::VisualizeTargetingUsingActor(UDNAAbility* OwningAbility, ADNAAbilityTargetActor* InTargetActor, FName TaskInstanceName, float Duration)
{
	UDNAAbilityTask_VisualizeTargeting* MyObj = NewDNAAbilityTask<UDNAAbilityTask_VisualizeTargeting>(OwningAbility, TaskInstanceName);		//Register for task list here, providing a given FName as a key
	MyObj->TargetClass = NULL;
	MyObj->TargetActor = InTargetActor;
	MyObj->SetDuration(Duration);
	return MyObj;
}

void UDNAAbilityTask_VisualizeTargeting::Activate()
{
	// Need to handle case where target actor was passed into task
	if (Ability && (TargetClass == NULL))
	{
		if (TargetActor.IsValid())
		{
			ADNAAbilityTargetActor* SpawnedActor = TargetActor.Get();

			TargetClass = SpawnedActor->GetClass();

			if (ShouldSpawnTargetActor())
			{
				InitializeTargetActor(SpawnedActor);
				FinalizeTargetActor(SpawnedActor);
			}
			else
			{
				TargetActor = NULL;

				// We may need a better solution here.  We don't know the target actor isn't needed till after it's already been spawned.
				SpawnedActor->Destroy();
				SpawnedActor = nullptr;
			}
		}
		else
		{
			EndTask();
		}
	}
}

bool UDNAAbilityTask_VisualizeTargeting::BeginSpawningActor(UDNAAbility* OwningAbility, TSubclassOf<ADNAAbilityTargetActor> InTargetClass, ADNAAbilityTargetActor*& SpawnedActor)
{
	SpawnedActor = nullptr;

	if (Ability)
	{
		if (ShouldSpawnTargetActor())
		{
			UClass* Class = *InTargetClass;
			if (Class != NULL)
			{
				UWorld* const World = GEngine->GetWorldFromContextObject(OwningAbility);
				if (World)
				{
					SpawnedActor = World->SpawnActorDeferred<ADNAAbilityTargetActor>(Class, FTransform::Identity, NULL, NULL, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
				}
			}

			if (SpawnedActor)
			{
				TargetActor = SpawnedActor;
				InitializeTargetActor(SpawnedActor);
			}
		}
	}

	return (SpawnedActor != nullptr);
}

void UDNAAbilityTask_VisualizeTargeting::FinishSpawningActor(UDNAAbility* OwningAbility, ADNAAbilityTargetActor* SpawnedActor)
{
	if (SpawnedActor)
	{
		check(TargetActor == SpawnedActor);

		const FTransform SpawnTransform = DNAAbilitySystemComponent->GetOwner()->GetTransform();

		SpawnedActor->FinishSpawning(SpawnTransform);

		FinalizeTargetActor(SpawnedActor);
	}
}

void UDNAAbilityTask_VisualizeTargeting::SetDuration(const float Duration)
{
	if (Duration > 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(TimerHandle_OnTimeElapsed, this, &UDNAAbilityTask_VisualizeTargeting::OnTimeElapsed, Duration, false);
	}
}

bool UDNAAbilityTask_VisualizeTargeting::ShouldSpawnTargetActor() const
{
	check(TargetClass);
	check(Ability);

	// Spawn the actor if this is a locally controlled ability (always) or if this is a replicating targeting mode.
	// (E.g., server will spawn this target actor to replicate to all non owning clients)

	const ADNAAbilityTargetActor* CDO = CastChecked<ADNAAbilityTargetActor>(TargetClass->GetDefaultObject());

	const bool bReplicates = CDO->GetIsReplicated();
	const bool bIsLocallyControlled = Ability->GetCurrentActorInfo()->IsLocallyControlled();

	return (bReplicates || bIsLocallyControlled);
}

void UDNAAbilityTask_VisualizeTargeting::InitializeTargetActor(ADNAAbilityTargetActor* SpawnedActor) const
{
	check(SpawnedActor);
	check(Ability);

	SpawnedActor->MasterPC = Ability->GetCurrentActorInfo()->PlayerController.Get();
}

void UDNAAbilityTask_VisualizeTargeting::FinalizeTargetActor(ADNAAbilityTargetActor* SpawnedActor) const
{
	check(SpawnedActor);
	check(Ability);

	DNAAbilitySystemComponent->SpawnedTargetActors.Push(SpawnedActor);

	SpawnedActor->StartTargeting(Ability);
}

void UDNAAbilityTask_VisualizeTargeting::OnDestroy(bool AbilityEnded)
{
	if (TargetActor.IsValid())
	{
		TargetActor->Destroy();
	}

	GetWorld()->GetTimerManager().ClearTimer(TimerHandle_OnTimeElapsed);

	Super::OnDestroy(AbilityEnded);
}

void UDNAAbilityTask_VisualizeTargeting::OnTimeElapsed()
{
	TimeElapsed.Broadcast();
	EndTask();
}
