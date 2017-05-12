// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "CollisionQueryParams.h"
#include "WorldCollision.h"
#include "Engine/CollisionProfile.h"
#include "Abilities/DNAAbilityTargetTypes.h"
#include "Abilities/DNAAbilityTargetDataFilter.h"
#include "Abilities/DNAAbilityTargetActor.h"
#include "DNAAbilityTargetActor_Trace.generated.h"

class UDNAAbility;

/** Intermediate base class for all line-trace type targeting actors. */
UCLASS(Abstract, Blueprintable, notplaceable, config=Game)
class DNAABILITIES_API ADNAAbilityTargetActor_Trace : public ADNAAbilityTargetActor
{
	GENERATED_UCLASS_BODY()

public:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Traces as normal, but will manually filter all hit actors */
	static void LineTraceWithFilter(FHitResult& OutHitResult, const UWorld* World, const FDNATargetDataFilterHandle FilterHandle, const FVector& Start, const FVector& End, FName ProfileName, const FCollisionQueryParams Params);

	/** Sweeps as normal, but will manually filter all hit actors */
	static void SweepWithFilter(FHitResult& OutHitResult, const UWorld* World, const FDNATargetDataFilterHandle FilterHandle, const FVector& Start, const FVector& End, const FQuat& Rotation, const FCollisionShape CollisionShape, FName ProfileName, const FCollisionQueryParams Params);

	void AimWithPlayerController(const AActor* InSourceActor, FCollisionQueryParams Params, const FVector& TraceStart, FVector& OutTraceEnd, bool bIgnorePitch = false) const;

	static bool ClipCameraRayToAbilityRange(FVector CameraLocation, FVector CameraDirection, FVector AbilityCenter, float AbilityRange, FVector& ClippedPosition);

	virtual void StartTargeting(UDNAAbility* Ability) override;

	virtual void ConfirmTargetingAndContinue() override;

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = true), Category = Trace)
	float MaxRange;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, config, meta = (ExposeOnSpawn = true), Category = Trace)
	FCollisionProfileName TraceProfile;

	// Does the trace affect the aiming pitch
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = true), Category = Trace)
	bool bTraceAffectsAimPitch;

protected:
	virtual FHitResult PerformTrace(AActor* InSourceActor) PURE_VIRTUAL(ADNAAbilityTargetActor_Trace, return FHitResult(););

	FDNAAbilityTargetDataHandle MakeTargetData(const FHitResult& HitResult) const;

	TWeakObjectPtr<ADNAAbilityWorldReticle> ReticleActor;
};
