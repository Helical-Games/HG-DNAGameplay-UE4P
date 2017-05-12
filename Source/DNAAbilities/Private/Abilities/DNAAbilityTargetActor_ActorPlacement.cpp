// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/DNAAbilityTargetActor_ActorPlacement.h"
#include "Engine/World.h"
#include "Abilities/DNAAbilityWorldReticle_ActorVisualization.h"

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	ADNAAbilityTargetActor_ActorPlacement
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

ADNAAbilityTargetActor_ActorPlacement::ADNAAbilityTargetActor_ActorPlacement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void ADNAAbilityTargetActor_ActorPlacement::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ActorVisualizationReticle.IsValid())
	{
		ActorVisualizationReticle.Get()->Destroy();
	}

	Super::EndPlay(EndPlayReason);
}

void ADNAAbilityTargetActor_ActorPlacement::StartTargeting(UDNAAbility* InAbility)
{
	Super::StartTargeting(InAbility);
	if (AActor *VisualizationActor = GetWorld()->SpawnActor(PlacedActorClass))
	{
		ActorVisualizationReticle = GetWorld()->SpawnActor<ADNAAbilityWorldReticle_ActorVisualization>();
		ActorVisualizationReticle->InitializeReticleVisualizationInformation(this, VisualizationActor, PlacedActorMaterial);
		GetWorld()->DestroyActor(VisualizationActor);
	}
	if (ADNAAbilityWorldReticle* CachedReticleActor = ReticleActor.Get())
	{
		ActorVisualizationReticle->AttachToActor(CachedReticleActor, FAttachmentTransformRules::KeepRelativeTransform);
	}
	else
	{
		ReticleActor = ActorVisualizationReticle;
		ActorVisualizationReticle = nullptr;
	}
}

//Might want to override this function to allow for a radius check against the ground, possibly including a height check. Or might want to do it in ground trace.
//FHitResult ADNAAbilityTargetActor_ActorPlacement::PerformTrace(AActor* InSourceActor) const
