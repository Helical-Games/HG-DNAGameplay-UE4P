// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNATagsEditorModulePrivatePCH.h"
#include "DNATagsK2Node_LiteralDNATag.h"
#include "KismetCompiler.h"
#include "DNATags.h"
#include "BlueprintNodeSpawner.h"
#include "EditorCategoryUtils.h"
#include "BlueprintActionDatabaseRegistrar.h"

#define LOCTEXT_NAMESPACE "DNATagsK2Node_LiteralDNATag"

UDNATagsK2Node_LiteralDNATag::UDNATagsK2Node_LiteralDNATag(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void UDNATagsK2Node_LiteralDNATag::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	CreatePin(EGPD_Input, K2Schema->PC_String, TEXT("LiteralDNATagContainer"), NULL, false, false, TEXT("TagIn"));
	CreatePin(EGPD_Output, K2Schema->PC_Struct, TEXT(""), FDNATagContainer::StaticStruct(), false, false, K2Schema->PN_ReturnValue);
}

FLinearColor UDNATagsK2Node_LiteralDNATag::GetNodeTitleColor() const
{
	return FLinearColor(1.0f, 0.51f, 0.0f);
}

FText UDNATagsK2Node_LiteralDNATag::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NSLOCTEXT("K2Node", "LiteralDNATag", "Make Literal DNATagContainer");
}

bool UDNATagsK2Node_LiteralDNATag::CanCreateUnderSpecifiedSchema( const UEdGraphSchema* Schema ) const
{
	return Schema->IsA(UEdGraphSchema_K2::StaticClass());
}

void UDNATagsK2Node_LiteralDNATag::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();

	// Get The input and output pins to our node
	UEdGraphPin* TagInPin = FindPin(TEXT("TagIn"));
	UEdGraphPin* TagOutPin = FindPinChecked(Schema->PN_ReturnValue);

	// Create a Make Struct
	UK2Node_MakeStruct* MakeStructNode = SourceGraph->CreateBlankNode<UK2Node_MakeStruct>();
	MakeStructNode->StructType = FDNATagContainer::StaticStruct();
	MakeStructNode->AllocateDefaultPins();

	// Create a Make Array
	UK2Node_MakeArray* MakeArrayNode = SourceGraph->CreateBlankNode<UK2Node_MakeArray>();
	MakeArrayNode->AllocateDefaultPins();
		
	// Connect the output of our MakeArray to the Input of our MakeStruct so it sets the Array Type
	UEdGraphPin* InPin = MakeStructNode->FindPin( TEXT("DNATags") );
	if( InPin )
	{
		InPin->MakeLinkTo( MakeArrayNode->GetOutputPin() );
	}

	// Add the FName Values to the MakeArray input pins
	UEdGraphPin* ArrayInputPin = NULL;
	FString TagString = TagInPin->GetDefaultAsString();

	if( TagString.StartsWith( TEXT("(") ) && TagString.EndsWith( TEXT(")") ) )
	{
		TagString = TagString.LeftChop(1);
		TagString = TagString.RightChop(1);
		TagString.Split("=", NULL, &TagString);
		TagString = TagString.LeftChop(1);
		TagString = TagString.RightChop(1);

		FString ReadTag;
		FString Remainder;
		int32 MakeIndex = 0;
		while( TagString.Split( TEXT(","), &ReadTag, &Remainder ) ) 
		{
			TagString = Remainder;

			ArrayInputPin = MakeArrayNode->FindPin( FString::Printf( TEXT("[%d]"), MakeIndex ) );
			ArrayInputPin->PinType.PinCategory = TEXT("struct");
			ArrayInputPin->PinType.PinSubCategoryObject = FDNATag::StaticStruct();
			ArrayInputPin->DefaultValue = ReadTag;

			MakeIndex++;
			MakeArrayNode->AddInputPin();
		}
		if( Remainder.IsEmpty() )
		{
			Remainder = TagString;
		}
		if( !Remainder.IsEmpty() )
		{
			ArrayInputPin = MakeArrayNode->FindPin( FString::Printf( TEXT("[%d]"), MakeIndex ) );
			ArrayInputPin->PinType.PinCategory = TEXT("struct");
			ArrayInputPin->PinType.PinSubCategoryObject = FDNATag::StaticStruct();
			ArrayInputPin->DefaultValue = Remainder;

			MakeArrayNode->PostReconstructNode();
		}
		else
		{
			MakeArrayNode->RemoveInputPin(MakeArrayNode->FindPin(TEXT("[0]")));
			MakeArrayNode->PostReconstructNode();
		}
	}
	else
	{
		MakeArrayNode->RemoveInputPin(MakeArrayNode->FindPin(TEXT("[0]")));
		MakeArrayNode->PostReconstructNode();
	}

	// Move the Output of the MakeArray to the Output of our node
	UEdGraphPin* OutPin = MakeStructNode->FindPin( MakeStructNode->StructType->GetName() );
	if( OutPin && TagOutPin )
	{
		OutPin->PinType = TagOutPin->PinType; // Copy type so it uses the right actor subclass
		CompilerContext.MovePinLinksToIntermediate(*TagOutPin, *OutPin);
	}

	// Break any links to the expanded node
	BreakAllNodeLinks();
}

void UDNATagsK2Node_LiteralDNATag::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
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

FText UDNATagsK2Node_LiteralDNATag::GetMenuCategory() const
{
	return LOCTEXT("ActionMenuCategory", "DNA Tags|Tag Container");
}

#undef LOCTEXT_NAMESPACE
