// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNATagsEditorModulePrivatePCH.h"
#include "DNATagContainerCustomization.h"
#include "Editor/PropertyEditor/Public/PropertyEditing.h"
#include "MainFrame.h"
#include "SDNATagWidget.h"
#include "DNATagContainer.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "DNATagContainerCustomization"

void FDNATagContainerCustomization::CustomizeHeader(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructPropertyHandle = InStructPropertyHandle;

	FSimpleDelegate OnTagContainerChanged = FSimpleDelegate::CreateSP(this, &FDNATagContainerCustomization::RefreshTagList);
	StructPropertyHandle->SetOnPropertyValueChanged(OnTagContainerChanged);

	BuildEditableContainerList();

	HeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MaxDesiredWidth(512)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SButton)
					.IsEnabled(!StructPropertyHandle->GetProperty()->HasAnyPropertyFlags(CPF_EditConst))
					.Text(LOCTEXT("DNATagContainerCustomization_Edit", "Edit..."))
					.OnClicked(this, &FDNATagContainerCustomization::OnEditButtonClicked)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SButton)
					.IsEnabled(!StructPropertyHandle->GetProperty()->HasAnyPropertyFlags(CPF_EditConst))
					.Text(LOCTEXT("DNATagContainerCustomization_Clear", "Clear All"))
					.OnClicked(this, &FDNATagContainerCustomization::OnClearAllButtonClicked)
					.Visibility(this, &FDNATagContainerCustomization::GetClearAllVisibility)
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBorder)
				.Padding(4.0f)
				.Visibility(this, &FDNATagContainerCustomization::GetTagsListVisibility)
				[
					ActiveTags()
				]
			]
		];

	GEditor->RegisterForUndo(this);
}

TSharedRef<SWidget> FDNATagContainerCustomization::ActiveTags()
{	
	RefreshTagList();
	
	SAssignNew( TagListView, SListView<TSharedPtr<FString>> )
	.ListItemsSource(&TagNames)
	.SelectionMode(ESelectionMode::None)
	.OnGenerateRow(this, &FDNATagContainerCustomization::MakeListViewWidget);

	return TagListView->AsShared();
}

void FDNATagContainerCustomization::RefreshTagList()
{
	// Rebuild Editable Containers as container references can become unsafe
	BuildEditableContainerList();

	// Clear the list
	TagNames.Empty();

	// Add tags to list
	for (int32 ContainerIdx = 0; ContainerIdx < EditableContainers.Num(); ++ContainerIdx)
	{
		FDNATagContainer* Container = EditableContainers[ContainerIdx].TagContainer;
		if (Container)
		{
			for (auto It = Container->CreateConstIterator(); It; ++It)
			{
				TagNames.Add(MakeShareable(new FString(It->ToString())));
			}
		}
	}

	// Refresh the slate list
	if( TagListView.IsValid() )
	{
		TagListView->RequestListRefresh();
	}
}

TSharedRef<ITableRow> FDNATagContainerCustomization::MakeListViewWidget(TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew( STableRow< TSharedPtr<FString> >, OwnerTable )
			[
				SNew(STextBlock) .Text( FText::FromString(*Item.Get()) )
			];
}

