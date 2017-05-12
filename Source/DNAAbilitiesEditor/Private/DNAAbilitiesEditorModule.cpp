// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_DNAAbilitiesBlueprint.h"
#include "DNAAbilitiesEditorModule.h"
#include "Stats/StatsMisc.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Textures/SlateIcon.h"
#include "Framework/Docking/TabManager.h"
#include "EditorStyleSet.h"
#include "HAL/IConsoleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "PropertyEditorModule.h"
#include "AttributeDetails.h"
#include "DNAEffectTypes.h"
#include "DNAEffect.h"
#include "Misc/FeedbackContext.h"
#include "DNAAbilitiesModule.h"
#include "DNATagsManager.h"
#include "DNATagsModule.h"
#include "AbilitySystemGlobals.h"
#include "DNAEffectDetails.h"
#include "DNAEffectExecutionScopedModifierInfoDetails.h"
#include "DNAEffectExecutionDefinitionDetails.h"
#include "DNAEffectModifierMagnitudeDetails.h"
#include "DNAModEvaluationChannelSettingsDetails.h"
#include "AttributeBasedFloatDetails.h"
#include "AssetToolsModule.h"
#include "IAssetTypeActions.h"
#include "AssetTypeActions_DNAAbilitiesBlueprint.h"
#include "EdGraphUtilities.h"
#include "DNAAbilitiesGraphPanelPinFactory.h"
#include "DNACueTagDetails.h"
#include "BlueprintActionDatabase.h"
#include "K2Node_DNACueEvent.h"
#include "SDNACueEditor.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Widgets/Docking/SDockTab.h"
#include "LevelEditor.h"
#include "Misc/HotReloadInterface.h"
#include "EditorReimportHandler.h"


class FDNAAbilitiesEditorModule : public IDNAAbilitiesEditorModule
{
	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End IModuleInterface

	TSharedRef<SDockTab> SpawnDNACueEditorTab(const FSpawnTabArgs& Args);
	TSharedPtr<SWidget> SummonDNACueEditorUI();

	FGetDNACueNotifyClasses& GetDNACueNotifyClassesDelegate() override
	{
		return GetDNACueNotifyClasses;
	}

	FGetDNACuePath& GetDNACueNotifyPathDelegate()
	{
		return GetDNACueNotifyPath;
	}

	FGetDNACueInterfaceClasses& GetDNACueInterfaceClassesDelegate()
	{
		return GetDNACueInterfaceClasses;

	}

	FGetDNACueEditorStrings& GetDNACueEditorStringsDelegate()
	{
		return GetDNACueEditorStrings;
	}

protected:
	void RegisterAssetTypeAction(class IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action);
	static void DNATagTreeChanged();

private:

	/** Helper function to apply the DNA mod evaluation channel aliases as display name data to the enum */
	void ApplyDNAModEvaluationChannelAliasesToEnumMetadata();

	/** All created asset type actions.  Cached here so that we can unregister it during shutdown. */
	TArray< TSharedPtr<IAssetTypeActions> > CreatedAssetTypeActions;

	/** Pin factory for abilities graph; Cached so it can be unregistered */
	TSharedPtr<FDNAAbilitiesGraphPanelPinFactory> DNAAbilitiesGraphPanelPinFactory;

	/** Handle to the registered DNATagTreeChanged delegate */
	FDelegateHandle DNATagTreeChangedDelegateHandle;

	FGetDNACueNotifyClasses GetDNACueNotifyClasses;

	FGetDNACuePath GetDNACueNotifyPath;

	FGetDNACueInterfaceClasses GetDNACueInterfaceClasses;
	
	FGetDNACueEditorStrings GetDNACueEditorStrings;

	TWeakPtr<SDockTab> DNACueEditorTab;

	TWeakPtr<SDNACueEditor> DNACueEditor;

public:
	void HandleNotify_OpenAssetInEditor(FString AssetName, int AssetType);
	void HandleNotify_FindAssetInEditor(FString AssetName, int AssetType);
	static void RegisterDebuggingCallbacks();
	


};

IMPLEMENT_MODULE(FDNAAbilitiesEditorModule, DNAAbilitiesEditor)

