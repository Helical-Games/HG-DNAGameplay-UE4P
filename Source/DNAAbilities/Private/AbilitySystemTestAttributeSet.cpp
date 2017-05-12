// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "AbilitySystemTestAttributeSet.h"
#include "DNATagContainer.h"
#include "DNAEffect.h"
#include "DNATagsModule.h"
#include "DNAEffectExtension.h"

UDNAAbilitySystemTestAttributeSet::UDNAAbilitySystemTestAttributeSet(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	Health = MaxHealth = 100.f;
	Mana = MaxMana = 100.f;
	
	Damage = 0.f;
	CritChance = 0.f;
	SpellDamage = 0.f;
	PhysicalDamage = 0.f;
	Strength = 0.f;
	StackingAttribute1 = 0.f;
	StackingAttribute2 = 0.f;
	NoStackAttribute = 0.f;
}

bool UDNAAbilitySystemTestAttributeSet::PreDNAEffectExecute(struct FDNAEffectModCallbackData &Data)
{
#if 0
	static UProperty *HealthProperty = FindFieldChecked<UProperty>(UDNAAbilitySystemTestAttributeSet::StaticClass(), GET_MEMBER_NAME_CHECKED(UDNAAbilitySystemTestAttributeSet, Health));
	static UProperty *DamageProperty = FindFieldChecked<UProperty>(UDNAAbilitySystemTestAttributeSet::StaticClass(), GET_MEMBER_NAME_CHECKED(UDNAAbilitySystemTestAttributeSet, Damage));

	// In this function, our DNAEffect mod has been evaluated. We have a magnitude and a Tags collection that we can still modify before it is applied.
	// We also still have the Aggregation data that calculated Data.EvaluatedData. If we really needed to, we could look at this, remove or change things at the aggregator level, and reevaluate ourselves.
	// But that would be considered very advanced/rare.

	UProperty *ModifiedProperty = Data.ModifierSpec.Info.Attribute.GetUProperty();

	// Is Damage about to be applied?
	if (DamageProperty == ModifiedProperty)
	{
		// Can the target dodge this completely?
		if (DodgeChance > 0.f)
		{
			if (FMath::FRand() <= DodgeChance)
			{
				// Dodge!
				Data.EvaluatedData.Magnitude = 0.f;
				Data.EvaluatedData.Tags.AddTag(FDNATag::RequestDNATag(FName(TEXT("Dodged"))));

				// How dodge is handled will be game dependant. There are a few options I think of:
				// -We still apply 0 damage, but tag it as Dodged. The DNACue system could pick up on this and play a visual effect. The combat log could pick up in and print it out too.
				// -We throw out this DNAEffect right here, and apply another DNAEffect for 'Dodge' it wouldn't modify an attribute but could trigger DNA cues, it could serve as a 'cooldown' for dodge
				//		if the game wanted rules like 'you can't dodge more than once every .5 seconds', etc.
			}
		}		
		
		if (Data.EvaluatedData.Magnitude > 0.f)
		{
			// Check the source - does he have Crit?
			const UDNAAbilitySystemTestAttributeSet* SourceAttributes = Data.EffectSpec.EffectContext.GetOriginalInstigatorDNAAbilitySystemComponent()->GetSet<UDNAAbilitySystemTestAttributeSet>();
			if (SourceAttributes && SourceAttributes->CritChance > 0.f)
			{
				if (FMath::FRand() <= SourceAttributes->CritChance)
				{
					// Crit!
					Data.EvaluatedData.Magnitude *= SourceAttributes->CritMultiplier;
					Data.EvaluatedData.Tags.AddTag(FDNATag::RequestDNATag(FName(TEXT("Damage.Crit"))));
				}
			}

			// Now apply armor reduction
			if (Data.EvaluatedData.Tags.HasTag(FDNATag::RequestDNATag(FName(TEXT("Damage.Physical")))))
			{
				// This is a trivial/naive implementation of armor. It assumes the rmorDamageReduction is an actual % to reduce physics damage by.
				// Real games would probably use armor rating as an attribute and calculate a % here based on the damage source's level, etc.
				Data.EvaluatedData.Magnitude *= (1.f - ArmorDamageReduction);
				Data.EvaluatedData.Tags.AddTag(FDNATag::RequestDNATag(FName(TEXT("Damage.Mitigated.Armor"))));
			}
		}

		// At this point, the Magnitude of the applied damage may have been modified by us. We still do the translation to Health in UDNAAbilitySystemTestAttributeSet::PostAttributeModify.
	}

#endif

	return true;
}

void UDNAAbilitySystemTestAttributeSet::PostDNAEffectExecute(const struct FDNAEffectModCallbackData &Data)
{
	static UProperty* HealthProperty = FindFieldChecked<UProperty>(UDNAAbilitySystemTestAttributeSet::StaticClass(), GET_MEMBER_NAME_CHECKED(UDNAAbilitySystemTestAttributeSet, Health));
	static UProperty* DamageProperty = FindFieldChecked<UProperty>(UDNAAbilitySystemTestAttributeSet::StaticClass(), GET_MEMBER_NAME_CHECKED(UDNAAbilitySystemTestAttributeSet, Damage));

	UProperty* ModifiedProperty = Data.EvaluatedData.Attribute.GetUProperty();

	// What property was modified?
	if (DamageProperty == ModifiedProperty)
	{
		// Anytime Damage is applied with 'Damage.Fire' tag, there is a chance to apply a burning DOT
		if (Data.EffectSpec.CapturedSourceTags.GetAggregatedTags()->HasTag( FDNATag::RequestDNATag(FName(TEXT("FireDamage")))))
		{
			// Logic to rand() a burning DOT, if successful, apply DOT DNAEffect to the target
		}

		// Treat damage as minus health
		Health -= Damage;
		Damage = 0.f;

		// Check for Death?
		//  -This could be defined here or at the actor level.
		//  -Doing it here makes a lot of sense to me, but we have legacy code in ::TakeDamage function, so some games may just want to punt to that pipeline from here.
	}
}


void UDNAAbilitySystemTestAttributeSet::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	/*
	DOREPLIFETIME( UDNAAbilitySystemTestAttributeSet, MaxHealth);
	DOREPLIFETIME( UDNAAbilitySystemTestAttributeSet, Health);
	DOREPLIFETIME( UDNAAbilitySystemTestAttributeSet, Mana);
	DOREPLIFETIME( UDNAAbilitySystemTestAttributeSet, MaxMana);

	DOREPLIFETIME( UDNAAbilitySystemTestAttributeSet, SpellDamage);
	DOREPLIFETIME( UDNAAbilitySystemTestAttributeSet, PhysicalDamage);

	DOREPLIFETIME( UDNAAbilitySystemTestAttributeSet, CritChance);
	DOREPLIFETIME( UDNAAbilitySystemTestAttributeSet, CritMultiplier);
	DOREPLIFETIME( UDNAAbilitySystemTestAttributeSet, ArmorDamageReduction);

	DOREPLIFETIME( UDNAAbilitySystemTestAttributeSet, DodgeChance);
	DOREPLIFETIME( UDNAAbilitySystemTestAttributeSet, LifeSteal);

	DOREPLIFETIME( UDNAAbilitySystemTestAttributeSet, Strength);
	*/
}
