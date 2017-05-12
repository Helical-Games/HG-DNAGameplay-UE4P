// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_DNAAbilitiesBlueprint.h"
#include "DNAModEvaluationChannelSettingsDetails.h"
#include "DetailWidgetRow.h"
#include "DNAEffectTypes.h"
#include "AbilitySystemGlobals.h"
#include "IDetailChildrenBuilder.h"

#define LOCTEXT_NAMESPACE "DNAEffectExecutionScopedModifierInfoDetailsCustomization"

TSharedRef<IPropertyTypeCustomization> FDNAModEvaluationChannelSettingsDetails::MakeInstance()
{
	return MakeShareable(new FDNAModEvaluationChannelSettingsDetails());
}

void FDNAModEvaluationChannelSettingsDetails::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	// See if the game even wants to allow DNA effect eval. channels in the first place
	bShouldBeVisible = UDNAAbilitySystemGlobals::Get().ShouldAllowDNAModEvaluationChannels();
	
	// Verify that a parent handle hasn't forcibly marked evaluation channels hidden
	if (bShouldBeVisible)
	{
		TSharedPtr<IPropertyHandle> ParentHandle = StructPropertyHandle->GetParentHandle();
		while (ParentHandle.IsValid() && ParentHandle->IsValidHandle())
		{
			const FString* Meta = ParentHandle->GetInstanceMetaData(FDNAModEvaluationChannelSettings::ForceHideMetadataKey);
			if (Meta && !Meta->IsEmpty())
			{
				bShouldBeVisible = false;
				break;
			}
			ParentHandle = ParentHandle->GetParentHandle();
		}
	}

	if (bShouldBeVisible)
	{
		HeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		];
	}
	else
	{
		StructPropertyHandle->MarkHiddenByCustomization();
	}
}

void FDNAModEvaluationChannelSettingsDetails::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	if (bShouldBeVisible && StructPropertyHandle->IsValidHandle())
	{
		TSharedPtr<IPropertyHandle> ChannelHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDNAModEvaluationChannelSettings, Channel));
		if (ChannelHandle.IsValid() && ChannelHandle->IsValidHandle())
		{
			StructBuilder.AddChildProperty(ChannelHandle.ToSharedRef());
		}
	}
}

#undef LOCTEXT_NAMESPACE
