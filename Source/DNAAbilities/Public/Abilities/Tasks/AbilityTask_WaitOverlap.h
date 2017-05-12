// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_WaitOverlap.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWaitOverlapDelegate, const FDNAAbilityTargetDataHandle&, TargetData);

class AActor;
class UPrimitiveComponent;

/**
 *	Fixme: this is still incomplete and probablyh not what most games want for melee systems.
 *		-Only actually activates on Blocking hits
 *		-Uses first PrimitiveComponent instead of being able to specify arbitrary component.
 */
UCLASS(MinimalAPI)
class UDNAAbilityTask_WaitOverlap : public UDNAAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FWaitOverlapDelegate	OnOverlap;

	UFUNCTION()
	void OnHitCallback(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	virtual void Activate() override;

	/** Wait until an overlap occurs. This will need to be better fleshed out so we can specify game specific collision requirements */
	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UDNAAbilityTask_WaitOverlap* WaitForOverlap(UDNAAbility* OwningAbility);

private:

	virtual void OnDestroy(bool AbilityEnded) override;

	UPrimitiveComponent* GetComponent();
	
};