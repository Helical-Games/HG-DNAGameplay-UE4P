// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "DNADebuggerCategory.h"
#include "DNADebugger.h"

class ADNADebuggerCategoryReplicator;
class FDNADebuggerExtension;

DECLARE_MULTICAST_DELEGATE(FOnDNADebuggerAddonEvent);

class FDNADebuggerCategory;
class ADNADebuggerCategoryReplicator;

struct FDNADebuggerCategoryInfo
{
	IDNADebugger::FOnGetCategory MakeInstanceDelegate;
	EDNADebuggerCategoryState DefaultCategoryState;
	EDNADebuggerCategoryState CategoryState;
	int32 SlotIdx;
};

struct FDNADebuggerExtensionInfo
{
	IDNADebugger::FOnGetExtension MakeInstanceDelegate;
	uint32 bDefaultEnabled : 1;
	uint32 bEnabled : 1;
};

class FDNADebuggerAddonManager
{
public:
	FDNADebuggerAddonManager();

	/** adds new category to managed collection */
	void RegisterCategory(FName CategoryName, IDNADebugger::FOnGetCategory MakeInstanceDelegate, EDNADebuggerCategoryState CategoryState, int32 SlotIdx);

	/** removes category from managed collection */
	void UnregisterCategory(FName CategoryName);

	/** notify about change in known categories */
	void NotifyCategoriesChanged();

	/** creates new category objects for all known types */
	void CreateCategories(ADNADebuggerCategoryReplicator& Owner, TArray<TSharedRef<FDNADebuggerCategory> >& CategoryObjects);

	/** adds new extension to managed collection */
	void RegisterExtension(FName ExtensionName, IDNADebugger::FOnGetExtension MakeInstanceDelegate);

	/** removes extension from managed collection */
	void UnregisterExtension(FName ExtensionName);

	/** notify about change in known extensions */
	void NotifyExtensionsChanged();

	/** creates new extension objects for all known types */
	void CreateExtensions(ADNADebuggerCategoryReplicator& Replicator, TArray<TSharedRef<FDNADebuggerExtension> >& ExtensionObjects);

	/** refresh category and extension data from config */
	void UpdateFromConfig();

	/** get slot-Id map */
	const TArray<TArray<int32> >& GetSlotMap() const { return SlotMap; }

	/** get slot-Name map */
	const TArray<FString> GetSlotNames() const { return SlotNames; }

	/** get number of visible categories */
	int32 GetNumVisibleCategories() const { return NumVisibleCategories; }

	/** singleton accessor */
	static FDNADebuggerAddonManager& GetCurrent();

	/** event called when CategoryMap changes */
	FOnDNADebuggerAddonEvent OnCategoriesChanged;

	/** event called when ExtensionMap changes */
	FOnDNADebuggerAddonEvent OnExtensionsChanged;

private:
	/** map of all known extensions indexed by their names */
	TMap<FName, FDNADebuggerExtensionInfo> ExtensionMap;

	/** map of all known categories indexed by their names */
	TMap<FName, FDNADebuggerCategoryInfo> CategoryMap;

	/** list of all slots and their categories Ids */
	TArray<TArray<int32> > SlotMap;

	/** list of slot names */
	TArray<FString> SlotNames;

	/** number of categories, excluding hidden ones */
	int32 NumVisibleCategories;
};
