// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_WaitVelocityChange.generated.h"

class UMovementComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWaitVelocityChangeDelegate);

UCLASS(MinimalAPI)
class UDNAAbilityTask_WaitVelocityChange: public UDNAAbilityTask
{
	GENERATED_UCLASS_BODY()

	/** Delegate called when velocity requirements are met */
	UPROPERTY(BlueprintAssignable)
	FWaitVelocityChangeDelegate OnVelocityChage;

	virtual void TickTask(float DeltaTime) override;

	/** Wait for the actor's movement component velocity to be of minimum magnitude when projected along given direction */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (DisplayName="WaitVelocityChange",HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UDNAAbilityTask_WaitVelocityChange* CreateWaitVelocityChange(UDNAAbility* OwningAbility, FVector Direction, float MinimumMagnitude);
		
	virtual void Activate() override;

protected:

	UPROPERTY()
	UMovementComponent*	CachedMovementComponent;

	float	MinimumMagnitude;
	FVector Direction;
};
