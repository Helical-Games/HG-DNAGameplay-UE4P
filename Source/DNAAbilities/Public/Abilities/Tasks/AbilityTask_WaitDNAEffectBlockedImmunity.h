// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "DNAEffectTypes.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_WaitDNAEffectBlockedImmunity.generated.h"

class UDNAAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDNAEffectBlockedDelegate, FDNAEffectSpecHandle, BlockedSpec, FActiveDNAEffectHandle, ImmunityDNAEffectHandle);

UCLASS(MinimalAPI)
class UDNAAbilityTask_WaitDNAEffectBlockedImmunity : public UDNAAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FDNAEffectBlockedDelegate Blocked;

	/** Listens for GE immunity. By default this means "this hero blocked a GE due to immunity". Setting OptionalExternalTarget will listen for a GE being blocked on an external target. Note this only works on the server. */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UDNAAbilityTask_WaitDNAEffectBlockedImmunity* WaitDNAEffectBlockedByImmunity(UDNAAbility* OwningAbility, FDNATagRequirements SourceTagRequirements, FDNATagRequirements TargetTagRequirements, AActor* OptionalExternalTarget=nullptr, bool OnlyTriggerOnce=false);

	virtual void Activate() override;

	virtual void ImmunityCallback(const FDNAEffectSpec& BlockedSpec, const FActiveDNAEffect* ImmunityGE);
	
	FDNATagRequirements SourceTagRequirements;
	FDNATagRequirements TargetTagRequirements;

	bool TriggerOnce;
	bool ListenForPeriodicEffects;

	void SetExternalActor(AActor* InActor);

protected:

	UDNAAbilitySystemComponent* GetASC();
		
	void RegisterDelegate();
	void RemoveDelegate();

	virtual void OnDestroy(bool AbilityEnded) override;

	bool RegisteredCallback;
	bool UseExternalOwner;

	UPROPERTY()
	UDNAAbilitySystemComponent* ExternalOwner;

	FDelegateHandle DelegateHandle;
};
