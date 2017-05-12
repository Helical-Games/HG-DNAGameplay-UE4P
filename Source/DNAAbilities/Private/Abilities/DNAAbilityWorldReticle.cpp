// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/DNAAbilityWorldReticle.h"
#include "GameFramework/PlayerController.h"

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	ADNAAbilityWorldReticle
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

ADNAAbilityWorldReticle::ADNAAbilityWorldReticle(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	bIsTargetValid = true;
	bIsTargetAnActor = false;
	bFaceOwnerFlat = true;
}


void ADNAAbilityWorldReticle::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FaceTowardSource(bFaceOwnerFlat);
}

void ADNAAbilityWorldReticle::InitializeReticle(AActor* InTargetingActor, APlayerController* PlayerController, FWorldReticleParameters InParameters)
{
	check(InTargetingActor);
	TargetingActor = InTargetingActor;
	MasterPC = PlayerController;
	AddTickPrerequisiteActor(TargetingActor);		//We want the reticle to tick after the targeting actor so that designers have the final say on the position
	Parameters = InParameters;
	OnParametersInitialized();
}

bool ADNAAbilityWorldReticle::IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const
{
	//The player who created the ability doesn't need to be updated about it - there should be local prediction in place.
	if (RealViewer == MasterPC)
	{
		return false;
	}

	return Super::IsNetRelevantFor(RealViewer, ViewTarget, SrcLocation);
}

void ADNAAbilityWorldReticle::SetIsTargetValid(bool bNewValue)
{
	if (bIsTargetValid != bNewValue)
	{
		bIsTargetValid = bNewValue;
		OnValidTargetChanged(bNewValue);
	}
}

void ADNAAbilityWorldReticle::SetIsTargetAnActor(bool bNewValue)
{
	if (bIsTargetAnActor != bNewValue)
	{
		bIsTargetAnActor = bNewValue;
		OnTargetingAnActor(bNewValue);
	}
}

void ADNAAbilityWorldReticle::FaceTowardSource(bool bFaceIn2D)
{
	if (TargetingActor)
	{
		if (bFaceIn2D)
		{
			FVector FacingVector = (TargetingActor->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
			if (FacingVector.IsZero())
			{
				FacingVector = -GetActorForwardVector().GetSafeNormal2D();
			}
			if (!FacingVector.IsZero())
			{
				SetActorRotation(FacingVector.Rotation());
			}
		}
		else
		{
			FVector FacingVector = (TargetingActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
			if (FacingVector.IsZero())
			{
				FacingVector = -GetActorForwardVector().GetSafeNormal();
			}
			SetActorRotation(FacingVector.Rotation());
		}
	}
}
