// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "Abilities/DNAAbilityTargetActor_Trace.h"
#include "DNAAbilityTargetActor_SingleLineTrace.generated.h"

UCLASS(Blueprintable)
class DNAABILITIES_API ADNAAbilityTargetActor_SingleLineTrace : public ADNAAbilityTargetActor_Trace
{
	GENERATED_UCLASS_BODY()

protected:
	virtual FHitResult PerformTrace(AActor* InSourceActor) override;
};
