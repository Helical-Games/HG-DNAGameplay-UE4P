// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"
#include "AbilityTask.h"
#include "AbilityTask_ApplyRootMotion_Base.generated.h"

class UCharacterMovementComponent;

UCLASS(MinimalAPI)
class UDNAAbilityTask_ApplyRootMotion_Base : public UDNAAbilityTask
{
	GENERATED_UCLASS_BODY()

	virtual void InitSimulatedTask(UDNATasksComponent& InDNATasksComponent) override;

protected:

	virtual void SharedInitAndApply() {};

	void SetFinishVelocity(FName RootMotionSourceName, FVector FinishVelocity);
	void ClampFinishVelocity(FName RootMotionSourceName, float VelocityClamp);

protected:

	UPROPERTY(Replicated)
	FName ForceName;

	UPROPERTY()
	UCharacterMovementComponent* MovementComponent; 
	
	uint16 RootMotionSourceID;

	bool bIsFinished;

	float StartTime;
	float EndTime;
};