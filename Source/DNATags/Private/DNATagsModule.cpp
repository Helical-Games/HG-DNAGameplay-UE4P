// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNATagsModule.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/IConsoleManager.h"
#include "UObject/Package.h"

FSimpleMulticastDelegate IDNATagsModule::OnDNATagTreeChanged;
FSimpleMulticastDelegate IDNATagsModule::OnTagSettingsChanged;

class FDNATagsModule : public IDNATagsModule
{
	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End IModuleInterface
};

IMPLEMENT_MODULE( FDNATagsModule, DNATags )
DEFINE_LOG_CATEGORY(LogDNATags);

void FDNATagsModule::StartupModule()
{
	// This will force initialization
	UDNATagsManager::Get();
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
int32 DNATagPrintReportOnShutdown = 0;
static FAutoConsoleVariableRef CVarDNATagPrintReportOnShutdown(TEXT("DNATags.PrintReportOnShutdown"), DNATagPrintReportOnShutdown, TEXT("Print DNA tag replication report on shutdown"), ECVF_Default );
#endif


void FDNATagsModule::ShutdownModule()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (DNATagPrintReportOnShutdown)
	{
		UDNATagsManager::Get().PrintReplicationFrequencyReport();
	}
#endif

	UDNATagsManager::SingletonManager = nullptr;
}
