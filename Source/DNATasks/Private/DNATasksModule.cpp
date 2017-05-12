// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNATasksModule.h"
#include "Stats/Stats.h"
#include "DNATask.h"
#include "DNATasksPrivate.h"

//////////////////////////////////////////////////////////////////////////
// FCharacterAIModule

class FDNATasksModule : public IDNATasksModule
{
public:
	virtual void StartupModule() override
	{
		check(GConfig);		
	}

	virtual void ShutdownModule() override
	{
	}
};

//////////////////////////////////////////////////////////////////////////

IMPLEMENT_MODULE(FDNATasksModule, DNATasks);
DEFINE_LOG_CATEGORY(LogDNATasks);
DEFINE_STAT(STAT_TickDNATasks);
