// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNACueNotify_HitImpact.h"
#include "Kismet/DNAStatics.h"

UDNACueNotify_HitImpact::UDNACueNotify_HitImpact(const FObjectInitializer& PCIP)
: Super(PCIP)
{

}

bool UDNACueNotify_HitImpact::HandlesEvent(EDNACueEvent::Type EventType) const
{
	return (EventType == EDNACueEvent::Executed);
}

void UDNACueNotify_HitImpact::HandleDNACue(AActor* Self, EDNACueEvent::Type EventType, const FDNACueParameters& Parameters)
{
	check(EventType == EDNACueEvent::Executed);
	check(Self);
	
	const FHitResult* HitResult = Parameters.EffectContext.GetHitResult();
	if (HitResult)
	{
		if (ParticleSystem)
		{
			UDNAStatics::SpawnEmitterAtLocation(Self, ParticleSystem, HitResult->ImpactPoint, HitResult->ImpactNormal.Rotation(), true);
		}
	}
	else
	{
		if (ParticleSystem)
		{
			UDNAStatics::SpawnEmitterAtLocation(Self, ParticleSystem, Self->GetActorLocation(), Self->GetActorRotation(), true);
		}
	}
}
