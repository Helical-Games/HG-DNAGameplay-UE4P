// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"
#include "DNADebuggerPlayerManager.generated.h"

class ADNADebuggerCategoryReplicator;
class APlayerController;
class UDNADebuggerLocalController;
class UInputComponent;

USTRUCT()
struct FDNADebuggerPlayerData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	UDNADebuggerLocalController* Controller;

	UPROPERTY()
	UInputComponent* InputComponent;

	UPROPERTY()
	ADNADebuggerCategoryReplicator* Replicator;
};

UCLASS(NotBlueprintable, NotBlueprintType, notplaceable, noteditinlinenew, hidedropdown, Transient)
class ADNADebuggerPlayerManager : public AActor
{
	GENERATED_UCLASS_BODY()

	virtual void TickActor(float DeltaTime, enum ELevelTick TickType, FActorTickFunction& ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

public:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
		
	void UpdateAuthReplicators();
	void RegisterReplicator(ADNADebuggerCategoryReplicator& Replicator);
	void RefreshInputBindings(ADNADebuggerCategoryReplicator& Replicator);

	ADNADebuggerCategoryReplicator* GetReplicator(const APlayerController& OwnerPC) const;
	UInputComponent* GetInputComponent(const APlayerController& OwnerPC) const;
	UDNADebuggerLocalController* GetLocalController(const APlayerController& OwnerPC) const;
	
	const FDNADebuggerPlayerData* GetPlayerData(const APlayerController& OwnerPC) const;

	static ADNADebuggerPlayerManager& GetCurrent(UWorld* World);

protected:

	UPROPERTY()
	TArray<FDNADebuggerPlayerData> PlayerData;

	UPROPERTY()
	TArray<ADNADebuggerCategoryReplicator*> PendingRegistrations;

	uint32 bHasAuthority : 1;
	uint32 bIsLocal : 1;
	uint32 bInitialized : 1;
};
