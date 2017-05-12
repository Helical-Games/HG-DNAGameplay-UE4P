// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_DNAAbilitiesBlueprint.h"
#include "SDNACueEditor.h"
#include "Modules/ModuleManager.h"
#include "DNAAbilitiesEditorModule.h"
#include "Misc/Paths.h"
#include "Stats/StatsMisc.h"
#include "Misc/ScopedSlowTask.h"
#include "UObject/Package.h"
#include "Misc/StringAssetReference.h"
#include "Misc/PackageName.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/SlateTypes.h"
#include "SlateOptMacros.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Views/SExpanderArrow.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorStyleSet.h"
#include "Engine/Blueprint.h"
#include "Factories/BlueprintFactory.h"
#include "AssetData.h"
#include "Engine/ObjectLibrary.h"
#include "Widgets/Input/SComboButton.h"
#include "DNATagContainer.h"
#include "UObject/UnrealType.h"
#include "AbilitySystemLog.h"
#include "DNACueSet.h"
#include "DNACueNotify_Actor.h"
#include "DNATagsManager.h"
#include "DNACueTranslator.h"
#include "DNACueManager.h"
#include "IAssetTools.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "DNACueNotify_Static.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Input/SSearchBox.h"
#include "DNATagsModule.h"
#include "Toolkits/AssetEditorManager.h"
#include "AbilitySystemGlobals.h"
#include "AssetToolsModule.h"
#include "DNATagsEditorModule.h"
#include "Editor/LevelEditor/Public/LevelEditor.h"
#include "AssetRegistryModule.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE "SDNACueEditor"
#include "SDNACueEditor_Picker.h"

static const FName CueTagColumnName("DNACueTags");
static const FName CueHandlerColumnName("DNACueHandlers");

// Whether to show the Hotreload button in the GC editor.
#define DNACUEEDITOR_HOTRELOAD_BUTTON 1

// Whether to enable the "show only leaf tags option", if 0, the option is enabled by default. (This is probably not a useful thing to have, in case it ever is, this can be reenabled)
#define DNACUEEDITOR_SHOW_ONLY_LEAFTAGS_OPTION 0

/** Base class for any item in the the Cue/Handler Tree. */
struct FGCTreeItem : public TSharedFromThis<FGCTreeItem>
{
	FGCTreeItem()
	{
		TranslationUniqueID = 0;
	}
	virtual ~FGCTreeItem(){}


	FName DNACueTagName;
	FDNATag DNACueTag;
	FString Description;

	FStringAssetReference DNACueNotifyObj;
	FStringAssetReference ParentDNACueNotifyObj;
	TWeakObjectPtr<UFunction> FunctionPtr;

	int32 TranslationUniqueID;

	TArray< TSharedPtr< FGCTreeItem > > Children;
};

typedef STreeView< TSharedPtr< FGCTreeItem > > SDNACueTreeView;

// -----------------------------------------------------------------

/** Base class for items in the filtering tree (for DNA cue translator filtering) */
struct FGCFilterTreeItem : public TSharedFromThis<FGCFilterTreeItem>
{
	virtual ~FGCFilterTreeItem() {}
	
	FDNACueTranslationEditorOnlyData Data;	
	TArray<FName> ToNames;
	TArray< TSharedPtr< FGCFilterTreeItem > > Children;
};

typedef STreeView< TSharedPtr< FGCFilterTreeItem > > SFilterTreeView;

// -----------------------------------------------------------------

class SDNACueEditorImpl : public SDNACueEditor
{
public:

	virtual void Construct(const FArguments& InArgs) override;

private:

	// ---------------------------------------------------------------------

	/** Show all GC Tags, even ones without handlers */
	bool bShowAll;

	/** Show all possible overrides, even ones that don't exist */
	bool bShowAllOverrides;

	/** Show only GC Tags that explicitly exist. If a.b.c is in the dictionary, don't show a.b as a distinct tag */
	bool bShowOnlyLeafTags;

	/** Track when filter state is dirty, so that we only rebuilt view when it has changed, once menu is closed. */
	bool bFilterIDsDirty;

	/** Global flag suppress rebuilding cue tree view. Needed when doing operations that would rebuld it multiple times */
	static bool bSuppressCueViewUpdate;

	/** Text box for creating new GC tag */
	TSharedPtr<SEditableTextBox> NewDNACueTextBox;

	/** Main widgets that shows the DNA cue tree */
	TSharedPtr<SDNACueTreeView> DNACueTreeView;

	/** Source of GC tree view items */
	TArray< TSharedPtr< FGCTreeItem > > DNACueListItems;

	/** Widget for the override/transition filters */
	TSharedPtr< SFilterTreeView > FilterTreeView;

	/** Source of filter items */
	TArray< TSharedPtr< FGCFilterTreeItem > > FilterListItems;

	/** Tracking which filters are selected (by transition UniqueIDs) */
	TArray<int32> FilterIds;

	/** Map for viewing GC blueprint events (only built if user wants to) */
	TMultiMap<FDNATag, UFunction*> EventMap;

	/** Last selected tag. Used to keep tag selection in between recreation of GC view */
	FName SelectedTag;

	/** Last selected tag, uniqueID if it came from a translated tag. Used to get the right tag selected (nested vs root) */
	int32 SelectedUniqueID;

	/** Pointer to actual selected item */
	TSharedPtr<FGCTreeItem> SelectedItem;

	/** Search text for highlighting */
	FText SearchText;

	/** The search box widget */
	TSharedPtr< class SSearchBox > SearchBoxPtr;

	/** For tracking expanded tags in between recreation of GC view */
	TSet<FName>	ExpandedTags;

public:

	virtual void OnNewDNACueTagCommited(const FText& InText, ETextCommit::Type InCommitType) override
	{
		// Only support adding tags via ini file
		if (UDNATagsManager::Get().ShouldImportTagsFromINI() == false)
		{
			return;
		}

		if (InCommitType == ETextCommit::OnEnter)
		{
			CreateNewDNACueTag();
		}
	}

