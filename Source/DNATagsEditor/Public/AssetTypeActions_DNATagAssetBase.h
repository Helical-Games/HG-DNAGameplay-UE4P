// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "AssetTypeActions_Base.h"

/** Base asset type actions for any classes with DNA tagging */
class DNATAGSEDITOR_API FAssetTypeActions_DNATagAssetBase : public FAssetTypeActions_Base
{
public:

	/** Constructor */
	FAssetTypeActions_DNATagAssetBase(FName InTagPropertyName);

	/** Overridden to specify that the DNA tag base has actions */
	virtual bool HasActions(const TArray<UObject*>& InObjects) const override;

	/** Overridden to offer the DNA tagging options */
	virtual void GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder) override;

	/** Overridden to specify misc category */
	virtual uint32 GetCategories() override;

private:
	/**
	 * Open the DNA tag editor
	 * 
	 * @param TagAssets	Assets to open the editor with
	 */
	void OpenDNATagEditor(TArray<class UObject*> Objects, TArray<struct FDNATagContainer*> Containers);

	/** Name of the property of the owned DNA tag container */
	FName OwnedDNATagPropertyName;
};