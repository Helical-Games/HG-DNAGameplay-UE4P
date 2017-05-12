// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "K2Node_Switch.h"
#include "DNATagContainer.h"
#include "DNATagsK2Node_SwitchDNATag.generated.h"

struct FDNATag;

UCLASS(MinimalAPI)
class UDNATagsK2Node_SwitchDNATag : public UK2Node_Switch
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = PinOptions)
	TArray<FDNATag> PinTags;

	//UPROPERTY(EditAnywhere, Category = PinOptions)
	//bool UseInputsOnGraph;

	UPROPERTY()
	TArray<FName> PinNames;

	// UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostLoad() override;
	// End of UObject interface

	// UEdGraphNode interface
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual bool ShouldShowNodeProperties() const override { return true; }
	// End of UEdGraphNode interface

	// UK2Node interface
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	// End of UK2Node interface

	// UK2Node_Switch Interface
	DNATAGSEDITOR_API virtual void AddPinToSwitchNode() override;
	virtual FString GetUniquePinName() override;
	virtual FEdGraphPinType GetPinType() const override;
	// End of UK2Node_Switch Interface

	virtual FString GetPinNameGivenIndex(int32 Index) override;

protected:
	virtual void CreateFunctionPin() override;
	virtual void CreateSelectionPin() override;
	virtual void CreateCasePins() override;
	virtual void RemovePin(UEdGraphPin* TargetPin) override;
};
