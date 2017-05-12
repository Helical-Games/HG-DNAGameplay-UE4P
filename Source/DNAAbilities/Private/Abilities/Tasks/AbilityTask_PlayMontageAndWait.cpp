// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GameFramework/Character.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"

UDNAAbilityTask_PlayMontageAndWait::UDNAAbilityTask_PlayMontageAndWait(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Rate = 1.f;
	bStopWhenAbilityEnds = true;
}

void UDNAAbilityTask_PlayMontageAndWait::OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	if (Ability && Ability->GetCurrentMontage() == MontageToPlay)
	{
		if (Montage == MontageToPlay)
		{
			DNAAbilitySystemComponent->ClearAnimatingAbility(Ability);

			// Reset AnimRootMotionTranslationScale
			ACharacter* Character = Cast<ACharacter>(GetAvatarActor());
			if (Character && (Character->Role == ROLE_Authority ||
							  (Character->Role == ROLE_AutonomousProxy && Ability->GetNetExecutionPolicy() == EDNAAbilityNetExecutionPolicy::LocalPredicted)))
			{
				Character->SetAnimRootMotionTranslationScale(1.f);
			}

		}
	}

	if (bInterrupted)
	{
		OnInterrupted.Broadcast();
	}
	else
	{
		OnBlendOut.Broadcast();
	}
}

void UDNAAbilityTask_PlayMontageAndWait::OnMontageInterrupted()
{
	if (StopPlayingMontage())
	{
		// Let the BP handle the interrupt as well
		OnInterrupted.Broadcast();
	}
}

void UDNAAbilityTask_PlayMontageAndWait::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!bInterrupted)
	{
		OnCompleted.Broadcast();
	}

	EndTask();
}

UDNAAbilityTask_PlayMontageAndWait* UDNAAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(UDNAAbility* OwningAbility,
	FName TaskInstanceName, UAnimMontage *MontageToPlay, float Rate, FName StartSection, bool bStopWhenAbilityEnds, float AnimRootMotionTranslationScale)
{

	UDNAAbilitySystemGlobals::NonShipping_ApplyGlobalAbilityScaler_Rate(Rate);

	UDNAAbilityTask_PlayMontageAndWait* MyObj = NewDNAAbilityTask<UDNAAbilityTask_PlayMontageAndWait>(OwningAbility, TaskInstanceName);
	MyObj->MontageToPlay = MontageToPlay;
	MyObj->Rate = Rate;
	MyObj->StartSection = StartSection;
	MyObj->AnimRootMotionTranslationScale = AnimRootMotionTranslationScale;
	MyObj->bStopWhenAbilityEnds = bStopWhenAbilityEnds;

	return MyObj;
}

void UDNAAbilityTask_PlayMontageAndWait::Activate()
{
	if (Ability == nullptr)
	{
		return;
	}

	bool bPlayedMontage = false;

	if (DNAAbilitySystemComponent)
	{
		const FDNAAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
		UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();
		if (AnimInstance != nullptr)
		{
			if (DNAAbilitySystemComponent->PlayMontage(Ability, Ability->GetCurrentActivationInfo(), MontageToPlay, Rate, StartSection) > 0.f)
			{
				// Playing a montage could potentially fire off a callback into game code which could kill this ability! Early out if we are  pending kill.
				if (IsPendingKill())
				{
					OnCancelled.Broadcast();
					return;
				}

				InterruptedHandle = Ability->OnDNAAbilityCancelled.AddUObject(this, &UDNAAbilityTask_PlayMontageAndWait::OnMontageInterrupted);

				BlendingOutDelegate.BindUObject(this, &UDNAAbilityTask_PlayMontageAndWait::OnMontageBlendingOut);
				AnimInstance->Montage_SetBlendingOutDelegate(BlendingOutDelegate, MontageToPlay);

				MontageEndedDelegate.BindUObject(this, &UDNAAbilityTask_PlayMontageAndWait::OnMontageEnded);
				AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, MontageToPlay);

				ACharacter* Character = Cast<ACharacter>(GetAvatarActor());
				if (Character && (Character->Role == ROLE_Authority ||
								  (Character->Role == ROLE_AutonomousProxy && Ability->GetNetExecutionPolicy() == EDNAAbilityNetExecutionPolicy::LocalPredicted)))
				{
					Character->SetAnimRootMotionTranslationScale(AnimRootMotionTranslationScale);
				}

				bPlayedMontage = true;
			}
		}
		else
		{
			ABILITY_LOG(Warning, TEXT("UDNAAbilityTask_PlayMontageAndWait call to PlayMontage failed!"));
		}
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UDNAAbilityTask_PlayMontageAndWait called on invalid DNAAbilitySystemComponent"));
	}

	if (!bPlayedMontage)
	{
		ABILITY_LOG(Warning, TEXT("UDNAAbilityTask_PlayMontageAndWait called in Ability %s failed to play montage %s; Task Instance Name %s."), *Ability->GetName(), *GetNameSafe(MontageToPlay),*InstanceName.ToString());
		OnCancelled.Broadcast();
	}

	SetWaitingOnAvatar();
}

void UDNAAbilityTask_PlayMontageAndWait::ExternalCancel()
{
	check(DNAAbilitySystemComponent);

	OnCancelled.Broadcast();
	Super::ExternalCancel();
}

void UDNAAbilityTask_PlayMontageAndWait::OnDestroy(bool AbilityEnded)
{
	// Note: Clearing montage end delegate isn't necessary since its not a multicast and will be cleared when the next montage plays.
	// (If we are destroyed, it will detect this and not do anything)

	// This delegate, however, should be cleared as it is a multicast
	if (Ability)
	{
		Ability->OnDNAAbilityCancelled.Remove(InterruptedHandle);
		if (AbilityEnded && bStopWhenAbilityEnds)
		{
			StopPlayingMontage();
		}
	}

	Super::OnDestroy(AbilityEnded);

}

bool UDNAAbilityTask_PlayMontageAndWait::StopPlayingMontage()
{
	const FDNAAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	if (!ActorInfo)
	{
		return false;
	}

	UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();
	if (AnimInstance == nullptr)
	{
		return false;
	}

	// Check if the montage is still playing
	// The ability would have been interrupted, in which case we should automatically stop the montage
	if (DNAAbilitySystemComponent && Ability)
	{
		if (DNAAbilitySystemComponent->GetAnimatingAbility() == Ability
			&& DNAAbilitySystemComponent->GetCurrentMontage() == MontageToPlay)
		{
			// Unbind delegates so they don't get called as well
			FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(MontageToPlay);
			if (MontageInstance)
			{
				MontageInstance->OnMontageBlendingOutStarted.Unbind();
				MontageInstance->OnMontageEnded.Unbind();
			}

			DNAAbilitySystemComponent->CurrentMontageStop();
			return true;
		}
	}

	return false;
}

FString UDNAAbilityTask_PlayMontageAndWait::GetDebugString() const
{
	UAnimMontage* PlayingMontage = nullptr;
	if (Ability)
	{
		const FDNAAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
		UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();

		if (AnimInstance != nullptr)
		{
			PlayingMontage = AnimInstance->Montage_IsActive(MontageToPlay) ? MontageToPlay : AnimInstance->GetCurrentActiveMontage();
		}
	}

	return FString::Printf(TEXT("PlayMontageAndWait. MontageToPlay: %s  (Currently Playing): %s"), *GetNameSafe(MontageToPlay), *GetNameSafe(PlayingMontage));
}
