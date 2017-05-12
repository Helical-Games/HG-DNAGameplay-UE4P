// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
// Meant to be only by SDNACueEditor.h

#pragma once

#include "Core.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"
#include "SDNACueEditor.h"
#include "DNAAbilitiesEditorModule.h"
#include "Widgets/SBoxPanel.h"
#include "SlateOptMacros.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/SListView.h"
#include "EditorStyleSet.h"
#include "Widgets/SWindow.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Styling/SlateIconFinder.h"
#include "EditorClassUtils.h"
#include "Editor.h"

/** Widget for picking a new DNACue Notify class (similar to actor class picker)  */
class SDNACuePickerDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SDNACuePickerDialog )
	{}

		SLATE_ARGUMENT( TSharedPtr<SWindow>, ParentWindow )
		SLATE_ARGUMENT( TArray<UClass*>, DefaultClasses )
		SLATE_ARGUMENT( FString, DNACueTag )

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

	static bool PickDNACue(const FText& TitleText, const TArray<UClass*>& DefaultClasses, UClass*& OutChosenClass, FString InDNACueTag);

private:
	/** Handler for when a class is picked in the class picker */
	void OnClassPicked(UClass* InChosenClass);

	/** Creates the default class widgets */
	TSharedRef<ITableRow> GenerateListRow(UClass* InItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** Handler for when "Ok" we selected in the class viewer */
	FReply OnClassPickerConfirmed();

	FReply OnDefaultClassPicked(UClass* InChosenClass);

	/** Handler for the custom button to hide/unhide the default class viewer */
	void OnDefaultAreaExpansionChanged(bool bExpanded);

	/** Handler for the custom button to hide/unhide the class viewer */
	void OnCustomAreaExpansionChanged(bool bExpanded);	

private:
	/** A pointer to the window that is asking the user to select a parent class */
	TWeakPtr<SWindow> WeakParentWindow;

	/** The class that was last clicked on */
	UClass* ChosenClass;

	/** A flag indicating that Ok was selected */
	bool bPressedOk;

	/**  An array of default classes used in the dialog **/
	TArray<UClass*> DefaultClasses;

	FString DNACueTag;
};

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SDNACuePickerDialog::Construct(const FArguments& InArgs)
{
	WeakParentWindow = InArgs._ParentWindow;
	DefaultClasses = InArgs._DefaultClasses;
	DNACueTag = InArgs._DNACueTag;

	FLinearColor AssetColor = FLinearColor::White;

	bPressedOk = false;
	ChosenClass = NULL;

	FString PathStr = SDNACueEditor::GetPathNameForDNACueTag(DNACueTag);
	FDNACueEditorStrings Strings;
	auto del = IDNAAbilitiesEditorModule::Get().GetDNACueEditorStringsDelegate();
	if (del.IsBound())
	{
		Strings = del.Execute();
	}

	ChildSlot
	[
		SNew(SBorder)
		.Visibility(EVisibility::Visible)
		.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
		[
			SNew(SBox)
			.Visibility(EVisibility::Visible)
			.Padding(2.f)
			.WidthOverride(520.0f)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.Padding(2.f, 2.f)
				.AutoHeight()
				[
					SNew(SBorder)
					.Visibility(EVisibility::Visible)
					.BorderImage( FEditorStyle::GetBrush("AssetThumbnail.AssetBackground") )
					.BorderBackgroundColor(AssetColor.CopyWithNewOpacity(0.3f))
					[
						SNew(SExpandableArea)
						.AreaTitle(NSLOCTEXT("SDNACuePickerDialog", "CommonClassesAreaTitle", "DNACue Notifies"))
						.BodyContent()
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.Padding(2.f, 2.f)
							.AutoHeight()
							[
								SNew(STextBlock)
								
								.Text(FText::FromString(Strings.DNACueNotifyDescription1))
								.AutoWrapText(true)
							]

							+SVerticalBox::Slot()
							.AutoHeight( )
							[
								SNew(SListView < UClass*  >)
								.ItemHeight(48)
								.SelectionMode(ESelectionMode::None)
								.ListItemsSource(&DefaultClasses)
								.OnGenerateRow(this, &SDNACuePickerDialog::GenerateListRow)
							]

							+SVerticalBox::Slot()
							.Padding(2.f, 2.f)
							.AutoHeight()
							[
								SNew(STextBlock)
								.Text(FText::FromString(FString::Printf(TEXT("This will create a new DNACue Notify here:"))))
								.AutoWrapText(true)
							]
							+SVerticalBox::Slot()
							.Padding(2.f, 2.f)
							.AutoHeight()
							[
								SNew(STextBlock)
								.Text(FText::FromString(PathStr))
								.HighlightText(FText::FromString(PathStr))
								.AutoWrapText(true)
							]
							+SVerticalBox::Slot()
							.Padding(2.f, 2.f)
							.AutoHeight()
							[
								SNew(STextBlock)
								.Text(FText::FromString(Strings.DNACueNotifyDescription2))
								.AutoWrapText(true)
							]
						]
					]
				]

				+SVerticalBox::Slot()
				.Padding(2.f, 2.f)
				.AutoHeight()
				[
					SNew(SBorder)
					.Visibility(EVisibility::Visible)
					.BorderImage( FEditorStyle::GetBrush("AssetThumbnail.AssetBackground") )
					.BorderBackgroundColor(AssetColor.CopyWithNewOpacity(0.3f))
					[
						SNew(SExpandableArea)
						.AreaTitle(NSLOCTEXT("SDNACuePickerDialogEvents", "CommonClassesAreaTitleEvents", "Custom BP Events"))
						.BodyContent()
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.Padding(2.f, 2.f)
							.AutoHeight()
							[
								SNew(STextBlock)
								.Text(FText::FromString(Strings.DNACueEventDescription1))
								.AutoWrapText(true)

							]

							+SVerticalBox::Slot()
							.Padding(2.f, 2.f)
							.AutoHeight()
							[
								SNew(STextBlock)
								.Text(FText::FromString(Strings.DNACueEventDescription2))
								.AutoWrapText(true)
							]							
						]
					]
				]
			]
		]
	];
}