void FDNAAbilitiesEditorModule::StartupModule()
{
	// Register the details customizer
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout( "DNAAttribute", FOnGetPropertyTypeCustomizationInstance::CreateStatic( &FAttributePropertyDetails::MakeInstance ) );
	PropertyModule.RegisterCustomPropertyTypeLayout( "ScalableFloat", FOnGetPropertyTypeCustomizationInstance::CreateStatic( &FScalableFloatDetails::MakeInstance ) );
	PropertyModule.RegisterCustomPropertyTypeLayout( "DNAEffectExecutionScopedModifierInfo", FOnGetPropertyTypeCustomizationInstance::CreateStatic( &FDNAEffectExecutionScopedModifierInfoDetails::MakeInstance ) );
	PropertyModule.RegisterCustomPropertyTypeLayout( "DNAEffectExecutionDefinition", FOnGetPropertyTypeCustomizationInstance::CreateStatic( &FDNAEffectExecutionDefinitionDetails::MakeInstance ) );
	PropertyModule.RegisterCustomPropertyTypeLayout( "DNAEffectModifierMagnitude", FOnGetPropertyTypeCustomizationInstance::CreateStatic( &FDNAEffectModifierMagnitudeDetails::MakeInstance ) );
	PropertyModule.RegisterCustomPropertyTypeLayout( "DNACueTag", FOnGetPropertyTypeCustomizationInstance::CreateStatic( &FDNACueTagDetails::MakeInstance ) );
	PropertyModule.RegisterCustomPropertyTypeLayout( "DNAModEvaluationChannelSettings", FOnGetPropertyTypeCustomizationInstance::CreateStatic( &FDNAModEvaluationChannelSettingsDetails::MakeInstance ) );
	PropertyModule.RegisterCustomPropertyTypeLayout( "AttributeBasedFloat", FOnGetPropertyTypeCustomizationInstance::CreateStatic( &FAttributeBasedFloatDetails::MakeInstance ) );

	PropertyModule.RegisterCustomClassLayout( "AttributeSet", FOnGetDetailCustomizationInstance::CreateStatic( &FAttributeDetails::MakeInstance ) );
	PropertyModule.RegisterCustomClassLayout( "DNAEffect", FOnGetDetailCustomizationInstance::CreateStatic( &FDNAEffectDetails::MakeInstance ) );

	// Register asset types
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	TSharedRef<IAssetTypeActions> GABAction = MakeShareable(new FAssetTypeActions_DNAAbilitiesBlueprint());
	RegisterAssetTypeAction(AssetTools, GABAction);

	// Register factories for pins and nodes
	DNAAbilitiesGraphPanelPinFactory = MakeShareable(new FDNAAbilitiesGraphPanelPinFactory());
	FEdGraphUtilities::RegisterVisualPinFactory(DNAAbilitiesGraphPanelPinFactory);

	// Listen for changes to the DNA tag tree so we can refresh blueprint actions for the DNACueEvent node
	UDNATagsManager& DNATagsManager = UDNATagsManager::Get();
	DNATagTreeChangedDelegateHandle = IDNATagsModule::OnDNATagTreeChanged.AddStatic(&FDNAAbilitiesEditorModule::DNATagTreeChanged);

	// DNACue editor
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner( FName(TEXT("DNACueApp")), FOnSpawnTab::CreateRaw(this, &FDNAAbilitiesEditorModule::SpawnDNACueEditorTab))
		.SetDisplayName(NSLOCTEXT("DNAAbilitiesEditorModule", "DNACueTabTitle", "DNACue Editor"))
		.SetTooltipText(NSLOCTEXT("DNAAbilitiesEditorModule", "DNACueTooltipText", "Open DNACue Editor tab."))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
		//.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory());
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.EventGraph.ExpandHotPath16"));
		
	ApplyDNAModEvaluationChannelAliasesToEnumMetadata();

#if WITH_HOT_RELOAD
	// This code attempts to relaunch the DNACueEditor tab when you hotreload this module
	if (GIsHotReload && FSlateApplication::IsInitialized())
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>( TEXT("LevelEditor") );
		TSharedPtr<FTabManager> LevelEditorTabManager = LevelEditorModule.GetLevelEditorTabManager();
		LevelEditorTabManager->InvokeTab(FName("DNACueApp"));
	}
#endif // WITH_HOT_RELOAD

	IDNAAbilitiesModule::Get().CallOrRegister_OnDNAAbilitySystemGlobalsReady(FSimpleMulticastDelegate::FDelegate::CreateLambda([] {
		FDNAAbilitiesEditorModule::RegisterDebuggingCallbacks();
	}));
	
	// Invalidate all internal cacheing of FRichCurve* in FScalableFlaots when a UCurveTable is reimported
	FReimportManager::Instance()->OnPostReimport().AddLambda([](UObject* InObject, bool b){ FScalableFloat::InvalidateAllCachedCurves(); });
}