	virtual void OnSearchTagCommited(const FText& InText, ETextCommit::Type InCommitType) override
	{
		if (InCommitType == ETextCommit::OnEnter || InCommitType == ETextCommit::OnCleared || InCommitType == ETextCommit::OnUserMovedFocus)
		{
			if (SearchText.EqualTo(InText) == false)
			{
				SearchText = InText;
				UpdateDNACueListItems();
			}
		}
	}

	FReply DoSearch()
	{
		UpdateDNACueListItems();
		return FReply::Handled();
	}


	virtual FReply OnNewDNACueButtonPressed() override
	{
		CreateNewDNACueTag();
		return FReply::Handled();
	}

	
	/** Checks out config file, adds new tag, repopulates widget cue list */
	void CreateNewDNACueTag()
	{
		FScopedSlowTask SlowTask(0.f, LOCTEXT("AddingNewDNAcue", "Adding new DNACue Tag"));
		SlowTask.MakeDialog();

		FString str = NewDNACueTextBox->GetText().ToString();
		if (str.IsEmpty())
		{
			return;
		}

		SelectedTag = FName(*str);
		SelectedUniqueID = 0;

		IDNATagsEditorModule::Get().AddNewDNATagToINI(str);

		UpdateDNACueListItems();

		NewDNACueTextBox->SetText(FText::GetEmpty());
	}

	void OnFilterMenuOpenChanged(bool bOpen)
	{
		if (bOpen == false && bFilterIDsDirty)
		{
			UpdateDNACueListItems();
			bFilterIDsDirty = false;
		}
	}

	void HandleShowAllCheckedStateChanged(ECheckBoxState NewValue)
	{
		bShowAll = (NewValue == ECheckBoxState::Unchecked);
		UpdateDNACueListItems();
	}

	void HandleShowAllOverridesCheckedStateChanged(ECheckBoxState NewValue)
	{
		bShowAllOverrides = (NewValue == ECheckBoxState::Checked);
		UpdateDNACueListItems();
	}

	void HandleShowOnLeafTagsCheckedStateChanged(ECheckBoxState NewValue)
	{
		bShowOnlyLeafTags = (NewValue == ECheckBoxState::Checked);
		UpdateDNACueListItems();
	}

	ECheckBoxState HandleShowAllCheckBoxIsChecked() const
	{
		return bShowAll ? ECheckBoxState::Unchecked : ECheckBoxState::Checked;
	}	
	
