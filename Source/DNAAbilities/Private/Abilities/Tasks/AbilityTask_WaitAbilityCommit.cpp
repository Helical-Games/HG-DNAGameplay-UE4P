// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/Tasks/AbilityTask_WaitAbilityCommit.h"
#include "AbilitySystemComponent.h"

UDNAAbilityTask_WaitAbilityCommit::UDNAAbilityTask_WaitAbilityCommit(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UDNAAbilityTask_WaitAbilityCommit* UDNAAbilityTask_WaitAbilityCommit::WaitForAbilityCommit(UDNAAbility* OwningAbility, FDNATag InWithTag, FDNATag InWithoutTag, bool InTriggerOnce)
{
	auto MyObj = NewDNAAbilityTask<UDNAAbilityTask_WaitAbilityCommit>(OwningAbility);
	MyObj->WithTag = InWithTag;
	MyObj->WithoutTag = InWithoutTag;
	MyObj->TriggerOnce = InTriggerOnce;

	return MyObj;
}

void UDNAAbilityTask_WaitAbilityCommit::Activate()
{
	if (DNAAbilitySystemComponent)	
	{		
		OnAbilityCommitDelegateHandle = DNAAbilitySystemComponent->AbilityCommitedCallbacks.AddUObject(this, &UDNAAbilityTask_WaitAbilityCommit::OnAbilityCommit);
	}
}

void UDNAAbilityTask_WaitAbilityCommit::OnDestroy(bool AbilityEnded)
{
	if (DNAAbilitySystemComponent)
	{
		DNAAbilitySystemComponent->AbilityCommitedCallbacks.Remove(OnAbilityCommitDelegateHandle);
	}

	Super::OnDestroy(AbilityEnded);
}

void UDNAAbilityTask_WaitAbilityCommit::OnAbilityCommit(UDNAAbility *ActivatedAbility)
{
	if ( (WithTag.IsValid() && !ActivatedAbility->AbilityTags.HasTag(WithTag)) ||
		 (WithoutTag.IsValid() && ActivatedAbility->AbilityTags.HasTag(WithoutTag)))
	{
		// Failed tag check
		return;
	}

	OnCommit.Broadcast(ActivatedAbility);

	if (TriggerOnce)
	{
		EndTask();
	}
}
