// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_DNAAbilitiesBlueprint.h"
#include "SDNAAttributeGraphPin.h"
#include "Widgets/SBoxPanel.h"
#include "AttributeSet.h"
#include "SDNAAttributeWidget.h"

#define LOCTEXT_NAMESPACE "K2Node"

void SDNAAttributeGraphPin::Construct( const FArguments& InArgs, UEdGraphPin* InGraphPinObj )
{
	SGraphPin::Construct( SGraphPin::FArguments(), InGraphPinObj );
	LastSelectedProperty = NULL;

}

TSharedRef<SWidget>	SDNAAttributeGraphPin::GetDefaultValueWidget()
{
	// Parse out current default value
	// It will be in the form (Attribute=/Script/<PackageName>.<ObjectName>:<PropertyName>)
	
	FString DefaultString = GraphPinObj->GetDefaultAsString();
	FDNAAttribute DefaultAttribute;

	if (DefaultString.StartsWith(TEXT("(")) && DefaultString.EndsWith(TEXT(")")))
	{
		DefaultString = DefaultString.LeftChop(1);
		DefaultString = DefaultString.RightChop(1);

		DefaultString.Split("=", NULL, &DefaultString);

		DefaultString = DefaultString.LeftChop(1);
		DefaultString = DefaultString.RightChop(1);

		DefaultAttribute.SetUProperty(FindObject<UProperty>(ANY_PACKAGE, *DefaultString));
	}

	//Create widget
	return SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SDNAAttributeWidget)
			.OnAttributeChanged(this, &SDNAAttributeGraphPin::OnAttributeChanged)
			.DefaultProperty(DefaultAttribute.GetUProperty())
		];
}

void SDNAAttributeGraphPin::OnAttributeChanged(UProperty* SelectedAttribute)
{
	FString FinalValue;

	if (SelectedAttribute == nullptr)
	{
		FinalValue = FString(TEXT("()"));
	}
	else
	{
		FinalValue = FString::Printf(TEXT("(Attribute=\"%s\")"), *SelectedAttribute->GetPathName());
	}

	GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, FinalValue);


	FString DefTagString = GraphPinObj->GetDefaultAsString();

	LastSelectedProperty = SelectedAttribute;

}

#undef LOCTEXT_NAMESPACE
