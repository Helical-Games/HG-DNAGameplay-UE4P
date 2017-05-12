// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_WaitMovementModeChange.generated.h"

class ACharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMovementModeChangedDelegate, EMovementMode, NewMovementMode);

class ACharacter;

UCLASS(MinimalAPI)
class UDNAAbilityTask_WaitMovementModeChange : public UDNAAbilityTask
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(BlueprintAssignable)
	FMovementModeChangedDelegate	OnChange;

	UFUNCTION()
	void OnMovementModeChange(ACharacter * Character, EMovementMode PrevMovementMode, uint8 PreviousCustomMode);

	EMovementMode	RequiredMode;

	/** Wait until movement mode changes (E.g., landing) */
	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE", DisplayName="WaitMovementModeChange"))
	static UDNAAbilityTask_WaitMovementModeChange* CreateWaitMovementModeChange(UDNAAbility* OwningAbility, EMovementMode NewMode);

	virtual void Activate() override;
private:

	virtual void OnDestroy(bool AbilityEnded) override;

	TWeakObjectPtr<ACharacter>	MyCharacter;
};
