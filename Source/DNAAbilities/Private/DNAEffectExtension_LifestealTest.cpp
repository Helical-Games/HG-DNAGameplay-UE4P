// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNAEffectExtension_LifestealTest.h"
#include "DNAEffect.h"
#include "DNATagsModule.h"
#include "AbilitySystemTestAttributeSet.h"
#include "AbilitySystemComponent.h"

UDNAEffectExtension_LifestealTest::UDNAEffectExtension_LifestealTest(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	HealthRestoreDNAEffect = NULL;
}


void UDNAEffectExtension_LifestealTest::PreDNAEffectExecute(const FDNAModifierEvaluatedData &SelfData, FDNAEffectModCallbackData &Data) const
{

}

void UDNAEffectExtension_LifestealTest::PostDNAEffectExecute(const FDNAModifierEvaluatedData &SelfData, const FDNAEffectModCallbackData &Data) const
{
	float DamageDone = Data.EvaluatedData.Magnitude;
	float LifestealPCT = SelfData.Magnitude;

	float HealthToRestore = -DamageDone * LifestealPCT;
	if (HealthToRestore > 0.f)
	{
		UDNAAbilitySystemComponent *Source = Data.EffectSpec.GetContext().GetOriginalInstigatorDNAAbilitySystemComponent();

		UDNAEffect * LocalHealthRestore = HealthRestoreDNAEffect;
		if (!LocalHealthRestore)
		{
			UProperty *HealthProperty = FindFieldChecked<UProperty>(UDNAAbilitySystemTestAttributeSet::StaticClass(), GET_MEMBER_NAME_CHECKED(UDNAAbilitySystemTestAttributeSet, Health));

			// Since this is a test class and we don't want to tie it any actual content assets, just construct a DNAEffect here.
			LocalHealthRestore = NewObject<UDNAEffect>(GetTransientPackage(), FName(TEXT("LifestealHealthRestore")));
			LocalHealthRestore->Modifiers.SetNum(1);
			LocalHealthRestore->Modifiers[0].Magnitude.SetValue(HealthToRestore);
			LocalHealthRestore->Modifiers[0].ModifierOp = EDNAModOp::Additive;
			LocalHealthRestore->Modifiers[0].Attribute.SetUProperty(HealthProperty);
			LocalHealthRestore->DurationPolicy = EDNAEffectDurationType::Instant;
			LocalHealthRestore->Period.Value = UDNAEffect::NO_PERIOD;
		}

		if (SelfData.Handle.IsValid())
		{
			// We are coming from an active DNA effect
			check(SelfData.Handle.IsValid());
		}

		// Apply a DNAEffect to restore health
		// We make the DNAEffect's level = the health restored. This is one approach. We could also
		// try a basic restore 1 health item but apply a 2nd GE to modify that - but that seems like too many levels of indirection
		Source->ApplyDNAEffectToSelf(LocalHealthRestore, HealthToRestore, Source->MakeEffectContext());
	}
}
