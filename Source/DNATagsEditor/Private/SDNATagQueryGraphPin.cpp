// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNATagsEditorModulePrivatePCH.h"
#include "SDNATagQueryGraphPin.h"
#include "DNATagsModule.h"
#include "DNATags.h"
#include "SScaleBox.h"

#define LOCTEXT_NAMESPACE "DNATagQueryGraphPin"

void SDNATagQueryGraphPin::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	TagQuery = MakeShareable(new FDNATagQuery());
	SGraphPin::Construct( SGraphPin::FArguments(), InGraphPinObj );
}

TSharedRef<SWidget>	SDNATagQueryGraphPin::GetDefaultValueWidget()
{
	ParseDefaultValueData();

	//Create widget
	return SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew( ComboButton, SComboButton )
			.OnGetMenuContent(this, &SDNATagQueryGraphPin::GetListContent)
			.ContentPadding( FMargin( 2.0f, 2.0f ) )
			.Visibility( this, &SGraphPin::GetDefaultValueVisibility )
			.ButtonContent()
			[
				SNew( STextBlock )
				.Text( LOCTEXT("DNATagQueryWidget_Edit", "Edit") )
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			QueryDesc()
		];
}

void SDNATagQueryGraphPin::ParseDefaultValueData()
{
	FString const TagQueryString = GraphPinObj->GetDefaultAsString();

	UProperty* const TQProperty = FindField<UProperty>(UEditableDNATagQuery::StaticClass(), TEXT("TagQueryExportText_Helper"));
	if (TQProperty)
	{
		FDNATagQuery* const TQ = TagQuery.Get();
		TQProperty->ImportText(*TagQueryString, TQ, 0, nullptr, GLog);
	}
}

TSharedRef<SWidget> SDNATagQueryGraphPin::GetListContent()
{
	EditableQueries.Empty();
	EditableQueries.Add(SDNATagQueryWidget::FEditableDNATagQueryDatum(GraphPinObj->GetOwningNode(), TagQuery.Get(), &TagQueryExportText));

	return SNew( SVerticalBox )
		+SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight( 400 )
		[
			SNew(SScaleBox)
			.HAlign(EHorizontalAlignment::HAlign_Left)
			.VAlign(EVerticalAlignment::VAlign_Top)
			.StretchDirection(EStretchDirection::DownOnly)
			.Stretch(EStretch::ScaleToFit)
			.Content()
			[
				SNew(SDNATagQueryWidget, EditableQueries)
				.OnQueryChanged(this, &SDNATagQueryGraphPin::OnQueryChanged)
				.Visibility( this, &SGraphPin::GetDefaultValueVisibility )
				.AutoSave(true)
			]
		];
}

void SDNATagQueryGraphPin::OnQueryChanged()
{
	// Set Pin Data
	GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, TagQueryExportText);
	QueryDescription = TagQuery->GetDescription();
}

TSharedRef<SWidget> SDNATagQueryGraphPin::QueryDesc()
{
	QueryDescription = TagQuery->GetDescription();

	return SNew(STextBlock)
		.Text(this, &SDNATagQueryGraphPin::GetQueryDescText)
		.AutoWrapText(true);
}

FText SDNATagQueryGraphPin::GetQueryDescText() const
{
	return FText::FromString(QueryDescription);
}

#undef LOCTEXT_NAMESPACE
