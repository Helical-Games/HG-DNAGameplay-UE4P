// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DNADebuggerAddonManager.h"
#include "DNADebuggerRenderingComponent.h"
#include "DNADebuggerCategoryReplicator.h"
#include "DNADebuggerCategory.h"

//////////////////////////////////////////////////////////////////////////
// FDNADebuggerCompositeSceneProxy

class FDNADebuggerCompositeSceneProxy : public FDebugRenderSceneProxy
{
	friend class FDNADebuggerDebugDrawDelegateHelper;
public:
	FDNADebuggerCompositeSceneProxy(const UPrimitiveComponent* InComponent) : FDebugRenderSceneProxy(InComponent) { }

	virtual ~FDNADebuggerCompositeSceneProxy()
	{
		for (int32 Idx = 0; Idx < ChildProxies.Num(); Idx++)
		{
			delete ChildProxies[Idx];
		}
	}

	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) override
	{
		for (int32 Idx = 0; Idx < ChildProxies.Num(); Idx++)
		{
			ChildProxies[Idx]->DrawStaticElements(PDI);
		}
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		for (int32 Idx = 0; Idx < ChildProxies.Num(); Idx++)
		{
			ChildProxies[Idx]->GetDynamicMeshElements(Views, ViewFamily, VisibilityMap, Collector);
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		for (int32 Idx = 0; Idx < ChildProxies.Num(); Idx++)
		{
			Result |= ChildProxies[Idx]->GetViewRelevance(View);
		}
		return Result;
	}

	virtual uint32 GetMemoryFootprint(void) const override
	{
		return sizeof(*this) + GetAllocatedSize();
	}

	uint32 GetAllocatedSize(void) const
	{
		uint32 Size = ChildProxies.GetAllocatedSize();
		for (int32 Idx = 0; Idx < ChildProxies.Num(); Idx++)
		{
			Size += ChildProxies[Idx]->GetMemoryFootprint();
		}

		return Size;
	}

	void AddChild(FDebugRenderSceneProxy* NewChild)
	{
		ChildProxies.AddUnique(NewChild);
	}

	void AddRange(TArray<FDebugRenderSceneProxy*> Children)
	{
		ChildProxies.Append(Children);
	}

protected:
	TArray<FDebugRenderSceneProxy*> ChildProxies;
};

void FDNADebuggerDebugDrawDelegateHelper::RegisterDebugDrawDelgate()
{
	ensureMsgf(State != RegisteredState, TEXT("RegisterDebugDrawDelgate is already Registered!"));
	if (State == InitializedState)
	{
		for (int32 Idx = 0; Idx < DebugDrawDelegateHelpers.Num(); Idx++)
		{
			DebugDrawDelegateHelpers[Idx]->RegisterDebugDrawDelgate();
		}
		State = RegisteredState;
	}
}

void FDNADebuggerDebugDrawDelegateHelper::UnregisterDebugDrawDelgate()
{
	ensureMsgf(State != InitializedState, TEXT("UnegisterDebugDrawDelgate is in an invalid State: %i !"), State);
	if (State == RegisteredState)
	{
		for (int32 Idx = 0; Idx < DebugDrawDelegateHelpers.Num(); Idx++)
		{
			DebugDrawDelegateHelpers[Idx]->UnregisterDebugDrawDelgate();
		}
		State = InitializedState;
	}
}

void FDNADebuggerDebugDrawDelegateHelper::Reset()
{
	for (int32 Idx = 0; Idx < DebugDrawDelegateHelpers.Num(); Idx++)
	{
		delete DebugDrawDelegateHelpers[Idx];
	}
	DebugDrawDelegateHelpers.Reset();
}

void FDNADebuggerDebugDrawDelegateHelper::AddDelegateHelper(FDebugDrawDelegateHelper* InDebugDrawDelegateHelper)
{
	check(InDebugDrawDelegateHelper);
	DebugDrawDelegateHelpers.Add(InDebugDrawDelegateHelper);
}

void FDNADebuggerDebugDrawDelegateHelper::InitDelegateHelper(const FDNADebuggerCompositeSceneProxy* InSceneProxy)
{
	Super::InitDelegateHelper(InSceneProxy);
}

//////////////////////////////////////////////////////////////////////////
// UDNADebuggerRenderingComponent

UDNADebuggerRenderingComponent::UDNADebuggerRenderingComponent(const FObjectInitializer& ObjInitializer) : Super(ObjInitializer)
{
}

FPrimitiveSceneProxy* UDNADebuggerRenderingComponent::CreateSceneProxy()
{
	DNADebuggerDebugDrawDelegateHelper.Reset();

	FDNADebuggerCompositeSceneProxy* CompositeProxy = nullptr;

	ADNADebuggerCategoryReplicator* OwnerReplicator = Cast<ADNADebuggerCategoryReplicator>(GetOwner());
	if (OwnerReplicator && OwnerReplicator->IsEnabled())
	{
		TArray<FDebugRenderSceneProxy*> SceneProxies;
		for (int32 Idx = 0; Idx < OwnerReplicator->GetNumCategories(); Idx++)
		{
			TSharedRef<FDNADebuggerCategory> Category = OwnerReplicator->GetCategory(Idx);
			if (Category->IsCategoryEnabled())
			{
				FDebugDrawDelegateHelper* DebugDrawDelegateHelper;
				FDebugRenderSceneProxy* CategorySceneProxy = Category->CreateDebugSceneProxy(this, DebugDrawDelegateHelper);
				if (CategorySceneProxy)
				{
					DNADebuggerDebugDrawDelegateHelper.AddDelegateHelper(DebugDrawDelegateHelper);
					SceneProxies.Add(CategorySceneProxy);
				}
			}
		}

		if (SceneProxies.Num())
		{
			CompositeProxy = new FDNADebuggerCompositeSceneProxy(this);
			CompositeProxy->AddRange(SceneProxies);
		}
	}

	if (CompositeProxy)
	{
		DNADebuggerDebugDrawDelegateHelper.InitDelegateHelper(CompositeProxy);
		DNADebuggerDebugDrawDelegateHelper.ReregisterDebugDrawDelgate();
	}
	return CompositeProxy;
}

FBoxSphereBounds UDNADebuggerRenderingComponent::CalcBounds(const FTransform &LocalToWorld) const
{
	return FBoxSphereBounds(FBox::BuildAABB(FVector::ZeroVector, FVector(1000000.0f, 1000000.0f, 1000000.0f)));
}

void UDNADebuggerRenderingComponent::CreateRenderState_Concurrent()
{
	Super::CreateRenderState_Concurrent();

	DNADebuggerDebugDrawDelegateHelper.RegisterDebugDrawDelgate();
}

void UDNADebuggerRenderingComponent::DestroyRenderState_Concurrent()
{
	DNADebuggerDebugDrawDelegateHelper.UnregisterDebugDrawDelgate();

	Super::DestroyRenderState_Concurrent();
}
