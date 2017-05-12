// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "DNACueNotify_Static.h"
#include "DNACueNotify_HitImpact.generated.h"

class UParticleSystem;
class USoundBase;

/**
 *	Non instanced DNACueNotify for spawning particle and sound FX.
 *	Still WIP - needs to be fleshed out more.
 */

UCLASS()
class DNAABILITIES_API UDNACueNotify_HitImpact : public UDNACueNotify_Static
{
	GENERATED_UCLASS_BODY()

	/** Does this DNACueNotify handle this type of DNACueEvent? */
	virtual bool HandlesEvent(EDNACueEvent::Type EventType) const override;
	
	virtual void HandleDNACue(AActor* MyTarget, EDNACueEvent::Type EventType, const FDNACueParameters& Parameters) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = DNACue)
	USoundBase* Sound;

	/** Effects to play for weapon attacks against specific surfaces */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = DNACue)
	UParticleSystem* ParticleSystem;
};
