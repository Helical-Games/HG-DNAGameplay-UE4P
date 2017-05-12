// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_DNAAbilitiesBlueprint.h"
#include "DNAEffectModifierMagnitudeDetails.h"
#include "IDetailChildrenBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"

#define LOCTEXT_NAMESPACE "DNAEffectModifierMagnitudeDetailsCustomization"

TSharedRef<IPropertyTypeCustomization> FDNAEffectModifierMagnitudeDetails::MakeInstance()
{
	return MakeShareable(new FDNAEffectModifierMagnitudeDetails());
}

void FDNAEffectModifierMagnitudeDetails::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	HeaderRow
	.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	];
}

void FDNAEffectModifierMagnitudeDetails::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	// Set up acceleration map
	PropertyToCalcEnumMap.Empty();
	PropertyToCalcEnumMap.Add(FindFieldChecked<UProperty>(FDNAEffectModifierMagnitude::StaticStruct(), GET_MEMBER_NAME_CHECKED(FDNAEffectModifierMagnitude, ScalableFloatMagnitude)), EDNAEffectMagnitudeCalculation::ScalableFloat);
	PropertyToCalcEnumMap.Add(FindFieldChecked<UProperty>(FDNAEffectModifierMagnitude::StaticStruct(), GET_MEMBER_NAME_CHECKED(FDNAEffectModifierMagnitude, AttributeBasedMagnitude)), EDNAEffectMagnitudeCalculation::AttributeBased);
	PropertyToCalcEnumMap.Add(FindFieldChecked<UProperty>(FDNAEffectModifierMagnitude::StaticStruct(), GET_MEMBER_NAME_CHECKED(FDNAEffectModifierMagnitude, CustomMagnitude)), EDNAEffectMagnitudeCalculation::CustomCalculationClass);
	PropertyToCalcEnumMap.Add(FindFieldChecked<UProperty>(FDNAEffectModifierMagnitude::StaticStruct(), GET_MEMBER_NAME_CHECKED(FDNAEffectModifierMagnitude, SetByCallerMagnitude)), EDNAEffectMagnitudeCalculation::SetByCaller);

	// Hook into calculation type changes
	MagnitudeCalculationTypePropertyHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDNAEffectModifierMagnitude, MagnitudeCalculationType));
	if (MagnitudeCalculationTypePropertyHandle.IsValid())
	{
		MagnitudeCalculationTypePropertyHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FDNAEffectModifierMagnitudeDetails::OnCalculationTypeChanged));
		StructBuilder.AddChildProperty(MagnitudeCalculationTypePropertyHandle.ToSharedRef());
	}
	OnCalculationTypeChanged();

	// Set up visibility delegates on all of the magnitude calculations
	TSharedPtr<IPropertyHandle> ScalableFloatMagnitudePropertyHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDNAEffectModifierMagnitude, ScalableFloatMagnitude));
	if (ScalableFloatMagnitudePropertyHandle.IsValid())
	{
		IDetailPropertyRow& PropRow = StructBuilder.AddChildProperty(ScalableFloatMagnitudePropertyHandle.ToSharedRef());
		PropRow.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FDNAEffectModifierMagnitudeDetails::GetMagnitudeCalculationPropertyVisibility, ScalableFloatMagnitudePropertyHandle->GetProperty())));
	}

	TSharedPtr<IPropertyHandle> AttributeBasedMagnitudePropertyHandle  = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDNAEffectModifierMagnitude, AttributeBasedMagnitude));
	if (AttributeBasedMagnitudePropertyHandle.IsValid())
	{
		IDetailPropertyRow& PropRow = StructBuilder.AddChildProperty(AttributeBasedMagnitudePropertyHandle.ToSharedRef());
		PropRow.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FDNAEffectModifierMagnitudeDetails::GetMagnitudeCalculationPropertyVisibility, AttributeBasedMagnitudePropertyHandle->GetProperty())));
	}

	TSharedPtr<IPropertyHandle> CustomMagnitudePropertyHandle  = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDNAEffectModifierMagnitude, CustomMagnitude));
	if (CustomMagnitudePropertyHandle.IsValid())
	{
		IDetailPropertyRow& PropRow = StructBuilder.AddChildProperty(CustomMagnitudePropertyHandle.ToSharedRef());
		PropRow.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FDNAEffectModifierMagnitudeDetails::GetMagnitudeCalculationPropertyVisibility, CustomMagnitudePropertyHandle->GetProperty())));
	}

	TSharedPtr<IPropertyHandle> SetByCallerPropertyHandle  = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDNAEffectModifierMagnitude, SetByCallerMagnitude));
	if (SetByCallerPropertyHandle.IsValid())
	{
		IDetailPropertyRow& PropRow = StructBuilder.AddChildProperty(SetByCallerPropertyHandle.ToSharedRef());
		PropRow.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FDNAEffectModifierMagnitudeDetails::GetMagnitudeCalculationPropertyVisibility, SetByCallerPropertyHandle->GetProperty())));
	}
}

void FDNAEffectModifierMagnitudeDetails::OnCalculationTypeChanged()
{
	uint8 EnumVal;
	MagnitudeCalculationTypePropertyHandle->GetValue(EnumVal);
	VisibleCalculationType = static_cast<EDNAEffectMagnitudeCalculation>(EnumVal);
}

EVisibility FDNAEffectModifierMagnitudeDetails::GetMagnitudeCalculationPropertyVisibility(UProperty* InProperty) const
{
	const EDNAEffectMagnitudeCalculation* EnumVal = PropertyToCalcEnumMap.Find(InProperty);
	return ((EnumVal && (*EnumVal == VisibleCalculationType)) ? EVisibility::Visible : EVisibility::Collapsed);
}

#undef LOCTEXT_NAMESPACE
