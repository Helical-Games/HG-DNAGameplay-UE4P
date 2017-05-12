// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/Tasks/AbilityTask_WaitAttributeChangeThreshold.h"
#include "AbilitySystemComponent.h"

UDNAAbilityTask_WaitAttributeChangeThreshold::UDNAAbilityTask_WaitAttributeChangeThreshold(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTriggerOnce = false;
	bMatchedComparisonLastAttributeChange = false;
}

UDNAAbilityTask_WaitAttributeChangeThreshold* UDNAAbilityTask_WaitAttributeChangeThreshold::WaitForAttributeChangeThreshold(UDNAAbility* OwningAbility, FDNAAttribute Attribute, TEnumAsByte<EWaitAttributeChangeComparison::Type> ComparisonType, float ComparisonValue, bool bTriggerOnce)
{
	auto MyTask = NewDNAAbilityTask<UDNAAbilityTask_WaitAttributeChangeThreshold>(OwningAbility);
	MyTask->Attribute = Attribute;
	MyTask->ComparisonType = ComparisonType;
	MyTask->ComparisonValue = ComparisonValue;
	MyTask->bTriggerOnce = bTriggerOnce;

	return MyTask;
}

void UDNAAbilityTask_WaitAttributeChangeThreshold::Activate()
{
	if (DNAAbilitySystemComponent)
	{
		const float CurrentValue = DNAAbilitySystemComponent->GetNumericAttribute(Attribute);
		bMatchedComparisonLastAttributeChange = DoesValuePassComparison(CurrentValue);

		// Broadcast OnChange immediately with current value
		OnChange.Broadcast(bMatchedComparisonLastAttributeChange, CurrentValue);

		OnAttributeChangeDelegateHandle = DNAAbilitySystemComponent->RegisterDNAAttributeEvent(Attribute).AddUObject(this, &UDNAAbilityTask_WaitAttributeChangeThreshold::OnAttributeChange);
	}
}

void UDNAAbilityTask_WaitAttributeChangeThreshold::OnAttributeChange(float NewValue, const FDNAEffectModCallbackData* Data)
{
	bool bPassedComparison = DoesValuePassComparison(NewValue);
	if (bPassedComparison != bMatchedComparisonLastAttributeChange)
	{
		bMatchedComparisonLastAttributeChange = bPassedComparison;
		OnChange.Broadcast(bPassedComparison, NewValue);
		if (bTriggerOnce)
		{
			EndTask();
		}
	}
}

bool UDNAAbilityTask_WaitAttributeChangeThreshold::DoesValuePassComparison(float Value) const
{
	bool bPassedComparison = true;
	switch (ComparisonType)
	{
	case EWaitAttributeChangeComparison::ExactlyEqualTo:
		bPassedComparison = (Value == ComparisonValue);
		break;		
	case EWaitAttributeChangeComparison::GreaterThan:
		bPassedComparison = (Value > ComparisonValue);
		break;
	case EWaitAttributeChangeComparison::GreaterThanOrEqualTo:
		bPassedComparison = (Value >= ComparisonValue);
		break;
	case EWaitAttributeChangeComparison::LessThan:
		bPassedComparison = (Value < ComparisonValue);
		break;
	case EWaitAttributeChangeComparison::LessThanOrEqualTo:
		bPassedComparison = (Value <= ComparisonValue);
		break;
	case EWaitAttributeChangeComparison::NotEqualTo:
		bPassedComparison = (Value != ComparisonValue);
		break;
	default:
		break;
	}
	return bPassedComparison;
}

void UDNAAbilityTask_WaitAttributeChangeThreshold::OnDestroy(bool AbilityEnded)
{
	if (DNAAbilitySystemComponent)
	{
		DNAAbilitySystemComponent->RegisterDNAAttributeEvent(Attribute).Remove(OnAttributeChangeDelegateHandle);
	}

	Super::OnDestroy(AbilityEnded);
}
