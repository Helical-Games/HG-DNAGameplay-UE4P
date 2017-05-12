// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "AttributeSet.h"
#include "DNATagContainer.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_WaitAttributeChange.generated.h"

struct FDNAEffectModCallbackData;

UENUM()
namespace EWaitAttributeChangeComparison
{
	enum Type
	{
		None,
		GreaterThan,
		LessThan,
		GreaterThanOrEqualTo,
		LessThanOrEqualTo,
		NotEqualTo,
		ExactlyEqualTo,
		MAX UMETA(Hidden)
	};
}

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWaitAttributeChangeDelegate);

/**
 *	Waits for the actor to activate another ability
 */
UCLASS(MinimalAPI)
class UDNAAbilityTask_WaitAttributeChange : public UDNAAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FWaitAttributeChangeDelegate	OnChange;

	virtual void Activate() override;

	void OnAttributeChange(float NewValue, const FDNAEffectModCallbackData*);

	/** Wait until an attribute changes. */
	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UDNAAbilityTask_WaitAttributeChange* WaitForAttributeChange(UDNAAbility* OwningAbility, FDNAAttribute Attribute, FDNATag WithSrcTag, FDNATag WithoutSrcTag, bool TriggerOnce=true);

	/** Wait until an attribute changes to pass a given test. */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UDNAAbilityTask_WaitAttributeChange* WaitForAttributeChangeWithComparison(UDNAAbility* OwningAbility, FDNAAttribute InAttribute, FDNATag InWithTag, FDNATag InWithoutTag, TEnumAsByte<EWaitAttributeChangeComparison::Type> InComparisonType, float InComparisonValue, bool TriggerOnce=true);

	FDNATag WithTag;
	FDNATag WithoutTag;
	FDNAAttribute	Attribute;
	TEnumAsByte<EWaitAttributeChangeComparison::Type> ComparisonType;
	float ComparisonValue;
	bool bTriggerOnce;
	FDelegateHandle OnAttributeChangeDelegateHandle;

protected:

	virtual void OnDestroy(bool AbilityEnded) override;
};