FReply FDNATagContainerCustomization::OnEditButtonClicked()
{
	if (!StructPropertyHandle.IsValid() || StructPropertyHandle->GetProperty() == nullptr)
	{
		return FReply::Handled();
	}

	TArray<UObject*> OuterObjects;
	StructPropertyHandle->GetOuterObjects(OuterObjects);

	FString Categories;
	if( StructPropertyHandle->GetProperty()->HasMetaData( TEXT("Categories") ) )
	{
		Categories = StructPropertyHandle->GetProperty()->GetMetaData( TEXT("Categories") );
	}

	bool bReadOnly = StructPropertyHandle->GetProperty()->HasAnyPropertyFlags( CPF_EditConst );

	FText Title;
	FText AssetName;
	FText PropertyName = StructPropertyHandle->GetPropertyDisplayName();

	if (OuterObjects.Num() > 1)
	{
		AssetName = FText::Format( LOCTEXT("DNATagDetailsBase_MultipleAssets", "{0} Assets"), FText::AsNumber( OuterObjects.Num() ) );
		Title = FText::Format( LOCTEXT("DNATagContainerCustomization_BaseWidgetTitle", "Tag Editor: {0} {1}"), PropertyName, AssetName );
	}
	else if (OuterObjects.Num() > 0 && OuterObjects[0])
	{
		AssetName = FText::FromString( OuterObjects[0]->GetName() );
		Title = FText::Format( LOCTEXT("DNATagContainerCustomization_BaseWidgetTitle", "Tag Editor: {0} {1}"), PropertyName, AssetName );
	}

	DNATagWidgetWindow = SNew(SWindow)
	.Title(Title)
	.ClientSize(FVector2D(600, 400))
	[
		SAssignNew(DNATagWidget, SDNATagWidget, EditableContainers)
		.Filter( Categories )
		.OnTagChanged(this, &FDNATagContainerCustomization::RefreshTagList)
		.ReadOnly(bReadOnly)
		.TagContainerName( StructPropertyHandle->GetPropertyDisplayName().ToString() )
		.PropertyHandle( StructPropertyHandle )
	];

	DNATagWidgetWindow->GetOnWindowDeactivatedEvent().AddRaw(this, &FDNATagContainerCustomization::OnDNATagWidgetWindowDeactivate);

	if (FGlobalTabmanager::Get()->GetRootWindow().IsValid())
	{
		FSlateApplication::Get().AddWindowAsNativeChild(DNATagWidgetWindow.ToSharedRef(), FGlobalTabmanager::Get()->GetRootWindow().ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(DNATagWidgetWindow.ToSharedRef());
	}

	return FReply::Handled();
}

FReply FDNATagContainerCustomization::OnClearAllButtonClicked()
{
	FScopedTransaction Transaction(LOCTEXT("DNATagContainerCustomization_RemoveAllTags", "Remove All DNA Tags"));

	for (int32 ContainerIdx = 0; ContainerIdx < EditableContainers.Num(); ++ContainerIdx)
	{
		FDNATagContainer* Container = EditableContainers[ContainerIdx].TagContainer;

		if (Container)
		{
			FDNATagContainer EmptyContainer;
			StructPropertyHandle->SetValueFromFormattedString(EmptyContainer.ToString());
			RefreshTagList();
		}
	}
	return FReply::Handled();
}

EVisibility FDNATagContainerCustomization::GetClearAllVisibility() const
{
	return TagNames.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FDNATagContainerCustomization::GetTagsListVisibility() const
{
	return TagNames.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed;
}

void FDNATagContainerCustomization::PostUndo( bool bSuccess )
{
	if( bSuccess )
	{
		RefreshTagList();
	}	
}

void FDNATagContainerCustomization::PostRedo( bool bSuccess )
{
	if( bSuccess )
	{
		RefreshTagList();
	}	
}

FDNATagContainerCustomization::~FDNATagContainerCustomization()
{
	if( DNATagWidgetWindow.IsValid() )
	{
		DNATagWidgetWindow->RequestDestroyWindow();
	}
	GEditor->UnregisterForUndo(this);
}

void FDNATagContainerCustomization::BuildEditableContainerList()
{	
	EditableContainers.Empty();

	if( StructPropertyHandle.IsValid() )
	{
		TArray<void*> RawStructData;
		StructPropertyHandle->AccessRawData(RawStructData);

		for (int32 ContainerIdx = 0; ContainerIdx < RawStructData.Num(); ++ContainerIdx)
		{
			EditableContainers.Add(SDNATagWidget::FEditableDNATagContainerDatum(nullptr, (FDNATagContainer*)RawStructData[ContainerIdx]));
		}
	}	
}

void FDNATagContainerCustomization::OnDNATagWidgetWindowDeactivate()
{
	if( DNATagWidgetWindow.IsValid() )
	{
		if (DNATagWidget.IsValid() && DNATagWidget->IsAddingNewTag() == false)
		{
			DNATagWidgetWindow->RequestDestroyWindow();
		}
	}
}

#undef LOCTEXT_NAMESPACE
