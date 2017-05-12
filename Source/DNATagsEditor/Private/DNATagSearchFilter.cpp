// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNATagsEditorModulePrivatePCH.h"
#include "DNATagSearchFilter.h"
#include "SDNATagWidget.h"
#include "DNATagsModule.h"
#include "Editor/ContentBrowser/Public/ContentBrowserModule.h"
#include "Editor/ContentBrowser/Public/FrontendFilterBase.h"

#define LOCTEXT_NAMESPACE "DNATagSearchFilter"

//////////////////////////////////////////////////////////////////////////
//

/** A filter that search for assets using a specific DNA tag */
class FFrontendFilter_DNATags : public FFrontendFilter
{
public:
	FFrontendFilter_DNATags(TSharedPtr<FFrontendFilterCategory> InCategory)
		: FFrontendFilter(InCategory)
	{
		TagContainer = MakeShareable(new FDNATagContainer);

		EditableContainers.Add(SDNATagWidget::FEditableDNATagContainerDatum(/*TagContainerOwner=*/ nullptr, TagContainer.Get()));
	}

	// FFrontendFilter implementation
	virtual FLinearColor GetColor() const override { return FLinearColor::Red; }
	virtual FString GetName() const override { return TEXT("DNATagFilter"); }
	virtual FText GetDisplayName() const override;
	virtual FText GetToolTipText() const override;
	virtual void ModifyContextMenu(FMenuBuilder& MenuBuilder) override;
	virtual void SaveSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString) const override;
	virtual void LoadSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString) override;
	// End of FFrontendFilter implementation

	// IFilter implementation
	virtual bool PassesFilter(FAssetFilterType InItem) const override;
	// End of IFilter implementation

protected:
	// Container of selected search tags (the asset is shown if *any* of these match)
	TSharedPtr<FDNATagContainer> TagContainer;

	// Adaptor for the SDNATagWidget to edit our tag container
	TArray<SDNATagWidget::FEditableDNATagContainerDatum> EditableContainers;

protected:
	bool ProcessStruct(void* Data, UStruct* Struct) const;

	bool ProcessProperty(void* Data, UProperty* Prop) const;

	void OnTagWidgetChanged();
};

void FFrontendFilter_DNATags::ModifyContextMenu(FMenuBuilder& MenuBuilder)
{
	FUIAction Action;

	MenuBuilder.BeginSection(TEXT("ComparsionSection"), LOCTEXT("ComparisonSectionHeading", "DNA Tag(s) to search for"));

	TSharedRef<SWidget> TagWidget =
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight(300)
		[
			SNew(SDNATagWidget, EditableContainers)
			.MultiSelect(true)
			.OnTagChanged_Raw(this, &FFrontendFilter_DNATags::OnTagWidgetChanged)
		];
 	MenuBuilder.AddWidget(TagWidget, FText::GetEmpty(), /*bNoIndent=*/ false);
}

FText FFrontendFilter_DNATags::GetDisplayName() const
{
	if (TagContainer->Num() == 0)
	{
		return LOCTEXT("AnyDNATagDisplayName", "DNA Tags");
	}
	else
	{
		FString QueryString;

		int32 Count = 0;
		for (const FDNATag& Tag : *TagContainer.Get())
		{
			if (Count > 0)
			{
				QueryString += TEXT(" | ");
			}

			QueryString += Tag.ToString();
			++Count;
		}


		return FText::Format(LOCTEXT("DNATagListDisplayName", "DNA Tags ({0})"), FText::AsCultureInvariant(QueryString));
	}
}

FText FFrontendFilter_DNATags::GetToolTipText() const
{
	if (TagContainer->Num() == 0)
	{
		return LOCTEXT("AnyDNATagFilterDisplayTooltip", "Search for any *loaded* Blueprint or asset that contains a DNA tag (right-click to choose tags).");
	}
	else
	{
		return LOCTEXT("DNATagFilterDisplayTooltip", "Search for any *loaded* Blueprint or asset that has a DNA tag which matches any of the selected tags (right-click to choose tags).");
	}
}

