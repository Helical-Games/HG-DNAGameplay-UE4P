// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNATagsEditorModulePrivatePCH.h"
#include "SDNATagWidget.h"
#include "DNATagsModule.h"
#include "DNATags.h"
#include "ScopedTransaction.h"
#include "Editor/PropertyEditor/Public/PropertyHandle.h"
#include "SSearchBox.h"
#include "SScaleBox.h"
#include "AssetEditorManager.h"
#include "AssetToolsModule.h"
#include "SHyperlink.h"
#include "SNotificationList.h"
#include "NotificationManager.h"
#include "SSearchBox.h"
#include "DNATagsSettings.h"

#define LOCTEXT_NAMESPACE "DNATagWidget"

const FString SDNATagWidget::SettingsIniSection = TEXT("DNATagWidget");

void SDNATagWidget::Construct(const FArguments& InArgs, const TArray<FEditableDNATagContainerDatum>& EditableTagContainers)
{
	ensure(EditableTagContainers.Num() > 0);
	TagContainers = EditableTagContainers;

	OnTagChanged = InArgs._OnTagChanged;
	bReadOnly = InArgs._ReadOnly;
	TagContainerName = InArgs._TagContainerName;
	bMultiSelect = InArgs._MultiSelect;
	PropertyHandle = InArgs._PropertyHandle;
	bIsAddingNewTag = false;
	RootFilterString = InArgs._Filter;

	IDNATagsModule::Get().GetDNATagsManager().GetFilteredDNARootTags(InArgs._Filter, TagItems);
	bool CanAddFromINI = UDNATagsManager::ShouldImportTagsFromINI(); // We only support adding new tags to the ini files.

	// Tag the assets as transactional so they can support undo/redo
	TArray<UObject*> ObjectsToMarkTransactional;
	if (PropertyHandle.IsValid())
	{
		// If we have a property handle use that to find the objects that need to be transactional
		PropertyHandle->GetOuterObjects(ObjectsToMarkTransactional);
	}
	else
	{
		// Otherwise use the owner list
		for (int32 AssetIdx = 0; AssetIdx < TagContainers.Num(); ++AssetIdx)
		{
			ObjectsToMarkTransactional.Add(TagContainers[AssetIdx].TagContainerOwner.Get());
		}
	}

	// Now actually mark the assembled objects
	for (UObject* ObjectToMark : ObjectsToMarkTransactional)
	{
		if (ObjectToMark)
		{
			ObjectToMark->SetFlags(RF_Transactional);
		}
	}

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Top)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(2.0f, 2.0f)
				.AutoWidth()
				[
					SAssignNew(NewTagTextBox, SEditableTextBox)
					.MinDesiredWidth(210.0f)
					.HintText(LOCTEXT("NewTag", "X.Y.Z"))
					.OnTextCommitted(this, &SDNATagWidget::OnNewDNATagCommited)
					.Visibility(CanAddFromINI ? EVisibility::Visible : EVisibility::Collapsed)
				]
				+ SHorizontalBox::Slot()
				.Padding(2.0f, 2.0f)
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("AddNew", "Add New"))
					.OnClicked(this, &SDNATagWidget::OnNewDNATagButtonPressed)
					.Visibility(CanAddFromINI ? EVisibility::Visible : EVisibility::Collapsed)
				]
			]

			+SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Top)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.OnClicked(this, &SDNATagWidget::OnExpandAllClicked)
					.Text(LOCTEXT("DNATagWidget_ExpandAll", "Expand All"))
				]
				+SHorizontalBox::Slot()
				.AutoWidth()

				[
					SNew(SButton)
					.OnClicked(this, &SDNATagWidget::OnCollapseAllClicked)
					.Text(LOCTEXT("DNATagWidget_CollapseAll", "Collapse All"))
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.IsEnabled(!bReadOnly)
					.OnClicked(this, &SDNATagWidget::OnClearAllClicked)
					.Text(LOCTEXT("DNATagWidget_ClearAll", "Clear All"))
				]
				+SHorizontalBox::Slot()
				.VAlign( VAlign_Center )
				.FillWidth(1.f)
				.Padding(5,1,5,1)
				[
					SAssignNew(SearchTagBox, SSearchBox)
					.HintText(LOCTEXT("DNATagWidget_SearchBoxHint", "Search DNA Tags"))
					.OnTextChanged( this, &SDNATagWidget::OnFilterTextChanged )
				]
			]
			+SVerticalBox::Slot()
			[
				SNew(SBorder)
				.Padding(FMargin(4.f))
				[
					SAssignNew(TagTreeWidget, STreeView< TSharedPtr<FDNATagNode> >)
					.TreeItemsSource(&TagItems)
					.OnGenerateRow(this, &SDNATagWidget::OnGenerateRow)
					.OnGetChildren(this, &SDNATagWidget::OnGetChildren)
					.OnExpansionChanged( this, &SDNATagWidget::OnExpansionChanged)
					.SelectionMode(ESelectionMode::Multi)
				]
			]
		]
	];

	// Force the entire tree collapsed to start
	SetTagTreeItemExpansion(false);
	 
	LoadSettings();

	// Strip any invalid tags from the assets being edited
	VerifyAssetTagValidity();
}

