// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_WaitInputRelease.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInputReleaseDelegate, float, TimeHeld);

/**
 *	Waits until the input is released from activating an ability. Clients will replicate a 'release input' event to the server, but not the exact time it was held locally.
 *	We expect server to execute this task in parallel and keep its own time.
 */
UCLASS()
class DNAABILITIES_API UDNAAbilityTask_WaitInputRelease : public UDNAAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FInputReleaseDelegate	OnRelease;

	UFUNCTION()
	void OnReleaseCallback();

	virtual void Activate() override;

	/** Wait until the user releases the input button for this ability's activation. Returns time from hitting this node, till release. Will return 0 if input was already released. */
	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UDNAAbilityTask_WaitInputRelease* WaitInputRelease(UDNAAbility* OwningAbility, bool bTestAlreadyReleased=false);

protected:

	virtual void OnDestroy(bool AbilityEnded) override;

	float StartTime;
	bool bTestInitialState;
	FDelegateHandle DelegateHandle;
};
