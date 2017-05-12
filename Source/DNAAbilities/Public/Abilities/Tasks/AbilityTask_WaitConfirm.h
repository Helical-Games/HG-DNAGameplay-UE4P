// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_WaitConfirm.generated.h"

UCLASS(MinimalAPI)
class UDNAAbilityTask_WaitConfirm : public UDNAAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FGenericDNATaskDelegate	OnConfirm;

	UFUNCTION()
	void OnConfirmCallback(UDNAAbility* InAbility);

	virtual void Activate() override;

	/** Wait until the server confirms the use of this ability. This is used to gate predictive portions of the ability */
	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UDNAAbilityTask_WaitConfirm* WaitConfirm(UDNAAbility* OwningAbility);

protected:

	virtual void OnDestroy(bool AbilityEnded) override;

	bool RegisteredCallback;

	FDelegateHandle OnConfirmCallbackDelegateHandle;
};
