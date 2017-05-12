// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "DNATagContainer.h"
#include "EngineGlobals.h"
#include "DNATagsManager.h"

/**
 * The public interface to this module, generally you should access the manager directly instead
 */
class IDNATagsModule : public IModuleInterface
{

public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IDNATagsModule& Get()
	{
		static const FName DNATagModuleName(TEXT("DNATags"));
		return FModuleManager::LoadModuleChecked< IDNATagsModule >(DNATagModuleName);
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		static const FName DNATagModuleName(TEXT("DNATags"));
		return FModuleManager::Get().IsModuleLoaded(DNATagModuleName);
	}

	/** Delegate for when assets are added to the tree */
	static DNATAGS_API FSimpleMulticastDelegate OnDNATagTreeChanged;

	/** Delegate that gets called after the settings have changed in the editor */
	static DNATAGS_API FSimpleMulticastDelegate OnTagSettingsChanged;

	DEPRECATED(4.15, "Call FDNATag::RequestDNATag or RequestDNATag on the manager instead")
	FORCEINLINE_DEBUGGABLE static FDNATag RequestDNATag(FName InTagName, bool ErrorIfNotFound=true)
	{
		return UDNATagsManager::Get().RequestDNATag(InTagName, ErrorIfNotFound);
	}

	DEPRECATED(4.15, "Call UDNATagsManager::Get instead")
	FORCEINLINE_DEBUGGABLE static UDNATagsManager& GetDNATagsManager()
	{
		return UDNATagsManager::Get();
	}

};

