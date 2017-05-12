// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "EdGraph/EdGraph.h"
#include "DNATask.h"
#include "K2Node_LatentDNATaskCall.h"
#include "K2Node_LatentDNAAbilityCall.generated.h"

class FBlueprintActionDatabaseRegistrar;

UCLASS()
class UK2Node_LatentDNAAbilityCall : public UK2Node_LatentDNATaskCall
{
	GENERATED_BODY()

public:
	UK2Node_LatentDNAAbilityCall(const FObjectInitializer& ObjectInitializer);

	// UEdGraphNode interface
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual bool IsCompatibleWithGraph(UEdGraph const* TargetGraph) const override;
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;
	// End of UEdGraphNode interface
	
protected:
	virtual bool IsHandling(TSubclassOf<UDNATask> TaskClass) const override;
};
