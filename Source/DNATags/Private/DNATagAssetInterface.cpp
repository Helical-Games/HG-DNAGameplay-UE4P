// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNATagAssetInterface.h"

UDNATagAssetInterface::UDNATagAssetInterface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool IDNATagAssetInterface::HasMatchingDNATag(FDNATag TagToCheck) const
{
	FDNATagContainer OwnedTags;
	GetOwnedDNATags(OwnedTags);

	return OwnedTags.HasTag(TagToCheck);
}

bool IDNATagAssetInterface::HasAllMatchingDNATags(const FDNATagContainer& TagContainer) const
{
	FDNATagContainer OwnedTags;
	GetOwnedDNATags(OwnedTags);

	return OwnedTags.HasAll(TagContainer);
}

bool IDNATagAssetInterface::HasAnyMatchingDNATags(const FDNATagContainer& TagContainer) const
{
	FDNATagContainer OwnedTags;
	GetOwnedDNATags(OwnedTags);

	return OwnedTags.HasAny(TagContainer);
}
