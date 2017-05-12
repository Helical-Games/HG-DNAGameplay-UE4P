// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "SGraphPin.h"

class SDNAAttributeGraphPin : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SDNAAttributeGraphPin) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

	//~ Begin SGraphPin Interface
	virtual TSharedRef<SWidget>	GetDefaultValueWidget() override;
	//~ End SGraphPin Interface

	void OnAttributeChanged(UProperty* SelectedAttribute);

	UProperty* LastSelectedProperty;

private:

};
