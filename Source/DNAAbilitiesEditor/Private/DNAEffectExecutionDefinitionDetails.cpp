// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_DNAAbilitiesBlueprint.h"
#include "DNAEffectExecutionDefinitionDetails.h"
#include "IDetailChildrenBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "DNAEffectTypes.h"
#include "DNAEffect.h"
#include "DNAEffectExecutionCalculation.h"
#include "IPropertyUtilities.h"

#define LOCTEXT_NAMESPACE "DNAEffectExecutionDefinitionDetailsCustomization"

TSharedRef<IPropertyTypeCustomization> FDNAEffectExecutionDefinitionDetails::MakeInstance()
{
	return MakeShareable(new FDNAEffectExecutionDefinitionDetails());
}

void FDNAEffectExecutionDefinitionDetails::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	HeaderRow
	.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	];
}

void FDNAEffectExecutionDefinitionDetails::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	bShowCalculationModifiers = false;
	bShowPassedInTags = false;

	// @todo: For now, only allow single editing
	if (StructPropertyHandle->GetNumOuterObjects() == 1)
	{
		CalculationClassPropHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDNAEffectExecutionDefinition, CalculationClass));
		TSharedPtr<IPropertyHandle> ConditionalEffectsPropHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDNAEffectExecutionDefinition, ConditionalDNAEffects));
		TSharedPtr<IPropertyHandle> CalcModPropHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDNAEffectExecutionDefinition, CalculationModifiers));
		TSharedPtr<IPropertyHandle> PassedInTagsHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDNAEffectExecutionDefinition, PassedInTags));
		CalculationModifiersArrayPropHandle = CalcModPropHandle.IsValid() ? CalcModPropHandle->AsArray() : nullptr;

		if (CalculationClassPropHandle.IsValid())
		{
			CalculationClassPropHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FDNAEffectExecutionDefinitionDetails::OnCalculationClassChanged));
			StructBuilder.AddChildProperty(CalculationClassPropHandle.ToSharedRef());
			StructCustomizationUtils.GetPropertyUtilities()->EnqueueDeferredAction(FSimpleDelegate::CreateSP(this, &FDNAEffectExecutionDefinitionDetails::UpdateCalculationModifiers));
		}

		if (CalculationModifiersArrayPropHandle.IsValid())
		{
			IDetailPropertyRow& PropRow = StructBuilder.AddChildProperty(CalcModPropHandle.ToSharedRef());
			PropRow.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FDNAEffectExecutionDefinitionDetails::GetCalculationModifierVisibility)));
		}

		if (ConditionalEffectsPropHandle.IsValid())
		{
			StructBuilder.AddChildProperty(ConditionalEffectsPropHandle.ToSharedRef());
		}

		if (PassedInTagsHandle.IsValid())
		{
			IDetailPropertyRow& PropRow = StructBuilder.AddChildProperty(PassedInTagsHandle.ToSharedRef());
			PropRow.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FDNAEffectExecutionDefinitionDetails::GetPassedInTagsVisibility)));
		}
	}
}

void FDNAEffectExecutionDefinitionDetails::OnCalculationClassChanged()
{
	UpdateCalculationModifiers();
}

void FDNAEffectExecutionDefinitionDetails::UpdateCalculationModifiers()
{
	TArray<FDNAEffectAttributeCaptureDefinition> ValidCaptureDefinitions;
	
	// Try to extract the collection of valid capture definitions from the execution class CDO, if possible
	if (CalculationClassPropHandle.IsValid())
	{
		UObject* ObjValue = nullptr;
		CalculationClassPropHandle->GetValue(ObjValue);

		UClass* ClassObj = Cast<UClass>(ObjValue);
		if (ClassObj)
		{
			const UDNAEffectExecutionCalculation* ExecutionCDO = ClassObj->GetDefaultObject<UDNAEffectExecutionCalculation>();
			if (ExecutionCDO)
			{
				ExecutionCDO->GetValidScopedModifierAttributeCaptureDefinitions(ValidCaptureDefinitions);

				// Grab this while we are at it so we know if we should show the 'Passed In Tags' property
				bShowPassedInTags = ExecutionCDO->DoesRequirePassedInTags();
			}
		}
	}

	// Want to hide the calculation modifiers if there are no valid definitions
	bShowCalculationModifiers = (ValidCaptureDefinitions.Num() > 0);

	// Need to prune out any modifiers that are specified for definitions that aren't specified by the execution class
	if (CalculationModifiersArrayPropHandle.IsValid())
	{
		uint32 NumChildren = 0;
		CalculationModifiersArrayPropHandle->GetNumElements(NumChildren);

		// If there aren't any valid definitions, just dump the whole array
		if (ValidCaptureDefinitions.Num() == 0)
		{
			if (NumChildren > 0)
			{
				CalculationModifiersArrayPropHandle->EmptyArray();
			}
		}
		// There are some valid definitions, so verify any existing ones to make sure they are in the valid array
		else
		{
			for (int32 ChildIdx = NumChildren - 1; ChildIdx >= 0; --ChildIdx)
			{
				TSharedRef<IPropertyHandle> ChildPropHandle = CalculationModifiersArrayPropHandle->GetElement(ChildIdx);
				
				TArray<const void*> RawScopedModInfoStructs;
				ChildPropHandle->AccessRawData(RawScopedModInfoStructs);

				// @todo: For now, only allow single editing
				ensure(RawScopedModInfoStructs.Num() == 1);
				const FDNAEffectExecutionScopedModifierInfo& CurModInfo = *reinterpret_cast<const FDNAEffectExecutionScopedModifierInfo*>(RawScopedModInfoStructs[0]);
				if (!ValidCaptureDefinitions.Contains(CurModInfo.CapturedAttribute))
				{
					CalculationModifiersArrayPropHandle->DeleteItem(ChildIdx);
				}
			}
		}
	}
}

EVisibility FDNAEffectExecutionDefinitionDetails::GetCalculationModifierVisibility() const
{
	return (bShowCalculationModifiers ? EVisibility::Visible : EVisibility::Collapsed);
}

EVisibility FDNAEffectExecutionDefinitionDetails::GetPassedInTagsVisibility() const
{
	return (bShowPassedInTags ? EVisibility::Visible : EVisibility::Collapsed);
}

#undef LOCTEXT_NAMESPACE
