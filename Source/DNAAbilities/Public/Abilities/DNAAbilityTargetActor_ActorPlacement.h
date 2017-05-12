// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "Abilities/DNAAbilityTargetActor_GroundTrace.h"
#include "DNAAbilityTargetActor_ActorPlacement.generated.h"

class ADNAAbilityWorldReticle_ActorVisualization;
class UDNAAbility;

UCLASS(Blueprintable)
class ADNAAbilityTargetActor_ActorPlacement : public ADNAAbilityTargetActor_GroundTrace
{
	GENERATED_UCLASS_BODY()

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void StartTargeting(UDNAAbility* InAbility) override;

	/** Actor we intend to place. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = true), Category = Targeting)
	UClass* PlacedActorClass;		//Using a special class for replication purposes. (Not implemented yet)
	
	/** Override Material 0 on our placed actor's meshes with this material for visualization. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = true), Category = Projectile)
	UMaterialInterface *PlacedActorMaterial;

	/** Visualization for the intended location of the placed actor. */
	TWeakObjectPtr<ADNAAbilityWorldReticle_ActorVisualization> ActorVisualizationReticle;
};
