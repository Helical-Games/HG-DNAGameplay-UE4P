// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/DNAAbility_CharacterJump.h"
#include "GameFramework/Character.h"

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	UDNAAbility_CharacterJump
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

UDNAAbility_CharacterJump::UDNAAbility_CharacterJump(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NetExecutionPolicy = EDNAAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EDNAAbilityInstancingPolicy::NonInstanced;
}

void UDNAAbility_CharacterJump::ActivateAbility(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo, const FDNAEventData* TriggerEventData)
{
	
	if (HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo))
	{
		if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
		{
			return;
		}

		ACharacter * Character = CastChecked<ACharacter>(ActorInfo->AvatarActor.Get());
		Character->Jump();
	}
}

void UDNAAbility_CharacterJump::InputReleased(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo)
{
	if (ActorInfo != NULL && ActorInfo->AvatarActor != NULL)
	{
		CancelAbility(Handle, ActorInfo, ActivationInfo, true);
	}
}

bool UDNAAbility_CharacterJump::CanActivateAbility(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNATagContainer* SourceTags, const FDNATagContainer* TargetTags, OUT FDNATagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const ACharacter* Character = CastChecked<ACharacter>(ActorInfo->AvatarActor.Get(), ECastCheckedType::NullAllowed);
	return (Character && Character->CanJump());
}

/**
 *	Canceling an non instanced ability is tricky. Right now this works for Jump since there is nothing that can go wrong by calling
 *	StopJumping() if you aren't already jumping. If we had a montage playing non instanced ability, it would need to make sure the
 *	Montage that *it* played was still playing, and if so, to cancel it. If this is something we need to support, we may need some
 *	light weight data structure to represent 'non intanced abilities in action' with a way to cancel/end them.
 */
void UDNAAbility_CharacterJump::CancelAbility(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility)
{
	if (ScopeLockCount > 0)
	{
		WaitingToExecute.Add(FPostLockDelegate::CreateUObject(this, &UDNAAbility_CharacterJump::CancelAbility, Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility));
		return;
	}

	Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
	
	ACharacter * Character = CastChecked<ACharacter>(ActorInfo->AvatarActor.Get());
	Character->StopJumping();
}
