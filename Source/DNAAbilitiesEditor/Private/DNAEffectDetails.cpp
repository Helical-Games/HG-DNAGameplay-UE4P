// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_DNAAbilitiesBlueprint.h"
#include "DNAEffectDetails.h"
#include "UObject/UnrealType.h"
#include "DetailLayoutBuilder.h"
#include "DNAEffectTypes.h"
#include "DNAEffect.h"
#include "DNAEffectTemplate.h"

#define LOCTEXT_NAMESPACE "DNAEffectDetailsCustomization"

DEFINE_LOG_CATEGORY(LogDNAEffectDetails);

// --------------------------------------------------------FDNAEffectDetails---------------------------------------

TSharedRef<IDetailCustomization> FDNAEffectDetails::MakeInstance()
{
	return MakeShareable(new FDNAEffectDetails);
}

void FDNAEffectDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	MyDetailLayout = &DetailLayout;

	TArray< TWeakObjectPtr<UObject> > Objects;
	DetailLayout.GetObjectsBeingCustomized(Objects);
	if (Objects.Num() != 1)
	{
		// I don't know what to do here or what could be expected. Just return to disable all templating functionality
		return;
	}

	TemplateProperty = DetailLayout.GetProperty(FName("Template"), UDNAEffect::StaticClass());
	ShowAllProperty = DetailLayout.GetProperty(FName("ShowAllProperties"), UDNAEffect::StaticClass());

	FSimpleDelegate UpdateShowAllDelegate = FSimpleDelegate::CreateSP(this, &FDNAEffectDetails::OnShowAllChange);
	ShowAllProperty->SetOnPropertyValueChanged(UpdateShowAllDelegate);

	FSimpleDelegate UpdatTemplateDelegate = FSimpleDelegate::CreateSP(this, &FDNAEffectDetails::OnTemplateChange);
	TemplateProperty->SetOnPropertyValueChanged(UpdatTemplateDelegate);

	TSharedPtr<IPropertyHandle> DurationPolicyProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UDNAEffect, DurationPolicy), UDNAEffect::StaticClass());
	FSimpleDelegate UpdateDurationPolicyDelegate = FSimpleDelegate::CreateSP(this, &FDNAEffectDetails::OnDurationPolicyChange);
	DurationPolicyProperty->SetOnPropertyValueChanged(UpdateDurationPolicyDelegate);
	
	// Hide properties where necessary
	UDNAEffect* Obj = Cast<UDNAEffect>(Objects[0].Get());
	if (Obj)
	{
		if ( !Obj->ShowAllProperties )
		{
			UDNAEffectTemplate* Template = Obj->Template;

			if (Template)
			{
				UDNAEffect* DefObj = Obj->Template->GetClass()->GetDefaultObject<UDNAEffect>();

				for (TFieldIterator<UProperty> PropIt(UDNAEffect::StaticClass(), EFieldIteratorFlags::ExcludeSuper); PropIt; ++PropIt)
				{
					UProperty* Property = *PropIt;
					TSharedPtr<IPropertyHandle> PropHandle = DetailLayout.GetProperty(Property->GetFName());
					HideProperties(DetailLayout, PropHandle, Template);
				}
			}
		}

		if (Obj->DurationPolicy != EDNAEffectDurationType::HasDuration)
		{
			TSharedPtr<IPropertyHandle> DurationMagnitudeProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UDNAEffect, DurationMagnitude), UDNAEffect::StaticClass());
			DetailLayout.HideProperty(DurationMagnitudeProperty);
		}

		if (Obj->DurationPolicy == EDNAEffectDurationType::Instant)
		{
			TSharedPtr<IPropertyHandle> PeriodProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UDNAEffect, Period), UDNAEffect::StaticClass());
			TSharedPtr<IPropertyHandle> ExecutePeriodicEffectOnApplicationProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UDNAEffect, bExecutePeriodicEffectOnApplication), UDNAEffect::StaticClass());
			DetailLayout.HideProperty(PeriodProperty);
			DetailLayout.HideProperty(ExecutePeriodicEffectOnApplicationProperty);
		}

		// The modifier array needs to be told to specifically hide evaluation channel settings for instant effects, as they do not factor evaluation channels at all
		// and instead only operate on base values. To that end, mark the instance metadata so that the customization for the evaluation channel is aware it has to hide
		// (see FDNAModEvaluationChannelSettingsDetails for handling)
		TSharedPtr<IPropertyHandle> ModifierArrayProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UDNAEffect, Modifiers), UDNAEffect::StaticClass());
		if (ModifierArrayProperty.IsValid() && ModifierArrayProperty->IsValidHandle())
		{
			FString ForceHideMetadataValue;
			if (Obj->DurationPolicy == EDNAEffectDurationType::Instant)
			{
				ForceHideMetadataValue = FDNAModEvaluationChannelSettings::ForceHideMetadataEnabledValue;
			}
			ModifierArrayProperty->SetInstanceMetaData(FDNAModEvaluationChannelSettings::ForceHideMetadataKey, ForceHideMetadataValue);
		}
	}
}

