// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "DNAEffectTypes.h"
#include "Abilities/DNAAbilityTargetDataFilter.h"
#include "Abilities/Tasks/AbilityTask_WaitDNAEffectApplied.h"
#include "AbilityTask_WaitDNAEffectApplied_Self.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDNAEffectAppliedSelfDelegate, AActor*, Source, FDNAEffectSpecHandle, SpecHandle,  FActiveDNAEffectHandle, ActiveHandle );

UCLASS(MinimalAPI)
class UDNAAbilityTask_WaitDNAEffectApplied_Self : public UDNAAbilityTask_WaitDNAEffectApplied
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FDNAEffectAppliedSelfDelegate OnApplied;

	/**	 
	 * Wait until the owner *receives* a DNAEffect from a given source (the source may be the owner too!). If TriggerOnce is true, this task will only return one time. Otherwise it will return everytime a GE is applied that meets the requirements over the life of the ability
	 * Optional External Owner can be used to run this task on someone else (not the owner of the ability). By default you can leave this empty.
	 */	 	 
	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UDNAAbilityTask_WaitDNAEffectApplied_Self* WaitDNAEffectAppliedToSelf(UDNAAbility* OwningAbility, const FDNATargetDataFilterHandle SourceFilter, FDNATagRequirements SourceTagRequirements, FDNATagRequirements TargetTagRequirements, bool TriggerOnce=false, AActor* OptionalExternalOwner=nullptr, bool ListenForPeriodicEffect=false );
	
protected:

	virtual void BroadcastDelegate(AActor* Avatar, FDNAEffectSpecHandle SpecHandle, FActiveDNAEffectHandle ActiveHandle) override;
	virtual void RegisterDelegate() override;
	virtual void RemoveDelegate() override;

private:
	FDelegateHandle OnApplyDNAEffectCallbackDelegateHandle;
	FDelegateHandle OnPeriodicDNAEffectExecuteCallbackDelegateHandle;
};
