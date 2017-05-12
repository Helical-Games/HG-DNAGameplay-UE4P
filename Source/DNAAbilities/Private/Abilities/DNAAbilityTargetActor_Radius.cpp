// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/DNAAbilityTargetActor_Radius.h"
#include "GameFramework/Pawn.h"
#include "WorldCollision.h"
#include "Abilities/DNAAbility.h"

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	ADNAAbilityTargetActor_Radius
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

ADNAAbilityTargetActor_Radius::ADNAAbilityTargetActor_Radius(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	ShouldProduceTargetDataOnServer = true;
}

void ADNAAbilityTargetActor_Radius::StartTargeting(UDNAAbility* InAbility)
{
	Super::StartTargeting(InAbility);
	SourceActor = InAbility->GetCurrentActorInfo()->AvatarActor.Get();
}

void ADNAAbilityTargetActor_Radius::ConfirmTargetingAndContinue()
{
	check(ShouldProduceTargetData());
	if (SourceActor)
	{
		FVector Origin = StartLocation.GetTargetingTransform().GetLocation();
		FDNAAbilityTargetDataHandle Handle = MakeTargetData(PerformOverlap(Origin), Origin);
		TargetDataReadyDelegate.Broadcast(Handle);
	}
}

FDNAAbilityTargetDataHandle ADNAAbilityTargetActor_Radius::MakeTargetData(const TArray<TWeakObjectPtr<AActor>>& Actors, const FVector& Origin) const
{
	if (OwningAbility)
	{
		/** Use the source location instead of the literal origin */
		return StartLocation.MakeTargetDataHandleFromActors(Actors, false);
	}

	return FDNAAbilityTargetDataHandle();
}

TArray<TWeakObjectPtr<AActor> >	ADNAAbilityTargetActor_Radius::PerformOverlap(const FVector& Origin)
{
	static FName RadiusTargetingOverlap = FName(TEXT("RadiusTargetingOverlap"));
	bool bTraceComplex = false;
	
	FCollisionQueryParams Params(RadiusTargetingOverlap, bTraceComplex);
	Params.bReturnPhysicalMaterial = false;
	Params.bTraceAsyncScene = false;

	TArray<FOverlapResult> Overlaps;

	SourceActor->GetWorld()->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(Radius), Params);

	TArray<TWeakObjectPtr<AActor>>	HitActors;

	for (int32 i = 0; i < Overlaps.Num(); ++i)
	{
		//Should this check to see if these pawns are in the AimTarget list?
		APawn* PawnActor = Cast<APawn>(Overlaps[i].GetActor());
		if (PawnActor && !HitActors.Contains(PawnActor) && Filter.FilterPassesForActor(PawnActor))
		{
			HitActors.Add(PawnActor);
		}
	}

	return HitActors;
}
