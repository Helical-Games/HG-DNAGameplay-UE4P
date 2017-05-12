// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "EdGraphUtilities.h"
#include "SDNATagGraphPin.h"
#include "SDNATagContainerGraphPin.h"
#include "SDNATagQueryGraphPin.h"
#include "DNATags.h"

class FDNATagsGraphPanelPinFactory: public FGraphPanelPinFactory
{
	virtual TSharedPtr<class SGraphPin> CreatePin(class UEdGraphPin* InPin) const override
	{
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
		if (InPin->PinType.PinCategory == K2Schema->PC_Struct && InPin->PinType.PinSubCategoryObject == FDNATag::StaticStruct())
		{
			return SNew(SDNATagGraphPin, InPin);
		}
		if (InPin->PinType.PinCategory == K2Schema->PC_Struct && InPin->PinType.PinSubCategoryObject == FDNATagContainer::StaticStruct())
		{
			return SNew(SDNATagContainerGraphPin, InPin);
		}
		if (InPin->PinType.PinCategory == K2Schema->PC_String && InPin->PinType.PinSubCategory == TEXT("LiteralDNATagContainer"))
		{
			return SNew(SDNATagContainerGraphPin, InPin);
		}
		if (InPin->PinType.PinCategory == K2Schema->PC_Struct && InPin->PinType.PinSubCategoryObject == FDNATagQuery::StaticStruct())
		{
			return SNew(SDNATagQueryGraphPin, InPin);
		}

		return nullptr;
	}
};
