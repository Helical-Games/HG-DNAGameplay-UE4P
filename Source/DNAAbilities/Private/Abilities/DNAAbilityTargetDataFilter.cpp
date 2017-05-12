// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/DNAAbilityTargetDataFilter.h"

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	FDNATargetDataFilter
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

void FDNATargetDataFilter::InitializeFilterContext(AActor* FilterActor)
{
	SelfActor = FilterActor;
}
