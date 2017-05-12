// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DNADebuggerAddonManager.h"
#include "DNADebuggerInputConfigCustomization.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Engine/GameViewportClient.h"
#include "DNADebuggerTypes.h"

#if WITH_EDITOR
#include "Widgets/Text/STextBlock.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DNADebuggerConfig.h"

#define LOCTEXT_NAMESPACE "DNADebuggerConfig"

void FDNADebuggerInputConfigCustomization::CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	ConfigNameProp = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDNADebuggerInputConfig, ConfigName));
	KeyProp = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDNADebuggerInputConfig, Key));
	ModShiftProp = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDNADebuggerInputConfig, bModShift));
	ModCtrlProp = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDNADebuggerInputConfig, bModCtrl));
	ModAltProp = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDNADebuggerInputConfig, bModAlt));
	ModCmdProp = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDNADebuggerInputConfig, bModCmd));

	FSimpleDelegate RefreshHeaderDelegate = FSimpleDelegate::CreateSP(this, &FDNADebuggerInputConfigCustomization::OnChildValueChanged);
	StructPropertyHandle->SetOnChildPropertyValueChanged(RefreshHeaderDelegate);
	OnChildValueChanged();

	HeaderRow.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent().VAlign(VAlign_Center).MinDesiredWidth(300.0f)
	[
		SNew(STextBlock)
		.Text(this, &FDNADebuggerInputConfigCustomization::GetHeaderDesc)
		.Font(IDetailLayoutBuilder::GetDetailFont())
	];
};

void FDNADebuggerInputConfigCustomization::CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	uint32 NumChildProps = 0;
	StructPropertyHandle->GetNumChildren(NumChildProps);

	for (uint32 Idx = 0; Idx < NumChildProps; Idx++)
	{
		TSharedPtr<IPropertyHandle> PropHandle = StructPropertyHandle->GetChildHandle(Idx);
		if (PropHandle->GetProperty() && PropHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FDNADebuggerInputConfig, ConfigName))
		{
			continue;
		}

		StructBuilder.AddChildProperty(PropHandle.ToSharedRef());
	}
};

void FDNADebuggerInputConfigCustomization::OnChildValueChanged()
{
	FString ConfigNameValue;
	if (ConfigNameProp.IsValid())
	{
		ConfigNameProp->GetValue(ConfigNameValue);
	}

	FString KeyValue;
	if (KeyProp.IsValid())
	{
		KeyProp->GetPerObjectValue(0, KeyValue);
	}

	bool bModShift = false;
	if (ModShiftProp.IsValid())
	{
		ModShiftProp->GetValue(bModShift);
	}

	bool bModCtrl = false;
	if (ModCtrlProp.IsValid())
	{
		ModCtrlProp->GetValue(bModCtrl);
	}
	
	bool bModAlt = false;
	if (ModAltProp.IsValid())
	{
		ModAltProp->GetValue(bModAlt);
	}
	
	bool bModCmd = false;
	if (ModCmdProp.IsValid())
	{
		ModCmdProp->GetValue(bModCmd);
	}

	FDNADebuggerInputHandler DummyHandler;
	DummyHandler.KeyName = *KeyValue;
	DummyHandler.Modifier.bShift = bModShift;
	DummyHandler.Modifier.bCtrl = bModCtrl;
	DummyHandler.Modifier.bAlt = bModAlt;
	DummyHandler.Modifier.bCmd = bModCmd;

	CachedHeader = FText::FromString(FString::Printf(TEXT("%s: %s"),
		ConfigNameValue.Len() ? *ConfigNameValue : TEXT("??"),
		*DummyHandler.ToString()
		));
}

#undef LOCTEXT_NAMESPACE
#endif // WITH_EDITOR
