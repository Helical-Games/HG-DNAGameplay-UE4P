// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "DNAAbilitySpec.h"
#include "DNAEffect.h"
#include "Abilities/DNAAbility.h"
#include "DNAAbility_Montage.generated.h"

class UDNAAbilitySystemComponent;
class UAnimMontage;

/**
 *	A DNA ability that plays a single montage and applies a DNAEffect
 */
UCLASS()
class DNAABILITIES_API UDNAAbility_Montage : public UDNAAbility
{
	GENERATED_UCLASS_BODY()

public:
	
	virtual void ActivateAbility(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* OwnerInfo, const FDNAAbilityActivationInfo ActivationInfo, const FDNAEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, Category = MontageAbility)
	UAnimMontage *	MontageToPlay;

	UPROPERTY(EditDefaultsOnly, Category = MontageAbility)
	float	PlayRate;

	UPROPERTY(EditDefaultsOnly, Category = MontageAbility)
	FName	SectionName;

	/** DNAEffects to apply and then remove while the animation is playing */
	UPROPERTY(EditDefaultsOnly, Category = MontageAbility)
	TArray<TSubclassOf<UDNAEffect>> DNAEffectClassesWhileAnimating;

	/** Deprecated. Use DNAEffectClassesWhileAnimating instead. */
	UPROPERTY(VisibleDefaultsOnly, Category = Deprecated)
	TArray<const UDNAEffect*>	DNAEffectsWhileAnimating;

	void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted, TWeakObjectPtr<UDNAAbilitySystemComponent> DNAAbilitySystemComponent, TArray<struct FActiveDNAEffectHandle>	AppliedEffects);

	void GetDNAEffectsWhileAnimating(TArray<const UDNAEffect*>& OutEffects) const;
};
