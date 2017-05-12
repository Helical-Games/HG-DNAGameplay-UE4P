// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

struct DNAABILITIESEDITOR_API FDNACueEditorStrings
{
	FString DNACueNotifyDescription1;
	FString DNACueNotifyDescription2;

	FString DNACueEventDescription1;
	FString DNACueEventDescription2;

	FDNACueEditorStrings()
	{
		DNACueNotifyDescription1 = FString(TEXT("DNACue Notifies are stand alone handlers, similiar to AnimNotifies. Most GameplyCues can be implemented through these notifies. Notifies excel at handling standardized effects. The classes below provide the most common functionality needed."));
		DNACueNotifyDescription2 = FString(TEXT(""));

		DNACueEventDescription1 = FString(TEXT("DNACues can also be implemented via custom events on character blueprints."));
		DNACueEventDescription2 = FString(TEXT("To add a custom BP event, open the blueprint and look for custom events starting with DNACue.*"));
	}

	FDNACueEditorStrings(FString Notify1, FString Notify2, FString Event1, FString Event2)
	{
		DNACueNotifyDescription1 = Notify1;
		DNACueNotifyDescription2 = Notify2;
		DNACueEventDescription1 = Event1;
		DNACueEventDescription2 = Event2;
	}
};

DECLARE_DELEGATE_OneParam(FGetDNACueNotifyClasses, TArray<UClass*>&);
DECLARE_DELEGATE_OneParam(FGetDNACueInterfaceClasses, TArray<UClass*>&);
DECLARE_DELEGATE_RetVal_OneParam(FString, FGetDNACuePath, FString)

DECLARE_DELEGATE_RetVal(FDNACueEditorStrings, FGetDNACueEditorStrings);

/**
 * The public interface to this module
 */
class IDNAAbilitiesEditorModule : public IModuleInterface
{

public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IDNAAbilitiesEditorModule& Get()
	{
		return FModuleManager::LoadModuleChecked< IDNAAbilitiesEditorModule >("DNAAbilitiesEditor");
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "DNAAbilitiesEditor" );
	}

	/** Sets delegate that will be called to retrieve list of DNA cue notify classes to be presented by DNACue Editor when creating a new notify */
	virtual FGetDNACueNotifyClasses& GetDNACueNotifyClassesDelegate() = 0;

	virtual FGetDNACueInterfaceClasses& GetDNACueInterfaceClassesDelegate() = 0;

	/** Sets delegate that will be called to get the save path for a DNA cue notify that is created through the DNACue Editor */
	virtual FGetDNACuePath& GetDNACueNotifyPathDelegate() = 0;

	/** Returns strings used in the DNACue Editor widgets. Useful for games to override with game specific information for designers (real examples, etc). */
	virtual FGetDNACueEditorStrings& GetDNACueEditorStringsDelegate() = 0;
	
};