void SDNATagWidget::OnNewDNATagCommited(const FText& InText, ETextCommit::Type InCommitType)
{
	if (InCommitType == ETextCommit::OnEnter)
	{
		CreateNewDNATag();
	}
}

FReply SDNATagWidget::OnNewDNATagButtonPressed()
{
	CreateNewDNATag();
	return FReply::Handled();
}

void SDNATagWidget::CreateNewDNATag()
{
	// Only support adding tags via ini file
	if (UDNATagsManager::ShouldImportTagsFromINI() == false)
	{
		return;
	}

	FString str = NewTagTextBox->GetText().ToString();
	if (str.IsEmpty())
	{
		return;
	}

	// set bIsAddingNewTag, this guards against the window closing when it loses focus due to source control checking out a file
	TGuardValue<bool>	Guard(bIsAddingNewTag, true);

	UDNATagsManager::AddNewDNATagToINI(str);

	NewTagTextBox->SetText(FText::GetEmpty());

	IDNATagsModule::Get().GetDNATagsManager().GetFilteredDNARootTags(RootFilterString, TagItems);
	TagTreeWidget->RequestTreeRefresh();

	auto node = IDNATagsModule::Get().GetDNATagsManager().FindTagNode(FName(*str));
	if (node.IsValid())
	{
		OnTagChecked(node);
	}

	// Filter on the new tag
	SearchTagBox->SetText(FText::FromString(str));
}

void SDNATagWidget::OnFilterTextChanged( const FText& InFilterText )
{
	FilterString = InFilterText.ToString();	

	if( FilterString.IsEmpty() )
	{
		TagTreeWidget->SetTreeItemsSource( &TagItems );

		for( int32 iItem = 0; iItem < TagItems.Num(); ++iItem )
		{
			SetDefaultTagNodeItemExpansion( TagItems[iItem] );
		}
	}
	else
	{
		FilteredTagItems.Empty();

		for( int32 iItem = 0; iItem < TagItems.Num(); ++iItem )
		{
			if( FilterChildrenCheck( TagItems[iItem] ) )
			{
				FilteredTagItems.Add( TagItems[iItem] );
				SetTagNodeItemExpansion( TagItems[iItem], true );
			}
			else
			{
				SetTagNodeItemExpansion( TagItems[iItem], false );
			}
		}

		TagTreeWidget->SetTreeItemsSource( &FilteredTagItems );	
	}
		
	TagTreeWidget->RequestTreeRefresh();	
}

bool SDNATagWidget::FilterChildrenCheck( TSharedPtr<FDNATagNode> InItem )
{
	if( !InItem.IsValid() )
	{
		return false;
	}

	if( InItem->GetCompleteTag().ToString().Contains( FilterString ) || FilterString.IsEmpty() )
	{
		return true;
	}

	TArray< TSharedPtr<FDNATagNode> > Children = InItem->GetChildTagNodes();

	for( int32 iChild = 0; iChild < Children.Num(); ++iChild )
	{
		if( FilterChildrenCheck( Children[iChild] ) )
		{
			return true;
		}
	}

	return false;
}

