// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DNADebuggerAddonManager.h"
#include "DNADebuggerCategoryConfigCustomization.h"

#if WITH_EDITOR
#include "UObject/UnrealType.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Application/SlateWindowHelper.h"
#include "Widgets/Text/STextBlock.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DNADebuggerConfig.h"

#define LOCTEXT_NAMESPACE "DNADebuggerConfig"

void FDNADebuggerCategoryConfigCustomization::CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	CategoryNameProp = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDNADebuggerCategoryConfig, CategoryName));
	SlotIdxProp = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDNADebuggerCategoryConfig, SlotIdx));
	ActiveInGameProp = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDNADebuggerCategoryConfig, ActiveInGame));
	ActiveInSimulateProp = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDNADebuggerCategoryConfig, ActiveInSimulate));
	
	FSimpleDelegate RefreshHeaderDelegate = FSimpleDelegate::CreateSP(this, &FDNADebuggerCategoryConfigCustomization::OnChildValueChanged);
	StructPropertyHandle->SetOnChildPropertyValueChanged(RefreshHeaderDelegate);
	OnChildValueChanged();

	HeaderRow.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent().VAlign(VAlign_Center).MinDesiredWidth(300.0f)
	[
		SNew(STextBlock)
		.Text(this, &FDNADebuggerCategoryConfigCustomization::GetHeaderDesc)
		.Font(IDetailLayoutBuilder::GetDetailFont())
	];
};

void FDNADebuggerCategoryConfigCustomization::CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	uint32 NumChildProps = 0;
	StructPropertyHandle->GetNumChildren(NumChildProps);

	for (uint32 Idx = 0; Idx < NumChildProps; Idx++)
	{
		TSharedPtr<IPropertyHandle> PropHandle = StructPropertyHandle->GetChildHandle(Idx);
		if (PropHandle->GetProperty() && PropHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FDNADebuggerCategoryConfig, CategoryName))
		{
			continue;
		}

		StructBuilder.AddChildProperty(PropHandle.ToSharedRef());
	}
};

void FDNADebuggerCategoryConfigCustomization::OnChildValueChanged()
{
	FString CategoryNameValue;
	if (CategoryNameProp.IsValid())
	{
		CategoryNameProp->GetValue(CategoryNameValue);
	}

	int32 SlotIdxValue = -1;
	if (SlotIdxProp.IsValid())
	{
		SlotIdxProp->GetValue(SlotIdxValue);
	}

	uint8 ActiveInGameValue = 0;
	if (ActiveInGameProp.IsValid())
	{
		ActiveInGameProp->GetValue(ActiveInGameValue);
	}

	uint8 ActiveInSimulateValue = 0;
	if (ActiveInSimulateProp.IsValid())
	{
		ActiveInSimulateProp->GetValue(ActiveInSimulateValue);
	}

	CachedHeader = FText::FromString(FString::Printf(TEXT("[%s]:%s%s%s"),
		SlotIdxValue < 0 ? TEXT("-") : *TTypeToString<int32>::ToString(SlotIdxValue),
		CategoryNameValue.Len() ? *CategoryNameValue : TEXT("??"),
		(ActiveInGameValue == (uint8)EDNADebuggerOverrideMode::UseDefault) ? TEXT("") :
			(ActiveInGameValue == (uint8)EDNADebuggerOverrideMode::Enable) ? TEXT(" game:ON") : TEXT(" game:OFF"),
		(ActiveInSimulateValue == (uint8)EDNADebuggerOverrideMode::UseDefault) ? TEXT("") :
			(ActiveInSimulateValue == (uint8)EDNADebuggerOverrideMode::Enable) ? TEXT(" simulate:ON") : TEXT(" simulate:OFF")
		));
}

#undef LOCTEXT_NAMESPACE
#endif // WITH_EDITOR
