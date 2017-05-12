// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "DNATagContainer.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_WaitDNATagBase.generated.h"

class UDNAAbilitySystemComponent;

UCLASS(MinimalAPI)
class UDNAAbilityTask_WaitDNATag : public UDNAAbilityTask
{
	GENERATED_UCLASS_BODY()

	virtual void Activate() override;

	UFUNCTION()
	virtual void DNATagCallback(const FDNATag Tag, int32 NewCount);
	
	void SetExternalTarget(AActor* Actor);

	FDNATag	Tag;

protected:

	UDNAAbilitySystemComponent* GetTargetASC();

	virtual void OnDestroy(bool AbilityIsEnding) override;
	
	bool RegisteredCallback;
	
	UPROPERTY()
	UDNAAbilitySystemComponent* OptionalExternalTarget;

	bool UseExternalTarget;	
	bool OnlyTriggerOnce;

	FDelegateHandle DelegateHandle;
};
