// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/Tasks/AbilityTask_WaitAttributeChangeRatioThreshold.h"
#include "TimerManager.h"
#include "AbilitySystemComponent.h"

UDNAAbilityTask_WaitAttributeChangeRatioThreshold::UDNAAbilityTask_WaitAttributeChangeRatioThreshold(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTriggerOnce = false;
	bMatchedComparisonLastAttributeChange = false;
	LastAttributeNumeratorValue = 1.f;
	LastAttributeDenominatorValue = 1.f;
}

UDNAAbilityTask_WaitAttributeChangeRatioThreshold* UDNAAbilityTask_WaitAttributeChangeRatioThreshold::WaitForAttributeChangeRatioThreshold(UDNAAbility* OwningAbility, FDNAAttribute AttributeNumerator, FDNAAttribute AttributeDenominator, TEnumAsByte<EWaitAttributeChangeComparison::Type> ComparisonType, float ComparisonValue, bool bTriggerOnce)
{
	auto MyTask = NewDNAAbilityTask<UDNAAbilityTask_WaitAttributeChangeRatioThreshold>(OwningAbility);
	MyTask->AttributeNumerator = AttributeNumerator;
	MyTask->AttributeDenominator = AttributeDenominator;
	MyTask->ComparisonType = ComparisonType;
	MyTask->ComparisonValue = ComparisonValue;
	MyTask->bTriggerOnce = bTriggerOnce;

	return MyTask;
}

void UDNAAbilityTask_WaitAttributeChangeRatioThreshold::Activate()
{
	if (DNAAbilitySystemComponent)
	{
		LastAttributeNumeratorValue = DNAAbilitySystemComponent->GetNumericAttribute(AttributeNumerator);
		LastAttributeDenominatorValue = DNAAbilitySystemComponent->GetNumericAttribute(AttributeDenominator);
		bMatchedComparisonLastAttributeChange = DoesValuePassComparison(LastAttributeNumeratorValue, LastAttributeDenominatorValue);

		// Broadcast OnChange immediately with current value
		OnChange.Broadcast(bMatchedComparisonLastAttributeChange, LastAttributeDenominatorValue != 0.f ? LastAttributeNumeratorValue/LastAttributeDenominatorValue : 0.f);

		OnNumeratorAttributeChangeDelegateHandle = DNAAbilitySystemComponent->RegisterDNAAttributeEvent(AttributeNumerator).AddUObject(this, &UDNAAbilityTask_WaitAttributeChangeRatioThreshold::OnNumeratorAttributeChange);
		OnDenominatorAttributeChangeDelegateHandle = DNAAbilitySystemComponent->RegisterDNAAttributeEvent(AttributeDenominator).AddUObject(this, &UDNAAbilityTask_WaitAttributeChangeRatioThreshold::OnDenominatorAttributeChange);
	}
}

void UDNAAbilityTask_WaitAttributeChangeRatioThreshold::OnAttributeChange()
{
	UWorld* World = GetWorld();
	if (World && !CheckAttributeTimer.IsValid())
	{
		// Trigger OnRatioChange check at the end of this frame/next so that any individual attribute change
		// has time for the other attribute to update (in case they're linked)
		World->GetTimerManager().SetTimer(CheckAttributeTimer, this, &UDNAAbilityTask_WaitAttributeChangeRatioThreshold::OnRatioChange, 0.001f, false);
	}
}

void UDNAAbilityTask_WaitAttributeChangeRatioThreshold::OnRatioChange()
{
	CheckAttributeTimer.Invalidate();

	bool bPassedComparison = DoesValuePassComparison(LastAttributeNumeratorValue, LastAttributeDenominatorValue);
	if (bPassedComparison != bMatchedComparisonLastAttributeChange)
	{
		bMatchedComparisonLastAttributeChange = bPassedComparison;
		OnChange.Broadcast(bMatchedComparisonLastAttributeChange, LastAttributeDenominatorValue != 0.f ? LastAttributeNumeratorValue/LastAttributeDenominatorValue : 0.f);
		if (bTriggerOnce)
		{
			EndTask();
		}
	}
}

void UDNAAbilityTask_WaitAttributeChangeRatioThreshold::OnNumeratorAttributeChange(float NewValue, const FDNAEffectModCallbackData* Data)
{
	LastAttributeNumeratorValue = NewValue;
	OnAttributeChange();
}

void UDNAAbilityTask_WaitAttributeChangeRatioThreshold::OnDenominatorAttributeChange(float NewValue, const FDNAEffectModCallbackData* Data)
{
	LastAttributeDenominatorValue = NewValue;
	OnAttributeChange();
}

bool UDNAAbilityTask_WaitAttributeChangeRatioThreshold::DoesValuePassComparison(float ValueNumerator, float ValueDenominator) const
{
	if (ValueDenominator == 0.f)
	{
		return bMatchedComparisonLastAttributeChange;
	}

	const float CurrentRatio = ValueNumerator / ValueDenominator;
	bool bPassedComparison = true;
	switch (ComparisonType)
	{
	case EWaitAttributeChangeComparison::ExactlyEqualTo:
		bPassedComparison = (CurrentRatio == ComparisonValue);
		break;		
	case EWaitAttributeChangeComparison::GreaterThan:
		bPassedComparison = (CurrentRatio > ComparisonValue);
		break;
	case EWaitAttributeChangeComparison::GreaterThanOrEqualTo:
		bPassedComparison = (CurrentRatio >= ComparisonValue);
		break;
	case EWaitAttributeChangeComparison::LessThan:
		bPassedComparison = (CurrentRatio < ComparisonValue);
		break;
	case EWaitAttributeChangeComparison::LessThanOrEqualTo:
		bPassedComparison = (CurrentRatio <= ComparisonValue);
		break;
	case EWaitAttributeChangeComparison::NotEqualTo:
		bPassedComparison = (CurrentRatio != ComparisonValue);
		break;
	default:
		break;
	}
	return bPassedComparison;
}

void UDNAAbilityTask_WaitAttributeChangeRatioThreshold::OnDestroy(bool AbilityEnded)
{
	if (DNAAbilitySystemComponent)
	{
		DNAAbilitySystemComponent->RegisterDNAAttributeEvent(AttributeNumerator).Remove(OnNumeratorAttributeChangeDelegateHandle);
		DNAAbilitySystemComponent->RegisterDNAAttributeEvent(AttributeDenominator).Remove(OnDenominatorAttributeChangeDelegateHandle);
	}

	Super::OnDestroy(AbilityEnded);
}
