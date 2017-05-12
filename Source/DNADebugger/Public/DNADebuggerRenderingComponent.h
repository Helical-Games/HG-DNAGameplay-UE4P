// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "Components/PrimitiveComponent.h"
#include "DebugRenderSceneProxy.h"
#include "DNADebuggerRenderingComponent.generated.h"

class FDNADebuggerCompositeSceneProxy;

class FDNADebuggerDebugDrawDelegateHelper : public FDebugDrawDelegateHelper
{
	typedef FDebugDrawDelegateHelper Super;

public:
	~FDNADebuggerDebugDrawDelegateHelper()
	{
		Reset();
	}

	void Reset();

	virtual void InitDelegateHelper(const FDebugRenderSceneProxy* InSceneProxy) override
	{
		check(0);
	}

	void InitDelegateHelper(const FDNADebuggerCompositeSceneProxy* InSceneProxy);

	void AddDelegateHelper(FDebugDrawDelegateHelper* InDebugDrawDelegateHelper);

	virtual void RegisterDebugDrawDelgate() override;
	virtual void UnregisterDebugDrawDelgate() override;

private:
	TArray<FDebugDrawDelegateHelper*> DebugDrawDelegateHelpers;
};



UCLASS(NotBlueprintable, NotBlueprintType, noteditinlinenew, hidedropdown, Transient)
class UDNADebuggerRenderingComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform &LocalToWorld) const override;

	virtual void CreateRenderState_Concurrent() override;
	virtual void DestroyRenderState_Concurrent() override;

	FDNADebuggerDebugDrawDelegateHelper DNADebuggerDebugDrawDelegateHelper;
};