TSharedRef<ITableRow> SDNATagWidget::OnGenerateRow(TSharedPtr<FDNATagNode> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	FText TooltipText;
	if (InItem.IsValid())
	{
		TooltipText = FText::FromName(InItem.Get()->GetCompleteTag());
	}

	return SNew(STableRow< TSharedPtr<FDNATagNode> >, OwnerTable)
		.Style(FEditorStyle::Get(), "DNATagTreeView")
		[
			SNew(SCheckBox)
			.OnCheckStateChanged(this, &SDNATagWidget::OnTagCheckStatusChanged, InItem)
			.IsChecked(this, &SDNATagWidget::IsTagChecked, InItem)
			.ToolTipText(TooltipText)
			.IsEnabled(!bReadOnly)
			[
				SNew(STextBlock)
				.Text(FText::FromName(InItem->GetSimpleTag()))
			]
		];
}

void SDNATagWidget::OnGetChildren(TSharedPtr<FDNATagNode> InItem, TArray< TSharedPtr<FDNATagNode> >& OutChildren)
{
	TArray< TSharedPtr<FDNATagNode> > FilteredChildren;
	TArray< TSharedPtr<FDNATagNode> > Children = InItem->GetChildTagNodes();

	for( int32 iChild = 0; iChild < Children.Num(); ++iChild )
	{
		if( FilterChildrenCheck( Children[iChild] ) )
		{
			FilteredChildren.Add( Children[iChild] );
		}
	}
	OutChildren += FilteredChildren;
}

void SDNATagWidget::OnTagCheckStatusChanged(ECheckBoxState NewCheckState, TSharedPtr<FDNATagNode> NodeChanged)
{
	if (NewCheckState == ECheckBoxState::Checked)
	{
		OnTagChecked(NodeChanged);
	}
	else if (NewCheckState == ECheckBoxState::Unchecked)
	{
		OnTagUnchecked(NodeChanged);
	}
}

void SDNATagWidget::OnTagChecked(TSharedPtr<FDNATagNode> NodeChecked)
{
	FScopedTransaction Transaction( LOCTEXT("DNATagWidget_AddTags", "Add DNA Tags") );

	UDNATagsManager& TagsManager = IDNATagsModule::GetDNATagsManager();

	for (int32 ContainerIdx = 0; ContainerIdx < TagContainers.Num(); ++ContainerIdx)
	{
		TWeakPtr<FDNATagNode> CurNode(NodeChecked);
		UObject* OwnerObj = TagContainers[ContainerIdx].TagContainerOwner.Get();
		FDNATagContainer* Container = TagContainers[ContainerIdx].TagContainer;

		if (Container)
		{
			FDNATagContainer EditableContainer = *Container;

			bool bRemoveParents = false;

			while (CurNode.IsValid())
			{
				FDNATag DNATag = TagsManager.RequestDNATag(CurNode.Pin()->GetCompleteTag());

				if (bRemoveParents == false)
				{
					bRemoveParents = true;
					if (bMultiSelect == false)
					{
						EditableContainer.RemoveAllTags();
					}
					EditableContainer.AddTag(DNATag);
				}
				else
				{
					EditableContainer.RemoveTag(DNATag);
				}

				CurNode = CurNode.Pin()->GetParentTagNode();
			}
			SetContainer(Container, &EditableContainer, OwnerObj);
		}
	}
}

