// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_WaitCancel.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWaitCancelDelegate);

UCLASS(MinimalAPI)
class UDNAAbilityTask_WaitCancel : public UDNAAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FWaitCancelDelegate	OnCancel;
	
	UFUNCTION()
	void OnCancelCallback();

	UFUNCTION()
	void OnLocalCancelCallback();

	UFUNCTION(BlueprintCallable, meta=(HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true", DisplayName="Wait for Cancel Input"), Category="Ability|Tasks")
	static UDNAAbilityTask_WaitCancel* WaitCancel(UDNAAbility* OwningAbility);	

	virtual void Activate() override;

protected:

	virtual void OnDestroy(bool AbilityEnding) override;

	bool RegisteredCallbacks;
	
};