/** Recursively hide properties that are not default editable according to the Template */
bool FDNAEffectDetails::HideProperties(IDetailLayoutBuilder& DetailLayout, TSharedPtr<IPropertyHandle> PropHandle, UDNAEffectTemplate* Template)
{
	UProperty* UProp = PropHandle->GetProperty();

	// Never hide the Template or ShowAllProperties properties
	if (TemplateProperty->GetProperty() == UProp || ShowAllProperty->GetProperty() == UProp)
	{
		return false;
	}

	// Don't hide the EditableProperties
	for (FString MatchStr : Template->EditableProperties)
	{
		if (MatchStr.Equals(UProp->GetName(), ESearchCase::IgnoreCase))
		{			
			return false;
		}
	}
	
	// Recurse into children - if they are all hidden then we are hidden.
	uint32 NumChildren = 0;
	PropHandle->GetNumChildren(NumChildren);

	bool AllChildrenHidden = true;
	for (uint32 ChildIdx = 0; ChildIdx < NumChildren; ++ChildIdx)
	{
		TSharedPtr<IPropertyHandle> ChildHandle = PropHandle->GetChildHandle(ChildIdx);
		AllChildrenHidden &= HideProperties(DetailLayout, ChildHandle, Template);
	}

	if (AllChildrenHidden)
	{
		DetailLayout.HideProperty(PropHandle);
		return true;
	}

	return false;
}

void FDNAEffectDetails::OnShowAllChange()
{
	MyDetailLayout->ForceRefreshDetails();
}

void FDNAEffectDetails::OnTemplateChange()
{
	TArray< TWeakObjectPtr<UObject> > Objects;
	MyDetailLayout->GetObjectsBeingCustomized(Objects);
	if (Objects.Num() != 1)
	{
		return;
	}

	UDNAEffect* Obj = Cast<UDNAEffect>(Objects[0].Get());
	UDNAEffect* Template = Obj->Template;

	if (Template)
	{
		// Copy any non-default properties from the template into the current editable object
		UDNAEffect* DefObj = Template->GetClass()->GetDefaultObject<UDNAEffect>();
		for (TFieldIterator<UProperty> PropIt(UDNAEffect::StaticClass(), EFieldIteratorFlags::ExcludeSuper); PropIt; ++PropIt)
		{
			UProperty* Property = *PropIt;
			// don't overwrite the template property
			if (Property->GetFName() == "Template")
			{
				continue;
			}
			if (!Property->Identical_InContainer(Template, DefObj))
			{
				Property->CopyCompleteValue_InContainer(Obj, Template);
			}

			// Default to only showing template properties after changing template type
			Obj->ShowAllProperties = false;
		}
	}

	MyDetailLayout->ForceRefreshDetails();
}

void FDNAEffectDetails::OnDurationPolicyChange()
{
	MyDetailLayout->ForceRefreshDetails();
}


//-------------------------------------------------------------------------------------

#undef LOCTEXT_NAMESPACE
