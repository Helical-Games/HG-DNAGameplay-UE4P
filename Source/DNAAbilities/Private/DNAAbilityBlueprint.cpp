// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNAAbilityBlueprint.h"

//////////////////////////////////////////////////////////////////////////
// UDNAAbilityBlueprint

UDNAAbilityBlueprint::UDNAAbilityBlueprint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR

/** Returns the most base DNA ability blueprint for a given blueprint (if it is inherited from another ability blueprint, returning null if only native / non-ability BP classes are it's parent) */
UDNAAbilityBlueprint* UDNAAbilityBlueprint::FindRootDNAAbilityBlueprint(UDNAAbilityBlueprint* DerivedBlueprint)
{
	UDNAAbilityBlueprint* ParentBP = NULL;

	// Determine if there is a DNA ability blueprint in the ancestry of this class
	for (UClass* ParentClass = DerivedBlueprint->ParentClass; ParentClass != UObject::StaticClass(); ParentClass = ParentClass->GetSuperClass())
	{
		if (UDNAAbilityBlueprint* TestBP = Cast<UDNAAbilityBlueprint>(ParentClass->ClassGeneratedBy))
		{
			ParentBP = TestBP;
		}
	}

	return ParentBP;
}

#endif
