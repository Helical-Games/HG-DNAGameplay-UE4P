// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DNADebuggerAddonManager.h"
#include "Core.h"
#include "Modules/ModuleManager.h"
#include "Engine/World.h"
#include "DNADebugger.h"
#include "ISettingsModule.h"
#include "DNADebuggerAddonManager.h"
#include "DNADebuggerPlayerManager.h"
#include "DNADebuggerConfig.h"

#include "DNADebuggerExtension_Spectator.h"
#include "DNADebuggerExtension_HUD.h"

#if WITH_EDITOR
#include "PropertyEditorModule.h"
#include "EditorModeRegistry.h"
#include "Editor/DNADebuggerCategoryConfigCustomization.h"
#include "Editor/DNADebuggerExtensionConfigCustomization.h"
#include "Editor/DNADebuggerInputConfigCustomization.h"
#include "Editor/DNADebuggerEdMode.h"
#endif

class FDNADebuggerModule : public IDNADebugger
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	virtual void RegisterCategory(FName CategoryName, IDNADebugger::FOnGetCategory MakeInstanceDelegate, EDNADebuggerCategoryState CategoryState, int32 SlotIdx) override;
	virtual void UnregisterCategory(FName CategoryName) override;
	virtual void NotifyCategoriesChanged() override;
	virtual void RegisterExtension(FName ExtensionName, IDNADebugger::FOnGetExtension MakeInstanceDelegate) override;
	virtual void UnregisterExtension(FName ExtensionName) override;
	virtual void NotifyExtensionsChanged() override;

	ADNADebuggerPlayerManager& GetPlayerManager(UWorld* World);
	void OnWorldInitialized(UWorld* World, const UWorld::InitializationValues IVS);
	
	FDNADebuggerAddonManager AddonManager;
	TMap<TWeakObjectPtr<UWorld>, TWeakObjectPtr<ADNADebuggerPlayerManager>> PlayerManagers;
};

IMPLEMENT_MODULE(FDNADebuggerModule, DNADebugger)

void FDNADebuggerModule::StartupModule()
{
	// This code will execute after your module is loaded into memory (but after global variables are initialized, of course.)
	FWorldDelegates::OnPostWorldInitialization.AddRaw(this, &FDNADebuggerModule::OnWorldInitialized);

	UDNADebuggerConfig* SettingsCDO = UDNADebuggerConfig::StaticClass()->GetDefaultObject<UDNADebuggerConfig>();
	if (SettingsCDO)
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
		if (SettingsModule)
		{
			SettingsModule->RegisterSettings("Project", "Engine", "DNADebugger",
				NSLOCTEXT("DNADebuggerModule", "SettingsName", "DNA Debugger"),
				NSLOCTEXT("DNADebuggerModule", "SettingsDescription", "Settings for the DNA debugger tool."),
				SettingsCDO);
		}

#if WITH_EDITOR
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyEditorModule.RegisterCustomPropertyTypeLayout("DNADebuggerCategoryConfig", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FDNADebuggerCategoryConfigCustomization::MakeInstance));
		PropertyEditorModule.RegisterCustomPropertyTypeLayout("DNADebuggerExtensionConfig", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FDNADebuggerExtensionConfigCustomization::MakeInstance));
		PropertyEditorModule.RegisterCustomPropertyTypeLayout("DNADebuggerInputConfig", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FDNADebuggerInputConfigCustomization::MakeInstance));

		FEditorModeRegistry::Get().RegisterMode<FDNADebuggerEdMode>(FDNADebuggerEdMode::EM_DNADebugger);
#endif

		AddonManager.RegisterExtension("GameHUD", FOnGetExtension::CreateStatic(&FDNADebuggerExtension_HUD::MakeInstance));
		AddonManager.RegisterExtension("Spectator", FOnGetExtension::CreateStatic(&FDNADebuggerExtension_Spectator::MakeInstance));
		AddonManager.NotifyExtensionsChanged();
	}
}

void FDNADebuggerModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	FWorldDelegates::OnPostWorldInitialization.RemoveAll(this);

	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	if (SettingsModule)
	{
		SettingsModule->UnregisterSettings("Project", "Engine", "DNADebugger");
	}

#if WITH_EDITOR
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyEditorModule.UnregisterCustomPropertyTypeLayout("DNADebuggerCategoryConfig");
	PropertyEditorModule.UnregisterCustomPropertyTypeLayout("DNADebuggerExtensionConfig");
	PropertyEditorModule.UnregisterCustomPropertyTypeLayout("DNADebuggerInputConfig");

	FEditorModeRegistry::Get().UnregisterMode(FDNADebuggerEdMode::EM_DNADebugger);
#endif
}

void FDNADebuggerModule::RegisterCategory(FName CategoryName, IDNADebugger::FOnGetCategory MakeInstanceDelegate, EDNADebuggerCategoryState CategoryState, int32 SlotIdx)
{
	AddonManager.RegisterCategory(CategoryName, MakeInstanceDelegate, CategoryState, SlotIdx);
}

void FDNADebuggerModule::UnregisterCategory(FName CategoryName)
{
	AddonManager.UnregisterCategory(CategoryName);
}

void FDNADebuggerModule::NotifyCategoriesChanged()
{
	AddonManager.NotifyCategoriesChanged();
}

void FDNADebuggerModule::RegisterExtension(FName ExtensionName, IDNADebugger::FOnGetExtension MakeInstanceDelegate)
{
	AddonManager.RegisterExtension(ExtensionName, MakeInstanceDelegate);
}

void FDNADebuggerModule::UnregisterExtension(FName ExtensionName)
{
	AddonManager.UnregisterExtension(ExtensionName);
}

void FDNADebuggerModule::NotifyExtensionsChanged()
{
	AddonManager.NotifyExtensionsChanged();
}

FDNADebuggerAddonManager& FDNADebuggerAddonManager::GetCurrent()
{
	FDNADebuggerModule& Module = FModuleManager::LoadModuleChecked<FDNADebuggerModule>("DNADebugger");
	return Module.AddonManager;
}

ADNADebuggerPlayerManager& ADNADebuggerPlayerManager::GetCurrent(UWorld* World)
{
	FDNADebuggerModule& Module = FModuleManager::LoadModuleChecked<FDNADebuggerModule>("DNADebugger");
	return Module.GetPlayerManager(World);
}

ADNADebuggerPlayerManager& FDNADebuggerModule::GetPlayerManager(UWorld* World)
{
	const int32 PurgeInvalidWorldsSize = 5;
	if (PlayerManagers.Num() > PurgeInvalidWorldsSize)
	{
		for (TMap<TWeakObjectPtr<UWorld>, TWeakObjectPtr<ADNADebuggerPlayerManager> >::TIterator It(PlayerManagers); It; ++It)
		{
			if (!It.Key().IsValid())
			{
				It.RemoveCurrent();
			}
			else if (!It.Value().IsValid())
			{
				It.RemoveCurrent();
			}
		}
	}

	TWeakObjectPtr<ADNADebuggerPlayerManager> Manager = PlayerManagers.FindRef(World);
	ADNADebuggerPlayerManager* ManagerOb = Manager.Get();

	if (ManagerOb == nullptr)
	{
		ManagerOb = World->SpawnActor<ADNADebuggerPlayerManager>();
		PlayerManagers.Add(World, ManagerOb);
	}

	check(ManagerOb);
	return *ManagerOb;
}

void FDNADebuggerModule::OnWorldInitialized(UWorld* World, const UWorld::InitializationValues IVS)
{
	// make sure that world has valid player manager, create when it doesn't
	if (World && World->IsGameWorld())
	{
		GetPlayerManager(World);
	}
}