void FFrontendFilter_DNATags::SaveSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString) const
{
	TArray<FString> TagStrings;
	TagStrings.Reserve(TagContainer->Num());
	for (const FDNATag& Tag : *TagContainer.Get())
	{
		TagStrings.Add(Tag.GetTagName().ToString());
	}

	GConfig->SetArray(*IniSection, *(SettingsString + TEXT(".Tags")), TagStrings, IniFilename);
}

void FFrontendFilter_DNATags::LoadSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString)
{
	IDNATagsModule& DNATagModule = IDNATagsModule::Get();

	TArray<FString> TagStrings;
	GConfig->GetArray(*IniSection, *(SettingsString + TEXT(".Tags")), /*out*/ TagStrings, IniFilename);

	TagContainer->RemoveAllTags();
	for (const FString& TagString : TagStrings)
	{
		FDNATag NewTag = DNATagModule.RequestDNATag(*TagString, /*bErrorIfNotFound=*/ false);
		if (NewTag.IsValid())
		{
			TagContainer->AddTag(NewTag);
		}
	}
}

void FFrontendFilter_DNATags::OnTagWidgetChanged()
{
	BroadcastChangedEvent();
}

bool FFrontendFilter_DNATags::ProcessStruct(void* Data, UStruct* Struct) const
{
	for (TFieldIterator<UProperty> PropIt(Struct, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
	{
		UProperty* Prop = *PropIt;

		if (ProcessProperty(Data, Prop))
		{
			return true;
		}
	}

	return false;
}

bool FFrontendFilter_DNATags::ProcessProperty(void* Data, UProperty* Prop) const
{
	void* InnerData = Prop->ContainerPtrToValuePtr<void>(Data);

	if (UStructProperty* StructProperty = Cast<UStructProperty>(Prop))
	{
		if (StructProperty->Struct == FDNATag::StaticStruct())
		{
			FDNATag& ThisTag = *static_cast<FDNATag*>(InnerData);

			const bool bAnyTagIsOK = TagContainer->Num() == 0;
			const bool bPassesTagSearch = bAnyTagIsOK || TagContainer->HasTag(ThisTag, EDNATagMatchType::Explicit, EDNATagMatchType::IncludeParentTags);

			return bPassesTagSearch;
		}
		else
		{
			return ProcessStruct(InnerData, StructProperty->Struct);
		}
	}
	else if (UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Prop))
	{
		FScriptArrayHelper ArrayHelper(ArrayProperty, InnerData);
		for (int32 ArrayIndex = 0; ArrayIndex < ArrayHelper.Num(); ++ArrayIndex)
		{
			void* ArrayData = ArrayHelper.GetRawPtr(ArrayIndex);

			if (ProcessProperty(ArrayData, ArrayProperty->Inner))
			{
				return true;
			}
		}
	}

	return false;
}

bool FFrontendFilter_DNATags::PassesFilter(FAssetFilterType InItem) const
{
	if (InItem.IsAssetLoaded())
	{
		if (UObject* Object = InItem.GetAsset())
		{
			if (UBlueprint* Blueprint = Cast<UBlueprint>(Object))
			{
				return ProcessStruct(Blueprint->GeneratedClass->GetDefaultObject(), Blueprint->GeneratedClass);

				//@TODO: Check blueprint bytecode!
			}
			else if (UClass* Class = Cast<UClass>(Object))
			{
				return ProcessStruct(Class->GetDefaultObject(), Class);
			}
			else
			{
				return ProcessStruct(Object, Object->GetClass());
			}
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
//

void UDNATagSearchFilter::AddFrontEndFilterExtensions(TSharedPtr<FFrontendFilterCategory> DefaultCategory, TArray< TSharedRef<class FFrontendFilter> >& InOutFilterList) const
{
	InOutFilterList.Add(MakeShareable(new FFrontendFilter_DNATags(DefaultCategory)));
}

#undef LOCTEXT_NAMESPACE