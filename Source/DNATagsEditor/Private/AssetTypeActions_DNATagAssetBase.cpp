// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNATagsEditorModulePrivatePCH.h"
#include "SDNATagWidget.h"
#include "MainFrame.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FAssetTypeActions_DNATagAssetBase::FAssetTypeActions_DNATagAssetBase(FName InTagPropertyName)
	: OwnedDNATagPropertyName(InTagPropertyName)
{}

bool FAssetTypeActions_DNATagAssetBase::HasActions(const TArray<UObject*>& InObjects) const
{
	return true;
}

void FAssetTypeActions_DNATagAssetBase::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
	TArray<UObject*> ContainerObjectOwners;
	TArray<FDNATagContainer*> Containers;
	for (int32 ObjIdx = 0; ObjIdx < InObjects.Num(); ++ObjIdx)
	{
		UObject* CurObj = InObjects[ObjIdx];
		if (CurObj)
		{
			UStructProperty* StructProp = FindField<UStructProperty>(CurObj->GetClass(), OwnedDNATagPropertyName);
			if(StructProp != NULL)
			{
				ContainerObjectOwners.Add(CurObj);
				Containers.Add(StructProp->ContainerPtrToValuePtr<FDNATagContainer>(CurObj));
			}
		}
	}

	ensure(Containers.Num() == ContainerObjectOwners.Num());
	if (Containers.Num() > 0 && (Containers.Num() == ContainerObjectOwners.Num()))
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("DNATags_Edit", "Edit DNA Tags..."),
			LOCTEXT("DNATags_EditToolTip", "Opens the DNA Tag Editor."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &FAssetTypeActions_DNATagAssetBase::OpenDNATagEditor, ContainerObjectOwners, Containers), FCanExecuteAction()));
	}
}

void FAssetTypeActions_DNATagAssetBase::OpenDNATagEditor(TArray<UObject*> Objects, TArray<FDNATagContainer*> Containers)
{
	TArray<SDNATagWidget::FEditableDNATagContainerDatum> EditableContainers;
	for (int32 ObjIdx = 0; ObjIdx < Objects.Num() && ObjIdx < Containers.Num(); ++ObjIdx)
	{
		EditableContainers.Add(SDNATagWidget::FEditableDNATagContainerDatum(Objects[ObjIdx], Containers[ObjIdx]));
	}

	FText Title;
	FText AssetName;

	const int32 NumAssets = EditableContainers.Num();
	if (NumAssets > 1)
	{
		AssetName = FText::Format( LOCTEXT("AssetTypeActions_DNATagAssetBaseMultipleAssets", "{0} Assets"), FText::AsNumber( NumAssets ) );
		Title = FText::Format( LOCTEXT("AssetTypeActions_DNATagAssetBaseEditorTitle", "Tag Editor: Owned DNA Tags: {0}"), AssetName );
	}
	else if (NumAssets > 0 && EditableContainers[0].TagContainerOwner.IsValid())
	{
		AssetName = FText::FromString( EditableContainers[0].TagContainerOwner->GetName() );
		Title = FText::Format( LOCTEXT("AssetTypeActions_DNATagAssetBaseEditorTitle", "Tag Editor: Owned DNA Tags: {0}"), AssetName );
	}

	TSharedPtr<SWindow> Window = SNew(SWindow)
		.Title(Title)
		.ClientSize(FVector2D(600, 400))
		[
			SNew(SDNATagWidget, EditableContainers)
		];

	IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
	if (MainFrameModule.GetParentWindow().IsValid())
	{
		FSlateApplication::Get().AddWindowAsNativeChild(Window.ToSharedRef(), MainFrameModule.GetParentWindow().ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(Window.ToSharedRef());
	}
}

uint32 FAssetTypeActions_DNATagAssetBase::GetCategories()
{ 
	return EAssetTypeCategories::Misc; 
}

#undef LOCTEXT_NAMESPACE