// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "AbilitySystemComponent.h"

UDNAAbilityTask_WaitTargetData::UDNAAbilityTask_WaitTargetData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}

UDNAAbilityTask_WaitTargetData* UDNAAbilityTask_WaitTargetData::WaitTargetData(UDNAAbility* OwningAbility, FName TaskInstanceName, TEnumAsByte<EDNATargetingConfirmation::Type> ConfirmationType, TSubclassOf<ADNAAbilityTargetActor> InTargetClass)
{
	UDNAAbilityTask_WaitTargetData* MyObj = NewDNAAbilityTask<UDNAAbilityTask_WaitTargetData>(OwningAbility, TaskInstanceName);		//Register for task list here, providing a given FName as a key
	MyObj->TargetClass = InTargetClass;
	MyObj->TargetActor = nullptr;
	MyObj->ConfirmationType = ConfirmationType;
	return MyObj;
}

UDNAAbilityTask_WaitTargetData* UDNAAbilityTask_WaitTargetData::WaitTargetDataUsingActor(UDNAAbility* OwningAbility, FName TaskInstanceName, TEnumAsByte<EDNATargetingConfirmation::Type> ConfirmationType, ADNAAbilityTargetActor* InTargetActor)
{
	UDNAAbilityTask_WaitTargetData* MyObj = NewDNAAbilityTask<UDNAAbilityTask_WaitTargetData>(OwningAbility, TaskInstanceName);		//Register for task list here, providing a given FName as a key
	MyObj->TargetClass = nullptr;
	MyObj->TargetActor = InTargetActor;
	MyObj->ConfirmationType = ConfirmationType;
	return MyObj;
}

void UDNAAbilityTask_WaitTargetData::Activate()
{
	// Need to handle case where target actor was passed into task
	if (Ability && (TargetClass == nullptr))
	{
		if (TargetActor)
		{
			ADNAAbilityTargetActor* SpawnedActor = TargetActor;
			TargetClass = SpawnedActor->GetClass();

			RegisterTargetDataCallbacks();


			if (IsPendingKill())
			{
				return;
			}

			if (ShouldSpawnTargetActor())
			{
				InitializeTargetActor(SpawnedActor);
				FinalizeTargetActor(SpawnedActor);

				// Note that the call to FinalizeTargetActor, this task could finish and our owning ability may be ended.
			}
			else
			{
				TargetActor = nullptr;

				// We may need a better solution here.  We don't know the target actor isn't needed till after it's already been spawned.
				SpawnedActor->Destroy();
				SpawnedActor = nullptr;
			}
		}
		else
		{
			EndTask();
		}
	}
}

