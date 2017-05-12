// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/DNAAbilityTargetActor.h"
#include "DNAAbilitySpec.h"
#include "GameFramework/PlayerController.h"
#include "AbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	ADNAAbilityTargetActor
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

ADNAAbilityTargetActor::ADNAAbilityTargetActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ShouldProduceTargetDataOnServer = false;
	bDebug = false;
	bDestroyOnConfirmation = true;
}

void ADNAAbilityTargetActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GenericDelegateBoundASC)
	{
		// We must remove ourselves from GenericLocalConfirmCallbacks/GenericLocalCancelCallbacks, since while these are bound they will inhibit any *other* abilities
		// that are bound to the same key.

		UDNAAbilitySystemComponent* UnboundASC = nullptr;
		const FDNAAbilityActorInfo* Info = (OwningAbility ? OwningAbility->GetCurrentActorInfo() : nullptr);
		if (Info && Info->IsLocallyControlled())
		{
			UDNAAbilitySystemComponent* ASC = Info->DNAAbilitySystemComponent.Get();
			if (ASC)
			{
				ASC->GenericLocalConfirmCallbacks.RemoveDynamic(this, &ADNAAbilityTargetActor::ConfirmTargeting);
				ASC->GenericLocalCancelCallbacks.RemoveDynamic(this, &ADNAAbilityTargetActor::CancelTargeting);

				UnboundASC = ASC;
			}
		}

		ensure(GenericDelegateBoundASC == UnboundASC); // Error checking that we have removed delegates from the same ASC we bound them to
	}

	Super::EndPlay(EndPlayReason);
}

void ADNAAbilityTargetActor::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADNAAbilityTargetActor, StartLocation);
	DOREPLIFETIME(ADNAAbilityTargetActor, SourceActor);
	DOREPLIFETIME(ADNAAbilityTargetActor, bDebug);
	DOREPLIFETIME(ADNAAbilityTargetActor, bDestroyOnConfirmation);
}

void ADNAAbilityTargetActor::StartTargeting(UDNAAbility* Ability)
{
	OwningAbility = Ability;
}

bool ADNAAbilityTargetActor::IsConfirmTargetingAllowed()
{
	return true;
}

void ADNAAbilityTargetActor::ConfirmTargetingAndContinue()
{
	check(ShouldProduceTargetData());
	if (IsConfirmTargetingAllowed())
	{
		TargetDataReadyDelegate.Broadcast(FDNAAbilityTargetDataHandle());
	}
}

void ADNAAbilityTargetActor::ConfirmTargeting()
{
	const FDNAAbilityActorInfo* ActorInfo = (OwningAbility ? OwningAbility->GetCurrentActorInfo() : nullptr);
	UDNAAbilitySystemComponent* ASC = (ActorInfo ? ActorInfo->DNAAbilitySystemComponent.Get() : nullptr);
	if (ASC)
	{
		ASC->AbilityReplicatedEventDelegate(EAbilityGenericReplicatedEvent::GenericConfirm, OwningAbility->GetCurrentAbilitySpecHandle(), OwningAbility->GetCurrentActivationInfo().GetActivationPredictionKey() ).Remove(GenericConfirmHandle);
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("ADNAAbilityTargetActor::ConfirmTargeting called with null Ability/ASC! Actor %s"), *GetName());
	}

	if (IsConfirmTargetingAllowed())
	{
		ConfirmTargetingAndContinue();
		if (bDestroyOnConfirmation)
		{
			Destroy();
		}
	}
}

/** Outside code is saying 'stop everything and just forget about it' */
void ADNAAbilityTargetActor::CancelTargeting()
{
	const FDNAAbilityActorInfo* ActorInfo = (OwningAbility ? OwningAbility->GetCurrentActorInfo() : nullptr);
	UDNAAbilitySystemComponent* ASC = (ActorInfo ? ActorInfo->DNAAbilitySystemComponent.Get() : nullptr);
	if (ASC)
	{
		ASC->AbilityReplicatedEventDelegate(EAbilityGenericReplicatedEvent::GenericCancel, OwningAbility->GetCurrentAbilitySpecHandle(), OwningAbility->GetCurrentActivationInfo().GetActivationPredictionKey() ).Remove(GenericCancelHandle);
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("ADNAAbilityTargetActor::CancelTargeting called with null ASC! Actor %s"), *GetName());
	}

	CanceledDelegate.Broadcast(FDNAAbilityTargetDataHandle());
	Destroy();
}

bool ADNAAbilityTargetActor::IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const
{
	//The player who created the ability doesn't need to be updated about it - there should be local prediction in place.
	if (RealViewer == MasterPC)
	{
		return false;
	}

	const FDNAAbilityActorInfo* ActorInfo = (OwningAbility ? OwningAbility->GetCurrentActorInfo() : NULL);
	AActor* Avatar = (ActorInfo ? ActorInfo->AvatarActor.Get() : NULL);

	if (Avatar)
	{
		return Avatar->IsNetRelevantFor(RealViewer, ViewTarget, SrcLocation);
	}

	return Super::IsNetRelevantFor(RealViewer, ViewTarget, SrcLocation);
}

bool ADNAAbilityTargetActor::OnReplicatedTargetDataReceived(FDNAAbilityTargetDataHandle& Data) const
{
	return true;
}

bool ADNAAbilityTargetActor::ShouldProduceTargetData() const
{
	// return true if we are locally owned, or (we are the server and this is a DNAtarget ability that can produce target data server side)
	return (MasterPC && (MasterPC->IsLocalController() || ShouldProduceTargetDataOnServer));
}

void ADNAAbilityTargetActor::BindToConfirmCancelInputs()
{
	check(OwningAbility);

	UDNAAbilitySystemComponent* ASC = OwningAbility->GetCurrentActorInfo()->DNAAbilitySystemComponent.Get();
	if (ASC)
	{
		const FDNAAbilityActorInfo* Info = OwningAbility->GetCurrentActorInfo();

		if (Info->IsLocallyControlled())
		{
			// We have to wait for the callback from the DNAAbilitySystemComponent. Which will always be instigated locally
			ASC->GenericLocalConfirmCallbacks.AddDynamic(this, &ADNAAbilityTargetActor::ConfirmTargeting);	// Tell me if the confirm input is pressed
			ASC->GenericLocalCancelCallbacks.AddDynamic(this, &ADNAAbilityTargetActor::CancelTargeting);	// Tell me if the cancel input is pressed

			// Save off which ASC we bound so that we can error check that we're removing them later
			GenericDelegateBoundASC = ASC;
		}
		else
		{	
			FDNAAbilitySpecHandle Handle = OwningAbility->GetCurrentAbilitySpecHandle();
			FPredictionKey PredKey = OwningAbility->GetCurrentActivationInfo().GetActivationPredictionKey();

			GenericConfirmHandle = ASC->AbilityReplicatedEventDelegate(EAbilityGenericReplicatedEvent::GenericConfirm, Handle, PredKey ).AddUObject(this, &ADNAAbilityTargetActor::ConfirmTargeting);
			GenericCancelHandle = ASC->AbilityReplicatedEventDelegate(EAbilityGenericReplicatedEvent::GenericCancel, Handle, PredKey ).AddUObject(this, &ADNAAbilityTargetActor::CancelTargeting);
			
			if (ASC->CallReplicatedEventDelegateIfSet(EAbilityGenericReplicatedEvent::GenericConfirm, Handle, PredKey))
			{
				return;
			}
			
			if (ASC->CallReplicatedEventDelegateIfSet(EAbilityGenericReplicatedEvent::GenericCancel, Handle, PredKey))
			{
				return;
			}
		}
	}
}