	ECheckBoxState HandleShowAllOverridesCheckBoxIsChecked() const
	{
		return bShowAllOverrides ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	ECheckBoxState HandleShowOnlyLeafTagsCheckBoxIsChecked() const
	{
		return bShowOnlyLeafTags ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	void HandleNotify_OpenAssetInEditor(FString AssetName, int AssetType)
	{
		if (AssetType == 0)
		{
			if ( SearchBoxPtr.IsValid() )
			{
				SearchBoxPtr->SetText(FText::FromString(AssetName));
			}

			SearchText = FText::FromString(AssetName);
			UpdateDNACueListItems();

			if (DNACueListItems.Num() == 1)
			{	//If there is only one element, open it.
				if (DNACueListItems[0]->DNACueNotifyObj.IsValid())
				{
					SDNACueEditor::OpenEditorForNotify(DNACueListItems[0]->DNACueNotifyObj.ToString());
				}
				else if (DNACueListItems[0]->FunctionPtr.IsValid())
				{
					SDNACueEditor::OpenEditorForNotify(DNACueListItems[0]->FunctionPtr->GetOuter()->GetPathName());
				}
			}
		}
	}

	void HandleNotify_FindAssetInEditor(FString AssetName, int AssetType)
	{
		if (AssetType == 0)
		{
			if ( SearchBoxPtr.IsValid() )
			{
				SearchBoxPtr->SetText(FText::FromString(AssetName));
			}

			SearchText = FText::FromString(AssetName);
			UpdateDNACueListItems();
		}
	}


	// -----------------------------------------------------------------

	TSharedRef<SWidget> GetFilterListContent()
	{
		if (FilterTreeView.IsValid() == false)
		{
			SAssignNew(FilterTreeView, SFilterTreeView)
			.ItemHeight(24)
			.TreeItemsSource(&FilterListItems)
			.OnGenerateRow(this, &SDNACueEditorImpl::OnGenerateWidgetForFilterListView)
			.OnGetChildren( this, &SDNACueEditorImpl::OnGetFilterChildren )
			.HeaderRow
			(
				SNew(SHeaderRow)
				+ SHeaderRow::Column(CueTagColumnName)
				.DefaultLabel(NSLOCTEXT("DNACueEditor", "DNACueTagTrans", "Translator"))
			);
		}

		UpdateFilterListItems();
		ExpandFilterItems();
		bFilterIDsDirty = false;

		return 
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
		[
			FilterTreeView.ToSharedRef()
		];
	}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

	/** Builds widget for rows in the DNACue Editor tab */
	TSharedRef<ITableRow> OnGenerateWidgetForDNACueListView(TSharedPtr< FGCTreeItem > InItem, const TSharedRef<STableViewBase>& OwnerTable)
	{
		class SCueItemWidget : public SMultiColumnTableRow< TSharedPtr< FGCTreeItem > >
		{
		public:
			SLATE_BEGIN_ARGS(SCueItemWidget){}
			SLATE_END_ARGS()

			void Construct(const FArguments& InArgs, const TSharedRef<SDNACueTreeView>& InOwnerTable, TSharedPtr<FGCTreeItem> InListItem, SDNACueEditorImpl* InDNACueEditor )
			{
				Item = InListItem;
				DNACueEditor = InDNACueEditor;
				SMultiColumnTableRow< TSharedPtr< FGCTreeItem > >::Construct(
					FSuperRowType::FArguments()
				, InOwnerTable);
			}
		private:

			virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
			{
				if (ColumnName == CueTagColumnName)
				{
					return
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SExpanderArrow, SharedThis(this))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.ColorAndOpacity(Item->DNACueTag.IsValid() ? FSlateColor::UseForeground() : FSlateColor::UseSubduedForeground())
						.Text(FText::FromString(Item->Description.IsEmpty() ? Item->DNACueTagName.ToString() : FString::Printf(TEXT("%s (%s)"), *Item->Description, *Item->DNACueTagName.ToString())))
					];
				}
				else if (ColumnName == CueHandlerColumnName)
				{
					if (Item->DNACueNotifyObj.ToString().IsEmpty() == false)
					{
						FString ObjName = Item->DNACueNotifyObj.ToString();

						int32 idx;
						if (ObjName.FindLastChar(TEXT('.'), idx))
						{
							ObjName = ObjName.RightChop(idx + 1);
							if (ObjName.FindLastChar(TEXT('_'), idx))
							{
								ObjName = ObjName.Left(idx);
							}
						}

						return
						SNew(SBox)
						.HAlign(HAlign_Left)
						[
							SNew(SHyperlink)
							.Style(FEditorStyle::Get(), "Common.GotoBlueprintHyperlink")
							.Text(FText::FromString(ObjName))
							.OnNavigate(this, &SCueItemWidget::NavigateToHandler)
						];
					}
					else if (Item->FunctionPtr.IsValid())
					{
						FString ObjName;
						UFunction* Func = Item->FunctionPtr.Get();
						UClass* OuterClass = Cast<UClass>(Func->GetOuter());
						if (OuterClass)
						{
							ObjName = OuterClass->GetName();
							ObjName.RemoveFromEnd(TEXT("_c"));
						}

						return
						SNew(SBox)
						.HAlign(HAlign_Left)
						[
							SNew(SHyperlink)
							.Text(FText::FromString(ObjName))
							.OnNavigate(this, &SCueItemWidget::NavigateToHandler)
						];
					}
					else
					{
						return
						SNew(SBox)
						.HAlign(HAlign_Left)
						[
							SNew(SHyperlink)
							.Style(FEditorStyle::Get(), "Common.GotoNativeCodeHyperlink")
							.Text(LOCTEXT("AddNew", "Add New"))
							.OnNavigate(this, &SCueItemWidget::OnAddNewClicked)
						];
					}
				}
				else
				{
					return SNew(STextBlock).Text(LOCTEXT("UnknownColumn", "Unknown Column"));
				}
			}

			/** Create new DNACueNotify: brings up dialog to pick class, then creates it via the content browser. */
			void OnAddNewClicked()
			{
				{
					// Add the tag if its not already. Note that the FDNATag may be valid as an implicit tag, and calling this
					// will create it as an explicit tag, which is what we want. If the tag already exists

					TGuardValue<bool> SupressUpdate(SDNACueEditorImpl::bSuppressCueViewUpdate, true);
					
					IDNATagsEditorModule::Get().AddNewDNATagToINI(Item->DNACueTagName.ToString());
				}

				UClass* ParentClass=nullptr;
				
				// If this is an override, use the parent GC notify class as the base class
				if (Item->ParentDNACueNotifyObj.IsValid())
				{
					UObject* Obj = Item->ParentDNACueNotifyObj.ResolveObject();
					if (!Obj)
					{
						Obj = Item->ParentDNACueNotifyObj.TryLoad();
					}

					ParentClass = Cast<UClass>(Obj);
					if (ParentClass == nullptr)
					{
						ABILITY_LOG(Warning, TEXT("Unable to resolve object for parent GC notify: %s"), *Item->ParentDNACueNotifyObj.ToString());
					}
				}

				DNACueEditor->OnSelectionChanged(Item, ESelectInfo::Direct);

				SDNACueEditor::CreateNewDNACueNotifyDialogue(Item->DNACueTagName.ToString(), ParentClass);
			}

			void NavigateToHandler()
			{
				if (Item->DNACueNotifyObj.IsValid())
				{
					SDNACueEditor::OpenEditorForNotify(Item->DNACueNotifyObj.ToString());
					
				}
				else if (Item->FunctionPtr.IsValid())
				{
					SDNACueEditor::OpenEditorForNotify(Item->FunctionPtr->GetOuter()->GetPathName());
				}
			}

			TSharedPtr< FGCTreeItem > Item;
			SDNACueEditorImpl* DNACueEditor;
		};

		if ( InItem.IsValid() )
		{
			return SNew(SCueItemWidget, DNACueTreeView.ToSharedRef(), InItem, this);
		}
		else
		{
			return
			SNew(STableRow< TSharedPtr<FGCTreeItem> >, OwnerTable)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("UnknownItemType", "Unknown Item Type"))
			];
		}
	}

	void OnFilterStateChanged(ECheckBoxState NewValue, TSharedPtr< FGCFilterTreeItem > Item)
	{
		if (NewValue == ECheckBoxState::Checked)
		{
			FilterIds.AddUnique(Item->Data.UniqueID);
			bFilterIDsDirty = true;
		}
		else if (NewValue == ECheckBoxState::Unchecked)
		{
			FilterIds.Remove(Item->Data.UniqueID);
			bFilterIDsDirty = true;
		}
	}

	ECheckBoxState IsFilterChecked(TSharedPtr<FGCFilterTreeItem> Item) const
	{
		return FilterIds.Contains(Item->Data.UniqueID) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	TSharedRef<ITableRow> OnGenerateWidgetForFilterListView(TSharedPtr< FGCFilterTreeItem > InItem, const TSharedRef<STableViewBase>& OwnerTable)
	{
		class SFilterItemWidget : public SMultiColumnTableRow< TSharedPtr< FGCFilterTreeItem > >
		{
		public:
			SLATE_BEGIN_ARGS(SFilterItemWidget){}
			SLATE_END_ARGS()

			void Construct(const FArguments& InArgs, const TSharedRef<SFilterTreeView>& InOwnerTable, SDNACueEditorImpl* InDNACueEditor, TSharedPtr<FGCFilterTreeItem> InListItem )
			{
				Item = InListItem;
				DNACueEditor = InDNACueEditor;
				SMultiColumnTableRow< TSharedPtr< FGCFilterTreeItem > >::Construct(FSuperRowType::FArguments(), InOwnerTable);
			}
		private:

			virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
			{
				if (ColumnName == CueTagColumnName)
				{
					return
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SExpanderArrow, SharedThis(this))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1)
					.VAlign(VAlign_Center)
					[
						SNew(SCheckBox)
						.OnCheckStateChanged(DNACueEditor, &SDNACueEditorImpl::OnFilterStateChanged, Item)
						.IsChecked(DNACueEditor, &SDNACueEditorImpl::IsFilterChecked, Item)
						.IsEnabled(Item->Data.Enabled)
						.ToolTipText(FText::FromString(Item->Data.ToolTip))
						[
							SNew(STextBlock)
							.Text(FText::FromString(Item->Data.EditorDescription.ToString()))
							.ToolTipText(FText::FromString(Item->Data.ToolTip))
						]
					];
				}
				else
				{
					return SNew(STextBlock).Text(LOCTEXT("UnknownColumn", "Unknown Column"));
				}
			}

			TSharedPtr< FGCFilterTreeItem > Item;
			SDNACueEditorImpl* DNACueEditor;
		};

		if ( InItem.IsValid() )
		{
			return SNew(SFilterItemWidget, FilterTreeView.ToSharedRef(), this, InItem);
		}
		else
		{
			return
			SNew(STableRow< TSharedPtr<FGCTreeItem> >, OwnerTable)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("UnknownFilterType", "Unknown Filter Type"))
			];
		}
	}

	void OnPropertyValueChanged()
	{
		UpdateDNACueListItems();
	}

	void OnGetChildren(TSharedPtr<FGCTreeItem> Item, TArray< TSharedPtr<FGCTreeItem> >& Children)
	{
		if (Item.IsValid())
		{
			Children.Append(Item->Children);
		}
	}

	void OnGetFilterChildren(TSharedPtr<FGCFilterTreeItem> Item, TArray< TSharedPtr<FGCFilterTreeItem> >& Children)
	{
		if (Item.IsValid())
		{
			Children.Append(Item->Children);
		}
	}

	bool AddChildTranslatedTags_r(FName ThisDNACueTag, UDNACueManager* CueManager, TSharedPtr< FGCTreeItem > NewItem)
	{
		bool ChildPassedFilter = false;
		TArray<FDNACueTranslationEditorInfo> ChildrenTranslatedTags;
		if (CueManager->TranslationManager.GetTranslatedTags(ThisDNACueTag, ChildrenTranslatedTags)) 
		{
			for (FDNACueTranslationEditorInfo& ChildInfo : ChildrenTranslatedTags)
			{
				TSharedPtr< FGCTreeItem > NewHandlerItem(new FGCTreeItem());					
				NewHandlerItem->DNACueTagName = ChildInfo.DNATagName;
				NewHandlerItem->DNACueTag = ChildInfo.DNATag;
				NewHandlerItem->ParentDNACueNotifyObj = NewItem->DNACueNotifyObj.IsValid() ?  NewItem->DNACueNotifyObj :  NewItem->ParentDNACueNotifyObj;

				// Should this be filtered out?
				bool PassedFilter = (FilterIds.Num() == 0 || FilterIds.Contains(ChildInfo.EditorData.UniqueID));
				PassedFilter |= AddChildTranslatedTags_r(ChildInfo.DNATagName, CueManager, NewHandlerItem);
				ChildPassedFilter |= PassedFilter;

				if (PassedFilter)
				{
					FindDNACueNotifyObj(CueManager, NewHandlerItem);
					NewHandlerItem->Description = ChildInfo.EditorData.EditorDescription.ToString();
					NewHandlerItem->TranslationUniqueID = ChildInfo.EditorData.UniqueID;

					NewItem->Children.Add(NewHandlerItem);

					if (ExpandedTags.Contains(NewHandlerItem->DNACueTagName))
					{
						DNACueTreeView->SetItemExpansion(NewHandlerItem, true);
					}

					CheckSelectGCItem(NewHandlerItem);
				}				
			}
		}

		return ChildPassedFilter;
	}

	bool FindDNACueNotifyObj(UDNACueManager* CueManager, TSharedPtr< FGCTreeItem >& Item)
	{
		if (CueManager && Item->DNACueTag.IsValid())
		{
			UDNACueSet* EditorSet = CueManager->GetEditorCueSet();
			if (EditorSet == nullptr)
			{
				return false;
			}

			if (int32* idxPtr = EditorSet->DNACueDataMap.Find(Item->DNACueTag))
			{
				int32 idx = *idxPtr;
				if (EditorSet->DNACueData.IsValidIndex(idx))
				{
					FDNACueNotifyData& Data = EditorSet->DNACueData[idx];
					Item->DNACueNotifyObj = Data.DNACueNotifyObj;
					return true;
				}
			}
		}
		return false;
	}

	void CheckSelectGCItem(TSharedPtr< FGCTreeItem > NewItem)
	{
		if (SelectedTag != NAME_Name && !SelectedItem.IsValid() && (SelectedTag == NewItem->DNACueTagName && NewItem->TranslationUniqueID == SelectedUniqueID))
		{
			SelectedItem = NewItem;
		}
	}

	/** Builds content of the list in the DNACue Editor */
	void UpdateDNACueListItems()
	{
		if (bSuppressCueViewUpdate)
		{
			return;
		}

#if STATS
		//FString PerfMessage = FString::Printf(TEXT("SDNACueEditor::UpdateDNACueListItems"));
		//SCOPE_LOG_TIME_IN_SECONDS(*PerfMessage, nullptr)
#endif
		double FindDNACueNotifyObjTime = 0.f;
		double AddTranslationTagsTime = 0.f;
		double AddEventsTime = 0.f;


		UDNACueManager* CueManager = UDNAAbilitySystemGlobals::Get().GetDNACueManager();
		if (!CueManager)
		{
			return;
		}

		DNACueListItems.Reset();
		SelectedItem.Reset();

		UDNATagsManager& Manager = UDNATagsManager::Get();
		FString FullSearchString = SearchText.ToString();
		TArray<FString> SearchStrings;
		FullSearchString.ParseIntoArrayWS(SearchStrings);

		// ------------------------------------------------------
		if (bShowAllOverrides)
		{
			// Compute all possible override tags via _Forward method
			CueManager->TranslationManager.BuildTagTranslationTable_Forward();
		}
		else
		{
			// Compute only the existing override tags
			CueManager->TranslationManager.BuildTagTranslationTable();
		}
		// ------------------------------------------------------

		// Get all GC Tags
		FDNATagContainer AllDNACueTags;
		{
			FString RequestDNATagChildrenPerfMessage = FString::Printf(TEXT(" RequestDNATagChildren"));
			//SCOPE_LOG_TIME_IN_SECONDS(*RequestDNATagChildrenPerfMessage, nullptr)

			if (bShowOnlyLeafTags)
			{
				AllDNACueTags = Manager.RequestDNATagChildrenInDictionary(UDNACueSet::BaseDNACueTag());
			}
			else
			{
				AllDNACueTags = Manager.RequestDNATagChildren(UDNACueSet::BaseDNACueTag());
			}
		}

		// Create data structs for widgets
		for (const FDNATag& ThisDNACueTag : AllDNACueTags)
		{
			if (SearchStrings.Num() > 0)
			{
				FString DNACueString = ThisDNACueTag.ToString();
				if (!SearchStrings.ContainsByPredicate([&](const FString& SStr) { return DNACueString.Contains(SStr); }))
				{
					continue;
				}
			}
			
			TSharedPtr< FGCTreeItem > NewItem = TSharedPtr<FGCTreeItem>(new FGCTreeItem());
			NewItem->DNACueTag = ThisDNACueTag;
			NewItem->DNACueTagName = ThisDNACueTag.GetTagName();

			bool Handled = false;
			bool FilteredOut = false;

			// Add notifies from the global set
			{
				SCOPE_SECONDS_COUNTER(FindDNACueNotifyObjTime);
				Handled = FindDNACueNotifyObj(CueManager, NewItem);
			}

			CheckSelectGCItem(NewItem);

			// ----------------------------------------------------------------
			// Add children translated tags
			// ----------------------------------------------------------------

			{
				SCOPE_SECONDS_COUNTER(AddTranslationTagsTime);
				AddChildTranslatedTags_r(ThisDNACueTag.GetTagName(), CueManager, NewItem);
			}

			FilteredOut = FilterIds.Num() > 0 && NewItem->Children.Num() == 0;

			// ----------------------------------------------------------------
			// Add events implemented by IDNACueInterface blueprints
			// ----------------------------------------------------------------

			{
				SCOPE_SECONDS_COUNTER(AddEventsTime);

				TArray<UFunction*> Funcs;
				EventMap.MultiFind(ThisDNACueTag, Funcs);

				for (UFunction* Func : Funcs)
				{
					TSharedRef< FGCTreeItem > NewHandlerItem(new FGCTreeItem());
					NewHandlerItem->FunctionPtr = Func;
					NewHandlerItem->DNACueTag = ThisDNACueTag;
					NewHandlerItem->DNACueTagName = ThisDNACueTag.GetTagName();

					if (ensure(NewItem.IsValid()))
					{
						NewItem->Children.Add(NewHandlerItem);
						Handled = true;
					}
				}
			}

			// ----------------------------------------------------------------

			if (!FilteredOut && (bShowAll || Handled))
			{
				DNACueListItems.Add(NewItem);
			}

			if (ExpandedTags.Contains(NewItem->DNACueTagName))
			{
				DNACueTreeView->SetItemExpansion(NewItem, true);
			}
		}

		{
			FString RequestTreeRefreshMessage = FString::Printf(TEXT("  RequestTreeRefresh"));
			//SCOPE_LOG_TIME_IN_SECONDS(*RequestTreeRefreshMessage, nullptr)

			if (DNACueTreeView.IsValid())
			{
				DNACueTreeView->RequestTreeRefresh();
			}

			if (SelectedItem.IsValid())
			{
				DNACueTreeView->SetItemSelection(SelectedItem, true);
				DNACueTreeView->RequestScrollIntoView(SelectedItem);
			}

			//UE_LOG(LogTemp, Display, TEXT("Accumulated FindDNACueNotifyObjTime: %.4f"), FindDNACueNotifyObjTime);
			//UE_LOG(LogTemp, Display, TEXT("Accumulated FindDNACueNotifyObjTime: %.4f"), AddTranslationTagsTime);
			//UE_LOG(LogTemp, Display, TEXT("Accumulated FindDNACueNotifyObjTime: %.4f"), AddEventsTime);
		}
	}

	void UpdateFilterListItems(bool UpdateView=true)
	{
		UDNACueManager* CueManager = UDNAAbilitySystemGlobals::Get().GetDNACueManager();
		if (!CueManager)
		{
			return;
		}

		CueManager->TranslationManager.RefreshNameSwaps();

		const TArray<FNameSwapData>& AllNameSwapData = CueManager->TranslationManager.GetNameSwapData();
		FilterListItems.Reset();

		// Make two passes. In the first pass only add filters to the root if ShouldShowInTopLevelFilterList is true.
		// In the second pass, we only add filters as child nodes. This is to make a nice heirarchy of filters, rather than
		// having "sub" filters appear in the root view.
		for (int32 pass=0; pass < 2; ++pass)
		{
			for (const FNameSwapData& NameSwapGroup : AllNameSwapData)
			{
				for (const FDNACueTranslationNameSwap& NameSwapData : NameSwapGroup.NameSwaps)
				{
					bool Added = false;

					TSharedPtr< FGCFilterTreeItem > NewItem = TSharedPtr<FGCFilterTreeItem>(new FGCFilterTreeItem());
					NewItem->Data = NameSwapData.EditorData;
					NewItem->ToNames = NameSwapData.ToNames;

					// Look for existing entries
					for (TSharedPtr<FGCFilterTreeItem>& FilterItem : FilterListItems)
					{
						if (FilterItem->ToNames.Num() == 1 && NameSwapData.FromName == FilterItem->ToNames[0])
						{
							FilterItem->Children.Add(NewItem);
							Added = true;
						}
					}

					// Add to root, otherwise
					if (pass == 0 && NameSwapGroup.ClassCDO->ShouldShowInTopLevelFilterList())
					{
						FilterListItems.Add(NewItem);
					}
				}
			}
		}

		if (UpdateView && FilterTreeView.IsValid())
		{
			FilterTreeView->RequestTreeRefresh();
		}
	}

	void ExpandFilterItems()
	{
		// Expand filter items that are checked. This is to prevent people forgetting they have leaf nodes checked and enabled but not obvious in the UI
		// (E.g., they enable a filter, then collapse its parent. Then close override menu. Everytime they open override menu, we want to show default expansion)
		struct local
		{
			static bool ExpandFilterItems_r(TArray<TSharedPtr<FGCFilterTreeItem> >& Items, TArray<int32>& FilterIds, SFilterTreeView* FilterTreeView )
			{
				bool ShouldBeExpanded = false;
				for (TSharedPtr<FGCFilterTreeItem>& FilterItem : Items)
				{
					ShouldBeExpanded |= FilterIds.Contains(FilterItem->Data.UniqueID);
					if (ExpandFilterItems_r(FilterItem->Children, FilterIds, FilterTreeView))
					{
						FilterTreeView->SetItemExpansion(FilterItem, true);
						ShouldBeExpanded = true;
					}
				}
				return ShouldBeExpanded;
			}

		};

		local::ExpandFilterItems_r(FilterListItems, FilterIds, FilterTreeView.Get());
	}

	/** Slow task: use asset registry to find blueprints, load an inspect them to see what GC events they implement. */
	FReply BuildEventMap()
	{
		FScopedSlowTask SlowTask(100.f, LOCTEXT("BuildEventMap", "Searching Blueprints for DNACue events"));
		SlowTask.MakeDialog();
		SlowTask.EnterProgressFrame(10.f);

		EventMap.Empty();

		UDNATagsManager& Manager = UDNATagsManager::Get();

		auto del = IDNAAbilitiesEditorModule::Get().GetDNACueInterfaceClassesDelegate();
		if (del.IsBound())
		{
			TArray<UClass*> InterfaceClasses;
			del.ExecuteIfBound(InterfaceClasses);
			float WorkPerClass = InterfaceClasses.Num() > 0 ? 90.f / static_cast<float>(InterfaceClasses.Num()) : 0.f;

			for (UClass* InterfaceClass : InterfaceClasses)
			{
				SlowTask.EnterProgressFrame(WorkPerClass);

				TArray<FAssetData> DNACueInterfaceActors;
				{
#if STATS
					FString PerfMessage = FString::Printf(TEXT("Searched asset registry %s "), *InterfaceClass->GetName());
					SCOPE_LOG_TIME_IN_SECONDS(*PerfMessage, nullptr)
#endif

					UObjectLibrary* ObjLibrary = UObjectLibrary::CreateLibrary(InterfaceClass, true, true);
					ObjLibrary->LoadBlueprintAssetDataFromPath(TEXT("/Game/"));
					ObjLibrary->GetAssetDataList(DNACueInterfaceActors);
				}

			{
#if STATS
				FString PerfMessage = FString::Printf(TEXT("Fully Loaded DNACueNotify actors %s "), *InterfaceClass->GetName());
				SCOPE_LOG_TIME_IN_SECONDS(*PerfMessage, nullptr)
#endif				

					for (const FAssetData& AssetData : DNACueInterfaceActors)
					{
						UBlueprint* BP = Cast<UBlueprint>(AssetData.GetAsset());
						if (BP)
						{
							for (TFieldIterator<UFunction> FuncIt(BP->GeneratedClass, EFieldIteratorFlags::ExcludeSuper); FuncIt; ++FuncIt)
							{
								FString FuncName = FuncIt->GetName();
								if (FuncName.Contains("DNACue"))
								{
									FuncName.ReplaceInline(TEXT("_"), TEXT("."));
									FDNATag FoundTag = Manager.RequestDNATag(FName(*FuncName), false);
									if (FoundTag.IsValid())
									{
										EventMap.AddUnique(FoundTag, *FuncIt);
									}
								}

							}
						}
					}
				}
			}

			UpdateDNACueListItems();
		}

		return FReply::Handled();
	}

	void OnExpansionChanged( TSharedPtr<FGCTreeItem> InItem, bool bIsExpanded )
	{
		if (bIsExpanded)
		{
			ExpandedTags.Add(InItem->DNACueTagName);
		}
		else
		{
			ExpandedTags.Remove(InItem->DNACueTagName);
		}
	}

	void OnSelectionChanged( TSharedPtr<FGCTreeItem> InItem, ESelectInfo::Type SelectInfo )
	{
		if (InItem.IsValid())
		{
			SelectedTag = InItem->DNACueTagName;
			SelectedUniqueID = InItem->TranslationUniqueID;
		}
		else
		{
			SelectedTag = NAME_None;
			SelectedUniqueID = INDEX_NONE;
		}
	}

	void HandleOverrideTypeChange(bool NewValue)
	{
		bShowAllOverrides = NewValue;
		UpdateDNACueListItems();
	}
	
	TSharedRef<SWidget> OnGetShowOverrideTypeMenu()
	{
		FMenuBuilder MenuBuilder( true, NULL );

		FUIAction YesAction( FExecuteAction::CreateSP( this, &SDNACueEditorImpl::HandleOverrideTypeChange, true ) );
		MenuBuilder.AddMenuEntry( GetOverrideTypeDropDownText_Explicit(true), LOCTEXT("DNACueEditor", "Show ALL POSSIBLE tags for overrides: including Tags that could exist but currently dont"), FSlateIcon(), YesAction );

		FUIAction NoAction( FExecuteAction::CreateSP( this, &SDNACueEditorImpl::HandleOverrideTypeChange, false ) );
		MenuBuilder.AddMenuEntry( GetOverrideTypeDropDownText_Explicit(false), LOCTEXT("DNACueEditor", "ONLY show tags for overrides that exist/have been setup."), FSlateIcon(), NoAction );

		return MenuBuilder.MakeWidget();
	}

	FText GetOverrideTypeDropDownText() const
	{
		return GetOverrideTypeDropDownText_Explicit(bShowAllOverrides);
	}
	
	FText GetOverrideTypeDropDownText_Explicit(bool ShowAll) const
	{
		if (ShowAll)
		{
			return LOCTEXT("ShowAllOverrides_Tooltip_Yes", "Show all possible overrides");
		}

		return LOCTEXT("ShowAllOverrides_Tooltip_No", "Show only existing overrides");
	}

	EVisibility GetWaitingOnAssetRegistryVisiblity() const
	{
		if (UDNACueManager* CueManager = UDNAAbilitySystemGlobals::Get().GetDNACueManager())
		{
			return CueManager->EditorObjectLibraryFullyInitialized ? EVisibility::Collapsed : EVisibility::Visible;
		}
		return EVisibility::Visible;
	}
};

