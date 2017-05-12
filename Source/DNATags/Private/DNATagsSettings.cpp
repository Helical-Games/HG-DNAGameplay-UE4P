// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNATagsSettings.h"
#include "DNATagsModule.h"

UDNATagsList::UDNATagsList(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// No config filename, needs to be set at creation time
}

void UDNATagsList::SortTags()
{
	DNATagList.Sort();
}

UDNATagsSettings::UDNATagsSettings(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	ConfigFileName = GetDefaultConfigFilename();
	ImportTagsFromConfig = false;
	WarnOnInvalidTags = true;
	FastReplication = false;
	NumBitsForContainerSize = 6;
	NetIndexFirstBitSegment = 16;
}

#if WITH_EDITOR
void UDNATagsSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.Property)
	{
		IDNATagsModule::OnTagSettingsChanged.Broadcast();
	}
}
#endif

// ---------------------------------

UDNATagsDeveloperSettings::UDNATagsDeveloperSettings(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	
}
