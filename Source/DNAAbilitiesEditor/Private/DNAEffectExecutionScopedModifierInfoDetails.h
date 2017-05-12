// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "Widgets/SWidget.h"
#include "IPropertyTypeCustomization.h"
#include "DNAEffectTypes.h"
#include "PropertyHandle.h"
#include "IDetailChildrenBuilder.h"

class FDetailWidgetRow;
class SCaptureDefWidget;

/** Details customization for FDNAEffectExecutionScopedModifierInfo */
class FDNAEffectExecutionScopedModifierInfoDetails : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** Overridden to provide the property name */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	
	/** Overridden to allow for a custom selection widget for scoped modifiers inside a custom execution */
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

private:

	/** Delegate called when combo box selection is changed */
	void OnCaptureDefComboBoxSelectionChanged(TSharedPtr<FDNAEffectAttributeCaptureDefinition> InSelectedItem, ESelectInfo::Type InSelectInfo);
	
	/* Called to generate the widgets for custom combo box entries */
	TSharedRef<SWidget> OnGenerateCaptureDefComboWidget(TSharedPtr<FDNAEffectAttributeCaptureDefinition> InItem);
	
	/** Get the current capture definition as specified by the backing property, if possible; Otherwise falls back to first available definition from execution class */
	TSharedPtr<FDNAEffectAttributeCaptureDefinition> GetCurrentCaptureDef() const;

	/** Set the current capture definition */
	void SetCurrentCaptureDef(TSharedPtr<FDNAEffectAttributeCaptureDefinition> InDef);

	/** Cached property handle for the capture definition property */
	TSharedPtr<IPropertyHandle> CaptureDefPropertyHandle;

	/** Primary capture definition widget shown for the custom combo box */
	TSharedPtr<SCaptureDefWidget> PrimaryCaptureDefWidget;

	/** Backing source for the custom combo box; Populated by all valid definitions from the execution class */
	TArray< TSharedPtr<FDNAEffectAttributeCaptureDefinition> > AvailableCaptureDefs;
};
