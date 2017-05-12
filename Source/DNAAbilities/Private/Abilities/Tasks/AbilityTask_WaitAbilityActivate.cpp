// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/Tasks/AbilityTask_WaitAbilityActivate.h"
#include "AbilitySystemComponent.h"

UDNAAbilityTask_WaitAbilityActivate::UDNAAbilityTask_WaitAbilityActivate(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	IncludeTriggeredAbilities = false;
}

UDNAAbilityTask_WaitAbilityActivate* UDNAAbilityTask_WaitAbilityActivate::WaitForAbilityActivate(UDNAAbility* OwningAbility, FDNATag InWithTag, FDNATag InWithoutTag, bool InIncludeTriggeredAbilities, bool InTriggerOnce)
{
	auto MyObj = NewDNAAbilityTask<UDNAAbilityTask_WaitAbilityActivate>(OwningAbility);
	MyObj->WithTag = InWithTag;
	MyObj->WithoutTag = InWithoutTag;
	MyObj->IncludeTriggeredAbilities = InIncludeTriggeredAbilities;
	MyObj->TriggerOnce = InTriggerOnce;
	return MyObj;
}

UDNAAbilityTask_WaitAbilityActivate* UDNAAbilityTask_WaitAbilityActivate::WaitForAbilityActivateWithTagRequirements(UDNAAbility* OwningAbility, FDNATagRequirements TagRequirements, bool InIncludeTriggeredAbilities, bool InTriggerOnce)
{
	auto MyObj = NewDNAAbilityTask<UDNAAbilityTask_WaitAbilityActivate>(OwningAbility);
	MyObj->TagRequirements = TagRequirements;
	MyObj->IncludeTriggeredAbilities = InIncludeTriggeredAbilities;
	MyObj->TriggerOnce = InTriggerOnce;
	return MyObj;
}

void UDNAAbilityTask_WaitAbilityActivate::Activate()
{
	if (DNAAbilitySystemComponent)
	{
		OnAbilityActivateDelegateHandle = DNAAbilitySystemComponent->AbilityActivatedCallbacks.AddUObject(this, &UDNAAbilityTask_WaitAbilityActivate::OnAbilityActivate);
	}
}

void UDNAAbilityTask_WaitAbilityActivate::OnAbilityActivate(UDNAAbility* ActivatedAbility)
{
	if (!IncludeTriggeredAbilities && ActivatedAbility->IsTriggered())
	{
		return;
	}

	if (TagRequirements.IsEmpty())
	{
		if ((WithTag.IsValid() && !ActivatedAbility->AbilityTags.HasTag(WithTag)) ||
			(WithoutTag.IsValid() && ActivatedAbility->AbilityTags.HasTag(WithoutTag)))
		{
			// Failed tag check
			return;
		}
	}
	else
	{
		if (!TagRequirements.RequirementsMet(ActivatedAbility->AbilityTags))
		{
			// Failed tag check
			return;
		}
	}

	OnActivate.Broadcast(ActivatedAbility);

	if (TriggerOnce)
	{
		EndTask();
	}
}

void UDNAAbilityTask_WaitAbilityActivate::OnDestroy(bool AbilityEnded)
{
	if (DNAAbilitySystemComponent)
	{
		DNAAbilitySystemComponent->AbilityActivatedCallbacks.Remove(OnAbilityActivateDelegateHandle);
	}

	Super::OnDestroy(AbilityEnded);
}
