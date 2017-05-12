// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_WaitConfirmCancel.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWaitConfirmCancelDelegate);

// Fixme: this name is conflicting with DNAAbilityTask_WaitConfirm
// UDNAAbilityTask_WaitConfirmCancel = Wait for Targeting confirm/cancel
// UDNAAbilityTask_WaitConfirm = Wait for server to confirm ability activation

UCLASS()
class DNAABILITIES_API UDNAAbilityTask_WaitConfirmCancel : public UDNAAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FWaitConfirmCancelDelegate	OnConfirm;

	UPROPERTY(BlueprintAssignable)
	FWaitConfirmCancelDelegate	OnCancel;
	
	UFUNCTION()
	void OnConfirmCallback();

	UFUNCTION()
	void OnCancelCallback();

	UFUNCTION()
	void OnLocalConfirmCallback();

	UFUNCTION()
	void OnLocalCancelCallback();

	UFUNCTION(BlueprintCallable, meta=(HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true", DisplayName="Wait for Confirm Input"), Category="Ability|Tasks")
	static UDNAAbilityTask_WaitConfirmCancel* WaitConfirmCancel(UDNAAbility* OwningAbility);	

	virtual void Activate() override;

protected:

	virtual void OnDestroy(bool AbilityEnding) override;

	bool RegisteredCallbacks;
	
};
