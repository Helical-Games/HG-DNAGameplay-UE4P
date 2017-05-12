// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "DNAEffectTypes.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_WaitDNAEffectRemoved.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWaitDNAEffectRemovedDelegate);

class AActor;
class UPrimitiveComponent;

/**
 *	Waits for the actor to activate another ability
 */
UCLASS(MinimalAPI)
class UDNAAbilityTask_WaitDNAEffectRemoved : public UDNAAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FWaitDNAEffectRemovedDelegate	OnRemoved;

	UPROPERTY(BlueprintAssignable)
	FWaitDNAEffectRemovedDelegate	InvalidHandle;

	virtual void Activate() override;

	UFUNCTION()
	void OnDNAEffectRemoved();

	/** Wait until the specified DNA effect is removed. */
	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UDNAAbilityTask_WaitDNAEffectRemoved* WaitForDNAEffectRemoved(UDNAAbility* OwningAbility, FActiveDNAEffectHandle Handle);

	FActiveDNAEffectHandle Handle;

protected:

	virtual void OnDestroy(bool AbilityIsEnding) override;
	bool Registered;

	FDelegateHandle OnDNAEffectRemovedDelegateHandle;
};
