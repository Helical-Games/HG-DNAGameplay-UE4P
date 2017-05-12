// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_DNAAbilitiesBlueprint.h"
#include "DNAAbilityGraphSchema.h"
#include "EdGraphSchema_K2_Actions.h"
#include "DNAEffect.h"
#include "Kismet2/BlueprintEditorUtils.h"

UDNAAbilityGraphSchema::UDNAAbilityGraphSchema(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

UK2Node_VariableGet* UDNAAbilityGraphSchema::SpawnVariableGetNode(const FVector2D GraphPosition, class UEdGraph* ParentGraph, FName VariableName, UStruct* Source) const
{
	return Super::SpawnVariableGetNode(GraphPosition, ParentGraph, VariableName, Source);
}

UK2Node_VariableSet* UDNAAbilityGraphSchema::SpawnVariableSetNode(const FVector2D GraphPosition, class UEdGraph* ParentGraph, FName VariableName, UStruct* Source) const
{
	return Super::SpawnVariableSetNode(GraphPosition, ParentGraph, VariableName, Source);
}
