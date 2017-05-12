// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "DNATagsK2Node_MultiCompareBase.h"
#include "DNATagsK2Node_MultiCompareDNATagContainer.generated.h"


UCLASS()
class UDNATagsK2Node_MultiCompareDNATagContainer : public UDNATagsK2Node_MultiCompareBase
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	// End of UEdGraphNode interface

	// UK2Node interface
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;		
	// End of UK2Node interface

private:

	virtual void AddPinToSwitchNode() override;
};
