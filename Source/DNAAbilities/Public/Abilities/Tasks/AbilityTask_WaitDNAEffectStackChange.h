// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "DNAEffectTypes.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_WaitDNAEffectStackChange.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FWaitDNAEffectStackChangeDelegate, FActiveDNAEffectHandle, Handle, int32, NewCount, int32, OldCount);

class AActor;
class UPrimitiveComponent;

/**
 *	Waits for the actor to activate another ability
 */
UCLASS(MinimalAPI)
class UDNAAbilityTask_WaitDNAEffectStackChange : public UDNAAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FWaitDNAEffectStackChangeDelegate	OnChange;

	UPROPERTY(BlueprintAssignable)
	FWaitDNAEffectStackChangeDelegate	InvalidHandle;

	virtual void Activate() override;

	UFUNCTION()
	void OnDNAEffectStackChange(FActiveDNAEffectHandle Handle, int32 NewCount, int32 OldCount);

	/** Wait until the specified DNA effect is removed. */
	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UDNAAbilityTask_WaitDNAEffectStackChange* WaitForDNAEffectStackChange(UDNAAbility* OwningAbility, FActiveDNAEffectHandle Handle);

	FActiveDNAEffectHandle Handle;

protected:

	virtual void OnDestroy(bool AbilityIsEnding) override;
	bool Registered;

	FDelegateHandle OnDNAEffectStackChangeDelegateHandle;
};
