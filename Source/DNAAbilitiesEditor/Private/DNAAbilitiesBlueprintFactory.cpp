// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_DNAAbilitiesBlueprint.h"
#include "DNAAbilitiesBlueprintFactory.h"
#include "InputCoreTypes.h"
#include "UObject/Interface.h"
#include "Layout/Visibility.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "Abilities/DNAAbility.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "Editor.h"
#include "EdGraphSchema_K2.h"

#include "ClassViewerModule.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "BlueprintEditorSettings.h"

#include "DNAAbilityBlueprint.h"
#include "DNAAbilityGraph.h"
#include "DNAAbilityGraphSchema.h"

#include "ClassViewerFilter.h"

#include "SlateOptMacros.h"

#define LOCTEXT_NAMESPACE "UDNAAbilitiesBlueprintFactory"


// ------------------------------------------------------------------------------
// Dialog to configure creation properties
// ------------------------------------------------------------------------------
BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

class SDNAAbilityBlueprintCreateDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDNAAbilityBlueprintCreateDialog){}

	SLATE_END_ARGS()

		/** Constructs this widget with InArgs */
		void Construct(const FArguments& InArgs)
	{
			bOkClicked = false;
			ParentClass = UDNAAbility::StaticClass();

			ChildSlot
				[
					SNew(SBorder)
					.Visibility(EVisibility::Visible)
					.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
					[
						SNew(SBox)
						.Visibility(EVisibility::Visible)
						.WidthOverride(500.0f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.FillHeight(1)
							[
								SNew(SBorder)
								.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
								.Content()
								[
									SAssignNew(ParentClassContainer, SVerticalBox)
								]
							]

							// Ok/Cancel buttons
							+ SVerticalBox::Slot()
								.AutoHeight()
								.HAlign(HAlign_Right)
								.VAlign(VAlign_Bottom)
								.Padding(8)
								[
									SNew(SUniformGridPanel)
									.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
									.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
									.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
									+ SUniformGridPanel::Slot(0, 0)
									[
										SNew(SButton)
										.HAlign(HAlign_Center)
										.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
										.OnClicked(this, &SDNAAbilityBlueprintCreateDialog::OkClicked)
										.Text(LOCTEXT("CreateDNAAbilityBlueprintOk", "OK"))
									]
									+ SUniformGridPanel::Slot(1, 0)
										[
											SNew(SButton)
											.HAlign(HAlign_Center)
											.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
											.OnClicked(this, &SDNAAbilityBlueprintCreateDialog::CancelClicked)
											.Text(LOCTEXT("CreateDNAAbilityBlueprintCancel", "Cancel"))
										]
								]
						]
					]
				];

			MakeParentClassPicker();
		}

	/** Sets properties for the supplied DNAAbilitiesBlueprintFactory */
	bool ConfigureProperties(TWeakObjectPtr<UDNAAbilitiesBlueprintFactory> InDNAAbilitiesBlueprintFactory)
	{
		DNAAbilitiesBlueprintFactory = InDNAAbilitiesBlueprintFactory;

		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(LOCTEXT("CreateDNAAbilityBlueprintOptions", "Create DNA Ability Blueprint"))
			.ClientSize(FVector2D(400, 700))
			.SupportsMinimize(false).SupportsMaximize(false)
			[
				AsShared()
			];

		PickerWindow = Window;

		GEditor->EditorAddModalWindow(Window);
		DNAAbilitiesBlueprintFactory.Reset();

		return bOkClicked;
	}

private:
	class FDNAAbilityBlueprintParentFilter : public IClassViewerFilter
	{
	public:
		/** All children of these classes will be included unless filtered out by another setting. */
		TSet< const UClass* > AllowedChildrenOfClasses;

		FDNAAbilityBlueprintParentFilter() {}

		virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
		{
			// If it appears on the allowed child-of classes list (or there is nothing on that list)
			return InFilterFuncs->IfInChildOfClassesSet(AllowedChildrenOfClasses, InClass) != EFilterReturn::Failed;
		}

		virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
		{
			// If it appears on the allowed child-of classes list (or there is nothing on that list)
			return InFilterFuncs->IfInChildOfClassesSet(AllowedChildrenOfClasses, InUnloadedClassData) != EFilterReturn::Failed;
		}
	};

	/** Creates the combo menu for the parent class */
	void MakeParentClassPicker()
	{
		// Load the classviewer module to display a class picker
		FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");

		// Fill in options
		FClassViewerInitializationOptions Options;
		Options.Mode = EClassViewerMode::ClassPicker;

		// Only allow parenting to base blueprints.
		Options.bIsBlueprintBaseOnly = true;

		TSharedPtr<FDNAAbilityBlueprintParentFilter> Filter = MakeShareable(new FDNAAbilityBlueprintParentFilter);

		// All child child classes of UDNAAbility are valid.
		Filter->AllowedChildrenOfClasses.Add(UDNAAbility::StaticClass());
		Options.ClassFilter = Filter;

		ParentClassContainer->ClearChildren();
		ParentClassContainer->AddSlot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ParentClass", "Parent Class:"))
				.ShadowOffset(FVector2D(1.0f, 1.0f))
			];

		ParentClassContainer->AddSlot()
			[
				ClassViewerModule.CreateClassViewer(Options, FOnClassPicked::CreateSP(this, &SDNAAbilityBlueprintCreateDialog::OnClassPicked))
			];
	}

	/** Handler for when a parent class is selected */
	void OnClassPicked(UClass* ChosenClass)
	{
		ParentClass = ChosenClass;
	}

	/** Handler for when ok is clicked */
	FReply OkClicked()
	{
		if (DNAAbilitiesBlueprintFactory.IsValid())
		{
			DNAAbilitiesBlueprintFactory->BlueprintType = BPTYPE_Normal;
			DNAAbilitiesBlueprintFactory->ParentClass = ParentClass.Get();
		}

		CloseDialog(true);

		return FReply::Handled();
	}

	void CloseDialog(bool bWasPicked = false)
	{
		bOkClicked = bWasPicked;
		if (PickerWindow.IsValid())
		{
			PickerWindow.Pin()->RequestDestroyWindow();
		}
	}

	/** Handler for when cancel is clicked */
	FReply CancelClicked()
	{
		CloseDialog();
		return FReply::Handled();
	}

	FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
	{
		if (InKeyEvent.GetKey() == EKeys::Escape)
		{
			CloseDialog();
			return FReply::Handled();
		}
		return SWidget::OnKeyDown(MyGeometry, InKeyEvent);
	}

