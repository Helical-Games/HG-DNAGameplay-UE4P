// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_DNAAbilitiesBlueprint.h"
#include "Misc/MessageDialog.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "DNAAbilitiesEditor.h"
#include "DNAAbilityBlueprint.h"
#include "Abilities/DNAAbility.h"
#include "DNAAbilitiesBlueprintFactory.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FText FAssetTypeActions_DNAAbilitiesBlueprint::GetName() const
{ 
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_DNAAbilitiesBlueprint", "DNA Ability Blueprint"); 
}

FColor FAssetTypeActions_DNAAbilitiesBlueprint::GetTypeColor() const
{
	return FColor(0, 96, 128);
}

void FAssetTypeActions_DNAAbilitiesBlueprint::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Blueprint = Cast<UBlueprint>(*ObjIt);
		if (Blueprint && Blueprint->SkeletonGeneratedClass && Blueprint->GeneratedClass )
		{
			TSharedRef< FDNAAbilitiesEditor > NewEditor(new FDNAAbilitiesEditor());

			TArray<UBlueprint*> Blueprints;
			Blueprints.Add(Blueprint);

			NewEditor->InitDNAAbilitiesEditor(Mode, EditWithinLevelEditor, Blueprints, ShouldUseDataOnlyEditor(Blueprint));
		}
		else
		{
			FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT("FailedToLoadAbilityBlueprint", "DNA Ability Blueprint could not be loaded because it derives from an invalid class.  Check to make sure the parent class for this blueprint hasn't been removed!"));
		}
	}
}

bool FAssetTypeActions_DNAAbilitiesBlueprint::ShouldUseDataOnlyEditor(const UBlueprint* Blueprint) const
{
	return FBlueprintEditorUtils::IsDataOnlyBlueprint(Blueprint)
		&& !FBlueprintEditorUtils::IsLevelScriptBlueprint(Blueprint)
		&& !FBlueprintEditorUtils::IsInterfaceBlueprint(Blueprint)
		&& !Blueprint->bForceFullEditor
		&& !Blueprint->bIsNewlyCreated;
}

UClass* FAssetTypeActions_DNAAbilitiesBlueprint::GetSupportedClass() const
{ 
	return UDNAAbilityBlueprint::StaticClass(); 
}

UFactory* FAssetTypeActions_DNAAbilitiesBlueprint::GetFactoryForBlueprintType(UBlueprint* InBlueprint) const
{
	UDNAAbilitiesBlueprintFactory* DNAAbilitiesBlueprintFactory = NewObject<UDNAAbilitiesBlueprintFactory>();
	DNAAbilitiesBlueprintFactory->ParentClass = TSubclassOf<UDNAAbility>(*InBlueprint->GeneratedClass);
	return DNAAbilitiesBlueprintFactory;
}

#undef LOCTEXT_NAMESPACE
