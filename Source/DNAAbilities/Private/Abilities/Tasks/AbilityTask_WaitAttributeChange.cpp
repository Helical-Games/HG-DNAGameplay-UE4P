// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/Tasks/AbilityTask_WaitAttributeChange.h"
#include "AbilitySystemComponent.h"
#include "DNAEffectExtension.h"

UDNAAbilityTask_WaitAttributeChange::UDNAAbilityTask_WaitAttributeChange(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTriggerOnce = false;
}

UDNAAbilityTask_WaitAttributeChange* UDNAAbilityTask_WaitAttributeChange::WaitForAttributeChange(UDNAAbility* OwningAbility, FDNAAttribute InAttribute, FDNATag InWithTag, FDNATag InWithoutTag, bool TriggerOnce)
{
	auto MyObj = NewDNAAbilityTask<UDNAAbilityTask_WaitAttributeChange>(OwningAbility);
	MyObj->WithTag = InWithTag;
	MyObj->WithoutTag = InWithoutTag;
	MyObj->Attribute = InAttribute;
	MyObj->ComparisonType = EWaitAttributeChangeComparison::None;
	MyObj->bTriggerOnce = TriggerOnce;

	return MyObj;
}

UDNAAbilityTask_WaitAttributeChange* UDNAAbilityTask_WaitAttributeChange::WaitForAttributeChangeWithComparison(UDNAAbility* OwningAbility, FDNAAttribute InAttribute, FDNATag InWithTag, FDNATag InWithoutTag, TEnumAsByte<EWaitAttributeChangeComparison::Type> InComparisonType, float InComparisonValue, bool TriggerOnce)
{
	auto MyObj = NewDNAAbilityTask<UDNAAbilityTask_WaitAttributeChange>(OwningAbility);
	MyObj->WithTag = InWithTag;
	MyObj->WithoutTag = InWithoutTag;
	MyObj->Attribute = InAttribute;
	MyObj->ComparisonType = InComparisonType;
	MyObj->ComparisonValue = InComparisonValue;
	MyObj->bTriggerOnce = TriggerOnce;

	return MyObj;
}

void UDNAAbilityTask_WaitAttributeChange::Activate()
{
	if (DNAAbilitySystemComponent)
	{
		OnAttributeChangeDelegateHandle = DNAAbilitySystemComponent->RegisterDNAAttributeEvent(Attribute).AddUObject(this, &UDNAAbilityTask_WaitAttributeChange::OnAttributeChange);
	}
}

void UDNAAbilityTask_WaitAttributeChange::OnAttributeChange(float NewValue, const FDNAEffectModCallbackData* Data)
{
	if (Data == nullptr)
	{
		// There may be no execution data associated with this change, for example a GE being removed. 
		// In this case, we auto fail any WithTag requirement and auto pass any WithoutTag requirement
		if (WithTag.IsValid())
		{
			return;
		}
	}
	else
	{
		if ((WithTag.IsValid() && !Data->EffectSpec.CapturedSourceTags.GetAggregatedTags()->HasTag(WithTag)) ||
			(WithoutTag.IsValid() && Data->EffectSpec.CapturedSourceTags.GetAggregatedTags()->HasTag(WithoutTag)))
		{
			// Failed tag check
			return;
		}
	}	

	bool PassedComparison = true;
	switch (ComparisonType)
	{
	case EWaitAttributeChangeComparison::ExactlyEqualTo:
		PassedComparison = (NewValue == ComparisonValue);
		break;		
	case EWaitAttributeChangeComparison::GreaterThan:
		PassedComparison = (NewValue > ComparisonValue);
		break;
	case EWaitAttributeChangeComparison::GreaterThanOrEqualTo:
		PassedComparison = (NewValue >= ComparisonValue);
		break;
	case EWaitAttributeChangeComparison::LessThan:
		PassedComparison = (NewValue < ComparisonValue);
		break;
	case EWaitAttributeChangeComparison::LessThanOrEqualTo:
		PassedComparison = (NewValue <= ComparisonValue);
		break;
	case EWaitAttributeChangeComparison::NotEqualTo:
		PassedComparison = (NewValue != ComparisonValue);
		break;
	default:
		break;
	}
	if (PassedComparison)
	{
		OnChange.Broadcast();
		if (bTriggerOnce)
		{
			EndTask();
		}
	}
}

void UDNAAbilityTask_WaitAttributeChange::OnDestroy(bool AbilityEnded)
{
	if (DNAAbilitySystemComponent)
	{
		DNAAbilitySystemComponent->RegisterDNAAttributeEvent(Attribute).Remove(OnAttributeChangeDelegateHandle);
	}

	Super::OnDestroy(AbilityEnded);
}
