// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DNADebuggerAddonManager.h"
#include "DNADebuggerExtension.h"
#include "DNADebuggerCategoryReplicator.h"

void FDNADebuggerExtension::OnDNADebuggerActivated()
{
	const bool bIsLocal = IsLocal();
	if (bIsLocal)
	{
		OnActivated();
	}
}

void FDNADebuggerExtension::OnDNADebuggerDeactivated()
{
	const bool bIsLocal = IsLocal();
	if (bIsLocal)
	{
		OnDeactivated();
	}
}

void FDNADebuggerExtension::OnActivated()
{
	// empty in base class
}

void FDNADebuggerExtension::OnDeactivated()
{
	// empty in base class
}

FString FDNADebuggerExtension::GetDescription() const
{
	// empty in base class
	return FString();
}

APlayerController* FDNADebuggerExtension::GetPlayerController() const
{
	ADNADebuggerCategoryReplicator* Replicator = GetReplicator();
	return Replicator ? Replicator->GetReplicationOwner() : nullptr;
}

bool FDNADebuggerExtension::IsLocal() const
{
	ADNADebuggerCategoryReplicator* Replicator = GetReplicator();
	return Replicator ? Replicator->IsLocal() : true;
}
