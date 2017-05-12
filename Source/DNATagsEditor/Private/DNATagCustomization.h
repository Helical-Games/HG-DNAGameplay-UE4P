// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "SDNATagWidget.h"
#include "EditorUndoClient.h"

/** Customization for the DNA tag struct */
class FDNATagCustomization : public IPropertyTypeCustomization, public FEditorUndoClient
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FDNATagCustomization);
	}

	~FDNATagCustomization();

	/** Overridden to show an edit button to launch the DNA tag editor */
	virtual void CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	
	/** Overridden to do nothing */
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override {}

	//~ Begin FEditorUndoClient Interface
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
	//~ End FEditorUndoClient Interface

private:

	/** Updates the selected tag*/
	void OnTagChanged();

	/** Updates the selected tag*/
	void OnPropertyValueChanged();

	/** Build Editable Container */
	void BuildEditableContainerList();

	/** Callback function to create content for the combo button. */
	TSharedRef<SWidget> GetListContent();

	/** Returns Tag name currently selected*/
	FText SelectedTag() const;

	/** Combo Button for the drop down list. */
	TSharedPtr<SComboButton> ComboButton;

	/** Cached property handle */
	TSharedPtr<IPropertyHandle> StructPropertyHandle;

	/** Tag Container used for the DNATagWidget. */
	TSharedPtr<FDNATagContainer> TagContainer;

	/** Editable Container for holding our tag */
	TArray<SDNATagWidget::FEditableDNATagContainerDatum> EditableContainers;

	/** Tag name selected*/
	FString TagName;
};

