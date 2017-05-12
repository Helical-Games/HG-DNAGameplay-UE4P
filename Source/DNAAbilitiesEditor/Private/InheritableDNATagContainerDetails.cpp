// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_DNAAbilitiesBlueprint.h"
#include "InheritableDNATagContainerDetails.h"
#include "PropertyHandle.h"
#include "IDetailChildrenBuilder.h"
#include "DetailWidgetRow.h"

#define LOCTEXT_NAMESPACE "InheritableDNATagContainerDetailsCustomization"

TSharedRef<IPropertyTypeCustomization> FInheritableDNATagContainerDetails::MakeInstance()
{
	return MakeShareable(new FInheritableDNATagContainerDetails());
}

void FInheritableDNATagContainerDetails::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	HeaderRow.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		];
}

void FInheritableDNATagContainerDetails::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	uint32 NumChildren;
	StructPropertyHandle->GetNumChildren(NumChildren);

	CombinedTagContainerPropertyHandle = StructPropertyHandle->GetChildHandle(TEXT("CombinedTags"));
	AddedTagContainerPropertyHandle = StructPropertyHandle->GetChildHandle(TEXT("Added"));
	RemovedTagContainerPropertyHandle = StructPropertyHandle->GetChildHandle(TEXT("Removed"));

	FSimpleDelegate OnTagValueChangedDelegate = FSimpleDelegate::CreateSP(this, &FInheritableDNATagContainerDetails::OnTagsChanged);
	AddedTagContainerPropertyHandle->SetOnPropertyValueChanged(OnTagValueChangedDelegate);
	RemovedTagContainerPropertyHandle->SetOnPropertyValueChanged(OnTagValueChangedDelegate);

	StructBuilder.AddChildProperty(CombinedTagContainerPropertyHandle.ToSharedRef());
	StructBuilder.AddChildProperty(AddedTagContainerPropertyHandle.ToSharedRef());
	StructBuilder.AddChildProperty(RemovedTagContainerPropertyHandle.ToSharedRef());
}

void FInheritableDNATagContainerDetails::OnTagsChanged()
{
// 	CombinedTagContainerPropertyHandle->NotifyPreChange();
// 
// 	TSharedPtr<IPropertyHandle> StructPropertyHandle = CombinedTagContainerPropertyHandle->GetParentHandle();
// 	
// 	TArray<void*> RawData;
// 	StructPropertyHandle->AccessRawData(RawData);
// 
// 	for (void* RawPtr : RawData)
// 	{
// 		FInheritedTagContainer* DNATagContainer = reinterpret_cast<FInheritedTagContainer*>(RawPtr);
// 		if (DNATagContainer)
// 		{
// 			DNATagContainer->UpdateInheritedTagProperties();
// 		}
// 	}
// 
// 	CombinedTagContainerPropertyHandle->NotifyPostChange();
}

#undef LOCTEXT_NAMESPACE
