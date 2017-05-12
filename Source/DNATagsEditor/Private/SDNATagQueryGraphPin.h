// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "SlateBasics.h"
#include "SDNATagQueryWidget.h"
#include "SGraphPin.h"

class SDNATagQueryGraphPin : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SDNATagQueryGraphPin) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

	//~ Begin SGraphPin Interface
	virtual TSharedRef<SWidget>	GetDefaultValueWidget() override;
	//~ End SGraphPin Interface

private:

	/** Parses the Data from the pin to fill in the names of the array. */
	void ParseDefaultValueData();

	/** Callback function to create content for the combo button. */
	TSharedRef<SWidget> GetListContent();

	void OnQueryChanged();

	/**
	 * Creates SelectedTags List.
	 * @return widget that contains the read only tag names for displaying on the node.
	 */
	TSharedRef<SWidget> QueryDesc();

private:

	// Combo Button for the drop down list.
	TSharedPtr<SComboButton> ComboButton;

	// Tag Container used for the DNATagWidget.
	TSharedPtr<FDNATagQuery> TagQuery;

	FString TagQueryExportText;

	// Datum uses for the DNATagWidget.
	TArray<SDNATagQueryWidget::FEditableDNATagQueryDatum> EditableQueries;

	FText GetQueryDescText() const;

	FString QueryDescription;
};
