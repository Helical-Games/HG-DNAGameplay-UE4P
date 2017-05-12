// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "DNATagContainer.h"
#include "Abilities/DNAAbilityTargetActor.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_WaitTargetData.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWaitTargetDataDelegate, const FDNAAbilityTargetDataHandle&, Data);

/** Wait for targeting actor (spawned from parameter) to provide data. Can be set not to end upon outputting data. Can be ended by task name. */
UCLASS(notplaceable)
class DNAABILITIES_API UDNAAbilityTask_WaitTargetData: public UDNAAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FWaitTargetDataDelegate	ValidData;

	UPROPERTY(BlueprintAssignable)
	FWaitTargetDataDelegate	Cancelled;

	UFUNCTION()
	void OnTargetDataReplicatedCallback(const FDNAAbilityTargetDataHandle& Data, FDNATag ActivationTag);

	UFUNCTION()
	void OnTargetDataReplicatedCancelledCallback();

	UFUNCTION()
	void OnTargetDataReadyCallback(const FDNAAbilityTargetDataHandle& Data);

	UFUNCTION()
	void OnTargetDataCancelledCallback(const FDNAAbilityTargetDataHandle& Data);

	/** Spawns target actor and waits for it to return valid data or to be canceled. */
	UFUNCTION(BlueprintCallable, meta=(HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true", HideSpawnParms="Instigator"), Category="Ability|Tasks")
	static UDNAAbilityTask_WaitTargetData* WaitTargetData(UDNAAbility* OwningAbility, FName TaskInstanceName, TEnumAsByte<EDNATargetingConfirmation::Type> ConfirmationType, TSubclassOf<ADNAAbilityTargetActor> Class);

	/** Uses specified target actor and waits for it to return valid data or to be canceled. */
	UFUNCTION(BlueprintCallable, meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true", HideSpawnParms = "Instigator"), Category = "Ability|Tasks")
	static UDNAAbilityTask_WaitTargetData* WaitTargetDataUsingActor(UDNAAbility* OwningAbility, FName TaskInstanceName, TEnumAsByte<EDNATargetingConfirmation::Type> ConfirmationType, ADNAAbilityTargetActor* TargetActor);

	virtual void Activate() override;

	UFUNCTION(BlueprintCallable, meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true"), Category = "Abilities")
	bool BeginSpawningActor(UDNAAbility* OwningAbility, TSubclassOf<ADNAAbilityTargetActor> Class, ADNAAbilityTargetActor*& SpawnedActor);

	UFUNCTION(BlueprintCallable, meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true"), Category = "Abilities")
	void FinishSpawningActor(UDNAAbility* OwningAbility, ADNAAbilityTargetActor* SpawnedActor);

	/** Called when the ability is asked to confirm from an outside node. What this means depends on the individual task. By default, this does nothing other than ending if bEndTask is true. */
	virtual void ExternalConfirm(bool bEndTask) override;

	/** Called when the ability is asked to cancel from an outside node. What this means depends on the individual task. By default, this does nothing other than ending the task. */
	virtual void ExternalCancel() override;

protected:

	bool ShouldSpawnTargetActor() const;
	void InitializeTargetActor(ADNAAbilityTargetActor* SpawnedActor) const;
	void FinalizeTargetActor(ADNAAbilityTargetActor* SpawnedActor) const;

	void RegisterTargetDataCallbacks();

	virtual void OnDestroy(bool AbilityEnded) override;

	bool ShouldReplicateDataToServer() const;

protected:

	TSubclassOf<ADNAAbilityTargetActor> TargetClass;

	/** The TargetActor that we spawned */
	UPROPERTY()
	ADNAAbilityTargetActor* TargetActor;

	TEnumAsByte<EDNATargetingConfirmation::Type> ConfirmationType;

	FDelegateHandle OnTargetDataReplicatedCallbackDelegateHandle;
};


/**
*	Requirements for using Begin/Finish SpawningActor functionality:
*		-Have a parameters named 'Class' in your Proxy factor function (E.g., WaitTargetdata)
*		-Have a function named BeginSpawningActor w/ the same Class parameter
*			-This function should spawn the actor with SpawnActorDeferred and return true/false if it spawned something.
*		-Have a function named FinishSpawningActor w/ an AActor* of the class you spawned
*			-This function *must* call ExecuteConstruction + PostActorConstruction
*/
