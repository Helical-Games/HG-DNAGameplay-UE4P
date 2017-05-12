// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNAAbilitiesModule.h"
#include "UObject/Object.h"
#include "Misc/StringClassReference.h"
#include "GameFramework/HUD.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"

#if WITH_DNA_DEBUGGER
#include "DNADebugger.h"
#include "DNADebuggerCategory_Abilities.h"
#endif // WITH_DNA_DEBUGGER

class FDNAAbilitiesModule : public IDNAAbilitiesModule
{
	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End IModuleInterface

	virtual UDNAAbilitySystemGlobals* GetDNAAbilitySystemGlobals() override
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_IDNAAbilitiesModule_GetDNAAbilitySystemGlobals);
		// Defer loading of globals to the first time it is requested
		if (!DNAAbilitySystemGlobals)
		{
			FStringClassReference DNAAbilitySystemClassName = (UDNAAbilitySystemGlobals::StaticClass()->GetDefaultObject<UDNAAbilitySystemGlobals>())->DNAAbilitySystemGlobalsClassName;

			UClass* SingletonClass = DNAAbilitySystemClassName.TryLoadClass<UObject>();
			checkf(SingletonClass != nullptr, TEXT("Ability config value DNAAbilitySystemGlobalsClassName is not a valid class name."));

			DNAAbilitySystemGlobals = NewObject<UDNAAbilitySystemGlobals>(GetTransientPackage(), SingletonClass, NAME_None);
			DNAAbilitySystemGlobals->AddToRoot();
			DNAAbilitySystemGlobalsReadyCallback.Broadcast();
		}

		check(DNAAbilitySystemGlobals);
		return DNAAbilitySystemGlobals;
	}

	virtual bool IsDNAAbilitySystemGlobalsAvailable() override
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_IDNAAbilitiesModule_IsDNAAbilitySystemGlobalsAvailable);
		return DNAAbilitySystemGlobals != nullptr;
	}

	void CallOrRegister_OnDNAAbilitySystemGlobalsReady(FSimpleMulticastDelegate::FDelegate Delegate)
	{
		if (DNAAbilitySystemGlobals)
		{
			Delegate.Execute();
		}
		else
		{
			DNAAbilitySystemGlobalsReadyCallback.Add(Delegate);
		}
	}

	FSimpleMulticastDelegate DNAAbilitySystemGlobalsReadyCallback;
	UDNAAbilitySystemGlobals* DNAAbilitySystemGlobals;
	
};

IMPLEMENT_MODULE(FDNAAbilitiesModule, DNAAbilities)

void FDNAAbilitiesModule::StartupModule()
{	
	// This is loaded upon first request
	DNAAbilitySystemGlobals = nullptr;

#if WITH_DNA_DEBUGGER
	IDNADebugger& DNADebuggerModule = IDNADebugger::Get();
	DNADebuggerModule.RegisterCategory("Abilities", IDNADebugger::FOnGetCategory::CreateStatic(&FDNADebuggerCategory_Abilities::MakeInstance));
	DNADebuggerModule.NotifyCategoriesChanged();
#endif // WITH_DNA_DEBUGGER

	if (!IsRunningDedicatedServer())
	{
		AHUD::OnShowDebugInfo.AddStatic(&UDNAAbilitySystemComponent::OnShowDebugInfo);
	}
}

void FDNAAbilitiesModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	DNAAbilitySystemGlobals = NULL;

#if WITH_DNA_DEBUGGER
	if (IDNADebugger::IsAvailable())
	{
		IDNADebugger& DNADebuggerModule = IDNADebugger::Get();
		DNADebuggerModule.UnregisterCategory("Abilities");
		DNADebuggerModule.NotifyCategoriesChanged();
	}
#endif // WITH_DNA_DEBUGGER
}
