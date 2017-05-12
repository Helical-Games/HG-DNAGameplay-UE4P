// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "DNATagsManager.h"
#include "SlateBasics.h"

/** Widget allowing user to tag assets with DNA tags */
class SDNATagWidget : public SCompoundWidget
{
public:

	/** Called when a tag status is changed */
	DECLARE_DELEGATE( FOnTagChanged )

	SLATE_BEGIN_ARGS( SDNATagWidget )
	: _Filter(),
	  _ReadOnly(false),
	  _TagContainerName( TEXT("") ),
	  _MultiSelect(true),
	  _PropertyHandle(NULL)
	{}
		SLATE_ARGUMENT( FString, Filter ) // Comma delimited string of tag root names to filter by
		SLATE_ARGUMENT( bool, ReadOnly ) // Flag to set if the list is read only
		SLATE_ARGUMENT( FString, TagContainerName ) // The name that will be used for the settings file
		SLATE_ARGUMENT( bool, MultiSelect ) // If we can select multiple entries
		SLATE_ARGUMENT( TSharedPtr<IPropertyHandle>, PropertyHandle )
		SLATE_EVENT( FOnTagChanged, OnTagChanged ) // Called when a tag status changes
	SLATE_END_ARGS()

	/** Simple struct holding a tag container and its owner for generic re-use of the widget */
	struct FEditableDNATagContainerDatum
	{
		/** Constructor */
		FEditableDNATagContainerDatum(class UObject* InOwnerObj, struct FDNATagContainer* InTagContainer)
			: TagContainerOwner(InOwnerObj)
			, TagContainer(InTagContainer)
		{}

		/** Owning UObject of the container being edited */
		TWeakObjectPtr<class UObject> TagContainerOwner;

		/** Tag container to edit */
		struct FDNATagContainer* TagContainer; 
	};

	/** Construct the actual widget */
	void Construct(const FArguments& InArgs, const TArray<FEditableDNATagContainerDatum>& EditableTagContainers);

	/** Updates the tag list when the filter text changes */
	void OnFilterTextChanged( const FText& InFilterText );

	/** Returns true if this TagNode has any children that match the current filter */
	bool FilterChildrenCheck( TSharedPtr<FDNATagNode>  );

	bool IsAddingNewTag() const
	{
		return bIsAddingNewTag;
	}

private:

	/* string that sets the section of the ini file to use for this class*/ 
	static const FString SettingsIniSection;

	/* Holds the Name of this TagContainer used for saving out expansion settings */
	FString TagContainerName;

	/* Filter string used during search box */
	FString FilterString;

	/** root filter (passed in on creation) */
	FString RootFilterString;

	/* Flag to set if the list is read only*/
	bool bReadOnly;

	/* Flag to set if we can select multiple items form the list*/
	bool bMultiSelect;

	/* Flag set while we are in process of adding new tag */
	bool bIsAddingNewTag;

	/* Array of tags to be displayed in the TreeView*/
	TArray< TSharedPtr<FDNATagNode> > TagItems;

	/* Array of tags to be displayed in the TreeView*/
	TArray< TSharedPtr<FDNATagNode> > FilteredTagItems;

	/** Tree widget showing the DNA tag library */
	TSharedPtr< STreeView< TSharedPtr<FDNATagNode> > > TagTreeWidget;

	TSharedPtr<SEditableTextBox> NewTagTextBox;

	TSharedPtr<SSearchBox> SearchTagBox;

	/** Containers to modify */
	TArray<FEditableDNATagContainerDatum> TagContainers;

	/** Called when the Tag list changes*/
	FOnTagChanged OnTagChanged;

	TSharedPtr<IPropertyHandle> PropertyHandle;

	void OnNewDNATagCommited(const FText& InText, ETextCommit::Type InCommitType);

	FReply OnNewDNATagButtonPressed();

	void CreateNewDNATag();

	/**
	 * Generate a row widget for the specified item node and table
	 * 
	 * @param InItem		Tag node to generate a row widget for
	 * @param OwnerTable	Table that owns the row
	 * 
	 * @return Generated row widget for the item node
	 */
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FDNATagNode> InItem, const TSharedRef<STableViewBase>& OwnerTable);

	/**
	 * Get children nodes of the specified node
	 * 
	 * @param InItem		Node to get children of
	 * @param OutChildren	[OUT] Array of children nodes, if any
	 */
	void OnGetChildren(TSharedPtr<FDNATagNode> InItem, TArray< TSharedPtr<FDNATagNode> >& OutChildren);

	/**
	 * Called via delegate when the status of a check box in a row changes
	 * 
	 * @param NewCheckState	New check box state
	 * @param NodeChanged	Node that was checked/unchecked
	 */
	void OnTagCheckStatusChanged(ECheckBoxState NewCheckState, TSharedPtr<FDNATagNode> NodeChanged);

	/**
	 * Called via delegate to determine the checkbox state of the specified node
	 * 
	 * @param Node	Node to find the checkbox state of
	 * 
	 * @return Checkbox state of the specified node
	 */
	ECheckBoxState IsTagChecked(TSharedPtr<FDNATagNode> Node) const;

	/**
	 * Helper function called when the specified node is checked
	 * 
	 * @param NodeChecked	Node that was checked by the user
	 */
	void OnTagChecked(TSharedPtr<FDNATagNode> NodeChecked);

	/**
	 * Helper function called when the specified node is unchecked
	 * 
	 * @param NodeUnchecked	Node that was unchecked by the user
	 */
	void OnTagUnchecked(TSharedPtr<FDNATagNode> NodeUnchecked);

	/**
	 * Recursive function to uncheck all child tags
	 * 
	 * @param NodeUnchecked	Node that was unchecked by the user
	 * @param EditableContainer The container we are removing the tags from
	 */
	void UncheckChildren(TSharedPtr<FDNATagNode> NodeUnchecked, FDNATagContainer& EditableContainer);

	/** Called when the user clicks the "Clear All" button; Clears all tags */
	FReply OnClearAllClicked();

	/** Called when the user clicks the "Expand All" button; Expands the entire tag tree */
	FReply OnExpandAllClicked();

	/** Called when the user clicks the "Collapse All" button; Collapses the entire tag tree */
	FReply OnCollapseAllClicked();

	/**
	 * Helper function to set the expansion state of the tree widget
	 * 
	 * @param bExpand If true, expand the entire tree; Otherwise, collapse the entire tree
	 */
	void SetTagTreeItemExpansion(bool bExpand);

	/**
	 * Helper function to set the expansion state of a specific node
	 * 
	 * @param Node		Node to set the expansion state of
	 * @param bExapnd	If true, expand the node; Otherwise, collapse the node
	 */
	void SetTagNodeItemExpansion(TSharedPtr<FDNATagNode> Node, bool bExpand);
	
	/**
	 * Helper function to ensure the tag assets are only tagged with valid tags from
	 * the global library. Strips any invalid tags.
	 */
	void VerifyAssetTagValidity();

	/** Load settings for the tags*/
	void LoadSettings();

	/** Recursive load function to go through all tags in the tree and set the expansion*/
	void LoadTagNodeItemExpansion( TSharedPtr<FDNATagNode> Node );

	/** Recursive function to go through all tags in the tree and set the expansion to default*/
	void SetDefaultTagNodeItemExpansion( TSharedPtr<FDNATagNode> Node );

	/** Expansion changed callback */
	void OnExpansionChanged( TSharedPtr<FDNATagNode> InItem, bool bIsExpanded );

	void SetContainer(FDNATagContainer* OriginalContainer, FDNATagContainer* EditedContainer, UObject* OwnerObj);
};