void SDNATagWidget::OnTagUnchecked(TSharedPtr<FDNATagNode> NodeUnchecked)
{
	FScopedTransaction Transaction( LOCTEXT("DNATagWidget_RemoveTags", "Remove DNA Tags"));
	if (NodeUnchecked.IsValid())
	{
		UDNATagsManager& TagsManager = IDNATagsModule::GetDNATagsManager();

		for (int32 ContainerIdx = 0; ContainerIdx < TagContainers.Num(); ++ContainerIdx)
		{
			UObject* OwnerObj = TagContainers[ContainerIdx].TagContainerOwner.Get();
			FDNATagContainer* Container = TagContainers[ContainerIdx].TagContainer;
			FDNATag DNATag = TagsManager.RequestDNATag(NodeUnchecked->GetCompleteTag());

			if (Container)
			{
				FDNATagContainer EditableContainer = *Container;
				EditableContainer.RemoveTag(DNATag);

				TWeakPtr<FDNATagNode> ParentNode = NodeUnchecked->GetParentTagNode();
				if (ParentNode.IsValid())
				{
					// Check if there are other siblings before adding parent
					bool bOtherSiblings = false;
					for (auto It = ParentNode.Pin()->GetChildTagNodes().CreateConstIterator(); It; ++It)
					{
						DNATag = TagsManager.RequestDNATag(It->Get()->GetCompleteTag());
						if (EditableContainer.HasTag(DNATag, EDNATagMatchType::Explicit, EDNATagMatchType::Explicit))
						{
							bOtherSiblings = true;
							break;
						}
					}
					// Add Parent
					if (!bOtherSiblings)
					{
						DNATag = TagsManager.RequestDNATag(ParentNode.Pin()->GetCompleteTag());
						EditableContainer.AddTag(DNATag);
					}
				}

				// Uncheck Children
				for (const auto& ChildNode : NodeUnchecked->GetChildTagNodes())
				{
					UncheckChildren(ChildNode, EditableContainer);
				}

				SetContainer(Container, &EditableContainer, OwnerObj);
			}
			
		}
	}
}

void SDNATagWidget::UncheckChildren(TSharedPtr<FDNATagNode> NodeUnchecked, FDNATagContainer& EditableContainer)
{
	UDNATagsManager& TagsManager = IDNATagsModule::GetDNATagsManager();

	FDNATag DNATag = TagsManager.RequestDNATag(NodeUnchecked->GetCompleteTag());
	EditableContainer.RemoveTag(DNATag);

	// Uncheck Children
	for (const auto& ChildNode : NodeUnchecked->GetChildTagNodes())
	{
		UncheckChildren(ChildNode, EditableContainer);
	}
}

ECheckBoxState SDNATagWidget::IsTagChecked(TSharedPtr<FDNATagNode> Node) const
{
	int32 NumValidAssets = 0;
	int32 NumAssetsTagIsAppliedTo = 0;

	if (Node.IsValid())
	{
		UDNATagsManager& TagsManager = IDNATagsModule::GetDNATagsManager();

		for (int32 ContainerIdx = 0; ContainerIdx < TagContainers.Num(); ++ContainerIdx)
		{
			FDNATagContainer* Container = TagContainers[ContainerIdx].TagContainer;
			if (Container)
			{
				NumValidAssets++;
				FDNATag DNATag = TagsManager.RequestDNATag(Node->GetCompleteTag(), false);
				if (DNATag.IsValid())
				{
					if (Container->HasTag(DNATag, EDNATagMatchType::IncludeParentTags, EDNATagMatchType::Explicit))
					{
						++NumAssetsTagIsAppliedTo;
					}
				}
			}
		}
	}

	if (NumAssetsTagIsAppliedTo == 0)
	{
		return ECheckBoxState::Unchecked;
	}
	else if (NumAssetsTagIsAppliedTo == NumValidAssets)
	{
		return ECheckBoxState::Checked;
	}
	else
	{
		return ECheckBoxState::Undetermined;
	}
}

FReply SDNATagWidget::OnClearAllClicked()
{
	FScopedTransaction Transaction( LOCTEXT("DNATagWidget_RemoveAllTags", "Remove All DNA Tags") );

	for (int32 ContainerIdx = 0; ContainerIdx < TagContainers.Num(); ++ContainerIdx)
	{
		UObject* OwnerObj = TagContainers[ContainerIdx].TagContainerOwner.Get();
		FDNATagContainer* Container = TagContainers[ContainerIdx].TagContainer;

		if (Container)
		{
			FDNATagContainer EmptyContainer;
			SetContainer(Container, &EmptyContainer, OwnerObj);
		}
	}
	return FReply::Handled();
}

