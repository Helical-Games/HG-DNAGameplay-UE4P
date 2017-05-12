// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "DNAEffect.h"
#include "DNAEffectTemplate.generated.h"

/**
 *	The idea here is that UDNAEffectTemplates are templates created by system designers for commonly used
 *	DNAEffects. For example: StandardPhysicalDamage, StandardSnare, etc. 
 *	
 *	This allows for A) Less experienced designers to create new ones based on the templates B) Easier/quicker creation
 *	of DNAEffects: don't have to worry about forgeting a tag or stacking rule, etc.
 *	
 *	The template provides: 
 *		-Default values
 *		-What properties are editable by default
 *		
 *	Default values are simply the default values of the UDNAEffectTemplate (since it is a UDNAEffect). Whatever values
 *	are different than CDO values will be copied over when making a new DNAEffect from a template.
 *	
 *	EditableProperties describes what properties should be exposed by default when editing a templated DNAeffect. This is just
 *	a string list. Properties whose name match strings in that list will be shown, the rest will be hidden. All properties can always be
 *	seen/edited by just checking "ShowAllProperties" on the DNAEffect itself. 
 *	
 *	
 */
UCLASS()
class UDNAEffectTemplate : public UDNAEffect
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITORONLY_DATA
	/** Names of the properties that should be exposed in the limited/basic view when editing a DNAEffect based on this template */
	UPROPERTY(EditDefaultsOnly, Category="Template")
	TArray<FString>	EditableProperties;
#endif
};
