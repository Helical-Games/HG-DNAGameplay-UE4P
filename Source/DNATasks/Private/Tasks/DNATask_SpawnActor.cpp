// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Tasks/DNATask_SpawnActor.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"

UDNATask_SpawnActor* UDNATask_SpawnActor::SpawnActor(TScriptInterface<IDNATaskOwnerInterface> TaskOwner, FVector SpawnLocation, FRotator SpawnRotation, TSubclassOf<AActor> Class, bool bSpawnOnlyOnAuthority)
{
	if (TaskOwner.GetInterface() == nullptr)
	{
		return nullptr;
	}

	bool bCanSpawn = true;
	if (bSpawnOnlyOnAuthority == true)
	{
		AActor* TaskOwnerActor = TaskOwner->GetDNATaskOwner(nullptr);
		if (TaskOwnerActor)
		{
			bCanSpawn = (TaskOwnerActor->Role == ROLE_Authority);
		}
		else
		{
			// @todo add warning here
		}
	}

	UDNATask_SpawnActor* MyTask = nullptr;
	if (bCanSpawn)
	{
		MyTask = NewTask<UDNATask_SpawnActor>(TaskOwner);
		if (MyTask)
		{
			MyTask->CachedSpawnLocation = SpawnLocation;
			MyTask->CachedSpawnRotation = SpawnRotation;
			MyTask->ClassToSpawn = Class;
		}
	}
	return MyTask;
}

bool UDNATask_SpawnActor::BeginSpawningActor(UObject* WorldContextObject, AActor*& SpawnedActor)
{
	UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if (World)
	{
		SpawnedActor = World->SpawnActorDeferred<AActor>(ClassToSpawn, FTransform(CachedSpawnRotation, CachedSpawnLocation), NULL, NULL, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	}
	
	if (SpawnedActor == nullptr)
	{
		DidNotSpawn.Broadcast(nullptr);
		return false;
	}

	return true;
}

void UDNATask_SpawnActor::FinishSpawningActor(UObject* WorldContextObject, AActor* SpawnedActor)
{
	if (SpawnedActor)
	{
		const FTransform SpawnTransform(CachedSpawnRotation, CachedSpawnLocation);		
		SpawnedActor->FinishSpawning(SpawnTransform);
		Success.Broadcast(SpawnedActor);
	}

	EndTask();
}
