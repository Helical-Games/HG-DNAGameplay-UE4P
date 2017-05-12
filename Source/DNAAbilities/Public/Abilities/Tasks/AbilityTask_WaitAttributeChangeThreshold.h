// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "AttributeSet.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "Abilities/Tasks/AbilityTask_WaitAttributeChange.h"
#include "AbilityTask_WaitAttributeChangeThreshold.generated.h"

struct FDNAEffectModCallbackData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWaitAttributeChangeThresholdDelegate, bool, bMatchesComparison, float, CurrentValue);

/**
 *	Waits for an attribute to match a threshold
 */
UCLASS(MinimalAPI)
class UDNAAbilityTask_WaitAttributeChangeThreshold : public UDNAAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FWaitAttributeChangeThresholdDelegate OnChange;

	virtual void Activate() override;

	void OnAttributeChange(float NewValue, const FDNAEffectModCallbackData* Data);

	/** Wait on attribute change meeting a comparison threshold. */
	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UDNAAbilityTask_WaitAttributeChangeThreshold* WaitForAttributeChangeThreshold(UDNAAbility* OwningAbility, FDNAAttribute Attribute, TEnumAsByte<EWaitAttributeChangeComparison::Type> ComparisonType, float ComparisonValue, bool bTriggerOnce);

	FDNAAttribute Attribute;
	TEnumAsByte<EWaitAttributeChangeComparison::Type> ComparisonType;
	float ComparisonValue;
	bool bTriggerOnce;
	FDelegateHandle OnAttributeChangeDelegateHandle;

protected:

	bool bMatchedComparisonLastAttributeChange;

	virtual void OnDestroy(bool AbilityEnded) override;

	bool DoesValuePassComparison(float Value) const;
};