FReply SDNATagWidget::OnExpandAllClicked()
{
	SetTagTreeItemExpansion(true);
	return FReply::Handled();
}

FReply SDNATagWidget::OnCollapseAllClicked()
{
	SetTagTreeItemExpansion(false);
	return FReply::Handled();
}

void SDNATagWidget::SetTagTreeItemExpansion(bool bExpand)
{
	TArray< TSharedPtr<FDNATagNode> > TagArray;
	IDNATagsModule::Get().GetDNATagsManager().GetFilteredDNARootTags(TEXT(""), TagArray);
	for (int32 TagIdx = 0; TagIdx < TagArray.Num(); ++TagIdx)
	{
		SetTagNodeItemExpansion(TagArray[TagIdx], bExpand);
	}
	
}

void SDNATagWidget::SetTagNodeItemExpansion(TSharedPtr<FDNATagNode> Node, bool bExpand)
{
	if (Node.IsValid() && TagTreeWidget.IsValid())
	{
		TagTreeWidget->SetItemExpansion(Node, bExpand);

		const TArray< TSharedPtr<FDNATagNode> >& ChildTags = Node->GetChildTagNodes();
		for (int32 ChildIdx = 0; ChildIdx < ChildTags.Num(); ++ChildIdx)
		{
			SetTagNodeItemExpansion(ChildTags[ChildIdx], bExpand);
		}
	}
}

void SDNATagWidget::VerifyAssetTagValidity()
{
	FDNATagContainer LibraryTags;

	// Create a set that is the library of all valid tags
	TArray< TSharedPtr<FDNATagNode> > NodeStack;

	UDNATagsManager& TagsManager = IDNATagsModule::GetDNATagsManager();
	
	TagsManager.GetFilteredDNARootTags(TEXT(""), NodeStack);

	while (NodeStack.Num() > 0)
	{
		TSharedPtr<FDNATagNode> CurNode = NodeStack.Pop();
		if (CurNode.IsValid())
		{
			LibraryTags.AddTag(TagsManager.RequestDNATag(CurNode->GetCompleteTag()));
			NodeStack.Append(CurNode->GetChildTagNodes());
		}
	}

	// Find and remove any tags on the asset that are no longer in the library
	for (int32 ContainerIdx = 0; ContainerIdx < TagContainers.Num(); ++ContainerIdx)
	{
		UObject* OwnerObj = TagContainers[ContainerIdx].TagContainerOwner.Get();
		FDNATagContainer* Container = TagContainers[ContainerIdx].TagContainer;

		if (Container)
		{
			FDNATagContainer EditableContainer = *Container;

			// Use a set instead of a container so we can find and remove None tags
			TSet<FDNATag> InvalidTags;

			for (auto It = Container->CreateConstIterator(); It; ++It)
			{
				if (!LibraryTags.HasTag(*It, EDNATagMatchType::Explicit, EDNATagMatchType::Explicit))
				{
					InvalidTags.Add(*It);
				}
			}
			if (InvalidTags.Num() > 0)
			{
				FString InvalidTagNames;

				for (auto InvalidIter = InvalidTags.CreateConstIterator(); InvalidIter; ++InvalidIter)
				{
					EditableContainer.RemoveTag(*InvalidIter);
					InvalidTagNames += InvalidIter->ToString() + TEXT("\n");
				}
				SetContainer(Container, &EditableContainer, OwnerObj);

				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("Objects"), FText::FromString( InvalidTagNames ));
				FText DialogText = FText::Format( LOCTEXT("DNATagWidget_InvalidTags", "Invalid Tags that have been removed: \n\n{Objects}"), Arguments );
				OpenMsgDlgInt( EAppMsgType::Ok, DialogText, LOCTEXT("DNATagWidget_Warning", "Warning") );
			}
		}
	}
}

