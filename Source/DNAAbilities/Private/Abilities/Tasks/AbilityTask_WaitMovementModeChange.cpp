// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/Tasks/AbilityTask_WaitMovementModeChange.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UDNAAbilityTask_WaitMovementModeChange::UDNAAbilityTask_WaitMovementModeChange(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RequiredMode = MOVE_None;
}

UDNAAbilityTask_WaitMovementModeChange* UDNAAbilityTask_WaitMovementModeChange::CreateWaitMovementModeChange(class UDNAAbility* OwningAbility, EMovementMode NewMode)
{
	auto MyObj = NewDNAAbilityTask<UDNAAbilityTask_WaitMovementModeChange>(OwningAbility);
	MyObj->RequiredMode = NewMode;
	return MyObj;
}

void UDNAAbilityTask_WaitMovementModeChange::Activate()
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActor());
	if (Character)
	{
		Character->MovementModeChangedDelegate.AddDynamic(this, &UDNAAbilityTask_WaitMovementModeChange::OnMovementModeChange);
		MyCharacter = Character;
	}

	SetWaitingOnAvatar();
}

void UDNAAbilityTask_WaitMovementModeChange::OnMovementModeChange(ACharacter * Character, EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	if (Character)
	{
		if (UCharacterMovementComponent *MoveComp = Cast<UCharacterMovementComponent>(Character->GetMovementComponent()))
		{
			if (RequiredMode == MOVE_None || MoveComp->MovementMode == RequiredMode)
			{
				OnChange.Broadcast(MoveComp->MovementMode);
				EndTask();
				return;
			}
		}
	}
}

void UDNAAbilityTask_WaitMovementModeChange::OnDestroy(bool AbilityEnded)
{
	if (MyCharacter.IsValid())
	{
		MyCharacter->MovementModeChangedDelegate.RemoveDynamic(this, &UDNAAbilityTask_WaitMovementModeChange::OnMovementModeChange);
	}

	Super::OnDestroy(AbilityEnded);
}
