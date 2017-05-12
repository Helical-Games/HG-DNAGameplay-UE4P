// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_DNAAbilitiesBlueprint.h"
#include "K2Node_DNACueEvent.h"
#include "EdGraph/EdGraph.h"
#include "DNATagContainer.h"
#include "DNATagsManager.h"
#include "DNACueInterface.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintEventNodeSpawner.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "BlueprintActionDatabaseRegistrar.h"

#define LOCTEXT_NAMESPACE "K2Node_DNACueEvent"

UK2Node_DNACueEvent::UK2Node_DNACueEvent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	EventReference.SetExternalMember(DNAABILITIES_BlueprintCustomHandler, UDNACueInterface::StaticClass());
}

void UK2Node_DNACueEvent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if(Ar.IsLoading())
	{
		if(Ar.UE4Ver() < VER_UE4_K2NODE_EVENT_MEMBER_REFERENCE && EventSignatureName_DEPRECATED.IsNone() && EventSignatureClass_DEPRECATED == nullptr)
		{
			EventReference.SetExternalMember(DNAABILITIES_BlueprintCustomHandler, UDNACueInterface::StaticClass());
		}
	}
}

FText UK2Node_DNACueEvent::GetTooltipText() const
{
	return FText::Format(LOCTEXT("DNACueEvent_Tooltip", "Handle DNACue Event {0}"), FText::FromName(CustomFunctionName));
}

FText UK2Node_DNACueEvent::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FText::FromName(CustomFunctionName);
	//return LOCTEXT("HandleDNACueEvent", "HandleGameplaCueEvent");
}

bool UK2Node_DNACueEvent::IsCompatibleWithGraph(UEdGraph const* TargetGraph) const
{
	bool bIsCompatible = false;
	if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph))
	{
		check(Blueprint->GeneratedClass != nullptr);
		bIsCompatible = Blueprint->GeneratedClass->ImplementsInterface(UDNACueInterface::StaticClass());
	}	
	return bIsCompatible && Super::IsCompatibleWithGraph(TargetGraph);
}

void UK2Node_DNACueEvent::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	// actions get registered under specific object-keys; the idea is that 
	// actions might have to be updated (or deleted) if their object-key is  
	// mutated (or removed)... here we use the node's class (so if the node 
	// type disappears, then the action should go with it)
	UClass* ActionKey = GetClass();
	// to keep from needlessly instantiating a UBlueprintNodeSpawner, first   
	// check to make sure that the registrar is looking for actions of this type
	// (could be regenerating actions for a specific asset, and therefore the 
	// registrar would only accept actions corresponding to that asset)
	if (!ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		return;
	}

	auto CustomizeCueNodeLambda = [](UEdGraphNode* NewNode, bool bIsTemplateNode, FName TagName)
	{
		UK2Node_DNACueEvent* EventNode = CastChecked<UK2Node_DNACueEvent>(NewNode);
		EventNode->CustomFunctionName = TagName;
	};
	
	UDNATagsManager& Manager = UDNATagsManager::Get();
	FDNATag RootTag = Manager.RequestDNATag(FName(TEXT("DNACue")), false);
	if (RootTag.IsValid())
	{
		FDNATagContainer CueTags = Manager.RequestDNATagChildren(RootTag);
		// Add a root DNACue function as a default
		CueTags.AddTag(RootTag);
		for (auto TagIt = CueTags.CreateConstIterator(); TagIt; ++TagIt)
		{
			UBlueprintNodeSpawner::FCustomizeNodeDelegate PostSpawnDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateStatic(CustomizeCueNodeLambda, TagIt->GetTagName());
		
			UBlueprintNodeSpawner* NodeSpawner = UBlueprintEventNodeSpawner::Create(GetClass(), TagIt->GetTagName());
			check(NodeSpawner != nullptr);
			NodeSpawner->CustomizeNodeDelegate = PostSpawnDelegate;
		
			ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
		}
	}
}

#undef LOCTEXT_NAMESPACE
