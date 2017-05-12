// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "DNAAbilitySpec.h"
#include "Abilities/DNAAbility.h"
#include "DNAAbility_CharacterJump.generated.h"

/**
 *	Ability that jumps with a character.
 */
UCLASS()
class DNAABILITIES_API UDNAAbility_CharacterJump : public UDNAAbility
{
	GENERATED_UCLASS_BODY()

public:

	virtual bool CanActivateAbility(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNATagContainer* SourceTags = nullptr, const FDNATagContainer* TargetTags = nullptr, OUT FDNATagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* OwnerInfo, const FDNAAbilityActivationInfo ActivationInfo, const FDNAEventData* TriggerEventData) override;

	virtual void InputReleased(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo) override;

	virtual void CancelAbility(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility) override;
};