bool SDNACueEditorImpl::bSuppressCueViewUpdate = false;

static FReply RecompileDNACueEditor_OnClicked()
{	
	GEngine->DeferredCommands.Add( TEXT( "DNAAbilitiesEditor.HotReload" ) );
	return FReply::Handled();
}

void SDNACueEditorImpl::Construct(const FArguments& InArgs)
{
	UDNACueManager* CueManager = UDNAAbilitySystemGlobals::Get().GetDNACueManager();
	if (CueManager)
	{
		CueManager->OnDNACueNotifyAddOrRemove.AddSP(this, &SDNACueEditorImpl::OnPropertyValueChanged);
		CueManager->OnEditorObjectLibraryUpdated.AddSP(this, &SDNACueEditorImpl::UpdateDNACueListItems);
		CueManager->RequestPeriodicUpdateOfEditorObjectLibraryWhileWaitingOnAssetRegistry();
	}

	bShowAll = true;
	bShowAllOverrides = false;
	bShowOnlyLeafTags = true;
	bFilterIDsDirty = false;

	bool CanAddFromINI = UDNATagsManager::Get().ShouldImportTagsFromINI(); // We only support adding new tags to the ini files.
	
	ChildSlot
	[
		SNew(SVerticalBox)

		// -- Hot Reload -------------------------------------------------
#if DNACUEEDITOR_HOTRELOAD_BUTTON
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(2.0f, 2.0f)
			.AutoWidth()
			[

				SNew(SButton)
				.Text(LOCTEXT("HotReload", "Hot Reload"))
				.OnClicked(FOnClicked::CreateStatic(&RecompileDNACueEditor_OnClicked))
			]
		]
#endif
		// -------------------------------------------------------------

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(2.0f, 2.0f)
			.AutoWidth()
			[

				SNew(SButton)
				.Text(LOCTEXT("SearchBPEvents", "Search BP Events"))
				.OnClicked(this, &SDNACueEditorImpl::BuildEventMap)
			]

			+ SHorizontalBox::Slot()
			.Padding(2.0f, 2.0f)
			.AutoWidth()
			[
				SNew(SCheckBox)
				.IsChecked(this, &SDNACueEditorImpl::HandleShowAllCheckBoxIsChecked)
				.OnCheckStateChanged(this, &SDNACueEditorImpl::HandleShowAllCheckedStateChanged)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("HideUnhandled", "Hide Unhandled DNACues"))
				]
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(2.0f, 2.0f)
			.AutoWidth()
			[
				SAssignNew(NewDNACueTextBox, SEditableTextBox)
				.MinDesiredWidth(210.0f)
				.HintText(LOCTEXT("DNACueXY", "DNACue.X.Y"))
				.OnTextCommitted(this, &SDNACueEditorImpl::OnNewDNACueTagCommited)
			]

			+ SHorizontalBox::Slot()
			.Padding(2.0f, 2.0f)
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("AddNew", "Add New"))
				.OnClicked(this, &SDNACueEditorImpl::OnNewDNACueButtonPressed)
				.Visibility(CanAddFromINI ? EVisibility::Visible : EVisibility::Collapsed)
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(2.0f)
			.AutoWidth()
			[
				SAssignNew( SearchBoxPtr, SSearchBox )
				.MinDesiredWidth(210.0f)
				.OnTextCommitted(this, &SDNACueEditorImpl::OnSearchTagCommited)
			]
			+ SHorizontalBox::Slot()
			.Padding(2.0f)
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("Search", "Search"))
				.OnClicked(this, &SDNACueEditorImpl::DoSearch)
			]
		]

		// ---------------------------------------------------------------
		
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(2.0f)
			.AutoWidth()
			[
				SNew(SComboButton)
				.OnGetMenuContent(this, &SDNACueEditorImpl::GetFilterListContent)
				.OnMenuOpenChanged( this, &SDNACueEditorImpl::OnFilterMenuOpenChanged )
				.ContentPadding(FMargin(2.0f, 2.0f))
				//.Visibility(this, &FScalableFloatDetails::GetRowNameVisibility)
				.ButtonContent()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("DNACueOverrides", "Override Filter" ))//&FScalableFloatDetails::GetRowNameComboBoxContentText)
				]
			]
			
			+ SHorizontalBox::Slot()
			.Padding(2.0f, 2.0f)
			.AutoWidth()
			[
				SNew( SComboButton )
				.OnGetMenuContent( this, &SDNACueEditorImpl::OnGetShowOverrideTypeMenu )
				.VAlign( VAlign_Center )
				.ContentPadding(2)
				.ButtonContent()
				[
					SNew( STextBlock )
					.ToolTipText( LOCTEXT("ShowOverrideType", "Toggles how we display overrides. Either show the existing overrides, or show possible overrides") )
					.Text( this, &SDNACueEditorImpl::GetOverrideTypeDropDownText )
				]

				/*
				SNew(SCheckBox)
				.IsChecked(this, &SDNACueEditorImpl::HandleShowAllOverridesCheckBoxIsChecked)
				.OnCheckStateChanged(this, &SDNACueEditorImpl::HandleShowAllOverridesCheckedStateChanged)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ShowAllOverrides", "Show all possible overrides"))
				]
				*/


			]

