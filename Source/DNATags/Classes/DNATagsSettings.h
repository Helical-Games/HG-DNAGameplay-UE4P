// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Misc/StringAssetReference.h"
#include "DNATagsManager.h"
#include "DNATagsSettings.generated.h"

/** A single redirect from a deleted tag to the new tag that should replace it */
USTRUCT()
struct DNATAGS_API FDNATagRedirect
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = DNATags)
	FName OldTagName;

	UPROPERTY(EditAnywhere, Category = DNATags)
	FName NewTagName;

	friend inline bool operator==(const FDNATagRedirect& A, const FDNATagRedirect& B)
	{
		return A.OldTagName == B.OldTagName && A.NewTagName == B.NewTagName;
	}
};

/** Base class for storing a list of DNA tags as an ini list. This is used for both the central list and additional lists */
UCLASS(config = DNATagsList, notplaceable)
class DNATAGS_API UDNATagsList : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Relative path to the ini file that is backing this list */
	UPROPERTY()
	FString ConfigFileName;

	/** List of tags saved to this file */
	UPROPERTY(config, EditAnywhere, Category = DNATags)
	TArray<FDNATagTableRow> DNATagList;

	/** Sorts tags alphabetically */
	void SortTags();
};

/**
 *	Class for importing DNATags directly from a config file.
 *	FDNATagsEditorModule::StartupModule adds this class to the Project Settings menu to be edited.
 *	Editing this in Project Settings will output changes to Config/DefaultDNATags.ini.
 *	
 *	Primary advantages of this approach are:
 *	-Adding new tags doesn't require checking out external and editing file (CSV or xls) then reimporting.
 *	-New tags are mergeable since .ini are text and non exclusive checkout.
 *	
 *	To do:
 *	-Better support could be added for adding new tags. We could match existing tags and autocomplete subtags as
 *	the user types (e.g, autocomplete 'Damage.Physical' as the user is adding a 'Damage.Physical.Slash' tag).
 *	
 */
UCLASS(config=DNATags, defaultconfig, notplaceable)
class DNATAGS_API UDNATagsSettings : public UDNATagsList
{
	GENERATED_UCLASS_BODY()

	/** If true, will import tags from ini files in the config/tags folder */
	UPROPERTY(config, EditAnywhere, Category = DNATags)
	bool ImportTagsFromConfig;

	/** If true, will give load warnings when reading invalid tags off disk */
	UPROPERTY(config, EditAnywhere, Category = DNATags)
	bool WarnOnInvalidTags;

	/** If true, will replicate DNA tags by index instead of name. For this to work, tags must be identical on client and server */
	UPROPERTY(config, EditAnywhere, Category = "Advanced Replication")
	bool FastReplication;

	/** List of data tables to load tags from */
	UPROPERTY(config, EditAnywhere, Category = DNATags, meta = (AllowedClasses = "DataTable"))
	TArray<FStringAssetReference> DNATagTableList;

	/** List of active tag redirects */
	UPROPERTY(config, EditAnywhere, Category = DNATags)
	TArray<FDNATagRedirect> DNATagRedirects;

	/** List of tags most frequently replicated */
	UPROPERTY(config, EditAnywhere, Category = "Advanced Replication")
	TArray<FName> CommonlyReplicatedTags;

	/** Numbers of bits to use for replicating container size, set this based on how large your containers tend to be */
	UPROPERTY(config, EditAnywhere, Category = "Advanced Replication")
	int32 NumBitsForContainerSize;

	/** The length in bits of the first segment when net serializing tags. We will serialize NetIndexFirstBitSegment + 1 bit to indicate "more", which is slower to replicate */
	UPROPERTY(config, EditAnywhere, Category= "Advanced Replication")
	int32 NetIndexFirstBitSegment;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

};

UCLASS(config=DNATags, notplaceable)
class DNATAGS_API UDNATagsDeveloperSettings : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Allows new tags to be saved into their own INI file. This is make merging easier for non technical developers by setting up their own ini file. */
	UPROPERTY(config, EditAnywhere, Category=DNATags)
	FString DeveloperConfigName;
};
