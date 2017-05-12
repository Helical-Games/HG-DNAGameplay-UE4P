// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_DNAAbilitiesBlueprint.h"
#include "DNAAbilitiesEditor.h"
#include "EditorReimportHandler.h"

#if WITH_EDITOR
#include "Editor.h"
#endif



#include "DNAAbilityBlueprint.h"
#include "DNAAbilityGraphSchema.h"
#include "Kismet2/BlueprintEditorUtils.h"

#define LOCTEXT_NAMESPACE "FDNAAbilitiesEditor"


/////////////////////////////////////////////////////
// FDNAAbilitiesEditor

FDNAAbilitiesEditor::FDNAAbilitiesEditor()
{
	// todo: Do we need to register a callback for when properties are changed?
}

FDNAAbilitiesEditor::~FDNAAbilitiesEditor()
{
	FEditorDelegates::OnAssetPostImport.RemoveAll(this);
	FReimportManager::Instance()->OnPostReimport().RemoveAll(this);
	
	// NOTE: Any tabs that we still have hanging out when destroyed will be cleaned up by FBaseToolkit's destructor
}

void FDNAAbilitiesEditor::InitDNAAbilitiesEditor(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, const TArray<UBlueprint*>& InBlueprints, bool bShouldOpenInDefaultsMode)
{
	InitBlueprintEditor(Mode, InitToolkitHost, InBlueprints, bShouldOpenInDefaultsMode);

	for (auto Blueprint : InBlueprints)
	{
		EnsureDNAAbilityBlueprintIsUpToDate(Blueprint);
	}
}

void FDNAAbilitiesEditor::EnsureDNAAbilityBlueprintIsUpToDate(UBlueprint* Blueprint)
{
#if WITH_EDITORONLY_DATA
	int32 Count = Blueprint->UbergraphPages.Num();
	for (auto Graph : Blueprint->UbergraphPages)
	{
		// remove the default event graph, if it exists, from existing DNA Ability Blueprints
		if (Graph->GetName() == "EventGraph" && Graph->Nodes.Num() == 0)
		{
			check(!Graph->Schema->GetClass()->IsChildOf(UDNAAbilityGraphSchema::StaticClass()));
			FBlueprintEditorUtils::RemoveGraph(Blueprint, Graph);
			break;
		}
	}
#endif
}

// FRED_TODO: don't merge this back
// FName FDNAAbilitiesEditor::GetToolkitContextFName() const
// {
// 	return GetToolkitFName();
// }

FName FDNAAbilitiesEditor::GetToolkitFName() const
{
	return FName("DNAAbilitiesEditor");
}

FText FDNAAbilitiesEditor::GetBaseToolkitName() const
{
	return LOCTEXT("DNAAbilitiesEditorAppLabel", "DNA Abilities Editor");
}

FText FDNAAbilitiesEditor::GetToolkitName() const
{
	const TArray<UObject*>& EditingObjs = GetEditingObjects();

	check(EditingObjs.Num() > 0);

	FFormatNamedArguments Args;

	const UObject* EditingObject = EditingObjs[0];

	const bool bDirtyState = EditingObject->GetOutermost()->IsDirty();

	Args.Add(TEXT("ObjectName"), FText::FromString(EditingObject->GetName()));
	Args.Add(TEXT("DirtyState"), bDirtyState ? FText::FromString(TEXT("*")) : FText::GetEmpty());
	return FText::Format(LOCTEXT("DNAAbilitiesToolkitName", "{ObjectName}{DirtyState}"), Args);
}

FText FDNAAbilitiesEditor::GetToolkitToolTipText() const
{
	const UObject* EditingObject = GetEditingObject();

	check (EditingObject != NULL);

	return FAssetEditorToolkit::GetToolTipTextForObject(EditingObject);
}

FString FDNAAbilitiesEditor::GetWorldCentricTabPrefix() const
{
	return TEXT("DNAAbilitiesEditor");
}

FLinearColor FDNAAbilitiesEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor::White;
}

UBlueprint* FDNAAbilitiesEditor::GetBlueprintObj() const
{
	const TArray<UObject*>& EditingObjs = GetEditingObjects();
	for (int32 i = 0; i < EditingObjs.Num(); ++i)
	{
		if (EditingObjs[i]->IsA<UDNAAbilityBlueprint>()) 
		{ 
			return (UBlueprint*)EditingObjs[i]; 
		}
	}
	return nullptr;
}

FString FDNAAbilitiesEditor::GetDocumentationLink() const
{
	return FBlueprintEditor::GetDocumentationLink(); // todo: point this at the correct documentation
}

#undef LOCTEXT_NAMESPACE

