// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/Tasks/AbilityTask_WaitDNAEvent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"

// ----------------------------------------------------------------
UDNAAbilityTask_WaitDNAEvent::UDNAAbilityTask_WaitDNAEvent(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{

}

UDNAAbilityTask_WaitDNAEvent* UDNAAbilityTask_WaitDNAEvent::WaitDNAEvent(UDNAAbility* OwningAbility, FDNATag Tag, AActor* OptionalExternalTarget, bool OnlyTriggerOnce)
{
	auto MyObj = NewDNAAbilityTask<UDNAAbilityTask_WaitDNAEvent>(OwningAbility);
	MyObj->Tag = Tag;
	MyObj->SetExternalTarget(OptionalExternalTarget);
	MyObj->OnlyTriggerOnce = OnlyTriggerOnce;

	return MyObj;
}

void UDNAAbilityTask_WaitDNAEvent::Activate()
{
	UDNAAbilitySystemComponent* ASC = GetTargetASC();
	if (ASC)
	{	
		MyHandle = ASC->GenericDNAEventCallbacks.FindOrAdd(Tag).AddUObject(this, &UDNAAbilityTask_WaitDNAEvent::DNAEventCallback);
	}

	Super::Activate();
}

void UDNAAbilityTask_WaitDNAEvent::DNAEventCallback(const FDNAEventData* Payload)
{
	EventReceived.Broadcast(*Payload);
	if(OnlyTriggerOnce)
	{
		EndTask();
	}
}

void UDNAAbilityTask_WaitDNAEvent::SetExternalTarget(AActor* Actor)
{
	if (Actor)
	{
		UseExternalTarget = true;
		OptionalExternalTarget = UDNAAbilitySystemGlobals::GetDNAAbilitySystemComponentFromActor(Actor);
	}
}

UDNAAbilitySystemComponent* UDNAAbilityTask_WaitDNAEvent::GetTargetASC()
{
	if (UseExternalTarget)
	{
		return OptionalExternalTarget;
	}

	return DNAAbilitySystemComponent;
}

void UDNAAbilityTask_WaitDNAEvent::OnDestroy(bool AbilityEnding)
{
	UDNAAbilitySystemComponent* ASC = GetTargetASC();
	if (ASC && MyHandle.IsValid())
	{	
		ASC->GenericDNAEventCallbacks.FindOrAdd(Tag).Remove(MyHandle);
	}

	Super::OnDestroy(AbilityEnding);
}
