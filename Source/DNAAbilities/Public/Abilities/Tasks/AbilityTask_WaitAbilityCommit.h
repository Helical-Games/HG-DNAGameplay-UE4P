// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "DNATagContainer.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_WaitAbilityCommit.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWaitAbilityCommitDelegate, UDNAAbility*, ActivatedAbility);

/**
 *	Waits for the actor to activate another ability
 */
UCLASS(MinimalAPI)
class UDNAAbilityTask_WaitAbilityCommit : public UDNAAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FWaitAbilityCommitDelegate	OnCommit;

	virtual void Activate() override;

	UFUNCTION()
	void OnAbilityCommit(UDNAAbility *ActivatedAbility);

	/** Wait until a new ability (of the same or different type) is commited. Used to gracefully interrupt abilities in specific ways. */
	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE", DisplayName = "Wait For New Ability Commit"))
	static UDNAAbilityTask_WaitAbilityCommit* WaitForAbilityCommit(UDNAAbility* OwningAbility, FDNATag WithTag, FDNATag WithoutTage, bool TriggerOnce=true);

	FDNATag WithTag;
	FDNATag WithoutTag;
	bool TriggerOnce;

protected:

	virtual void OnDestroy(bool AbilityEnded) override;

	FDelegateHandle OnAbilityCommitDelegateHandle;
};