void FDNAAbilitiesEditorModule::HandleNotify_OpenAssetInEditor(FString AssetName, int AssetType)
{
	//Open the DNACue editor if it hasn't been opened.
	if (AssetType == 0)
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		TSharedPtr<FTabManager> LevelEditorTabManager = LevelEditorModule.GetLevelEditorTabManager();
		LevelEditorTabManager->InvokeTab(FName("DNACueApp"));
	}

	//UE_LOG(LogTemp, Display, TEXT("HandleNotify_OpenAssetInEditor!!! %s %d"), *AssetName, AssetType);
	if (DNACueEditor.IsValid())
	{
		DNACueEditor.Pin()->HandleNotify_OpenAssetInEditor(AssetName, AssetType);
	}
}

void FDNAAbilitiesEditorModule::HandleNotify_FindAssetInEditor(FString AssetName, int AssetType)
{
	//Find the DNACue editor if it hasn't been found.
	if (AssetType == 0)
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		TSharedPtr<FTabManager> LevelEditorTabManager = LevelEditorModule.GetLevelEditorTabManager();
		LevelEditorTabManager->InvokeTab(FName("DNACueApp"));
	}

	//UE_LOG(LogTemp, Display, TEXT("HandleNotify_FindAssetInEditor!!! %s %d"), *AssetName, AssetType);
	if (DNACueEditor.IsValid())
	{
		DNACueEditor.Pin()->HandleNotify_FindAssetInEditor(AssetName, AssetType);
	}
}

void FDNAAbilitiesEditorModule::RegisterDebuggingCallbacks()
{
	//register callbacks when Assets are requested to open from the game.
	UDNAAbilitySystemGlobals::Get().AbilityOpenAssetInEditorCallbacks.AddLambda([](FString AssetName, int AssetType) {
		((FDNAAbilitiesEditorModule *)&IDNAAbilitiesEditorModule::Get())->HandleNotify_OpenAssetInEditor(AssetName, AssetType);
	});

	UDNAAbilitySystemGlobals::Get().AbilityFindAssetInEditorCallbacks.AddLambda([](FString AssetName, int AssetType) {
		((FDNAAbilitiesEditorModule *)&IDNAAbilitiesEditorModule::Get())->HandleNotify_FindAssetInEditor(AssetName, AssetType);
	});
}

void FDNAAbilitiesEditorModule::RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action)
{
	AssetTools.RegisterAssetTypeActions(Action);
	CreatedAssetTypeActions.Add(Action);
}

void FDNAAbilitiesEditorModule::DNATagTreeChanged()
{
	// The tag tree changed so we should refresh which actions are provided by the DNA cue event
#if STATS
	FString PerfMessage = FString::Printf(TEXT("FDNAAbilitiesEditorModule::DNATagTreeChanged"));
	SCOPE_LOG_TIME_IN_SECONDS(*PerfMessage, nullptr)
#endif
	FBlueprintActionDatabase::Get().RefreshClassActions(UK2Node_DNACueEvent::StaticClass());
}

void FDNAAbilitiesEditorModule::ShutdownModule()
{
	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner( FName(TEXT("DNACueApp")) );

		if (DNACueEditorTab.IsValid())
		{
			DNACueEditorTab.Pin()->RequestCloseTab();
		}
	}

	// Unregister customizations
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomClassLayout("DNAEffect");
		PropertyModule.UnregisterCustomClassLayout("AttributeSet");

		PropertyModule.UnregisterCustomPropertyTypeLayout("AttributeBasedFloat");
		PropertyModule.UnregisterCustomPropertyTypeLayout("DNAModEvaluationChannelSettings");
		PropertyModule.UnregisterCustomPropertyTypeLayout("DNACueTag");
		PropertyModule.UnregisterCustomPropertyTypeLayout("DNAEffectModifierMagnitude");
		PropertyModule.UnregisterCustomPropertyTypeLayout("DNAEffectExecutionDefinition");
		PropertyModule.UnregisterCustomPropertyTypeLayout("DNAEffectExecutionScopedModifierInfo");
		PropertyModule.UnregisterCustomPropertyTypeLayout("ScalableFloat");
		PropertyModule.UnregisterCustomPropertyTypeLayout("DNAAttribute");
	}

	// Unregister asset type actions
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
		for (auto& AssetTypeAction : CreatedAssetTypeActions)
		{
			if (AssetTypeAction.IsValid())
			{
				AssetToolsModule.UnregisterAssetTypeActions(AssetTypeAction.ToSharedRef());
			}
		}
	}
	CreatedAssetTypeActions.Empty();

	// Unregister graph factories
	if (DNAAbilitiesGraphPanelPinFactory.IsValid())
	{
		FEdGraphUtilities::UnregisterVisualPinFactory(DNAAbilitiesGraphPanelPinFactory);
		DNAAbilitiesGraphPanelPinFactory.Reset();
	}

	if ( UObjectInitialized() && IDNATagsModule::IsAvailable() )
	{
		IDNATagsModule::OnDNATagTreeChanged.Remove(DNATagTreeChangedDelegateHandle);
	}
}


