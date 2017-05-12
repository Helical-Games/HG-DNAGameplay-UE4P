// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "DNAEffectTypes.h"
#include "Abilities/DNAAbilityTargetDataFilter.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_WaitDNAEffectApplied.generated.h"

class UDNAAbilitySystemComponent;

UCLASS(MinimalAPI)
class UDNAAbilityTask_WaitDNAEffectApplied : public UDNAAbilityTask
{
	GENERATED_UCLASS_BODY()

	UFUNCTION()
	void OnApplyDNAEffectCallback(UDNAAbilitySystemComponent* Target, const FDNAEffectSpec& SpecApplied, FActiveDNAEffectHandle ActiveHandle);

	virtual void Activate() override;

	FDNATargetDataFilterHandle Filter;
	FDNATagRequirements SourceTagRequirements;
	FDNATagRequirements TargetTagRequirements;
	bool TriggerOnce;
	bool ListenForPeriodicEffects;

	void SetExternalActor(AActor* InActor);

protected:

	UDNAAbilitySystemComponent* GetASC();

	virtual void BroadcastDelegate(AActor* Avatar, FDNAEffectSpecHandle SpecHandle, FActiveDNAEffectHandle ActiveHandle) { }
	virtual void RegisterDelegate() { }
	virtual void RemoveDelegate() { }

	virtual void OnDestroy(bool AbilityEnded) override;

	bool RegisteredCallback;
	bool UseExternalOwner;

	UPROPERTY()
	UDNAAbilitySystemComponent* ExternalOwner;

	// If we are in the process of broadcasting and should not accept additional GE callbacks
	bool Locked;
};