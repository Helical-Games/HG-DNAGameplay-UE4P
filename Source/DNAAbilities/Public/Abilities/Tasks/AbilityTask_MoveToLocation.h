// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_MoveToLocation.generated.h"

class UCurveFloat;
class UCurveVector;
class UDNATasksComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMoveToLocationDelegate);

class AActor;
class UPrimitiveComponent;
class UDNATasksComponent;

/**
 *	TODO:
 *		-Implement replicated time so that this can work as a simulated task for Join In Prgorss clients.
 */


/**
 *	Move to a location, ignoring clipping, over a given length of time. Ends when the TargetLocation is reached.
 *	This will RESET your character's current movement mode! If you wish to maintain PHYS_Flying or PHYS_Custom, you must
 *	reset it on completion.!
 */
UCLASS(MinimalAPI)
class UDNAAbilityTask_MoveToLocation : public UDNAAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FMoveToLocationDelegate		OnTargetLocationReached;

	virtual void InitSimulatedTask(UDNATasksComponent& InDNATasksComponent) override;

	/** Move to the specified location, using the vector curve (range 0 - 1) if specified, otherwise the float curve (range 0 - 1) or fallback to linear interpolation */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UDNAAbilityTask_MoveToLocation* MoveToLocation(UDNAAbility* OwningAbility, FName TaskInstanceName, FVector Location, float Duration, UCurveFloat* OptionalInterpolationCurve, UCurveVector* OptionalVectorInterpolationCurve);

	virtual void Activate() override;

	/** Tick function for this task, if bTickingTask == true */
	virtual void TickTask(float DeltaTime) override;

	virtual void OnDestroy(bool AbilityIsEnding) override;

protected:

	bool bIsFinished;

	UPROPERTY(Replicated)
	FVector StartLocation;

	//FVector 
	
	UPROPERTY(Replicated)
	FVector TargetLocation;

	UPROPERTY(Replicated)
	float DurationOfMovement;


	float TimeMoveStarted;

	float TimeMoveWillEnd;

	UPROPERTY(Replicated)
	UCurveFloat* LerpCurve;

	UPROPERTY(Replicated)
	UCurveVector* LerpCurveVector;

	
};
