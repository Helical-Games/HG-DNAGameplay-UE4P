// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "SDNATagQueryWidget.h"
#include "EditorUndoClient.h"

/** Customization for the DNA tag query struct */
class FDNATagQueryCustomization : public IPropertyTypeCustomization, public FEditorUndoClient
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FDNATagQueryCustomization);
	}

	~FDNATagQueryCustomization();

	/** Overridden to show an edit button to launch the DNA tag editor */
	virtual void CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override {}

private:
	/** Called when the edit button is clicked; Launches the DNA tag editor */
	FReply OnEditButtonClicked();

	/** Overridden to do nothing */
	FText GetEditButtonText() const;

	/** Called when the "clear all" button is clicked */
	FReply OnClearAllButtonClicked();

	/** Returns the visibility of the "clear all" button (collapsed when there are no tags) */
	EVisibility GetClearAllVisibility() const;

	/** Returns the visibility of the tags list box (collapsed when there are no tags) */
	EVisibility GetQueryDescVisibility() const;

	void RefreshQueryDescription();

	FText GetQueryDescText() const;

	void CloseWidgetWindow();

	/** Build List of Editable Queries */
	void BuildEditableQueryList();

	/** Cached property handle */
	TSharedPtr<IPropertyHandle> StructPropertyHandle;

	/** The array of queries this objects has */
	TArray<SDNATagQueryWidget::FEditableDNATagQueryDatum> EditableQueries;

	/** The Window for the DNATagWidget */
	TSharedPtr<SWindow> DNATagQueryWidgetWindow;

	FString QueryDescription;
};

