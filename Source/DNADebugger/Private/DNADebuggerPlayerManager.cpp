// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DNADebuggerAddonManager.h"
#include "DNADebuggerPlayerManager.h"
#include "Engine/World.h"
#include "Components/InputComponent.h"
#include "DNADebuggerCategoryReplicator.h"
#include "DNADebuggerLocalController.h"
#include "Engine/DebugCameraController.h"

ADNADebuggerPlayerManager::ADNADebuggerPlayerManager(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bAllowTickOnDedicatedServer = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickInterval = 0.5f;

#if WITH_EDITOR
	SetIsTemporarilyHiddenInEditor(true);
#endif

#if WITH_EDITORONLY_DATA
	bHiddenEdLevel = true;
	bHiddenEdLayer = true;
	bHiddenEd = true;
	bEditable = false;
#endif

	bIsLocal = false;
	bInitialized = false;
}

void ADNADebuggerPlayerManager::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	const ENetMode NetMode = World->GetNetMode();
	
	bHasAuthority = (NetMode != NM_Client);
	bIsLocal = (NetMode != NM_DedicatedServer);
	bInitialized = true;

	if (bHasAuthority)
	{
		UpdateAuthReplicators();
		SetActorTickEnabled(true);
	}
	
	for (int32 Idx = 0; Idx < PendingRegistrations.Num(); Idx++)
	{
		RegisterReplicator(*PendingRegistrations[Idx]);
	}

	PendingRegistrations.Empty();
}

void ADNADebuggerPlayerManager::EndPlay(const EEndPlayReason::Type Reason)
{
	Super::EndPlay(Reason);

	for (int32 Idx = 0; Idx < PlayerData.Num(); Idx++)
	{
		FDNADebuggerPlayerData& TestData = PlayerData[Idx];
		if (IsValid(TestData.Controller))
		{
			TestData.Controller->Cleanup();
			TestData.Controller = nullptr;
		}
	}
}

void ADNADebuggerPlayerManager::TickActor(float DeltaTime, enum ELevelTick TickType, FActorTickFunction& ThisTickFunction)
{
	Super::TickActor(DeltaTime, TickType, ThisTickFunction);
	UpdateAuthReplicators();
};

void ADNADebuggerPlayerManager::UpdateAuthReplicators()
{
	UWorld* World = GetWorld();
	for (int32 Idx = PlayerData.Num() - 1; Idx >= 0; Idx--)
	{
		FDNADebuggerPlayerData& TestData = PlayerData[Idx];

		if (!IsValid(TestData.Replicator) || !IsValid(TestData.Replicator->GetReplicationOwner()))
		{
			if (IsValid(TestData.Replicator))
			{
				World->DestroyActor(TestData.Replicator);
			}

			if (IsValid(TestData.Controller))
			{
				TestData.Controller->Cleanup();
			}

			PlayerData.RemoveAt(Idx, 1, false);
		}
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; It++)
	{
		APlayerController* TestPC = It->Get();
		if (TestPC && !TestPC->IsA<ADebugCameraController>())
		{
			const bool bNeedsReplicator = (GetReplicator(*TestPC) == nullptr);
			if (bNeedsReplicator)
			{
				ADNADebuggerCategoryReplicator* Replicator = World->SpawnActorDeferred<ADNADebuggerCategoryReplicator>(ADNADebuggerCategoryReplicator::StaticClass(), FTransform::Identity);
				Replicator->SetReplicatorOwner(TestPC);
				Replicator->FinishSpawning(FTransform::Identity, true);
			}
		}
	}

	PrimaryActorTick.TickInterval = PlayerData.Num() ? 5.0f : 0.5f;
}

void ADNADebuggerPlayerManager::RegisterReplicator(ADNADebuggerCategoryReplicator& Replicator)
{
	APlayerController* OwnerPC = Replicator.GetReplicationOwner();
	if (OwnerPC == nullptr)
	{
		return;
	}

	if (!bInitialized)
	{
		PendingRegistrations.Add(&Replicator);
		return;
	}

	// keep all player related objects together for easy access and GC
	FDNADebuggerPlayerData NewData;
	NewData.Replicator = &Replicator;
	
	if (bIsLocal)
	{
		NewData.InputComponent = NewObject<UInputComponent>(OwnerPC, TEXT("DNADebug_Input"));
		NewData.InputComponent->Priority = -1;

		NewData.Controller = NewObject<UDNADebuggerLocalController>(OwnerPC, TEXT("DNADebug_Controller"));
		NewData.Controller->Initialize(Replicator, *this);
		NewData.Controller->BindInput(*NewData.InputComponent);

		OwnerPC->PushInputComponent(NewData.InputComponent);
	}
	else
	{
		NewData.Controller = nullptr;
		NewData.InputComponent = nullptr;
	}

	PlayerData.Add(NewData);
}

void ADNADebuggerPlayerManager::RefreshInputBindings(ADNADebuggerCategoryReplicator& Replicator)
{
	for (int32 Idx = 0; Idx < PlayerData.Num(); Idx++)
	{
		FDNADebuggerPlayerData& TestData = PlayerData[Idx];
		if (TestData.Replicator == &Replicator)
		{
			TestData.InputComponent->ClearActionBindings();
			TestData.InputComponent->ClearBindingValues();
			TestData.InputComponent->KeyBindings.Empty();

			TestData.Controller->BindInput(*TestData.InputComponent);
		}
	}
}

ADNADebuggerCategoryReplicator* ADNADebuggerPlayerManager::GetReplicator(const APlayerController& OwnerPC) const
{
	const FDNADebuggerPlayerData* DataPtr = GetPlayerData(OwnerPC);
	return DataPtr ? DataPtr->Replicator : nullptr;
}

UInputComponent* ADNADebuggerPlayerManager::GetInputComponent(const APlayerController& OwnerPC) const
{
	const FDNADebuggerPlayerData* DataPtr = GetPlayerData(OwnerPC);
	return DataPtr ? DataPtr->InputComponent : nullptr;
}

UDNADebuggerLocalController* ADNADebuggerPlayerManager::GetLocalController(const APlayerController& OwnerPC) const
{
	const FDNADebuggerPlayerData* DataPtr = GetPlayerData(OwnerPC);
	return DataPtr ? DataPtr->Controller : nullptr;
}

const FDNADebuggerPlayerData* ADNADebuggerPlayerManager::GetPlayerData(const APlayerController& OwnerPC) const
{
	for (int32 Idx = 0; Idx < PlayerData.Num(); Idx++)
	{
		const FDNADebuggerPlayerData& TestData = PlayerData[Idx];
		if (TestData.Replicator && TestData.Replicator->GetReplicationOwner() == &OwnerPC)
		{
			return &TestData;
		}
	}

	return nullptr;
}
