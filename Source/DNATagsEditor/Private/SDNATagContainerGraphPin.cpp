// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNATagsEditorModulePrivatePCH.h"
#include "SDNATagContainerGraphPin.h"
#include "DNATagsModule.h"
#include "DNATags.h"
#include "SScaleBox.h"

#define LOCTEXT_NAMESPACE "DNATagGraphPin"

void SDNATagContainerGraphPin::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	TagContainer = MakeShareable( new FDNATagContainer() );
	SGraphPin::Construct( SGraphPin::FArguments(), InGraphPinObj );
}

TSharedRef<SWidget>	SDNATagContainerGraphPin::GetDefaultValueWidget()
{
	ParseDefaultValueData();

	//Create widget
	return SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew( ComboButton, SComboButton )
			.OnGetMenuContent(this, &SDNATagContainerGraphPin::GetListContent)
			.ContentPadding( FMargin( 2.0f, 2.0f ) )
			.Visibility( this, &SGraphPin::GetDefaultValueVisibility )
			.ButtonContent()
			[
				SNew( STextBlock )
				.Text( LOCTEXT("DNATagWidget_Edit", "Edit") )
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SelectedTags()
		];
}

void SDNATagContainerGraphPin::ParseDefaultValueData()
{
	FString TagString = GraphPinObj->GetDefaultAsString();

	if (TagString.StartsWith(TEXT("(")) && TagString.EndsWith(TEXT(")")))
	{
		TagString = TagString.LeftChop(1);
		TagString = TagString.RightChop(1);

		TagString.Split("=", NULL, &TagString);

		TagString = TagString.LeftChop(1);
		TagString = TagString.RightChop(1);

		FString ReadTag;
		FString Remainder;

		while (TagString.Split(TEXT(","), &ReadTag, &Remainder))
		{
			ReadTag.Split("=", NULL, &ReadTag);
			if (ReadTag.EndsWith(TEXT(")")))
			{
				ReadTag = ReadTag.LeftChop(1);
				if (ReadTag.StartsWith(TEXT("\"")) && ReadTag.EndsWith(TEXT("\"")))
				{
					ReadTag = ReadTag.LeftChop(1);
					ReadTag = ReadTag.RightChop(1);
				}
			}
			TagString = Remainder;
			FDNATag DNATag = IDNATagsModule::Get().GetDNATagsManager().RequestDNATag(FName(*ReadTag));
			TagContainer->AddTag(DNATag);
		}
		if (Remainder.IsEmpty())
		{
			Remainder = TagString;
		}
		if (!Remainder.IsEmpty())
		{
			Remainder.Split("=", NULL, &Remainder);
			if (Remainder.EndsWith(TEXT(")")))
			{
				Remainder = Remainder.LeftChop(1);
				if (Remainder.StartsWith(TEXT("\"")) && Remainder.EndsWith(TEXT("\"")))
				{
					Remainder = Remainder.LeftChop(1);
					Remainder = Remainder.RightChop(1);
				}
			}
			FDNATag DNATag = IDNATagsModule::Get().GetDNATagsManager().RequestDNATag(FName(*Remainder));
			TagContainer->AddTag(DNATag);
		}
	}
}

TSharedRef<SWidget> SDNATagContainerGraphPin::GetListContent()
{
	EditableContainers.Empty();
	EditableContainers.Add( SDNATagWidget::FEditableDNATagContainerDatum( GraphPinObj->GetOwningNode(), TagContainer.Get() ) );

	return SNew( SVerticalBox )
		+SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight( 400 )
		[
			SNew( SDNATagWidget, EditableContainers )
			.OnTagChanged(this, &SDNATagContainerGraphPin::RefreshTagList)
			.TagContainerName( TEXT("SDNATagContainerGraphPin") )
			.Visibility( this, &SGraphPin::GetDefaultValueVisibility )
		];
}

TSharedRef<SWidget> SDNATagContainerGraphPin::SelectedTags()
{
	RefreshTagList();

	SAssignNew( TagListView, SListView<TSharedPtr<FString>> )
		.ListItemsSource(&TagNames)
		.SelectionMode(ESelectionMode::None)
		.OnGenerateRow(this, &SDNATagContainerGraphPin::OnGenerateRow);

	return TagListView->AsShared();
}

TSharedRef<ITableRow> SDNATagContainerGraphPin::OnGenerateRow(TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew( STableRow< TSharedPtr<FString> >, OwnerTable )
		[
			SNew(STextBlock) .Text( FText::FromString(*Item.Get()) )
		];
}

void SDNATagContainerGraphPin::RefreshTagList()
{	
	// Clear the list
	TagNames.Empty();

	// Add tags to list
	if (TagContainer.IsValid())
	{
		for (auto It = TagContainer->CreateConstIterator(); It; ++It)
		{
			FString TagName = It->ToString();
			TagNames.Add( MakeShareable( new FString( TagName ) ) );
		}
	}

	// Refresh the slate list
	if( TagListView.IsValid() )
	{
		TagListView->RequestListRefresh();
	}

	// Set Pin Data
	FString TagContainerString = TagContainer->ToString();
	FString CurrentDefaultValue = GraphPinObj->GetDefaultAsString();
	if (CurrentDefaultValue.IsEmpty())
	{
		CurrentDefaultValue = FString(TEXT("(DNATags=)"));
	}
	if (!CurrentDefaultValue.Equals(TagContainerString))
	{
		GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, TagContainerString);
	}
}

#undef LOCTEXT_NAMESPACE
