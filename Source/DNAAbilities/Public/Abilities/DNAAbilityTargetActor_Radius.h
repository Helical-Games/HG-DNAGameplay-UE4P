// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "Abilities/DNAAbilityTargetTypes.h"
#include "Abilities/DNAAbilityTargetActor.h"
#include "DNAAbilityTargetActor_Radius.generated.h"

class UDNAAbility;

/** Selects everything within a given radius of the source actor. */
UCLASS(Blueprintable, notplaceable)
class DNAABILITIES_API ADNAAbilityTargetActor_Radius : public ADNAAbilityTargetActor
{
	GENERATED_UCLASS_BODY()

public:

	virtual void StartTargeting(UDNAAbility* Ability) override;
	
	virtual void ConfirmTargetingAndContinue() override;

	/** Radius of target acquisition around the ability's start location. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = true), Category = Radius)
	float Radius;

protected:

	TArray<TWeakObjectPtr<AActor> >	PerformOverlap(const FVector& Origin);

	FDNAAbilityTargetDataHandle MakeTargetData(const TArray<TWeakObjectPtr<AActor>>& Actors, const FVector& Origin) const;

};
