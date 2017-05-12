// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DNADebuggerAddonManager.h"
#include "DNADebuggerCategory.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "DNADebuggerCategoryReplicator.h"

FDNADebuggerCategory::FDNADebuggerCategory() :
	CollectDataInterval(0.0f),
	bShowDataPackReplication(false),
	bShowUpdateTimer(false),
	bShowCategoryName(true),
	bShowOnlyWithDebugActor(true),
	bIsLocal(false),
	bHasAuthority(true),
	bIsEnabled(true),
	CategoryId(INDEX_NONE),
	LastCollectDataTime(-FLT_MAX)
{
}

FDNADebuggerCategory::~FDNADebuggerCategory()
{
}

void FDNADebuggerCategory::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{
	// empty in base class
}

void FDNADebuggerCategory::DrawData(APlayerController* OwnerPC, FDNADebuggerCanvasContext& CanvasContext)
{
	// empty in base class
}

FDebugRenderSceneProxy* FDNADebuggerCategory::CreateDebugSceneProxy(const UPrimitiveComponent* InComponent, FDebugDrawDelegateHelper*& OutDelegateHelper)
{
	OutDelegateHelper = nullptr;
	// empty in base class
	return nullptr;
}

void FDNADebuggerCategory::OnDataPackReplicated(int32 DataPackId)
{
	// empty in base class
}

void FDNADebuggerCategory::AddTextLine(const FString& TextLine)
{
	if (bHasAuthority)
	{
		ReplicatedLines.Add(TextLine);
	}
}

void FDNADebuggerCategory::AddShape(const FDNADebuggerShape& Shape)
{
	if (bHasAuthority)
	{
		ReplicatedShapes.Add(Shape);
	}
}

void FDNADebuggerCategory::DrawCategory(APlayerController* OwnerPC, FDNADebuggerCanvasContext& CanvasContext)
{
	UWorld* World = OwnerPC->GetWorld();

	FString CategoryPrefix;
	if (!bShowCategoryName)
	{
		CategoryPrefix = FString::Printf(TEXT("{green}[%s]{white}  "), *CategoryName.ToString());
	}

	if (bShowUpdateTimer && bHasAuthority)
	{
		const float GameTime = World->GetTimeSeconds();
		CanvasContext.Printf(TEXT("%sNext update in: {yellow}%.0fs"), *CategoryPrefix, CollectDataInterval - (GameTime - LastCollectDataTime));
	}

	if (bShowDataPackReplication)
	{
		for (int32 Idx = 0; Idx < ReplicatedDataPacks.Num(); Idx++)
		{
			FDNADebuggerDataPack& DataPack = ReplicatedDataPacks[Idx];
			if (DataPack.IsInProgress())
			{
				const FString DataPackMessage = (ReplicatedDataPacks.Num() == 1) ?
					FString::Printf(TEXT("%sReplicating: {red}%.0f%% {white}(ver:%d)"), *CategoryPrefix, DataPack.GetProgress() * 100.0f, DataPack.Header.DataVersion) :
					FString::Printf(TEXT("%sReplicating data[%d]: {red}%.0f%% {white}(ver:%d)"), *CategoryPrefix, Idx, DataPack.GetProgress() * 100.0f, DataPack.Header.DataVersion);

				CanvasContext.Print(DataPackMessage);
			}
		}
	}

	for (int32 Idx = 0; Idx < ReplicatedLines.Num(); Idx++)
	{
		CanvasContext.Print(ReplicatedLines[Idx]);
	}

	for (int32 Idx = 0; Idx < ReplicatedShapes.Num(); Idx++)
	{
		ReplicatedShapes[Idx].Draw(World, CanvasContext);
	}

	DrawData(OwnerPC, CanvasContext);
}

void FDNADebuggerCategory::MarkDataPackDirty(int32 DataPackId)
{
	if (ReplicatedDataPacks.IsValidIndex(DataPackId))
	{
		ReplicatedDataPacks[DataPackId].bIsDirty = true;
	}
}

void FDNADebuggerCategory::MarkRenderStateDirty()
{
	if (bIsLocal)
	{
		ADNADebuggerCategoryReplicator* RepOwnerOb = GetReplicator();
		if (RepOwnerOb)
		{
			RepOwnerOb->MarkComponentsRenderStateDirty();
		}
	}
}

FString FDNADebuggerCategory::GetSceneProxyViewFlag() const
{
	const bool bIsSimulate = FDNADebuggerAddonBase::IsSimulateInEditor();
	return bIsSimulate ? TEXT("DebugAI") : TEXT("Game");
}
