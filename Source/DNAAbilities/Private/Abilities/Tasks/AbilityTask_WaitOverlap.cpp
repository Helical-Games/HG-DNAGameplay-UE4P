// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/Tasks/AbilityTask_WaitOverlap.h"

UDNAAbilityTask_WaitOverlap::UDNAAbilityTask_WaitOverlap(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void UDNAAbilityTask_WaitOverlap::OnHitCallback(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if(OtherActor)
	{
		// Construct TargetData
		FDNAAbilityTargetData_SingleTargetHit * TargetData = new FDNAAbilityTargetData_SingleTargetHit(Hit);

		// Give it a handle and return
		FDNAAbilityTargetDataHandle	Handle;
		Handle.Data.Add(TSharedPtr<FDNAAbilityTargetData>(TargetData));
		OnOverlap.Broadcast(Handle);

		// We are done. Kill us so we don't keep getting broadcast messages
		EndTask();
	}
}

/**
*	Need:
*	-Easy way to specify which primitive components should be used for this overlap test
*	-Easy way to specify which types of actors/collision overlaps that we care about/want to block on
*/

UDNAAbilityTask_WaitOverlap* UDNAAbilityTask_WaitOverlap::WaitForOverlap(UDNAAbility* OwningAbility)
{
	auto MyObj = NewDNAAbilityTask<UDNAAbilityTask_WaitOverlap>(OwningAbility);
	return MyObj;
}

void UDNAAbilityTask_WaitOverlap::Activate()
{
	SetWaitingOnAvatar();

	UPrimitiveComponent* PrimComponent = GetComponent();
	if (PrimComponent)
	{
		PrimComponent->OnComponentHit.AddDynamic(this, &UDNAAbilityTask_WaitOverlap::OnHitCallback);
	}
}

void UDNAAbilityTask_WaitOverlap::OnDestroy(bool AbilityEnded)
{
	UPrimitiveComponent* PrimComponent = GetComponent();
	if (PrimComponent)
	{
		PrimComponent->OnComponentHit.RemoveDynamic(this, &UDNAAbilityTask_WaitOverlap::OnHitCallback);
	}

	Super::OnDestroy(AbilityEnded);
}

UPrimitiveComponent* UDNAAbilityTask_WaitOverlap::GetComponent()
{
	// TEMP - we are just using root component's collision. A real system will need more data to specify which component to use
	UPrimitiveComponent * PrimComponent = nullptr;
	AActor* ActorOwner = GetAvatarActor();
	if (ActorOwner)
	{
		PrimComponent = Cast<UPrimitiveComponent>(ActorOwner->GetRootComponent());
		if (!PrimComponent)
		{
			PrimComponent = ActorOwner->FindComponentByClass<UPrimitiveComponent>();
		}
	}

	return PrimComponent;
}
