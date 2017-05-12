// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/DNAAbilityTypes.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/MovementComponent.h"
#include "Abilities/DNAAbility.h"
#include "AbilitySystemComponent.h"

//----------------------------------------------------------------------
void FDNAAbilityActorInfo::InitFromActor(AActor *InOwnerActor, AActor *InAvatarActor, UDNAAbilitySystemComponent* InDNAAbilitySystemComponent)
{
	check(InOwnerActor);
	check(InDNAAbilitySystemComponent);

	OwnerActor = InOwnerActor;
	AvatarActor = InAvatarActor;
	DNAAbilitySystemComponent = InDNAAbilitySystemComponent;

	APlayerController* OldPC = PlayerController.Get();

	// Look for a player controller or pawn in the owner chain.
	AActor *TestActor = InOwnerActor;
	while (TestActor)
	{
		if (APlayerController * CastPC = Cast<APlayerController>(TestActor))
		{
			PlayerController = CastPC;
			break;
		}

		if (APawn * Pawn = Cast<APawn>(TestActor))
		{
			PlayerController = Cast<APlayerController>(Pawn->GetController());
			break;
		}

		TestActor = TestActor->GetOwner();
	}

	// Notify ASC if PlayerController was found for first time
	if (OldPC == nullptr && PlayerController.IsValid())
	{
		InDNAAbilitySystemComponent->OnPlayerControllerSet();
	}

	if (AvatarActor.Get())
	{
		// Grab Components that we care about
		SkeletalMeshComponent = AvatarActor->FindComponentByClass<USkeletalMeshComponent>();
		MovementComponent = AvatarActor->FindComponentByClass<UMovementComponent>();
	}
	else
	{
		SkeletalMeshComponent = nullptr;
		MovementComponent = nullptr;
	}
}

void FDNAAbilityActorInfo::SetAvatarActor(AActor *InAvatarActor)
{
	InitFromActor(OwnerActor.Get(), InAvatarActor, DNAAbilitySystemComponent.Get());
}

void FDNAAbilityActorInfo::ClearActorInfo()
{
	OwnerActor = nullptr;
	AvatarActor = nullptr;
	PlayerController = nullptr;
	SkeletalMeshComponent = nullptr;
	MovementComponent = nullptr;
}

bool FDNAAbilityActorInfo::IsLocallyControlled() const
{
	if (PlayerController.IsValid())
	{
		return PlayerController->IsLocalController();
	}
	else if (IsNetAuthority())
	{
		// Non-players are always locally controlled on the server
		return true;
	}

	return false;
}

bool FDNAAbilityActorInfo::IsLocallyControlledPlayer() const
{
	if (PlayerController.IsValid())
	{
		return PlayerController->IsLocalController();
	}

	return false;
}

bool FDNAAbilityActorInfo::IsNetAuthority() const
{
	// Make sure this works on pending kill actors
	if (OwnerActor.IsValid(true))
	{
		return (OwnerActor.Get(true)->Role == ROLE_Authority);
	}

	// If we encounter issues with this being called before or after the owning actor is destroyed,
	// we may need to cache off the authority (or look for it on some global/world state).

	ensure(false);
	return false;
}

void FDNAAbilityActivationInfo::SetPredicting(FPredictionKey PredictionKey)
{
	ActivationMode = EDNAAbilityActivationMode::Predicting;
	PredictionKeyWhenActivated = PredictionKey;

	// Abilities can be cancelled by server at any time. There is no reason to have to wait until confirmation.
	// prediction keys keep previous activations of abilities from ending future activations.
	bCanBeEndedByOtherInstance = true;
}

void FDNAAbilityActivationInfo::ServerSetActivationPredictionKey(FPredictionKey PredictionKey)
{
	PredictionKeyWhenActivated = PredictionKey;
}

void FDNAAbilityActivationInfo::SetActivationConfirmed()
{
	ActivationMode = EDNAAbilityActivationMode::Confirmed;
	//Remote (server) commands to end the ability that come in after this point are considered for this instance
	bCanBeEndedByOtherInstance = true;
}

void FDNAAbilityActivationInfo::SetActivationRejected()
{
	ActivationMode = EDNAAbilityActivationMode::Rejected;
}

bool FDNAAbilitySpec::IsActive() const
{
	// If ability hasn't replicated yet we're not active
	return Ability != nullptr && ActiveCount > 0;
}

UDNAAbility* FDNAAbilitySpec::GetPrimaryInstance() const
{
	if (Ability && Ability->GetInstancingPolicy() == EDNAAbilityInstancingPolicy::InstancedPerActor)
	{
		if (NonReplicatedInstances.Num() > 0)
		{
			return NonReplicatedInstances[0];
		}
		if (ReplicatedInstances.Num() > 0)
		{
			return ReplicatedInstances[0];
		}
	}
	return nullptr;
}

void FDNAAbilitySpec::PreReplicatedRemove(const struct FDNAAbilitySpecContainer& InArraySerializer)
{
	if (InArraySerializer.Owner)
	{
		InArraySerializer.Owner->OnRemoveAbility(*this);
	}
}

void FDNAAbilitySpec::PostReplicatedAdd(const struct FDNAAbilitySpecContainer& InArraySerializer)
{
	if (InArraySerializer.Owner)
	{
		InArraySerializer.Owner->OnGiveAbility(*this);
	}
}

void FDNAAbilitySpecContainer::RegisterWithOwner(UDNAAbilitySystemComponent* InOwner)
{
	Owner = InOwner;
}

// ----------------------------------------------------

FDNAAbilitySpec::FDNAAbilitySpec(FDNAAbilitySpecDef& InDef, int32 InDNAEffectLevel, FActiveDNAEffectHandle InDNAEffectHandle)
	: Ability(InDef.Ability ? InDef.Ability->GetDefaultObject<UDNAAbility>() : nullptr)
	, InputID(InDef.InputID)
	, SourceObject(InDef.SourceObject)
	, ActiveCount(0)
	, InputPressed(false)
	, RemoveAfterActivation(false)
	, PendingRemove(false)
{
	Handle.GenerateNewHandle();
	InDef.AssignedHandle = Handle;
	DNAEffectHandle = InDNAEffectHandle;

	FString ContextString = FString::Printf(TEXT("FDNAAbilitySpec::FDNAAbilitySpec for %s from %s"), 
		(InDef.Ability ? *InDef.Ability->GetName() : TEXT("INVALID ABILITY")), 
		(InDef.SourceObject ? *InDef.SourceObject->GetName() : TEXT("INVALID ABILITY")));
	Level = InDef.LevelScalableFloat.GetValueAtLevel(InDNAEffectLevel, &ContextString);
}

// ----------------------------------------------------

FScopedAbilityListLock::FScopedAbilityListLock(UDNAAbilitySystemComponent& InDNAAbilitySystemComponent)
	: DNAAbilitySystemComponent(InDNAAbilitySystemComponent)
{
	DNAAbilitySystemComponent.IncrementAbilityListLock();
}

FScopedAbilityListLock::~FScopedAbilityListLock()
{
	DNAAbilitySystemComponent.DecrementAbilityListLock();
}

// ----------------------------------------------------

FScopedTargetListLock::FScopedTargetListLock(UDNAAbilitySystemComponent& InDNAAbilitySystemComponent, const UDNAAbility& InAbility)
	: DNAAbility(InAbility)
	, AbilityLock(InDNAAbilitySystemComponent)
{
	DNAAbility.IncrementListLock();
}

FScopedTargetListLock::~FScopedTargetListLock()
{
	DNAAbility.DecrementListLock();
}