TSharedRef<SDockTab> FDNAAbilitiesEditorModule::SpawnDNACueEditorTab(const FSpawnTabArgs& Args)
{
	return SAssignNew(DNACueEditorTab, SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SummonDNACueEditorUI().ToSharedRef()
		];
}


TSharedPtr<SWidget> FDNAAbilitiesEditorModule::SummonDNACueEditorUI()
{
	TSharedPtr<SWidget> ReturnWidget;
	if( IsInGameThread() )
	{
		TSharedPtr<SDNACueEditor> SharedPtr = SNew(SDNACueEditor);
		ReturnWidget = SharedPtr;
		DNACueEditor = SharedPtr;
	}
	return ReturnWidget;

}

void RecompileDNAAbilitiesEditor(const TArray<FString>& Args)
{
	GWarn->BeginSlowTask( NSLOCTEXT("DNAAbilities", "BeginRecompileDNAAbilitiesTask", "Recompiling DNAAbilitiesEditor Module..."), true);
	
	IHotReloadInterface* HotReload = IHotReloadInterface::GetPtr();
	if(HotReload != nullptr)
	{
		TArray< UPackage* > PackagesToRebind;
		UPackage* Package = FindPackage( NULL, TEXT("/Script/DNAAbilitiesEditor"));
		if( Package != NULL )
		{
			PackagesToRebind.Add( Package );
		}

		HotReload->RebindPackages(PackagesToRebind, TArray<FName>(), true, *GLog);
	}

	GWarn->EndSlowTask();
}

FAutoConsoleCommand RecompileDNAAbilitiesEditorCommand(
	TEXT("DNAAbilitiesEditor.HotReload"),
	TEXT("Recompiles the DNA abilities editor module"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&RecompileDNAAbilitiesEditor)
	);
	
void FDNAAbilitiesEditorModule::ApplyDNAModEvaluationChannelAliasesToEnumMetadata()
{
	UDNAAbilitySystemGlobals* DNAAbilitySystemGlobalsCDO = UDNAAbilitySystemGlobals::StaticClass()->GetDefaultObject<UDNAAbilitySystemGlobals>();
	const UEnum* EvalChannelEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EDNAModEvaluationChannel"));
	if (ensure(EvalChannelEnum) && ensure(DNAAbilitySystemGlobalsCDO))
	{
		const TCHAR* DisplayNameMeta = TEXT("DisplayName");
		const TCHAR* HiddenMeta = TEXT("Hidden");
		const TCHAR* UnusedMeta = TEXT("Unused");

		const int32 NumEnumValues = EvalChannelEnum->NumEnums();
			
		// First mark all of the enum values hidden and unused
		for (int32 EnumValIdx = 0; EnumValIdx < NumEnumValues; ++EnumValIdx)
		{
			EvalChannelEnum->SetMetaData(HiddenMeta, TEXT(""), EnumValIdx);
			EvalChannelEnum->SetMetaData(DisplayNameMeta, UnusedMeta, EnumValIdx);
		}

		// If allowed to use channels, mark the valid ones with aliases
		if (DNAAbilitySystemGlobalsCDO->ShouldAllowDNAModEvaluationChannels())
		{
			const int32 MaxChannelVal = static_cast<int32>(EDNAModEvaluationChannel::Channel_MAX);
			for (int32 AliasIdx = 0; AliasIdx < MaxChannelVal; ++AliasIdx)
			{
				const FName& Alias = DNAAbilitySystemGlobalsCDO->GetDNAModEvaluationChannelAlias(AliasIdx);
				if (!Alias.IsNone())
				{
					EvalChannelEnum->RemoveMetaData(HiddenMeta, AliasIdx);
					EvalChannelEnum->SetMetaData(DisplayNameMeta, *Alias.ToString(), AliasIdx);
				}
			}
		}
		else
		{
			// If not allowed to use channels, also hide the "Evaluate up to channel" option 
			const UEnum* AttributeBasedFloatCalculationTypeEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EAttributeBasedFloatCalculationType"));
			if (ensure(AttributeBasedFloatCalculationTypeEnum))
			{
				const int32 ChannelBasedCalcIdx = AttributeBasedFloatCalculationTypeEnum->GetIndexByValue(static_cast<int64>(EAttributeBasedFloatCalculationType::AttributeMagnitudeEvaluatedUpToChannel));
				AttributeBasedFloatCalculationTypeEnum->SetMetaData(HiddenMeta, TEXT(""), ChannelBasedCalcIdx);
			}
		}
	}
}
