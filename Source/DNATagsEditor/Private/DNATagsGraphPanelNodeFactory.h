// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "EdGraphUtilities.h"
#include "SGraphNode_MultiCompareDNATag.h"
#include "DNATagsK2Node_MultiCompareBase.h"

class FDNATagsGraphPanelNodeFactory : public FGraphPanelNodeFactory
{
	virtual TSharedPtr<class SGraphNode> CreateNode(class UEdGraphNode* InNode) const override
	{
		if (UDNATagsK2Node_MultiCompareBase* CompareNode = Cast<UDNATagsK2Node_MultiCompareBase>(InNode))
		{
			return SNew(SGraphNode_MultiCompareDNATag, CompareNode);
		}

		return NULL;
	}
};
