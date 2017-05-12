// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "DNATagContainer.h"
#include "Abilities/DNAAbilityTypes.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_WaitDNAEvent.generated.h"

class UDNAAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWaitDNAEventDelegate, FDNAEventData, Payload);

UCLASS(MinimalAPI)
class UDNAAbilityTask_WaitDNAEvent : public UDNAAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FWaitDNAEventDelegate	EventReceived;

	/**
	 * 	Wait until the specified DNA tag event is triggered. By default this will look at the owner of this ability. OptionalExternalTarget can be set to make this look at another actor's tags for changes. 
	 *  It will keep listening as long as OnlyTriggerOnce = false.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UDNAAbilityTask_WaitDNAEvent* WaitDNAEvent(UDNAAbility* OwningAbility, FDNATag EventTag, AActor* OptionalExternalTarget=nullptr, bool OnlyTriggerOnce=false);

	void SetExternalTarget(AActor* Actor);

	UDNAAbilitySystemComponent* GetTargetASC();

	virtual void Activate() override;

	virtual void DNAEventCallback(const FDNAEventData* Payload);

	void OnDestroy(bool AbilityEnding) override;

	FDNATag Tag;

	UPROPERTY()
	UDNAAbilitySystemComponent* OptionalExternalTarget;

	bool UseExternalTarget;	
	bool OnlyTriggerOnce;

	FDelegateHandle MyHandle;
};
