// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNATagsEditorModulePrivatePCH.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "DNATagContainer.h"
#include "DNATagsK2Node_SwitchDNATag.h"
#include "BlueprintDNATagLibrary.h"

UDNATagsK2Node_SwitchDNATag::UDNATagsK2Node_SwitchDNATag(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FunctionName = TEXT("NotEqual_TagTag");
	FunctionClass = UBlueprintDNATagLibrary::StaticClass();
}

void UDNATagsK2Node_SwitchDNATag::CreateFunctionPin()
{
	// Set properties on the function pin
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	UEdGraphPin* FunctionPin = CreatePin(EGPD_Input, K2Schema->PC_Object, TEXT(""), FunctionClass, false, false, FunctionName.ToString());
	FunctionPin->bDefaultValueIsReadOnly = true;
	FunctionPin->bNotConnectable = true;
	FunctionPin->bHidden = true;

	UFunction* Function = FindField<UFunction>(FunctionClass, FunctionName);
	const bool bIsStaticFunc = Function->HasAllFunctionFlags(FUNC_Static);
	if (bIsStaticFunc)
	{
		// Wire up the self to the CDO of the class if it's not us
		if (UBlueprint* BP = GetBlueprint())
		{
			UClass* FunctionOwnerClass = Function->GetOuterUClass();
			if (!BP->SkeletonGeneratedClass->IsChildOf(FunctionOwnerClass))
			{
				FunctionPin->DefaultObject = FunctionOwnerClass->GetDefaultObject();
			}
		}
	}
}

void UDNATagsK2Node_SwitchDNATag::PostLoad()
{
	Super::PostLoad();
	if (UEdGraphPin* FunctionPin = FindPin(FunctionName.ToString()))
	{
		FunctionPin->DefaultObject = FunctionClass->GetDefaultObject();
	}
}

void UDNATagsK2Node_SwitchDNATag::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	bool bIsDirty = false;
	FName PropertyName = (PropertyChangedEvent.Property != NULL) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if (PropertyName == TEXT("PinTags"))
	{
		bIsDirty = true;
	}

	if (bIsDirty)
	{
		ReconstructNode();
		GetGraph()->NotifyGraphChanged();
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

FText UDNATagsK2Node_SwitchDNATag::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NSLOCTEXT("K2Node", "Switch_Tag", "Switch on DNA Tag");
}

FText UDNATagsK2Node_SwitchDNATag::GetTooltipText() const
{
	return NSLOCTEXT("K2Node", "SwitchTag_ToolTip", "Selects an output that matches the input value");
}

void UDNATagsK2Node_SwitchDNATag::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
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
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

void UDNATagsK2Node_SwitchDNATag::CreateSelectionPin()
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	UEdGraphPin* Pin = CreatePin(EGPD_Input, K2Schema->PC_Struct, TEXT(""), FDNATag::StaticStruct(), false, false, TEXT("Selection"));
	K2Schema->SetPinDefaultValueBasedOnType(Pin);
}

FEdGraphPinType UDNATagsK2Node_SwitchDNATag::GetPinType() const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	FEdGraphPinType PinType;
	PinType.PinCategory = K2Schema->PC_Struct;
	PinType.PinSubCategoryObject = FDNATag::StaticStruct();
	return PinType;
}

FString UDNATagsK2Node_SwitchDNATag::GetPinNameGivenIndex(int32 Index)
{
	check(Index);
	return PinNames[Index].ToString();
}

void UDNATagsK2Node_SwitchDNATag::CreateCasePins()
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	for (int32 Index = 0; Index < PinNames.Num(); ++Index)
  	{	
		if (Index < PinTags.Num())
		{
			if (PinTags[Index].IsValid())
			{
				PinNames[Index] = FName(*PinTags[Index].ToString());
			}			
			else
			{
				PinNames[Index] = FName(*GetUniquePinName());
			}
		}
		CreatePin(EGPD_Output, K2Schema->PC_Exec, TEXT(""), NULL, false, false, PinNames[Index].ToString());
  	}
}

FString UDNATagsK2Node_SwitchDNATag::GetUniquePinName()
{
	FString NewPinName;
	int32 Index = 0;
	while (true)
	{
		NewPinName = FString::Printf(TEXT("Case_%d"), Index++);
		if (!FindPin(NewPinName))
		{
			break;
		}
	}
	return NewPinName;
}

void UDNATagsK2Node_SwitchDNATag::AddPinToSwitchNode()
{
	FString PinName = GetUniquePinName();
	PinNames.Add(FName(*PinName));

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	UEdGraphPin* NewPin = CreatePin(EGPD_Output, K2Schema->PC_Exec, TEXT(""), NULL, false, false, PinName);
	if (PinTags.Num() < PinNames.Num())
	{ 
		PinTags.Add(FDNATag());
	}
}

void UDNATagsK2Node_SwitchDNATag::RemovePin(UEdGraphPin* TargetPin)
{
	checkSlow(TargetPin);

	FString TagName = TargetPin->PinName;

	int32 Index = PinNames.IndexOfByKey(FName(*TagName));
	if (Index>=0)
	{
		if (Index < PinTags.Num())
		{ 
			PinTags.RemoveAt(Index);
		}		
		PinNames.RemoveSingle(FName(*TagName));
	}	
}