#if DNACUEEDITOR_SHOW_ONLY_LEAFTAGS_OPTION
			+ SHorizontalBox::Slot()
			.Padding(2.0f, 2.0f)
			.AutoWidth()
			[
				SNew(SCheckBox)
				.IsChecked(this, &SDNACueEditorImpl::HandleShowOnlyLeafTagsCheckBoxIsChecked)
				.OnCheckStateChanged(this, &SDNACueEditorImpl::HandleShowOnLeafTagsCheckedStateChanged)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ShowLeafTagsOnly", "Show leaf tags only"))
				]
			]
#endif			
		]
		
		// ---------------------------------------------------------------

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(2.0f)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("WaitingOnAssetRegister", "Waiting on Asset Registry to finish loading (all tags are present but some GC Notifies may not yet be discovered)"))
				.Visibility(this, &SDNACueEditorImpl::GetWaitingOnAssetRegistryVisiblity)
			]
		]
		
		// ---------------------------------------------------------------

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolBar.Background"))
			[
				SAssignNew(DNACueTreeView, SDNACueTreeView)
				.ItemHeight(24)
				.TreeItemsSource(&DNACueListItems)
				.OnGenerateRow(this, &SDNACueEditorImpl::OnGenerateWidgetForDNACueListView)
				.OnGetChildren(this, &SDNACueEditorImpl::OnGetChildren )
				.OnExpansionChanged(this, &SDNACueEditorImpl::OnExpansionChanged)
				.OnSelectionChanged(this, &SDNACueEditorImpl::OnSelectionChanged)
				.HeaderRow
				(
					SNew(SHeaderRow)
					+ SHeaderRow::Column(CueTagColumnName)
					.DefaultLabel(NSLOCTEXT("DNACueEditor", "DNACueTag", "Tag"))
					.FillWidth(0.50)
					

					+ SHeaderRow::Column(CueHandlerColumnName)
					.DefaultLabel(NSLOCTEXT("DNACueEditor", "DNACueHandlers", "Handlers"))
					//.FillWidth(1000)
				)
			]
		]
	];

	UpdateDNACueListItems();
	UpdateFilterListItems();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION


