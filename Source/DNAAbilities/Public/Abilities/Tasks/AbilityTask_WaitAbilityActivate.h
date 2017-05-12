// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "DNATagContainer.h"
#include "DNAEffectTypes.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_WaitAbilityActivate.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWaitAbilityActivateDelegate, UDNAAbility*, ActivatedAbility);

class AActor;

/**
 *	Waits for the actor to activate another ability
 */
UCLASS(MinimalAPI)
class UDNAAbilityTask_WaitAbilityActivate : public UDNAAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FWaitAbilityActivateDelegate	OnActivate;

	virtual void Activate() override;

	UFUNCTION()
	void OnAbilityActivate(UDNAAbility *ActivatedAbility);

	/** Wait until a new ability (of the same or different type) is activated. Only input based abilities will be counted unless IncludeTriggeredAbilities is true. */
	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UDNAAbilityTask_WaitAbilityActivate* WaitForAbilityActivate(UDNAAbility* OwningAbility, FDNATag WithTag, FDNATag WithoutTag, bool IncludeTriggeredAbilities=false, bool TriggerOnce=true);

	/** Wait until a new ability (of the same or different type) is activated. Only input based abilities will be counted unless IncludeTriggeredAbilities is true. Uses a tag requirements structure to filter abilities. */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UDNAAbilityTask_WaitAbilityActivate* WaitForAbilityActivateWithTagRequirements(UDNAAbility* OwningAbility, FDNATagRequirements TagRequirements, bool IncludeTriggeredAbilities = false, bool TriggerOnce = true);

	FDNATag WithTag;
	FDNATag WithoutTag;
	bool IncludeTriggeredAbilities;
	bool TriggerOnce;
	FDNATagRequirements TagRequirements;

protected:

	virtual void OnDestroy(bool AbilityEnded) override;

	FDelegateHandle OnAbilityActivateDelegateHandle;
};
