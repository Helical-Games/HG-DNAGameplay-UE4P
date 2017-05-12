// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "Misc/StringAssetReference.h"
#include "Layout/Visibility.h"
#include "Input/Reply.h"
#include "IPropertyTypeCustomization.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"

class IPropertyHandle;
struct FDNATag;

DECLARE_LOG_CATEGORY_EXTERN(LogDNACueDetails, Log, All);

class FDNACueTagDetails : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();


private:

	FDNACueTagDetails()
	{
	}

	virtual void CustomizeHeader( TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;
	virtual void CustomizeChildren( TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;	

	TSharedPtr<IPropertyHandle> DNATagProperty;

	TSharedRef<ITableRow> GenerateListRow(TSharedRef<FStringAssetReference> NotifyName, const TSharedRef<STableViewBase>& OwnerTable);
	TArray<TSharedRef<FStringAssetReference> > NotifyList;
	TSharedPtr< SListView < TSharedRef<FStringAssetReference> > > ListView;

	void NavigateToHandler(TSharedRef<FStringAssetReference> AssetRef);

	FReply OnAddNewNotifyClicked();

	/** Updates the selected tag*/
	void OnPropertyValueChanged();
	bool UpdateNotifyList();
	FDNATag* GetTag() const;
	FText GetTagText() const;

	EVisibility GetListViewVisibility() const;
	EVisibility GetAddNewNotifyVisibliy() const;
};
