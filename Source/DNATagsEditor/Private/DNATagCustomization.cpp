// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNATagsEditorModulePrivatePCH.h"
#include "DNATagCustomization.h"
#include "Editor/PropertyEditor/Public/PropertyEditing.h"
#include "MainFrame.h"
#include "SDNATagWidget.h"
#include "DNATagContainer.h"
#include "ScopedTransaction.h"
#include "SScaleBox.h"

#define LOCTEXT_NAMESPACE "DNATagCustomization"

void FDNATagCustomization::CustomizeHeader(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TagContainer = MakeShareable(new FDNATagContainer);
	StructPropertyHandle = InStructPropertyHandle;

	FSimpleDelegate OnTagChanged = FSimpleDelegate::CreateSP(this, &FDNATagCustomization::OnPropertyValueChanged);
	StructPropertyHandle->SetOnPropertyValueChanged(OnTagChanged);

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
		[
			SAssignNew(ComboButton, SComboButton)
			.OnGetMenuContent(this, &FDNATagCustomization::GetListContent)
			.ContentPadding(FMargin(2.0f, 2.0f))
			.MenuPlacement(MenuPlacement_BelowAnchor)
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("DNATagCustomization_Edit", "Edit"))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBorder)
			.Padding(4.0f)
			[
				SNew(STextBlock)
				.Text(this, &FDNATagCustomization::SelectedTag)
			]
		]
	];

	GEditor->RegisterForUndo(this);
}

TSharedRef<SWidget> FDNATagCustomization::GetListContent()
{
	BuildEditableContainerList();
	
	FString Categories;
	if (StructPropertyHandle->GetProperty()->HasMetaData(TEXT("Categories")))
	{
		Categories = StructPropertyHandle->GetProperty()->GetMetaData(TEXT("Categories"));
	}

	bool bReadOnly = StructPropertyHandle->GetProperty()->HasAnyPropertyFlags(CPF_EditConst);

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight(400)
		[
            SNew(SDNATagWidget, EditableContainers)
            .Filter(Categories)
            .ReadOnly(bReadOnly)
            .TagContainerName(StructPropertyHandle->GetPropertyDisplayName().ToString())
            .MultiSelect(false)
            .OnTagChanged(this, &FDNATagCustomization::OnTagChanged)
            .PropertyHandle(StructPropertyHandle)
		];
}

void FDNATagCustomization::OnPropertyValueChanged()
{
	TagName = TEXT("");
	if (StructPropertyHandle.IsValid() && StructPropertyHandle->GetProperty() && EditableContainers.Num() > 0)
	{
		TArray<void*> RawStructData;
		StructPropertyHandle->AccessRawData(RawStructData);
		if (RawStructData.Num() > 0)
		{
			FDNATag* Tag = (FDNATag*)(RawStructData[0]);
			FDNATagContainer* Container = EditableContainers[0].TagContainer;			
			if (Tag && Container)
			{
				Container->RemoveAllTags();
				Container->AddTag(*Tag);
				TagName = Tag->ToString();
			}			
		}
	}
}

void FDNATagCustomization::OnTagChanged()
{
	TagName = TEXT("");
	if (StructPropertyHandle.IsValid() && StructPropertyHandle->GetProperty() && EditableContainers.Num() > 0)
	{
		TArray<void*> RawStructData;
		StructPropertyHandle->AccessRawData(RawStructData);
		if (RawStructData.Num() > 0)
		{
			FDNATag* Tag = (FDNATag*)(RawStructData[0]);			

			// Update Tag from the one selected from list
			FDNATagContainer* Container = EditableContainers[0].TagContainer;
			if (Tag && Container)
			{
				for (auto It = Container->CreateConstIterator(); It; ++It)
				{
					*Tag = *It;
					TagName = It->ToString();
				}
			}
		}
	}
}

void FDNATagCustomization::PostUndo(bool bSuccess)
{
	if (bSuccess && !StructPropertyHandle.IsValid())
	{
		OnTagChanged();
	}
}

void FDNATagCustomization::PostRedo(bool bSuccess)
{
	if (bSuccess && !StructPropertyHandle.IsValid())
	{
		OnTagChanged();
	}
}

FDNATagCustomization::~FDNATagCustomization()
{
	GEditor->UnregisterForUndo(this);
}

void FDNATagCustomization::BuildEditableContainerList()
{
	EditableContainers.Empty();

	if(StructPropertyHandle.IsValid() && StructPropertyHandle->GetProperty())
	{
		TArray<void*> RawStructData;
		StructPropertyHandle->AccessRawData(RawStructData);

		if (RawStructData.Num() > 0)
		{
			FDNATag* Tag = (FDNATag*)(RawStructData[0]);
			if (Tag->IsValid())
			{
				TagName = Tag->ToString();
				TagContainer->AddTag(*Tag);
			}
		}

		EditableContainers.Add(SDNATagWidget::FEditableDNATagContainerDatum(nullptr, TagContainer.Get()));
	}
}

FText FDNATagCustomization::SelectedTag() const
{
	return FText::FromString(*TagName);
}

#undef LOCTEXT_NAMESPACE
