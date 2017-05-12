// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/Tasks/AbilityTask_WaitDNATag.h"
#include "AbilitySystemComponent.h"

// ----------------------------------------------------------------
UDNAAbilityTask_WaitDNATagAdded::UDNAAbilityTask_WaitDNATagAdded(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{

}

UDNAAbilityTask_WaitDNATagAdded* UDNAAbilityTask_WaitDNATagAdded::WaitDNATagAdd(UDNAAbility* OwningAbility, FDNATag Tag, AActor* InOptionalExternalTarget, bool OnlyTriggerOnce)
{
	auto MyObj = NewDNAAbilityTask<UDNAAbilityTask_WaitDNATagAdded>(OwningAbility);
	MyObj->Tag = Tag;
	MyObj->SetExternalTarget(InOptionalExternalTarget);
	MyObj->OnlyTriggerOnce = OnlyTriggerOnce;

	return MyObj;
}

void UDNAAbilityTask_WaitDNATagAdded::Activate()
{
	UDNAAbilitySystemComponent* ASC = GetTargetASC();
	if (ASC && ASC->HasMatchingDNATag(Tag))
	{			
		Added.Broadcast();
		if(OnlyTriggerOnce)
		{
			EndTask();
			return;
		}
	}

	Super::Activate();
}

void UDNAAbilityTask_WaitDNATagAdded::DNATagCallback(const FDNATag InTag, int32 NewCount)
{
	if (NewCount==1)
	{
		Added.Broadcast();
		if(OnlyTriggerOnce)
		{
			EndTask();
		}
	}
}

// ----------------------------------------------------------------

UDNAAbilityTask_WaitDNATagRemoved::UDNAAbilityTask_WaitDNATagRemoved(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{

}

UDNAAbilityTask_WaitDNATagRemoved* UDNAAbilityTask_WaitDNATagRemoved::WaitDNATagRemove(UDNAAbility* OwningAbility, FDNATag Tag, AActor* InOptionalExternalTarget, bool OnlyTriggerOnce)
{
	auto MyObj = NewDNAAbilityTask<UDNAAbilityTask_WaitDNATagRemoved>(OwningAbility);
	MyObj->Tag = Tag;
	MyObj->SetExternalTarget(InOptionalExternalTarget);
	MyObj->OnlyTriggerOnce = OnlyTriggerOnce;

	return MyObj;
}

void UDNAAbilityTask_WaitDNATagRemoved::Activate()
{
	UDNAAbilitySystemComponent* ASC = GetTargetASC();
	if (ASC && ASC->HasMatchingDNATag(Tag) == false)
	{			
		Removed.Broadcast();
		if(OnlyTriggerOnce)
		{
			EndTask();
			return;
		}
	}

	Super::Activate();
}

void UDNAAbilityTask_WaitDNATagRemoved::DNATagCallback(const FDNATag InTag, int32 NewCount)
{
	if (NewCount==0)
	{
		Removed.Broadcast();
		if(OnlyTriggerOnce)
		{
			EndTask();
		}
	}
}

// ----------------------------------------------------------------