private:
	/** The factory for which we are setting up properties */
	TWeakObjectPtr<UDNAAbilitiesBlueprintFactory> DNAAbilitiesBlueprintFactory;

	/** A pointer to the window that is asking the user to select a parent class */
	TWeakPtr<SWindow> PickerWindow;

	/** The container for the Parent Class picker */
	TSharedPtr<SVerticalBox> ParentClassContainer;

	/** The selected class */
	TWeakObjectPtr<UClass> ParentClass;

	/** True if Ok was clicked */
	bool bOkClicked;
};

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

/*------------------------------------------------------------------------------
	UDNAAbilitiesBlueprintFactory implementation.
------------------------------------------------------------------------------*/

UDNAAbilitiesBlueprintFactory::UDNAAbilitiesBlueprintFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UDNAAbilityBlueprint::StaticClass();
	ParentClass = UDNAAbility::StaticClass();
}

bool UDNAAbilitiesBlueprintFactory::ConfigureProperties()
{
	TSharedRef<SDNAAbilityBlueprintCreateDialog> Dialog = SNew(SDNAAbilityBlueprintCreateDialog);
	return Dialog->ConfigureProperties(this);
};

UObject* UDNAAbilitiesBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	// Make sure we are trying to factory a DNA ability blueprint, then create and init one
	check(Class->IsChildOf(UDNAAbilityBlueprint::StaticClass()));

	// If they selected an interface, force the parent class to be UInterface
	if (BlueprintType == BPTYPE_Interface)
	{
		ParentClass = UInterface::StaticClass();
	}

	if ( ( ParentClass == NULL ) || !FKismetEditorUtilities::CanCreateBlueprintOfClass(ParentClass) || !ParentClass->IsChildOf(UDNAAbility::StaticClass()) )
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("ClassName"), (ParentClass != NULL) ? FText::FromString( ParentClass->GetName() ) : LOCTEXT("Null", "(null)") );
		FMessageDialog::Open( EAppMsgType::Ok, FText::Format( LOCTEXT("CannotCreateDNAAbilityBlueprint", "Cannot create a DNA Ability Blueprint based on the class '{ClassName}'."), Args ) );
		return NULL;
	}
	else
	{
		UDNAAbilityBlueprint* NewBP = CastChecked<UDNAAbilityBlueprint>(FKismetEditorUtilities::CreateBlueprint(ParentClass, InParent, Name, BlueprintType, UDNAAbilityBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), CallingContext));

		if (NewBP)
		{
			UDNAAbilityBlueprint* AbilityBP = UDNAAbilityBlueprint::FindRootDNAAbilityBlueprint(NewBP);
			if (AbilityBP == NULL)
			{
				const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

				// Only allow a DNA ability graph if there isn't one in a parent blueprint
				UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(NewBP, TEXT("DNA Ability Graph"), UDNAAbilityGraph::StaticClass(), UDNAAbilityGraphSchema::StaticClass());
#if WITH_EDITORONLY_DATA
				if (NewBP->UbergraphPages.Num())
				{
					FBlueprintEditorUtils::RemoveGraphs(NewBP, NewBP->UbergraphPages);
				}
#endif
				FBlueprintEditorUtils::AddUbergraphPage(NewBP, NewGraph);
				NewBP->LastEditedDocuments.Add(NewGraph);
				NewGraph->bAllowDeletion = false;

				UBlueprintEditorSettings* Settings = GetMutableDefault<UBlueprintEditorSettings>();
				if(Settings && Settings->bSpawnDefaultBlueprintNodes)
				{
					int32 NodePositionY = 0;
					FKismetEditorUtilities::AddDefaultEventNode(NewBP, NewGraph, FName(TEXT("K2_ActivateAbility")), UDNAAbility::StaticClass(), NodePositionY);
					FKismetEditorUtilities::AddDefaultEventNode(NewBP, NewGraph, FName(TEXT("K2_OnEndAbility")), UDNAAbility::StaticClass(), NodePositionY);
				}
			}
		}

		return NewBP;
	}
}

UObject* UDNAAbilitiesBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return FactoryCreateNew(Class, InParent, Name, Flags, Context, Warn, NAME_None);
}

#undef LOCTEXT_NAMESPACE
