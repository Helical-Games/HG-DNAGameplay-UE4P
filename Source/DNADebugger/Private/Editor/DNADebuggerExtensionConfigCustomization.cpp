// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DNADebuggerAddonManager.h"
#include "DNADebuggerExtensionConfigCustomization.h"

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

void FDNADebuggerExtensionConfigCustomization::CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	ExtensionNameProp = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDNADebuggerExtensionConfig, ExtensionName));
	UseExtensionProp = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDNADebuggerExtensionConfig, UseExtension));
	
	FSimpleDelegate RefreshHeaderDelegate = FSimpleDelegate::CreateSP(this, &FDNADebuggerExtensionConfigCustomization::OnChildValueChanged);
	StructPropertyHandle->SetOnChildPropertyValueChanged(RefreshHeaderDelegate);
	OnChildValueChanged();

	HeaderRow.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent().VAlign(VAlign_Center).MinDesiredWidth(300.0f)
	[
		SNew(STextBlock)
		.Text(this, &FDNADebuggerExtensionConfigCustomization::GetHeaderDesc)
		.Font(IDetailLayoutBuilder::GetDetailFont())
	];
};

void FDNADebuggerExtensionConfigCustomization::CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	uint32 NumChildProps = 0;
	StructPropertyHandle->GetNumChildren(NumChildProps);

	for (uint32 Idx = 0; Idx < NumChildProps; Idx++)
	{
		TSharedPtr<IPropertyHandle> PropHandle = StructPropertyHandle->GetChildHandle(Idx);
		if (PropHandle->GetProperty() && PropHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FDNADebuggerExtensionConfig, ExtensionName))
		{
			continue;
		}

		StructBuilder.AddChildProperty(PropHandle.ToSharedRef());
	}
};

void FDNADebuggerExtensionConfigCustomization::OnChildValueChanged()
{
	FString ExtensionNameValue;
	if (ExtensionNameProp.IsValid())
	{
		ExtensionNameProp->GetValue(ExtensionNameValue);
	}

	uint8 UseExtensionValue = 0;
	if (UseExtensionProp.IsValid())
	{
		UseExtensionProp->GetValue(UseExtensionValue);
	}

	CachedHeader = FText::FromString(FString::Printf(TEXT("%s %s"),
		ExtensionNameValue.Len() ? *ExtensionNameValue : TEXT("??"),
		(UseExtensionValue == (uint8)EDNADebuggerOverrideMode::UseDefault) ? TEXT("") :
			(UseExtensionValue == (uint8)EDNADebuggerOverrideMode::Enable) ? TEXT("is enabled") : TEXT("is disabled")
		));
}

#undef LOCTEXT_NAMESPACE
#endif // WITH_EDITOR
