// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/Tasks/AbilityTask_SpawnActor.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "AbilitySystemComponent.h"

UDNAAbilityTask_SpawnActor::UDNAAbilityTask_SpawnActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}

UDNAAbilityTask_SpawnActor* UDNAAbilityTask_SpawnActor::SpawnActor(UDNAAbility* OwningAbility, FDNAAbilityTargetDataHandle TargetData, TSubclassOf<AActor> InClass)
{
	UDNAAbilityTask_SpawnActor* MyObj = NewDNAAbilityTask<UDNAAbilityTask_SpawnActor>(OwningAbility);
	MyObj->CachedTargetDataHandle = MoveTemp(TargetData);
	return MyObj;
}

// ---------------------------------------------------------------------------------------

bool UDNAAbilityTask_SpawnActor::BeginSpawningActor(UDNAAbility* OwningAbility, FDNAAbilityTargetDataHandle TargetData, TSubclassOf<AActor> InClass, AActor*& SpawnedActor)
{
	if (Ability && Ability->GetCurrentActorInfo()->IsNetAuthority())
	{
		UWorld* const World = GEngine->GetWorldFromContextObject(OwningAbility);
		if (World)
		{
			SpawnedActor = World->SpawnActorDeferred<AActor>(InClass, FTransform::Identity, NULL, NULL, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		}
	}
	
	if (SpawnedActor == nullptr)
	{
		DidNotSpawn.Broadcast(nullptr);
		return false;
	}

	return true;
}

void UDNAAbilityTask_SpawnActor::FinishSpawningActor(UDNAAbility* OwningAbility, FDNAAbilityTargetDataHandle TargetData, AActor* SpawnedActor)
{
	if (SpawnedActor)
	{
		bool bTransformSet = false;
		FTransform SpawnTransform;
		if (FDNAAbilityTargetData* LocationData = CachedTargetDataHandle.Get(0))		//Hardcode to use data 0. It's OK if data isn't useful/valid.
		{
			//Set location. Rotation is unaffected.
			if (LocationData->HasHitResult())
			{
				SpawnTransform.SetLocation(LocationData->GetHitResult()->Location);
				bTransformSet = true;
			}
			else if (LocationData->HasEndPoint())
			{
				SpawnTransform = LocationData->GetEndPointTransform();
				bTransformSet = true;
			}
			}
		if (!bTransformSet)
		{
			SpawnTransform = AbilitySystemComponent->GetOwner()->GetTransform();
		}

		SpawnedActor->FinishSpawning(SpawnTransform);

		Success.Broadcast(SpawnedActor);
	}

	EndTask();
}

// ---------------------------------------------------------------------------------------

