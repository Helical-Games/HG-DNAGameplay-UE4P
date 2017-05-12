// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_DNAAbilitiesBlueprint.h"
#include "DNACueTagDetails.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Views/SListView.h"
#include "PropertyHandle.h"
#include "IDetailChildrenBuilder.h"
#include "DetailWidgetRow.h"
#include "AbilitySystemGlobals.h"
#include "DNACueInterface.h"
#include "Widgets/Input/SHyperlink.h"
#include "DNACueManager.h"
#include "DNACueSet.h"
#include "SDNACueEditor.h"
#include "SlateOptMacros.h"

#define LOCTEXT_NAMESPACE "DNACueDetailsCustomization"

DEFINE_LOG_CATEGORY(LogDNACueDetails);

TSharedRef<IPropertyTypeCustomization> FDNACueTagDetails::MakeInstance()
{
	return MakeShareable(new FDNACueTagDetails());
}

FText FDNACueTagDetails::GetTagText() const
{
	FString Str;
	FDNATag* TagPtr = GetTag();
	if (TagPtr && TagPtr->IsValid())
	{
		Str = TagPtr->GetTagName().ToString();
	}

	return FText::FromString(Str);
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FDNACueTagDetails::CustomizeHeader( TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	uint32 NumChildren = 0;
	StructPropertyHandle->GetNumChildren(NumChildren);

	DNATagProperty = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDNACueTag,DNACueTag));

	FSimpleDelegate OnTagChanged = FSimpleDelegate::CreateSP(this, &FDNACueTagDetails::OnPropertyValueChanged);
	DNATagProperty->SetOnPropertyValueChanged(OnTagChanged);

	UDNACueManager* CueManager = UDNAAbilitySystemGlobals::Get().GetDNACueManager();
	if (CueManager)
	{
		CueManager->OnDNACueNotifyAddOrRemove.AddSP(this, &FDNACueTagDetails::OnPropertyValueChanged);
	}

	HeaderRow
	.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	];
}

void FDNACueTagDetails::CustomizeChildren( TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	DNATagProperty = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDNACueTag,DNACueTag));
	if (DNATagProperty.IsValid())
	{
		IDetailPropertyRow& PropRow = StructBuilder.AddChildProperty(DNATagProperty.ToSharedRef());
	}

	bool ValidTag = UpdateNotifyList();
	bool HasNotify = (NotifyList.Num() > 0);

	StructBuilder.AddChildContent( LOCTEXT("NotifyLinkStr", "Notify") )
	.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget(LOCTEXT("NotifyStr", "Notify"))
	]
	.ValueContent()
	.MaxDesiredWidth(512)
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.Padding(2.f, 2.f)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(2.0f, 2.0f)
			.AutoWidth()
			[
				SAssignNew(ListView, SListView < TSharedRef<FStringAssetReference> >)
				.ItemHeight(48)
				.SelectionMode(ESelectionMode::None)
				.ListItemsSource(&NotifyList)
				.OnGenerateRow(this, &FDNACueTagDetails::GenerateListRow)
				.Visibility(this, &FDNACueTagDetails::GetListViewVisibility)
			]
		]

		+SVerticalBox::Slot()
		.AutoHeight( )
		[
			SNew(SButton)
			.Text(LOCTEXT("AddNew", "Add New"))
			.Visibility(this, &FDNACueTagDetails::GetAddNewNotifyVisibliy)
			.OnClicked(this, &FDNACueTagDetails::OnAddNewNotifyClicked)
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

TSharedRef<ITableRow> FDNACueTagDetails::GenerateListRow(TSharedRef<FStringAssetReference> NotifyName, const TSharedRef<STableViewBase>& OwnerTable)
{	
	FString ShortName = NotifyName->ToString();
	int32 idx;
	if (ShortName.FindLastChar(TEXT('.'), idx))
	{
		ShortName = ShortName.Right(ShortName.Len() - (idx+1));
		ShortName.RemoveFromEnd(TEXT("_c"));
	}

	return
	SNew(STableRow< TSharedRef<FStringAssetReference> >, OwnerTable)
	[
		SNew(SBox)
		.HAlign(HAlign_Left)
		[
			SNew(SHyperlink)
			.Style(FEditorStyle::Get(), "Common.GotoBlueprintHyperlink")
			.Text(FText::FromString(ShortName))
			.OnNavigate(this, &FDNACueTagDetails::NavigateToHandler, NotifyName)
		]
	];
}

void FDNACueTagDetails::NavigateToHandler(TSharedRef<FStringAssetReference> AssetRef)
{
	SDNACueEditor::OpenEditorForNotify(AssetRef->ToString());
}

FReply FDNACueTagDetails::OnAddNewNotifyClicked()
{
	FDNATag* TagPtr = GetTag();
	if (TagPtr && TagPtr->IsValid())
	{
		SDNACueEditor::CreateNewDNACueNotifyDialogue(TagPtr->ToString(), nullptr);
		OnPropertyValueChanged();
	}
	return FReply::Handled();
}

void FDNACueTagDetails::OnPropertyValueChanged()
{
	UpdateNotifyList();
	if (ListView.IsValid())
	{
		ListView->RequestListRefresh();
	}
}

bool FDNACueTagDetails::UpdateNotifyList()
{
	NotifyList.Empty();
	
	FDNATag* Tag = GetTag();
	bool ValidTag = Tag && Tag->IsValid();
	if (ValidTag)
	{
		uint8 EnumVal;
		DNATagProperty->GetValue(EnumVal);
		if (UDNACueManager* CueManager = UDNAAbilitySystemGlobals::Get().GetDNACueManager())
		{
			if (UDNACueSet* CueSet = CueManager->GetEditorCueSet())
			{
				if (int32* idxPtr = CueSet->DNACueDataMap.Find(*Tag))
				{
					int32 idx = *idxPtr;
					if (CueSet->DNACueData.IsValidIndex(idx))
					{
						FDNACueNotifyData& Data = CueSet->DNACueData[*idxPtr];
						TSharedRef<FStringAssetReference> Item(MakeShareable(new FStringAssetReference(Data.DNACueNotifyObj)));
						NotifyList.Add(Item);
					}
				}
			}
		}
	}

	return ValidTag;
}

FDNATag* FDNACueTagDetails::GetTag() const
{
	FDNATag* Tag = nullptr;
	TArray<void*> RawStructData;
	if (DNATagProperty.IsValid())
	{
		DNATagProperty->AccessRawData(RawStructData);
		if (RawStructData.Num() > 0)
		{
			Tag = (FDNATag*)(RawStructData[0]);
		}
	}
	return Tag;
}

EVisibility FDNACueTagDetails::GetAddNewNotifyVisibliy() const
{
	FDNATag* Tag = GetTag();
	bool ValidTag = Tag && Tag->IsValid();
	bool HasNotify = NotifyList.Num() > 0;
	return (!HasNotify && ValidTag) ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FDNACueTagDetails::GetListViewVisibility() const
{
	return NotifyList.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE
