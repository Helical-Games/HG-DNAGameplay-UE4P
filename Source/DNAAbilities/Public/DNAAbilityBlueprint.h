// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "Engine/Blueprint.h"
#include "DNAAbilityBlueprint.generated.h"

/**
 * A DNA Ability Blueprint is essentially a specialized Blueprint whose graphs control a DNA ability.
 *	NOTE: This feature is EXPERIMENTAL. Use at your own risk!
 */

UCLASS(BlueprintType)
class DNAABILITIES_API UDNAAbilityBlueprint : public UBlueprint
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITOR

	// UBlueprint interface
	virtual bool SupportedByDefaultBlueprintFactory() const override
	{
		return false;
	}
	// End of UBlueprint interface

	/** Returns the most base DNA ability blueprint for a given blueprint (if it is inherited from another ability blueprint, returning null if only native / non-ability BP classes are it's parent) */
	static UDNAAbilityBlueprint* FindRootDNAAbilityBlueprint(UDNAAbilityBlueprint* DerivedBlueprint);

#endif
};
