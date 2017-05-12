// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "DNATagContainer.h"
#include "Abilities/Tasks/AbilityTask_WaitDNATagBase.h"
#include "AbilityTask_WaitDNATag.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWaitDNATagDelegate);

UCLASS(MinimalAPI)
class UDNAAbilityTask_WaitDNATagAdded : public UDNAAbilityTask_WaitDNATag
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FWaitDNATagDelegate	Added;

	/**
	 * 	Wait until the specified DNA tag is Added. By default this will look at the owner of this ability. OptionalExternalTarget can be set to make this look at another actor's tags for changes. 
	 *  If the tag is already present when this task is started, it will immediately broadcast the Added event. It will keep listening as long as OnlyTriggerOnce = false.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UDNAAbilityTask_WaitDNATagAdded* WaitDNATagAdd(UDNAAbility* OwningAbility, FDNATag Tag, AActor* InOptionalExternalTarget=nullptr, bool OnlyTriggerOnce=false);

	virtual void Activate() override;

	virtual void DNATagCallback(const FDNATag Tag, int32 NewCount) override;
};

UCLASS(MinimalAPI)
class UDNAAbilityTask_WaitDNATagRemoved : public UDNAAbilityTask_WaitDNATag
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FWaitDNATagDelegate	Removed;

	/**
	 * 	Wait until the specified DNA tag is Removed. By default this will look at the owner of this ability. OptionalExternalTarget can be set to make this look at another actor's tags for changes. 
	 *  If the tag is not present when this task is started, it will immediately broadcast the Removed event. It will keep listening as long as OnlyTriggerOnce = false.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UDNAAbilityTask_WaitDNATagRemoved* WaitDNATagRemove(UDNAAbility* OwningAbility, FDNATag Tag, AActor* InOptionalExternalTarget=nullptr, bool OnlyTriggerOnce=false);

	virtual void Activate() override;

	virtual void DNATagCallback(const FDNATag Tag, int32 NewCount) override;
};
