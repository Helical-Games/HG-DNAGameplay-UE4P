// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "Abilities/DNAAbilityWorldReticle.h"

#include "DNAAbilityWorldReticle_ActorVisualization.generated.h"

class ADNAAbilityTargetActor;
class UMaterialInterface;

/** This is a dummy reticle for internal use by visualization placement tasks. It builds a custom visual model of the visualization being placed. */
UCLASS(notplaceable)
class ADNAAbilityWorldReticle_ActorVisualization : public ADNAAbilityWorldReticle
{
	GENERATED_UCLASS_BODY()

public:
	void InitializeReticleVisualizationInformation(ADNAAbilityTargetActor* InTargetingActor, AActor* VisualizationActor, UMaterialInterface *VisualizationMaterial);

private:
	/** Hardcoded collision component, so other objects don't think they can collide with the visualization actor */
	DEPRECATED_FORGAME(4.6, "CollisionComponent should not be accessed directly, please use GetCollisionComponent() function instead. CollisionComponent will soon be private and your code will not compile.")
	UPROPERTY()
	class UCapsuleComponent* CollisionComponent;
public:

	UPROPERTY()
	TArray<UActorComponent*> VisualizationComponents;

	/** Overridable function called whenever this actor is being removed from a level */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Returns CollisionComponent subobject **/
	DNAABILITIES_API class UCapsuleComponent* GetCollisionComponent();
};
