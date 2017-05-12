// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "CollisionQueryParams.h"
#include "WorldCollision.h"
#include "Abilities/DNAAbilityTargetActor_Trace.h"
#include "DNAAbilityTargetActor_GroundTrace.generated.h"

class UDNAAbility;

UCLASS(Blueprintable)
class DNAABILITIES_API ADNAAbilityTargetActor_GroundTrace : public ADNAAbilityTargetActor_Trace
{
	GENERATED_UCLASS_BODY()

	virtual void StartTargeting(UDNAAbility* InAbility) override;

	/** Radius for a sphere or capsule. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = true), Category = Targeting)
	float CollisionRadius;

	/** Height for a capsule. Implicitly indicates a capsule is desired if this is greater than zero. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = true), Category = Targeting)
	float CollisionHeight;

	//TODO: Consider adding the ability to rotate the capsule with a supplied quaternion.

protected:
	virtual FHitResult PerformTrace(AActor* InSourceActor) override;

	virtual bool IsConfirmTargetingAllowed() override;

	bool AdjustCollisionResultForShape(const FVector OriginalStartPoint, const FVector OriginalEndPoint, const FCollisionQueryParams Params, FHitResult& OutHitResult) const;

	FCollisionShape CollisionShape;
	float CollisionHeightOffset;		//When tracing, give this much extra height to avoid start-in-ground problems. Dealing with thick placement actors while standing near walls may be trickier.
	bool bLastTraceWasGood;
};
