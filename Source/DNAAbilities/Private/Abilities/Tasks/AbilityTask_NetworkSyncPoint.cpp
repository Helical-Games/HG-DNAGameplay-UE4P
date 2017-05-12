// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/Tasks/AbilityTask_NetworkSyncPoint.h"
#include "AbilitySystemComponent.h"

UDNAAbilityTask_NetworkSyncPoint::UDNAAbilityTask_NetworkSyncPoint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ReplicatedEventToListenFor = EAbilityGenericReplicatedEvent::MAX;
}

void UDNAAbilityTask_NetworkSyncPoint::OnSignalCallback()
{
	if (DNAAbilitySystemComponent)
	{
		DNAAbilitySystemComponent->ConsumeGenericReplicatedEvent(ReplicatedEventToListenFor, GetAbilitySpecHandle(), GetActivationPredictionKey());
	}
	SyncFinished();
}

UDNAAbilityTask_NetworkSyncPoint* UDNAAbilityTask_NetworkSyncPoint::WaitNetSync(class UDNAAbility* OwningAbility, EDNAAbilityTaskNetSyncType InSyncType)
{
	auto MyObj = NewDNAAbilityTask<UDNAAbilityTask_NetworkSyncPoint>(OwningAbility);
	MyObj->SyncType = InSyncType;
	return MyObj;
}

void UDNAAbilityTask_NetworkSyncPoint::Activate()
{
	FScopedPredictionWindow ScopedPrediction(DNAAbilitySystemComponent, IsPredictingClient());

	if (DNAAbilitySystemComponent)
	{
		if (IsPredictingClient())
		{
			if (SyncType != EDNAAbilityTaskNetSyncType::OnlyServerWait )
			{
				// As long as we are waiting (!= OnlyServerWait), listen for the GenericSignalFromServer event
				ReplicatedEventToListenFor = EAbilityGenericReplicatedEvent::GenericSignalFromServer;
			}
			if (SyncType != EDNAAbilityTaskNetSyncType::OnlyClientWait)
			{
				// As long as the server is waiting (!= OnlyClientWait), send the Server and RPC for this signal
				DNAAbilitySystemComponent->ServerSetReplicatedEvent(EAbilityGenericReplicatedEvent::GenericSignalFromClient, GetAbilitySpecHandle(), GetActivationPredictionKey(), DNAAbilitySystemComponent->ScopedPredictionKey);
			}
			
		}
		else if (IsForRemoteClient())
		{
			if (SyncType != EDNAAbilityTaskNetSyncType::OnlyClientWait )
			{
				// As long as we are waiting (!= OnlyClientWait), listen for the GenericSignalFromClient event
				ReplicatedEventToListenFor = EAbilityGenericReplicatedEvent::GenericSignalFromClient;
			}
			if (SyncType != EDNAAbilityTaskNetSyncType::OnlyServerWait)
			{
				// As long as the client is waiting (!= OnlyServerWait), send the Server and RPC for this signal
				DNAAbilitySystemComponent->ClientSetReplicatedEvent(EAbilityGenericReplicatedEvent::GenericSignalFromServer, GetAbilitySpecHandle(), GetActivationPredictionKey());
			}
		}

		if (ReplicatedEventToListenFor != EAbilityGenericReplicatedEvent::MAX)
		{
			CallOrAddReplicatedDelegate(ReplicatedEventToListenFor, FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &UDNAAbilityTask_NetworkSyncPoint::OnSignalCallback));
		}
		else
		{
			// We aren't waiting for a replicated event, so the sync is complete.
			SyncFinished();
		}
	}
}

void UDNAAbilityTask_NetworkSyncPoint::OnDestroy(bool AbilityEnded)
{
	Super::OnDestroy(AbilityEnded);
}

void UDNAAbilityTask_NetworkSyncPoint::SyncFinished()
{
	if (!IsPendingKill())
	{
		OnSync.Broadcast();
		EndTask();
	}
}