// -----------------------------------------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------------------------------------

TSharedRef<SDNACueEditor> SDNACueEditor::New()
{
	return MakeShareable(new SDNACueEditorImpl());
}

void SDNACueEditor::CreateNewDNACueNotifyDialogue(FString DNACue, UClass* ParentClass)
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	// If there already is a parent class, use that. Otherwise the developer must select which class to use.
	UClass* ChosenClass = ParentClass;
	if (ChosenClass == nullptr)
	{

		TArray<UClass*>	NotifyClasses;
		auto del = IDNAAbilitiesEditorModule::Get().GetDNACueNotifyClassesDelegate();
		del.ExecuteIfBound(NotifyClasses);
		if (NotifyClasses.Num() == 0)
		{
			NotifyClasses.Add(UDNACueNotify_Static::StaticClass());
			NotifyClasses.Add(ADNACueNotify_Actor::StaticClass());
		}

		// --------------------------------------------------

		// Null the parent class to ensure one is selected	

		const FText TitleText = LOCTEXT("CreateBlueprintOptions", "New DNACue Handler");
	
		const bool bPressedOk = SDNACuePickerDialog::PickDNACue(TitleText, NotifyClasses, ChosenClass, DNACue);
		if (!bPressedOk)
		{
			return;
		}
	}

	if (ensure(ChosenClass))
	{
		FString NewDefaultPathName = SDNACueEditor::GetPathNameForDNACueTag(DNACue);

		// Make sure the name is unique
		FString AssetName;
		FString PackageName;
		AssetToolsModule.Get().CreateUniqueAssetName(NewDefaultPathName, TEXT(""), /*out*/ PackageName, /*out*/ AssetName);
		const FString PackagePath = FPackageName::GetLongPackagePath(PackageName);

		// Create the DNACue Notify
		UBlueprintFactory* BlueprintFactory = NewObject<UBlueprintFactory>();
		BlueprintFactory->ParentClass = ChosenClass;
		ContentBrowserModule.Get().CreateNewAsset(AssetName, PackagePath, UBlueprint::StaticClass(), BlueprintFactory);
	}
}