/** Spawns window for picking new DNACue handler/notify */
bool SDNACuePickerDialog::PickDNACue(const FText& TitleText, const TArray<UClass*>& DefaultClasses, UClass*& OutChosenClass, FString InDNACueName)
{
	// Create the window to pick the class
	TSharedRef<SWindow> PickerWindow = SNew(SWindow)
		.Title(TitleText)
		.SizingRule( ESizingRule::Autosized )
		.ClientSize( FVector2D( 0.f, 600.f ))
		.SupportsMaximize(false)
		.SupportsMinimize(false);

	TSharedRef<SDNACuePickerDialog> ClassPickerDialog = SNew(SDNACuePickerDialog)
		.ParentWindow(PickerWindow)
		.DefaultClasses(DefaultClasses)
		.DNACueTag(InDNACueName);

	PickerWindow->SetContent(ClassPickerDialog);

	GEditor->EditorAddModalWindow(PickerWindow);

	if (ClassPickerDialog->bPressedOk)
	{
		OutChosenClass = ClassPickerDialog->ChosenClass;
		return true;
	}
	else
	{
		// Ok was not selected, NULL the class
		OutChosenClass = NULL;
		return false;
	}
}

void SDNACuePickerDialog::OnClassPicked(UClass* InChosenClass)
{
	ChosenClass = InChosenClass;
}

/** Generates rows in the list of DNACueNotify classes to pick from */
TSharedRef<ITableRow> SDNACuePickerDialog::GenerateListRow(UClass* ItemClass, const TSharedRef<STableViewBase>& OwnerTable)
{	
	const FSlateBrush* ItemBrush = FSlateIconFinder::FindIconBrushForClass(ItemClass);

	return 
	SNew(STableRow< UClass* >, OwnerTable)
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.MaxHeight(60.0f)
		.Padding(10.0f, 6.0f, 0.0f, 4.0f)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(0.65f)
			[
				SNew(SButton)
				.OnClicked(this, &SDNACuePickerDialog::OnDefaultClassPicked, ItemClass)
				.Content()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.FillWidth(0.12f)
					[
						SNew(SImage)
						.Image(ItemBrush)
					]
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.Padding(4.0f, 0.0f)
					.FillWidth(0.8f)
					[
						SNew(STextBlock)
						.Text(ItemClass->GetDisplayNameText())
					]

				]
			]
			+SHorizontalBox::Slot()
			.Padding(10.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(ItemClass->GetToolTipText(true))
				.AutoWrapText(true)
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				FEditorClassUtils::GetDocumentationLinkWidget(ItemClass)
			]
		]
	];
}

FReply SDNACuePickerDialog::OnDefaultClassPicked(UClass* InChosenClass)
{
	ChosenClass = InChosenClass;
	bPressedOk = true;
	if (WeakParentWindow.IsValid())
	{
		WeakParentWindow.Pin()->RequestDestroyWindow();
	}
	return FReply::Handled();
}

FReply SDNACuePickerDialog::OnClassPickerConfirmed()
{
	if (ChosenClass == NULL)
	{
		FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("EditorFactories", "MustChooseClassWarning", "You must choose a class."));
	}
	else
	{
		bPressedOk = true;

		if (WeakParentWindow.IsValid())
		{
			WeakParentWindow.Pin()->RequestDestroyWindow();
		}
	}
	return FReply::Handled();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