bool UDNAAbilityTask_WaitTargetData::BeginSpawningActor(UDNAAbility* OwningAbility, TSubclassOf<ADNAAbilityTargetActor> InTargetClass, ADNAAbilityTargetActor*& SpawnedActor)
{
	SpawnedActor = nullptr;

	if (Ability)
	{
		if (ShouldSpawnTargetActor())
		{
			UClass* Class = *InTargetClass;
			if (Class != nullptr)
			{
				UWorld* World = GEngine->GetWorldFromContextObject(OwningAbility);
				SpawnedActor = World->SpawnActorDeferred<ADNAAbilityTargetActor>(Class, FTransform::Identity, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			}

			if (SpawnedActor)
			{
				TargetActor = SpawnedActor;
				InitializeTargetActor(SpawnedActor);
			}
		}

		RegisterTargetDataCallbacks();
	}

	return (SpawnedActor != nullptr);
}

void UDNAAbilityTask_WaitTargetData::FinishSpawningActor(UDNAAbility* OwningAbility, ADNAAbilityTargetActor* SpawnedActor)
{
	if (SpawnedActor && !SpawnedActor->IsPendingKill())
	{
		check(TargetActor == SpawnedActor);

		const FTransform SpawnTransform = DNAAbilitySystemComponent->GetOwner()->GetTransform();

		SpawnedActor->FinishSpawning(SpawnTransform);

		FinalizeTargetActor(SpawnedActor);
	}
}

bool UDNAAbilityTask_WaitTargetData::ShouldSpawnTargetActor() const
{
	check(TargetClass);
	check(Ability);

	// Spawn the actor if this is a locally controlled ability (always) or if this is a replicating targeting mode.
	// (E.g., server will spawn this target actor to replicate to all non owning clients)

	const ADNAAbilityTargetActor* CDO = CastChecked<ADNAAbilityTargetActor>(TargetClass->GetDefaultObject());

	const bool bReplicates = CDO->GetIsReplicated();
	const bool bIsLocallyControlled = Ability->GetCurrentActorInfo()->IsLocallyControlled();
	const bool bShouldProduceTargetDataOnServer = CDO->ShouldProduceTargetDataOnServer;

	return (bReplicates || bIsLocallyControlled || bShouldProduceTargetDataOnServer);
}

void UDNAAbilityTask_WaitTargetData::InitializeTargetActor(ADNAAbilityTargetActor* SpawnedActor) const
{
	check(SpawnedActor);
	check(Ability);

	SpawnedActor->MasterPC = Ability->GetCurrentActorInfo()->PlayerController.Get();

	// If we spawned the target actor, always register the callbacks for when the data is ready.
	SpawnedActor->TargetDataReadyDelegate.AddUObject(this, &UDNAAbilityTask_WaitTargetData::OnTargetDataReadyCallback);
	SpawnedActor->CanceledDelegate.AddUObject(this, &UDNAAbilityTask_WaitTargetData::OnTargetDataCancelledCallback);
}

void UDNAAbilityTask_WaitTargetData::FinalizeTargetActor(ADNAAbilityTargetActor* SpawnedActor) const
{
	check(SpawnedActor);
	check(Ability);

	// User ability activation is inhibited while this is active
	DNAAbilitySystemComponent->SpawnedTargetActors.Push(SpawnedActor);

	SpawnedActor->StartTargeting(Ability);

	if (SpawnedActor->ShouldProduceTargetData())
	{
		// If instant confirm, then stop targeting immediately.
		// Note this is kind of bad: we should be able to just call a static func on the CDO to do this. 
		// But then we wouldn't get to set ExposeOnSpawnParameters.
		if (ConfirmationType == EDNATargetingConfirmation::Instant)
		{
			SpawnedActor->ConfirmTargeting();
		}
		else if (ConfirmationType == EDNATargetingConfirmation::UserConfirmed)
		{
			// Bind to the Cancel/Confirm Delegates (called from local confirm or from repped confirm)
			SpawnedActor->BindToConfirmCancelInputs();
		}
	}
}

void UDNAAbilityTask_WaitTargetData::RegisterTargetDataCallbacks()
{
	if (!ensure(IsPendingKill() == false))
	{
		return;
	}

	check(TargetClass);
	check(Ability);

	const ADNAAbilityTargetActor* CDO = CastChecked<ADNAAbilityTargetActor>(TargetClass->GetDefaultObject());

	const bool bIsLocallyControlled = Ability->GetCurrentActorInfo()->IsLocallyControlled();
	const bool bShouldProduceTargetDataOnServer = CDO->ShouldProduceTargetDataOnServer;

	// If not locally controlled (server for remote client), see if TargetData was already sent
	// else register callback for when it does get here.
	if (!bIsLocallyControlled)
	{
		// Register with the TargetData callbacks if we are expecting client to send them
		if (!bShouldProduceTargetDataOnServer)
		{
			FDNAAbilitySpecHandle	SpecHandle = GetAbilitySpecHandle();
			FPredictionKey ActivationPredictionKey = GetActivationPredictionKey();

			//Since multifire is supported, we still need to hook up the callbacks
			DNAAbilitySystemComponent->AbilityTargetDataSetDelegate(SpecHandle, ActivationPredictionKey ).AddUObject(this, &UDNAAbilityTask_WaitTargetData::OnTargetDataReplicatedCallback);
			DNAAbilitySystemComponent->AbilityTargetDataCancelledDelegate(SpecHandle, ActivationPredictionKey ).AddUObject(this, &UDNAAbilityTask_WaitTargetData::OnTargetDataReplicatedCancelledCallback);

			DNAAbilitySystemComponent->CallReplicatedTargetDataDelegatesIfSet(SpecHandle, ActivationPredictionKey );

			SetWaitingOnRemotePlayerData();
		}
	}
}

/** Valid TargetData was replicated to use (we are server, was sent from client) */
void UDNAAbilityTask_WaitTargetData::OnTargetDataReplicatedCallback(const FDNAAbilityTargetDataHandle& Data, FDNATag ActivationTag)
{
	check(DNAAbilitySystemComponent);

	FDNAAbilityTargetDataHandle MutableData = Data;
	DNAAbilitySystemComponent->ConsumeClientReplicatedTargetData(GetAbilitySpecHandle(), GetActivationPredictionKey());

	/** 
	 *  Call into the TargetActor to sanitize/verify the data. If this returns false, we are rejecting
	 *	the replicated target data and will treat this as a cancel.
	 *	
	 *	This can also be used for bandwidth optimizations. OnReplicatedTargetDataReceived could do an actual
	 *	trace/check/whatever server side and use that data. So rather than having the client send that data
	 *	explicitly, the client is basically just sending a 'confirm' and the server is now going to do the work
	 *	in OnReplicatedTargetDataReceived.
	 */
	if (TargetActor && !TargetActor->OnReplicatedTargetDataReceived(MutableData))
	{
		Cancelled.Broadcast(MutableData);
	}
	else
	{
		ValidData.Broadcast(MutableData);
	}

	if (ConfirmationType != EDNATargetingConfirmation::CustomMulti)
	{
		EndTask();
	}
}

/** Client canceled this Targeting Task (we are the server) */
void UDNAAbilityTask_WaitTargetData::OnTargetDataReplicatedCancelledCallback()
{
	check(DNAAbilitySystemComponent);
	Cancelled.Broadcast(FDNAAbilityTargetDataHandle());
	EndTask();
}

/** The TargetActor we spawned locally has called back with valid target data */
void UDNAAbilityTask_WaitTargetData::OnTargetDataReadyCallback(const FDNAAbilityTargetDataHandle& Data)
{
	check(DNAAbilitySystemComponent);
	if (!Ability)
	{
		return;
	}

	FScopedPredictionWindow	ScopedPrediction(DNAAbilitySystemComponent, ShouldReplicateDataToServer());
	
	const FDNAAbilityActorInfo* Info = Ability->GetCurrentActorInfo();
	if (IsPredictingClient())
	{
		if (!TargetActor->ShouldProduceTargetDataOnServer)
		{
			FDNATag ApplicationTag; // Fixme: where would this be useful?
			DNAAbilitySystemComponent->ServerSetReplicatedTargetData(GetAbilitySpecHandle(), GetActivationPredictionKey(), Data, ApplicationTag, DNAAbilitySystemComponent->ScopedPredictionKey);
		}
		else if (ConfirmationType == EDNATargetingConfirmation::UserConfirmed)
		{
			// We aren't going to send the target data, but we will send a generic confirmed message.
			DNAAbilitySystemComponent->ServerSetReplicatedEvent(EAbilityGenericReplicatedEvent::GenericConfirm, GetAbilitySpecHandle(), GetActivationPredictionKey(), DNAAbilitySystemComponent->ScopedPredictionKey);
		}
	}

	ValidData.Broadcast(Data);

	if (ConfirmationType != EDNATargetingConfirmation::CustomMulti)
	{
		EndTask();
	}
}

/** The TargetActor we spawned locally has called back with a cancel event (they still include the 'last/best' targetdata but the consumer of this may want to discard it) */
void UDNAAbilityTask_WaitTargetData::OnTargetDataCancelledCallback(const FDNAAbilityTargetDataHandle& Data)
{
	check(DNAAbilitySystemComponent);

	FScopedPredictionWindow ScopedPrediction(DNAAbilitySystemComponent, IsPredictingClient());

	if (IsPredictingClient())
	{
		if (!TargetActor->ShouldProduceTargetDataOnServer)
		{
			DNAAbilitySystemComponent->ServerSetReplicatedTargetDataCancelled(GetAbilitySpecHandle(), GetActivationPredictionKey(), DNAAbilitySystemComponent->ScopedPredictionKey );
		}
		else
		{
			// We aren't going to send the target data, but we will send a generic confirmed message.
			DNAAbilitySystemComponent->ServerSetReplicatedEvent(EAbilityGenericReplicatedEvent::GenericCancel, GetAbilitySpecHandle(), GetActivationPredictionKey(), DNAAbilitySystemComponent->ScopedPredictionKey);
		}
	}
	Cancelled.Broadcast(Data);
	EndTask();
}

/** Called when the ability is asked to confirm from an outside node. What this means depends on the individual task. By default, this does nothing other than ending if bEndTask is true. */
void UDNAAbilityTask_WaitTargetData::ExternalConfirm(bool bEndTask)
{
	check(DNAAbilitySystemComponent);
	if (TargetActor)
	{
		if (TargetActor->ShouldProduceTargetData())
		{
			TargetActor->ConfirmTargetingAndContinue();
		}
	}
	Super::ExternalConfirm(bEndTask);
}

/** Called when the ability is asked to confirm from an outside node. What this means depends on the individual task. By default, this does nothing other than ending if bEndTask is true. */
void UDNAAbilityTask_WaitTargetData::ExternalCancel()
{
	check(DNAAbilitySystemComponent);
	Cancelled.Broadcast(FDNAAbilityTargetDataHandle());
	Super::ExternalCancel();
}

void UDNAAbilityTask_WaitTargetData::OnDestroy(bool AbilityEnded)
{
	if (TargetActor)
	{
		TargetActor->Destroy();
	}

	Super::OnDestroy(AbilityEnded);
}

bool UDNAAbilityTask_WaitTargetData::ShouldReplicateDataToServer() const
{
	if (!Ability || !TargetActor)
	{
		return false;
	}

	// Send TargetData to the server IFF we are the client and this isn't a DNATargetActor that can produce data on the server	
	const FDNAAbilityActorInfo* Info = Ability->GetCurrentActorInfo();
	if (!Info->IsNetAuthority() && !TargetActor->ShouldProduceTargetDataOnServer)
	{
		return true;
	}

	return false;
}


// --------------------------------------------------------------------------------------
