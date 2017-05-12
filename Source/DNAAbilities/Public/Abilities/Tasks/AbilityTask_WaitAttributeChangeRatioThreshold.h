// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "AttributeSet.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "Abilities/Tasks/AbilityTask_WaitAttributeChange.h"
#include "AbilityTask_WaitAttributeChangeRatioThreshold.generated.h"

struct FDNAEffectModCallbackData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWaitAttributeChangeRatioThresholdDelegate, bool, bMatchesComparison, float, CurrentRatio);

/**
 *	Waits for the ratio between two attributes to match a threshold
 */
UCLASS(MinimalAPI)
class UDNAAbilityTask_WaitAttributeChangeRatioThreshold : public UDNAAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FWaitAttributeChangeRatioThresholdDelegate OnChange;

	virtual void Activate() override;

	void OnNumeratorAttributeChange(float NewValue, const FDNAEffectModCallbackData* Data);
	void OnDenominatorAttributeChange(float NewValue, const FDNAEffectModCallbackData* Data);

	/** Wait on attribute ratio change meeting a comparison threshold. */
	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UDNAAbilityTask_WaitAttributeChangeRatioThreshold* WaitForAttributeChangeRatioThreshold(UDNAAbility* OwningAbility, FDNAAttribute AttributeNumerator, FDNAAttribute AttributeDenominator, TEnumAsByte<EWaitAttributeChangeComparison::Type> ComparisonType, float ComparisonValue, bool bTriggerOnce);

	FDNAAttribute AttributeNumerator;
	FDNAAttribute AttributeDenominator;
	TEnumAsByte<EWaitAttributeChangeComparison::Type> ComparisonType;
	float ComparisonValue;
	bool bTriggerOnce;
	FDelegateHandle OnNumeratorAttributeChangeDelegateHandle;
	FDelegateHandle OnDenominatorAttributeChangeDelegateHandle;

protected:

	float LastAttributeNumeratorValue;
	float LastAttributeDenominatorValue;
	bool bMatchedComparisonLastAttributeChange;

	/** Timer used when either numerator or denominator attribute is changed to delay checking of ratio to avoid false positives (MaxHealth changed before Health updates accordingly) */
	FTimerHandle CheckAttributeTimer;

	void OnAttributeChange();
	void OnRatioChange();

	virtual void OnDestroy(bool AbilityEnded) override;

	bool DoesValuePassComparison(float ValueNumerator, float ValueDenominator) const;
};
