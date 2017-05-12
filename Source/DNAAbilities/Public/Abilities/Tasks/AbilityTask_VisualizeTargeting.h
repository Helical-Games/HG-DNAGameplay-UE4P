// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Engine/EngineTypes.h"
#include "Abilities/DNAAbilityTargetActor.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_VisualizeTargeting.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVisualizeTargetingDelegate);

UCLASS(MinimalAPI)
class UDNAAbilityTask_VisualizeTargeting: public UDNAAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FVisualizeTargetingDelegate TimeElapsed;

	void OnTimeElapsed();

	/** Spawns target actor and uses it for visualization. */
	UFUNCTION(BlueprintCallable, meta=(HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true", HideSpawnParms="Instigator"), Category="Ability|Tasks")
	static UDNAAbilityTask_VisualizeTargeting* VisualizeTargeting(UDNAAbility* OwningAbility, TSubclassOf<ADNAAbilityTargetActor> Class, FName TaskInstanceName, float Duration = -1.0f);

	/** Visualize target using a specified target actor. */
	UFUNCTION(BlueprintCallable, meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true"), Category = "Ability|Tasks")
	static UDNAAbilityTask_VisualizeTargeting* VisualizeTargetingUsingActor(UDNAAbility* OwningAbility, ADNAAbilityTargetActor* TargetActor, FName TaskInstanceName, float Duration = -1.0f);

	virtual void Activate() override;

	UFUNCTION(BlueprintCallable, meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true"), Category = "Abilities")
	bool BeginSpawningActor(UDNAAbility* OwningAbility, TSubclassOf<ADNAAbilityTargetActor> Class, ADNAAbilityTargetActor*& SpawnedActor);

	UFUNCTION(BlueprintCallable, meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true"), Category = "Abilities")
	void FinishSpawningActor(UDNAAbility* OwningAbility, ADNAAbilityTargetActor* SpawnedActor);

protected:

	void SetDuration(const float Duration);

	bool ShouldSpawnTargetActor() const;
	void InitializeTargetActor(ADNAAbilityTargetActor* SpawnedActor) const;
	void FinalizeTargetActor(ADNAAbilityTargetActor* SpawnedActor) const;

	virtual void OnDestroy(bool AbilityEnded) override;

protected:

	TSubclassOf<ADNAAbilityTargetActor> TargetClass;

	/** The TargetActor that we spawned */
	TWeakObjectPtr<ADNAAbilityTargetActor>	TargetActor;

	/** Handle for efficient management of OnTimeElapsed timer */
	FTimerHandle TimerHandle_OnTimeElapsed;
};


/**
*	Requirements for using Begin/Finish SpawningActor functionality:
*		-Have a parameters named 'Class' in your Proxy factor function (E.g., WaitTargetdata)
*		-Have a function named BeginSpawningActor w/ the same Class parameter
*			-This function should spawn the actor with SpawnActorDeferred and return true/false if it spawned something.
*		-Have a function named FinishSpawningActor w/ an AActor* of the class you spawned
*			-This function *must* call ExecuteConstruction + PostActorConstruction
*/
