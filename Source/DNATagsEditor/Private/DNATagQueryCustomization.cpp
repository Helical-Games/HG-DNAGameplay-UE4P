// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNATagsEditorModulePrivatePCH.h"
#include "DNATagQueryCustomization.h"
#include "Editor/PropertyEditor/Public/PropertyEditing.h"
#include "MainFrame.h"
#include "SDNATagQueryWidget.h"
#include "DNATagContainer.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "DNATagQueryCustomization"

void FDNATagQueryCustomization::CustomizeHeader(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructPropertyHandle = InStructPropertyHandle;

	BuildEditableQueryList();

	bool const bReadOnly = StructPropertyHandle->GetProperty() ? StructPropertyHandle->GetProperty()->HasAnyPropertyFlags(CPF_EditConst) : false;

	HeaderRow
	.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MaxDesiredWidth(512)
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SButton)
				.Text(this, &FDNATagQueryCustomization::GetEditButtonText)
				.OnClicked(this, &FDNATagQueryCustomization::OnEditButtonClicked)
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SButton)
				.IsEnabled(!bReadOnly)
				.Text(LOCTEXT("DNATagQueryCustomization_Clear", "Clear All"))
				.OnClicked(this, &FDNATagQueryCustomization::OnClearAllButtonClicked)
				.Visibility(this, &FDNATagQueryCustomization::GetClearAllVisibility)
			]
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBorder)
			.Padding(4.0f)
			.Visibility(this, &FDNATagQueryCustomization::GetQueryDescVisibility)
			[
				SNew(STextBlock)
				.Text(this, &FDNATagQueryCustomization::GetQueryDescText)
				.AutoWrapText(true)
			]
		]
	];

	GEditor->RegisterForUndo(this);
}

FText FDNATagQueryCustomization::GetQueryDescText() const
{
	return FText::FromString(QueryDescription);
}

FText FDNATagQueryCustomization::GetEditButtonText() const
{
	UProperty const* const Prop = StructPropertyHandle.IsValid() ? StructPropertyHandle->GetProperty() : nullptr;
	if (Prop)
	{
		bool const bReadOnly = Prop->HasAnyPropertyFlags(CPF_EditConst);
		return
			bReadOnly
			? LOCTEXT("DNATagQueryCustomization_View", "View...")
			: LOCTEXT("DNATagQueryCustomization_Edit", "Edit...");
	}

	return FText();
}

FReply FDNATagQueryCustomization::OnClearAllButtonClicked()
{
	for (auto& EQ : EditableQueries)
	{
		if (EQ.TagQuery)
		{
			EQ.TagQuery->Clear();
		}
	}

	RefreshQueryDescription();

	return FReply::Handled();
}

EVisibility FDNATagQueryCustomization::GetClearAllVisibility() const
{
	bool bAtLeastOneQueryIsNonEmpty = false;

	for (auto& EQ : EditableQueries)
	{
		if (EQ.TagQuery && EQ.TagQuery->IsEmpty() == false)
		{
			bAtLeastOneQueryIsNonEmpty = true;
			break;
		}
	}

	return bAtLeastOneQueryIsNonEmpty ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FDNATagQueryCustomization::GetQueryDescVisibility() const
{
	return QueryDescription.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
}

void FDNATagQueryCustomization::RefreshQueryDescription()
{
	// Rebuild Editable Containers as container references can become unsafe
	BuildEditableQueryList();

	// Clear the list
	QueryDescription.Empty();

	if (EditableQueries.Num() > 1)
	{
		QueryDescription = TEXT("Multiple Selected");
	}
	else if ( (EditableQueries.Num() == 1) && (EditableQueries[0].TagQuery != nullptr) )
	{
		QueryDescription = EditableQueries[0].TagQuery->GetDescription();
	}
}


FReply FDNATagQueryCustomization::OnEditButtonClicked()
{
	if (DNATagQueryWidgetWindow.IsValid())
	{
		// already open, just show it
		DNATagQueryWidgetWindow->BringToFront(true);
	}
	else
	{
		TArray<UObject*> OuterObjects;
		StructPropertyHandle->GetOuterObjects(OuterObjects);

		bool const bReadOnly = StructPropertyHandle->GetProperty() ? StructPropertyHandle->GetProperty()->HasAnyPropertyFlags(CPF_EditConst) : false;

		FText Title;
		if (OuterObjects.Num() > 1)
		{
			FText const AssetName = FText::Format(LOCTEXT("DNATagDetailsBase_MultipleAssets", "{0} Assets"), FText::AsNumber(OuterObjects.Num()));
			FText const PropertyName = StructPropertyHandle->GetPropertyDisplayName();
			Title = FText::Format(LOCTEXT("DNATagQueryCustomization_BaseWidgetTitle", "Tag Editor: {0} {1}"), PropertyName, AssetName);
		}
		else if (OuterObjects.Num() > 0 && OuterObjects[0])
		{
			FText const AssetName = FText::FromString(OuterObjects[0]->GetName());
			FText const PropertyName = StructPropertyHandle->GetPropertyDisplayName();
			Title = FText::Format(LOCTEXT("DNATagQueryCustomization_BaseWidgetTitle", "Tag Editor: {0} {1}"), PropertyName, AssetName);
		}


		DNATagQueryWidgetWindow = SNew(SWindow)
			.Title(Title)
			.HasCloseButton(false)
			.ClientSize(FVector2D(600, 400))
			[
				SNew(SDNATagQueryWidget, EditableQueries)
				.OnSaveAndClose(this, &FDNATagQueryCustomization::CloseWidgetWindow)
				.OnCancel(this, &FDNATagQueryCustomization::CloseWidgetWindow)
				.ReadOnly(bReadOnly)
			];

		if (FGlobalTabmanager::Get()->GetRootWindow().IsValid())
		{
			FSlateApplication::Get().AddWindowAsNativeChild(DNATagQueryWidgetWindow.ToSharedRef(), FGlobalTabmanager::Get()->GetRootWindow().ToSharedRef());
		}
		else
		{
			FSlateApplication::Get().AddWindow(DNATagQueryWidgetWindow.ToSharedRef());
		}
	}

	return FReply::Handled();
}

FDNATagQueryCustomization::~FDNATagQueryCustomization()
{
	if( DNATagQueryWidgetWindow.IsValid() )
	{
		DNATagQueryWidgetWindow->RequestDestroyWindow();
	}
	GEditor->UnregisterForUndo(this);
}

void FDNATagQueryCustomization::BuildEditableQueryList()
{	
	EditableQueries.Empty();

	if( StructPropertyHandle.IsValid() )
	{
		TArray<void*> RawStructData;
		StructPropertyHandle->AccessRawData(RawStructData);

		TArray<UObject*> OuterObjects;
		StructPropertyHandle->GetOuterObjects(OuterObjects);

		ensure(RawStructData.Num() == OuterObjects.Num());
		for (int32 Idx = 0; Idx < RawStructData.Num() && Idx < OuterObjects.Num(); ++Idx)
		{
			EditableQueries.Add(SDNATagQueryWidget::FEditableDNATagQueryDatum(OuterObjects[Idx], (FDNATagQuery*)RawStructData[Idx]));
		}
	}	
}

void FDNATagQueryCustomization::CloseWidgetWindow()
{
 	if( DNATagQueryWidgetWindow.IsValid() )
 	{
 		DNATagQueryWidgetWindow->RequestDestroyWindow();
		DNATagQueryWidgetWindow = nullptr;

		RefreshQueryDescription();
 	}
}

#undef LOCTEXT_NAMESPACE
