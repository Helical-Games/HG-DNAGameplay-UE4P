// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNATagsEditorModulePrivatePCH.h"
#include "SDNATagGraphPin.h"
#include "DNATagsModule.h"
#include "DNATags.h"
#include "SScaleBox.h"

#define LOCTEXT_NAMESPACE "DNATagGraphPin"

void SDNATagGraphPin::Construct( const FArguments& InArgs, UEdGraphPin* InGraphPinObj )
{
	TagContainer = MakeShareable( new FDNATagContainer() );
	SGraphPin::Construct( SGraphPin::FArguments(), InGraphPinObj );
}

TSharedRef<SWidget>	SDNATagGraphPin::GetDefaultValueWidget()
{
	ParseDefaultValueData();

	//Create widget
	return SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew( ComboButton, SComboButton )
			.OnGetMenuContent( this, &SDNATagGraphPin::GetListContent )
			.ContentPadding( FMargin( 2.0f, 2.0f ) )
			.Visibility( this, &SGraphPin::GetDefaultValueVisibility )
			.MenuPlacement(MenuPlacement_BelowAnchor)
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

void SDNATagGraphPin::ParseDefaultValueData()
{
	FString TagString = GraphPinObj->GetDefaultAsString();

	UK2Node_CallFunction* CallFuncNode = Cast<UK2Node_CallFunction>(GraphPinObj->GetOwningNode());
	
	FilterString.Empty();
	if (CallFuncNode)
	{
		UFunction* ThisFunction = CallFuncNode->GetTargetFunction();
		if (ThisFunction)
		{
			if (ThisFunction->HasMetaData(TEXT("DNATagFilter")))
			{
				FilterString = ThisFunction->GetMetaData(TEXT("DNATagFilter"));
			}
		}
	}

	if (TagString.StartsWith(TEXT("(")) && TagString.EndsWith(TEXT(")")))
	{
		TagString = TagString.LeftChop(1);
		TagString = TagString.RightChop(1);
		TagString.Split("=", NULL, &TagString);
		if (TagString.StartsWith(TEXT("\"")) && TagString.EndsWith(TEXT("\"")))
		{
			TagString = TagString.LeftChop(1);
			TagString = TagString.RightChop(1);
		}
	}

	if (!TagString.IsEmpty())
	{
		FDNATag DNATag = IDNATagsModule::Get().GetDNATagsManager().RequestDNATag(FName(*TagString));
		TagContainer->AddTag(DNATag);
	}
}

TSharedRef<SWidget> SDNATagGraphPin::GetListContent()
{
	EditableContainers.Empty();
	EditableContainers.Add( SDNATagWidget::FEditableDNATagContainerDatum( GraphPinObj->GetOwningNode(), TagContainer.Get() ) );

	return SNew( SVerticalBox )
		+SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight( 400 )
		[
			SNew( SDNATagWidget, EditableContainers )
			.OnTagChanged( this, &SDNATagGraphPin::RefreshTagList )
			.TagContainerName( TEXT("SDNATagGraphPin") )
			.Visibility( this, &SGraphPin::GetDefaultValueVisibility )
			.MultiSelect(false)
			.Filter(FilterString)
		];
}

TSharedRef<SWidget> SDNATagGraphPin::SelectedTags()
{
	RefreshTagList();

	SAssignNew( TagListView, SListView<TSharedPtr<FString>> )
		.ListItemsSource(&TagNames)
		.SelectionMode(ESelectionMode::None)
		.OnGenerateRow(this, &SDNATagGraphPin::OnGenerateRow);

	return TagListView->AsShared();
}

TSharedRef<ITableRow> SDNATagGraphPin::OnGenerateRow(TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew( STableRow< TSharedPtr<FString> >, OwnerTable )
		[
			SNew(STextBlock) .Text( FText::FromString(*Item.Get()) )
		];
}

void SDNATagGraphPin::RefreshTagList()
{	
	// Clear the list
	TagNames.Empty();

	// Add tags to list
	FString TagName;
	if (TagContainer.IsValid())
	{
		for (auto It = TagContainer->CreateConstIterator(); It; ++It)
		{
			TagName = It->ToString();
			TagNames.Add( MakeShareable( new FString( TagName ) ) );
		}
	}

	// Refresh the slate list
	if( TagListView.IsValid() )
	{
		TagListView->RequestListRefresh();
	}

	// Set Pin Data
	FString TagString;
	if (!TagName.IsEmpty())
	{
		TagString = TEXT("(");
		TagString += TEXT("TagName=\"");
		TagString += TagName;
		TagString += TEXT("\"");
		TagString += TEXT(")");
	}
	FString CurrentDefaultValue = GraphPinObj->GetDefaultAsString();
	if (CurrentDefaultValue.IsEmpty())
	{
		CurrentDefaultValue = FString(TEXT(""));
	}
	if (!CurrentDefaultValue.Equals(TagString))
	{
		GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, TagString);
	}
}

#undef LOCTEXT_NAMESPACE
