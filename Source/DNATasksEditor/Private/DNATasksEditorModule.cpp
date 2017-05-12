// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNATasksEditorModule.h"

//////////////////////////////////////////////////////////////////////////
// FCharacterAIModule

class FDNATasksEditorModule : public IDNATasksEditorModule
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

IMPLEMENT_MODULE(FDNATasksEditorModule, DNATasksEditor);
