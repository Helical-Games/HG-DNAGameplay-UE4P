// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/DNAAbility_Montage.h"
#include "Animation/AnimInstance.h"
#include "AbilitySystemComponent.h"

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	UDNAAbility_Montage
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

UDNAAbility_Montage::UDNAAbility_Montage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PlayRate = 1.f;
	NetExecutionPolicy = EDNAAbilityNetExecutionPolicy::ServerInitiated;
}

void UDNAAbility_Montage::ActivateAbility(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo, const FDNAEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		return;
	}

	UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();

	if (MontageToPlay != nullptr && AnimInstance != nullptr && AnimInstance->GetActiveMontageInstance() == nullptr)
	{
		TArray<FActiveDNAEffectHandle>	AppliedEffects;

		// Apply DNAEffects
		TArray<const UDNAEffect*> Effects;
		GetDNAEffectsWhileAnimating(Effects);
		for (const UDNAEffect* Effect : Effects)
		{
			FActiveDNAEffectHandle EffectHandle = ActorInfo->DNAAbilitySystemComponent->ApplyDNAEffectToSelf(Effect, 1.f, MakeEffectContext(Handle, ActorInfo));
			if (EffectHandle.IsValid())
			{
				AppliedEffects.Add(EffectHandle);
			}
		}

		float const Duration = AnimInstance->Montage_Play(MontageToPlay, PlayRate);

		FOnMontageEnded EndDelegate;
		EndDelegate.BindUObject(this, &UDNAAbility_Montage::OnMontageEnded, ActorInfo->DNAAbilitySystemComponent, AppliedEffects);
		AnimInstance->Montage_SetEndDelegate(EndDelegate);

		if (SectionName != NAME_None)
		{
			AnimInstance->Montage_JumpToSection(SectionName);
		}
	}
}

void UDNAAbility_Montage::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted, TWeakObjectPtr<UDNAAbilitySystemComponent> DNAAbilitySystemComponent, TArray<FActiveDNAEffectHandle> AppliedEffects)
{
	// Remove any DNAEffects that we applied
	if (DNAAbilitySystemComponent.IsValid())
	{
		for (FActiveDNAEffectHandle Handle : AppliedEffects)
		{
			DNAAbilitySystemComponent->RemoveActiveDNAEffect(Handle);
		}
	}
}

void UDNAAbility_Montage::GetDNAEffectsWhileAnimating(TArray<const UDNAEffect*>& OutEffects) const
{
	OutEffects.Append(DNAEffectsWhileAnimating);

	for ( TSubclassOf<UDNAEffect> EffectClass : DNAEffectClassesWhileAnimating )
	{
		if ( EffectClass )
		{
			OutEffects.Add(EffectClass->GetDefaultObject<UDNAEffect>());
		}
	}
}
