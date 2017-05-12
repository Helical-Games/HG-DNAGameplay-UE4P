// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNATagsEditorModulePrivatePCH.h"
#include "SDNATagQueryWidget.h"
#include "ScopedTransaction.h"
#include "SScaleBox.h"
#include "SDNATagWidget.h"
#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "Editor/PropertyEditor/Public/IDetailsView.h"


#define LOCTEXT_NAMESPACE "DNATagQueryWidget"

void SDNATagQueryWidget::Construct(const FArguments& InArgs, const TArray<FEditableDNATagQueryDatum>& EditableTagQueries)
{
	ensure(EditableTagQueries.Num() > 0);
	TagQueries = EditableTagQueries;

	bReadOnly = InArgs._ReadOnly;
	bAutoSave = InArgs._AutoSave;
	OnSaveAndClose = InArgs._OnSaveAndClose;
	OnCancel = InArgs._OnCancel;
	OnQueryChanged = InArgs._OnQueryChanged;

	// Tag the assets as transactional so they can support undo/redo
	for (int32 AssetIdx = 0; AssetIdx < TagQueries.Num(); ++AssetIdx)
	{
		UObject* TagQueryOwner = TagQueries[AssetIdx].TagQueryOwner.Get();
		if (TagQueryOwner)
		{
			TagQueryOwner->SetFlags(RF_Transactional);
		}
	}

	// build editable query object tree from the runtime query data
	UEditableDNATagQuery* const EQ = CreateEditableQuery(*TagQueries[0].TagQuery);
	EditableQuery = EQ;

	// create details view for the editable query object
	FDetailsViewArgs ViewArgs;
	ViewArgs.bAllowSearch = false;
	ViewArgs.bHideSelectionTip = true;
	ViewArgs.bShowActorLabel = false;
	
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	Details = PropertyModule.CreateDetailView(ViewArgs);
	Details->SetObject(EQ);
	Details->OnFinishedChangingProperties().AddSP(this, &SDNATagQueryWidget::OnFinishedChangingProperties);

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Top)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.IsEnabled(!bReadOnly)
					.Visibility(this, &SDNATagQueryWidget::GetSaveAndCloseButtonVisibility)
					.OnClicked(this, &SDNATagQueryWidget::OnSaveAndCloseClicked)
					.Text(LOCTEXT("DNATagQueryWidget_SaveAndClose", "Save and Close"))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Visibility(this, &SDNATagQueryWidget::GetCancelButtonVisibility)
					.OnClicked(this, &SDNATagQueryWidget::OnCancelClicked)
					.Text(LOCTEXT("DNATagQueryWidget_Cancel", "Close Without Saving"))
				]
			]
			// to delete!
			+ SVerticalBox::Slot()
			[
				Details.ToSharedRef()
			]
		]
	];
}

void SDNATagQueryWidget::OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent)
{
	if (bAutoSave)
	{
		SaveToTagQuery();
	}

	OnQueryChanged.ExecuteIfBound();
}

EVisibility SDNATagQueryWidget::GetSaveAndCloseButtonVisibility() const
{
	return bAutoSave ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility SDNATagQueryWidget::GetCancelButtonVisibility() const
{
	return bAutoSave ? EVisibility::Collapsed : EVisibility::Visible;
}

UEditableDNATagQuery* SDNATagQueryWidget::CreateEditableQuery(FDNATagQuery& Q)
{
	UEditableDNATagQuery* const AnEditableQuery = Q.CreateEditableQuery();
	if (AnEditableQuery)
	{
		// prevent GC, will explicitly remove from root later
		AnEditableQuery->AddToRoot();
	}

	return AnEditableQuery;
}

SDNATagQueryWidget::~SDNATagQueryWidget()
{
	// clean up our temp editing uobjects
	UEditableDNATagQuery* const Q = EditableQuery.Get();
	if (Q)
	{
		Q->RemoveFromRoot();
	}
}


void SDNATagQueryWidget::SaveToTagQuery()
{
	// translate obj tree to token stream
	if (EditableQuery.IsValid() && !bReadOnly)
	{
		// write to all selected queries
		for (auto& TQ : TagQueries)
		{
			TQ.TagQuery->BuildFromEditableQuery(*EditableQuery.Get());
			if (TQ.TagQueryExportText != nullptr)
			{
				*TQ.TagQueryExportText = EditableQuery.Get()->GetTagQueryExportText(*TQ.TagQuery);
			}
			TQ.TagQueryOwner->MarkPackageDirty();
		}
	}
}

FReply SDNATagQueryWidget::OnSaveAndCloseClicked()
{
	SaveToTagQuery();

	OnSaveAndClose.ExecuteIfBound();
	return FReply::Handled();
}

FReply SDNATagQueryWidget::OnCancelClicked()
{
	OnCancel.ExecuteIfBound();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
