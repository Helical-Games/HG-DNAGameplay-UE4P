// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNATagsEditorModulePrivatePCH.h"
#include "DNATagsGraphPanelPinFactory.h"
#include "DNATagsGraphPanelNodeFactory.h"
#include "DNATagContainerCustomization.h"
#include "DNATagQueryCustomization.h"
#include "DNATagCustomization.h"
#include "DNATagsSettings.h"
#include "ISettingsModule.h"
#include "ModuleManager.h"


#define LOCTEXT_NAMESPACE "DNATagEditor"


class FDNATagsEditorModule
	: public IDNATagsEditorModule
{
public:

	// IModuleInterface

	virtual void StartupModule() override
	{
		// Register the details customizer
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomPropertyTypeLayout("DNATagContainer", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FDNATagContainerCustomization::MakeInstance));
		PropertyModule.RegisterCustomPropertyTypeLayout("DNATag", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FDNATagCustomization::MakeInstance));
		PropertyModule.RegisterCustomPropertyTypeLayout("DNATagQuery", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FDNATagQueryCustomization::MakeInstance));

		TSharedPtr<FDNATagsGraphPanelPinFactory> DNATagsGraphPanelPinFactory = MakeShareable( new FDNATagsGraphPanelPinFactory() );
		FEdGraphUtilities::RegisterVisualPinFactory(DNATagsGraphPanelPinFactory);

		TSharedPtr<FDNATagsGraphPanelNodeFactory> DNATagsGraphPanelNodeFactory = MakeShareable(new FDNATagsGraphPanelNodeFactory());
		FEdGraphUtilities::RegisterVisualNodeFactory(DNATagsGraphPanelNodeFactory);

		if (UDNATagsManager::ShouldImportTagsFromINI())
		{
			if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
			{
				SettingsModule->RegisterSettings("Project", "Project", "DNATags",
					LOCTEXT("DNATagSettingsName", "DNATags"),
					LOCTEXT("DNATagSettingsNameDesc", "DNATag Settings"),
					GetMutableDefault<UDNATagsSettings>()
					);

				SettingsModule->RegisterSettings("Project", "Project", "DNATags Developer",
					LOCTEXT("DNATagDeveloperSettingsName", "DNATags Developer"),
					LOCTEXT("DNATagDeveloperSettingsNameDesc", "DNATag Developer Settings"),
					GetMutableDefault<UDNATagsDeveloperSettings>()
					);
			}
		}
	}

	virtual void ShutdownModule() override
	{
		// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
		// we call this function before unloading the module.
	
	
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->UnregisterSettings("Project", "Project", "DNATags");
		}
	}
};


IMPLEMENT_MODULE(FDNATagsEditorModule, DNATagsEditor)

#undef LOCTEXT_NAMESPACE