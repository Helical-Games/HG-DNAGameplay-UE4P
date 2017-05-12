// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "K2Node.h"
#include "DNATagContainer.h"
#include "DNATagsK2Node_MultiCompareBase.generated.h"

struct FDNATagContainer;

UCLASS()
class UDNATagsK2Node_MultiCompareBase : public UK2Node
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = PinOptions)
	int32 NumberOfPins;

	UPROPERTY()
	TArray<FName> PinNames;

	// UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	// End of UObject interface

	// UEdGraphNode interface
	virtual FText GetTooltipText() const override;
	virtual bool CanDuplicateNode() const override { return false; }
	// End of UEdGraphNode interface

	// UK2Node interface
	virtual bool NodeCausesStructuralBlueprintChange() const override { return true; }
	virtual bool ShouldShowNodeProperties() const override { return true; }
	virtual FText GetMenuCategory() const override;
	virtual bool IsNodePure() const override { return true; }
	// End of UK2Node interface

	void AddPin();
	void RemovePin();

protected:

	virtual void AddPinToSwitchNode(){};
	FString GetUniquePinName();
};
