// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

class SGraphNode_MultiCompareDNATag : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SGraphNode_MultiCompareDNATag){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, class UDNATagsK2Node_MultiCompareBase* InNode);

protected:

	// SGraphNode interface
	virtual void CreateOutputSideAddButton(TSharedPtr<SVerticalBox> OutputBox) override;
	virtual void CreateOutputSideRemoveButton(TSharedPtr<SVerticalBox> OutputBox);
	virtual EVisibility IsAddPinButtonVisible() const override;
	virtual FReply OnAddPin() override;
	// End of SGraphNode interface

	EVisibility IsRemovePinButtonVisible() const;
	FReply OnRemovePin();
};