void SDNATagWidget::LoadSettings()
{
	TArray< TSharedPtr<FDNATagNode> > TagArray;
	IDNATagsModule::Get().GetDNATagsManager().GetFilteredDNARootTags(TEXT(""), TagArray);
	for (int32 TagIdx = 0; TagIdx < TagArray.Num(); ++TagIdx)
	{
		LoadTagNodeItemExpansion(TagArray[TagIdx] );
	}
}

void SDNATagWidget::SetDefaultTagNodeItemExpansion( TSharedPtr<FDNATagNode> Node )
{
	if ( Node.IsValid() && TagTreeWidget.IsValid() )
	{
		bool bExpanded = false;

		if ( IsTagChecked(Node) == ECheckBoxState::Checked )
		{
			bExpanded = true;
		}
		TagTreeWidget->SetItemExpansion(Node, bExpanded);

		const TArray< TSharedPtr<FDNATagNode> >& ChildTags = Node->GetChildTagNodes();
		for ( int32 ChildIdx = 0; ChildIdx < ChildTags.Num(); ++ChildIdx )
		{
			SetDefaultTagNodeItemExpansion(ChildTags[ChildIdx]);
		}
	}
}

void SDNATagWidget::LoadTagNodeItemExpansion( TSharedPtr<FDNATagNode> Node )
{
	if (Node.IsValid() && TagTreeWidget.IsValid())
	{
		bool bExpanded = false;

		if( GConfig->GetBool(*SettingsIniSection, *(TagContainerName + Node->GetCompleteTag().ToString() + TEXT(".Expanded")), bExpanded, GEditorPerProjectIni) )
		{
			TagTreeWidget->SetItemExpansion( Node, bExpanded );
		}
		else if( IsTagChecked( Node ) == ECheckBoxState::Checked ) // If we have no save data but its ticked then we probably lost our settings so we shall expand it
		{
			TagTreeWidget->SetItemExpansion( Node, true );
		}

		const TArray< TSharedPtr<FDNATagNode> >& ChildTags = Node->GetChildTagNodes();
		for (int32 ChildIdx = 0; ChildIdx < ChildTags.Num(); ++ChildIdx)
		{
			LoadTagNodeItemExpansion(ChildTags[ChildIdx]);
		}
	}
}

void SDNATagWidget::OnExpansionChanged( TSharedPtr<FDNATagNode> InItem, bool bIsExpanded )
{
	// Save the new expansion setting to ini file
	GConfig->SetBool(*SettingsIniSection, *(TagContainerName + InItem->GetCompleteTag().ToString() + TEXT(".Expanded")), bIsExpanded, GEditorPerProjectIni);
}

void SDNATagWidget::SetContainer(FDNATagContainer* OriginalContainer, FDNATagContainer* EditedContainer, UObject* OwnerObj)
{
	if (PropertyHandle.IsValid() && bMultiSelect)
	{
		// Case for a tag container 
		PropertyHandle->SetValueFromFormattedString(EditedContainer->ToString());
	}
	else if (PropertyHandle.IsValid() && !bMultiSelect)
	{
		// Case for a single Tag		
		FString FormattedString = TEXT("(TagName=\"");
		FormattedString += EditedContainer->First().GetTagName().ToString();
		FormattedString += TEXT("\")");
		PropertyHandle->SetValueFromFormattedString(FormattedString);
	}
	else
	{
		// Not sure if we should get here, means the property handle hasnt been setup which could be right or wrong.
		if (OwnerObj)
		{
			OwnerObj->PreEditChange(PropertyHandle.IsValid() ? PropertyHandle->GetProperty() : NULL);
		}

		*OriginalContainer = *EditedContainer;

		if (OwnerObj)
		{
			OwnerObj->PostEditChange();
		}
	}	

	if (!PropertyHandle.IsValid())
	{
		OnTagChanged.ExecuteIfBound();
	}
}

#undef LOCTEXT_NAMESPACE