void SDNACueEditor::OpenEditorForNotify(FString NotifyFullPath)
{
	// This nonsense is to handle the case where the asset only exists in memory
	// and there for does not have a linker/exist on disk. (The FString version
	// of OpenEditorForAsset does not handle this).
	FStringAssetReference AssetRef(NotifyFullPath);

	UObject* Obj = AssetRef.ResolveObject();
	if (!Obj)
	{
		Obj = AssetRef.TryLoad();
	}

	if (Obj)
	{
		UPackage* pkg = Cast<UPackage>(Obj->GetOuter());
		if (pkg)
		{
			FString AssetName = FPaths::GetBaseFilename(AssetRef.ToString());
			UObject* AssetObject = FindObject<UObject>(pkg, *AssetName);
			FAssetEditorManager::Get().OpenEditorForAsset(AssetObject);
		}
	}
}

FString SDNACueEditor::GetPathNameForDNACueTag(FString DNACueTagName)
{
	FString NewDefaultPathName;
	auto PathDel = IDNAAbilitiesEditorModule::Get().GetDNACueNotifyPathDelegate();
	if (PathDel.IsBound())
	{
		NewDefaultPathName = PathDel.Execute(DNACueTagName);
	}
	else
	{
		DNACueTagName = DNACueTagName.Replace(TEXT("DNACue."), TEXT(""), ESearchCase::IgnoreCase);
		NewDefaultPathName = FString::Printf(TEXT("/Game/GC_%s"), *DNACueTagName);
	}
	NewDefaultPathName.ReplaceInline(TEXT("."), TEXT("_"));
	return NewDefaultPathName;
}

#undef LOCTEXT_NAMESPACE
