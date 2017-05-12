// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Misc/CoreMisc.h"
#include "AbilitySystemGlobals.h"
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

struct FDNAAbilitiesExec : public FSelfRegisteringExec
{
	FDNAAbilitiesExec()
	{
	}

	// Begin FExec Interface
	virtual bool Exec(UWorld* Inworld, const TCHAR* Cmd, FOutputDevice& Ar) override;
	// End FExec Interface
};

FDNAAbilitiesExec DNAAbilitiesExecInstance;

bool FDNAAbilitiesExec::Exec(UWorld* Inworld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	if (Inworld == NULL)
	{
		return false;
	}

	bool bHandled = false;

	if (FParse::Command(&Cmd, TEXT("ToggleIgnoreDNAAbilitySystemCooldowns")))
	{
		UDNAAbilitySystemGlobals& DNAAbilitySystemGlobals = UDNAAbilitySystemGlobals::Get();
		DNAAbilitySystemGlobals.ToggleIgnoreDNAAbilitySystemCooldowns();
		bHandled = true;
	}
	else if (FParse::Command(&Cmd, TEXT("ToggleIgnoreDNAAbilitySystemCosts")))
	{
		UDNAAbilitySystemGlobals& DNAAbilitySystemGlobals = UDNAAbilitySystemGlobals::Get();
		DNAAbilitySystemGlobals.ToggleIgnoreDNAAbilitySystemCosts();
		bHandled = true;
	}

	return bHandled;
}

#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
