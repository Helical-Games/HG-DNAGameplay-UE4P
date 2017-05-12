// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// DNA DEBUGGER
// 
// This tool allows easy on screen debugging of DNA data, supporting client-server replication.
// Data is organized into named categories, which can be toggled during debugging.
// 
// To enable it, press Apostrophe key (UDNADebuggerConfig::ActivationKey)
//
// Category class:
// - derives from FDNADebuggerCategory
// - implements at least CollectData() and DrawData() functions
// - requires WITH_DNA_DEBUGGER define to compile (doesn't exist in shipping builds by default)
// - needs to be registered and unregistered manually by owning module
// - automatically replicate data added with FDNADebuggerCategory::AddTextLine, FDNADebuggerCategory::AddShape
// - automatically replicate data structs initialized with FDNADebuggerCategory::SetDataPackReplication
// - can define own input bindings (e.g. subcategories, etc)
//
// Extension class:
// - derives from FDNADebuggerExtension
// - needs to be registered and unregistered manually by owning module
// - can define own input bindings
// - basically it's a stateless, not replicated, not drawn category, ideal for making e.g. different actor selection mechanic
//
// 
// Check FDNADebuggerCategory_BehaviorTree for implementation example.
// Check AIModule/Private/AIModule.cpp for registration example.
//
//
// Remember to define WITH_DNA_DEBUGGER=1 when adding module to your project's Build.cs!
// 
// if (UEBuildConfiguration.bBuildDeveloperTools &&
//     Target.Configuration != UnrealTargetConfiguration.Shipping &&
//     Target.Configuration != UnrealTargetConfiguration.Test)
// {
//     PrivateDependencyModuleNames.Add("DNADebugger");
//     Definitions.Add("WITH_DNA_DEBUGGER=1");
// }
// 

#pragma once

#include "Core.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

enum class EDNADebuggerCategoryState : uint8
{
	EnabledInGameAndSimulate,
	EnabledInGame,
	EnabledInSimulate,
	Disabled,
	Hidden,
};

class IDNADebugger : public IModuleInterface
{

public:
	DECLARE_DELEGATE_RetVal(TSharedRef<class FDNADebuggerCategory>, FOnGetCategory);
	DECLARE_DELEGATE_RetVal(TSharedRef<class FDNADebuggerExtension>, FOnGetExtension);

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IDNADebugger& Get()
	{
		return FModuleManager::LoadModuleChecked< IDNADebugger >("DNADebugger");
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("DNADebugger");
	}

	virtual void RegisterCategory(FName CategoryName, FOnGetCategory MakeInstanceDelegate, EDNADebuggerCategoryState CategoryState = EDNADebuggerCategoryState::Disabled, int32 SlotIdx = INDEX_NONE) = 0;
	virtual void UnregisterCategory(FName CategoryName) = 0;
	virtual void NotifyCategoriesChanged() = 0;
	virtual void RegisterExtension(FName ExtensionName, FOnGetExtension MakeInstanceDelegate) = 0;
	virtual void UnregisterExtension(FName ExtensionName) = 0;
	virtual void NotifyExtensionsChanged() = 0;
};
