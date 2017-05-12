// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "UObject/Interface.h"
#include "DNATagContainer.h"
#include "DNATagAssetInterface.generated.h"

/** Interface for assets which contain DNA tags */
UINTERFACE(Blueprintable, MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UDNATagAssetInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class DNATAGS_API IDNATagAssetInterface
{
	GENERATED_IINTERFACE_BODY()

	/**
	 * Get any owned DNA tags on the asset
	 * 
	 * @param OutTags	[OUT] Set of tags on the asset
	 */
	 UFUNCTION(BlueprintCallable, Category = DNATags)
	virtual void GetOwnedDNATags(FDNATagContainer& TagContainer) const=0;

	/**
	 * Check if the asset has a DNA tag that matches against the specified tag (expands to include parents of asset tags)
	 * 
	 * @param TagToCheck	Tag to check for a match
	 * 
	 * @return True if the asset has a DNA tag that matches, false if not
	 */
	UFUNCTION(BlueprintCallable, Category=DNATags)
	virtual bool HasMatchingDNATag(FDNATag TagToCheck) const;

	/**
	 * Check if the asset has DNA tags that matches against all of the specified tags (expands to include parents of asset tags)
	 * 
	 * @param TagContainer			Tag container to check for a match
	 * 
	 * @return True if the asset has matches all of the DNA tags, will be true if container is empty
	 */
	UFUNCTION(BlueprintCallable, Category=DNATags)
	virtual bool HasAllMatchingDNATags(const FDNATagContainer& TagContainer) const;

	/**
	 * Check if the asset has DNA tags that matches against any of the specified tags (expands to include parents of asset tags)
	 * 
	 * @param TagContainer			Tag container to check for a match
	 * 
	 * @return True if the asset has matches any of the DNA tags, will be false if container is empty
	 */
	UFUNCTION(BlueprintCallable, Category=DNATags)
	virtual bool HasAnyMatchingDNATags(const FDNATagContainer& TagContainer) const;
};

