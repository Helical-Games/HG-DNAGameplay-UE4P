// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "AttributeSet.h"
#include "EdGraphUtilities.h"
#include "EdGraphSchema_K2.h"
#include "SGraphPin.h"
#include "SDNAAttributeGraphPin.h"

class FDNAAbilitiesGraphPanelPinFactory: public FGraphPanelPinFactory
{
	virtual TSharedPtr<class SGraphPin> CreatePin(class UEdGraphPin* InPin) const override
	{
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
		if (InPin->PinType.PinCategory == K2Schema->PC_Struct && InPin->PinType.PinSubCategoryObject == FDNAAttribute::StaticStruct())
		{
			return SNew(SDNAAttributeGraphPin, InPin);
		}
		return NULL;
	}
};
