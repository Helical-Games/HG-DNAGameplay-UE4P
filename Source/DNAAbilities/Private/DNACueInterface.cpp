// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNACueInterface.h"
#include "AbilitySystemStats.h"
#include "DNATagsModule.h"
#include "AbilitySystemComponent.h"
#include "DNACueSet.h"

namespace DNACueInterfacePrivate
{
	struct FCueNameAndUFunction
	{
		FDNATag Tag;
		UFunction* Func;
	};
	typedef TMap<FDNATag, TArray<FCueNameAndUFunction> > FDNACueTagFunctionList;
	static TMap<FObjectKey, FDNACueTagFunctionList > PerClassDNATagToFunctionMap;
}


UDNACueInterface::UDNACueInterface(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

void IDNACueInterface::DispatchBlueprintCustomHandler(AActor* Actor, UFunction* Func, EDNACueEvent::Type EventType, FDNACueParameters Parameters)
{
	DNACueInterface_eventBlueprintCustomHandler_Parms Parms;
	Parms.EventType = EventType;
	Parms.Parameters = Parameters;

	Actor->ProcessEvent(Func, &Parms);
}

void IDNACueInterface::ClearTagToFunctionMap()
{
	DNACueInterfacePrivate::PerClassDNATagToFunctionMap.Empty();
}

void IDNACueInterface::HandleDNACues(AActor *Self, const FDNATagContainer& DNACueTags, EDNACueEvent::Type EventType, FDNACueParameters Parameters)
{
	for (auto TagIt = DNACueTags.CreateConstIterator(); TagIt; ++TagIt)
	{
		HandleDNACue(Self, *TagIt, EventType, Parameters);
	}
}

bool IDNACueInterface::ShouldAcceptDNACue(AActor *Self, FDNATag DNACueTag, EDNACueEvent::Type EventType, FDNACueParameters Parameters)
{
	return true;
}

void IDNACueInterface::HandleDNACue(AActor *Self, FDNATag DNACueTag, EDNACueEvent::Type EventType, FDNACueParameters Parameters)
{
	SCOPE_CYCLE_COUNTER(STAT_DNACueInterface_HandleDNACue);

	// Look up a custom function for this DNA tag. 
	UClass* Class = Self->GetClass();
	FDNATagContainer TagAndParentsContainer = DNACueTag.GetDNATagParents();

	Parameters.OriginalTag = DNACueTag;

	//Find entry for the class
	FObjectKey ClassObjectKey(Class);
	DNACueInterfacePrivate::FDNACueTagFunctionList& DNATagFunctionList = DNACueInterfacePrivate::PerClassDNATagToFunctionMap.FindOrAdd(ClassObjectKey);
	TArray<DNACueInterfacePrivate::FCueNameAndUFunction>* FunctionList = DNATagFunctionList.Find(DNACueTag);
	if (FunctionList == NULL)
	{
		//generate new function list
		FunctionList = &DNATagFunctionList.Add(DNACueTag);

		for (auto InnerTagIt = TagAndParentsContainer.CreateConstIterator(); InnerTagIt; ++InnerTagIt)
		{
			UFunction* Func = NULL;
			FName CueName = InnerTagIt->GetTagName();

			Func = Class->FindFunctionByName(CueName, EIncludeSuperFlag::IncludeSuper);
			// If the handler calls ForwardDNACueToParent, keep calling functions until one consumes the cue and doesn't forward it
			while (Func)
			{
				DNACueInterfacePrivate::FCueNameAndUFunction NewCueFunctionPair;
				NewCueFunctionPair.Tag = *InnerTagIt;
				NewCueFunctionPair.Func = Func;
				FunctionList->Add(NewCueFunctionPair);

				Func = Func->GetSuperFunction();
			}

			// Native functions cant be named with ".", so look for them with _. 
			FName NativeCueFuncName = *CueName.ToString().Replace(TEXT("."), TEXT("_"));
			Func = Class->FindFunctionByName(NativeCueFuncName, EIncludeSuperFlag::IncludeSuper);

			while (Func)
			{
				DNACueInterfacePrivate::FCueNameAndUFunction NewCueFunctionPair;
				NewCueFunctionPair.Tag = *InnerTagIt;
				NewCueFunctionPair.Func = Func;
				FunctionList->Add(NewCueFunctionPair);

				Func = Func->GetSuperFunction();
			}
		}
	}

	//Iterate through all functions in the list until we should no longer continue
	check(FunctionList);
		
	bool bShouldContinue = true;
	for (int32 FunctionIndex = 0; bShouldContinue && (FunctionIndex < FunctionList->Num()); ++FunctionIndex)
	{
		DNACueInterfacePrivate::FCueNameAndUFunction& CueFunctionPair = FunctionList->GetData()[FunctionIndex];
		UFunction* Func = CueFunctionPair.Func;
		Parameters.MatchedTagName = CueFunctionPair.Tag;

		// Reset the forward parameter now, so we can check it after function
		bForwardToParent = false;
		IDNACueInterface::DispatchBlueprintCustomHandler(Self, Func, EventType, Parameters);

		bShouldContinue = bForwardToParent;
	}

	if (bShouldContinue)
	{
		TArray<UDNACueSet*> Sets;
		GetDNACueSets(Sets);
		for (UDNACueSet* Set : Sets)
		{
			bShouldContinue = Set->HandleDNACue(Self, DNACueTag, EventType, Parameters);
			if (!bShouldContinue)
			{
				break;
			}
		}
	}

	if (bShouldContinue)
	{
		Parameters.MatchedTagName = DNACueTag;
		DNACueDefaultHandler(EventType, Parameters);
	}
}

void IDNACueInterface::DNACueDefaultHandler(EDNACueEvent::Type EventType, FDNACueParameters Parameters)
{
	// No default handler, subclasses can implement
}

void IDNACueInterface::ForwardDNACueToParent()
{
	// Consumed by HandleDNACue
	bForwardToParent = true;
}

void FActiveDNACue::PreReplicatedRemove(const struct FActiveDNACueContainer &InArray)
{
	// We don't check the PredictionKey here like we do in PostReplicatedAdd. PredictionKey tells us
	// if we were predictely created, but this doesn't mean we will predictively remove ourselves.
	if (bPredictivelyRemoved == false)
	{
		// If predicted ignore the add/remove
		InArray.Owner->UpdateTagMap(DNACueTag, -1);
		InArray.Owner->InvokeDNACueEvent(DNACueTag, EDNACueEvent::Removed, Parameters);
	}
}

void FActiveDNACue::PostReplicatedAdd(const struct FActiveDNACueContainer &InArray)
{
	InArray.Owner->UpdateTagMap(DNACueTag, 1);

	if (PredictionKey.IsLocalClientKey() == false)
	{
		// If predicted ignore the add/remove
		InArray.Owner->InvokeDNACueEvent(DNACueTag, EDNACueEvent::WhileActive, Parameters);
	}
}

void FActiveDNACueContainer::AddCue(const FDNATag& Tag, const FPredictionKey& PredictionKey, const FDNACueParameters& Parameters)
{
	UWorld* World = Owner->GetWorld();

	// Store the prediction key so the client can investigate it
	FActiveDNACue&	NewCue = DNACues[DNACues.AddDefaulted()];
	NewCue.DNACueTag = Tag;
	NewCue.PredictionKey = PredictionKey;
	NewCue.Parameters = Parameters;
	MarkItemDirty(NewCue);
	
	Owner->UpdateTagMap(Tag, 1);
}

void FActiveDNACueContainer::RemoveCue(const FDNATag& Tag)
{
	for (int32 idx=0; idx < DNACues.Num(); ++idx)
	{
		FActiveDNACue& Cue = DNACues[idx];

		if (Cue.DNACueTag == Tag)
		{
			DNACues.RemoveAt(idx);
			MarkArrayDirty();
			Owner->UpdateTagMap(Tag, -1);
			return;
		}
	}
}

void FActiveDNACueContainer::PredictiveRemove(const FDNATag& Tag)
{
	for (int32 idx=0; idx < DNACues.Num(); ++idx)
	{
		FActiveDNACue& Cue = DNACues[idx];
		if (Cue.DNACueTag == Tag)
		{			
			// Predictive remove: mark the cue as predictive remove, invoke remove event, update tag map.
			// DONT remove from the replicated array.
			Cue.bPredictivelyRemoved = true;
			Owner->UpdateTagMap(Tag, -1);
			Owner->InvokeDNACueEvent(Tag, EDNACueEvent::Removed, Cue.Parameters);	
			return;
		}
	}
}

void FActiveDNACueContainer::PredictiveAdd(const FDNATag& Tag, FPredictionKey& PredictionKey)
{
	Owner->UpdateTagMap(Tag, 1);	
	PredictionKey.NewRejectOrCaughtUpDelegate(FPredictionKeyEvent::CreateUObject(Owner, &UDNAAbilitySystemComponent::OnPredictiveDNACueCatchup, Tag));
}

bool FActiveDNACueContainer::HasCue(const FDNATag& Tag) const
{
	for (int32 idx=0; idx < DNACues.Num(); ++idx)
	{
		const FActiveDNACue& Cue = DNACues[idx];
		if (Cue.DNACueTag == Tag)
		{
			return true;
		}
	}

	return false;
}

bool FActiveDNACueContainer::NetDeltaSerialize(FNetDeltaSerializeInfo & DeltaParms)
{
	if (bMinimalReplication && (Owner && Owner->ReplicationMode == EReplicationMode::Full))
	{
		return false;
	}

	return FastArrayDeltaSerialize<FActiveDNACue>(DNACues, DeltaParms, *this);
}
