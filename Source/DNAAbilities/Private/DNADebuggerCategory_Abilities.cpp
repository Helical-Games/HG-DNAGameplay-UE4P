// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNADebuggerCategory_Abilities.h"
#include "DNATagContainer.h"
#include "DNAAbilitySpec.h"
#include "DNAEffect.h"
#include "AbilitySystemComponent.h"

#if WITH_DNA_DEBUGGER

FDNADebuggerCategory_Abilities::FDNADebuggerCategory_Abilities()
{
	SetDataPackReplication<FRepData>(&DataPack);
}

TSharedRef<FDNADebuggerCategory> FDNADebuggerCategory_Abilities::MakeInstance()
{
	return MakeShareable(new FDNADebuggerCategory_Abilities());
}

void FDNADebuggerCategory_Abilities::FRepData::Serialize(FArchive& Ar)
{
	Ar << OwnedTags;

	int32 NumAbilities = Abilities.Num();
	Ar << NumAbilities;
	if (Ar.IsLoading())
	{
		Abilities.SetNum(NumAbilities);
	}

	for (int32 Idx = 0; Idx < NumAbilities; Idx++)
	{
		Ar << Abilities[Idx].Ability;
		Ar << Abilities[Idx].Source;
		Ar << Abilities[Idx].Level;
		Ar << Abilities[Idx].bIsActive;
	}

	int32 NumGE = DNAEffects.Num();
	Ar << NumGE;
	if (Ar.IsLoading())
	{
		DNAEffects.SetNum(NumGE);
	}

	for (int32 Idx = 0; Idx < NumGE; Idx++)
	{
		Ar << DNAEffects[Idx].Effect;
		Ar << DNAEffects[Idx].Context;
		Ar << DNAEffects[Idx].Duration;
		Ar << DNAEffects[Idx].Period;
		Ar << DNAEffects[Idx].Stacks;
		Ar << DNAEffects[Idx].Level;
	}
}

void FDNADebuggerCategory_Abilities::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{
	UDNAAbilitySystemComponent* AbilityComp = DebugActor ? DebugActor->FindComponentByClass<UDNAAbilitySystemComponent>() : nullptr;
	if (AbilityComp)
	{
		static FDNATagContainer OwnerTags;
		OwnerTags.Reset();
		AbilityComp->GetOwnedDNATags(OwnerTags);
		DataPack.OwnedTags = OwnerTags.ToStringSimple();

		TArray<FDNAEffectSpec> ActiveEffectSpecs;
		AbilityComp->GetAllActiveDNAEffectSpecs(ActiveEffectSpecs);
		for (int32 Idx = 0; Idx < ActiveEffectSpecs.Num(); Idx++)
		{
			const FDNAEffectSpec& EffectSpec = ActiveEffectSpecs[Idx];
			FRepData::FDNAEffectDebug ItemData;

			ItemData.Effect = EffectSpec.ToSimpleString();
			ItemData.Effect.RemoveFromStart(DEFAULT_OBJECT_PREFIX);
			ItemData.Effect.RemoveFromEnd(TEXT("_C"));

			ItemData.Context = EffectSpec.GetContext().ToString();
			ItemData.Duration = EffectSpec.GetDuration();
			ItemData.Period = EffectSpec.GetPeriod();
			ItemData.Stacks = EffectSpec.StackCount;
			ItemData.Level = EffectSpec.GetLevel();

			DataPack.DNAEffects.Add(ItemData);
		}

		const TArray<FDNAAbilitySpec>& AbilitySpecs = AbilityComp->GetActivatableAbilities();
		for (int32 Idx = 0; Idx < AbilitySpecs.Num(); Idx++)
		{
			const FDNAAbilitySpec& AbilitySpec = AbilitySpecs[Idx];
			FRepData::FDNAAbilityDebug ItemData;

			ItemData.Ability = GetNameSafe(AbilitySpec.Ability);
			ItemData.Ability.RemoveFromStart(DEFAULT_OBJECT_PREFIX);
			ItemData.Ability.RemoveFromEnd(TEXT("_C"));

			ItemData.Source = GetNameSafe(AbilitySpec.SourceObject);
			ItemData.Source.RemoveFromStart(DEFAULT_OBJECT_PREFIX);

			ItemData.Level = AbilitySpec.Level;
			ItemData.bIsActive = AbilitySpec.IsActive();

			DataPack.Abilities.Add(ItemData);
		}
	}
}

void FDNADebuggerCategory_Abilities::DrawData(APlayerController* OwnerPC, FDNADebuggerCanvasContext& CanvasContext)
{
	CanvasContext.Printf(TEXT("Owned Tags: {yellow}%s"), *DataPack.OwnedTags);

	AActor* LocalDebugActor = FindLocalDebugActor();
	UDNAAbilitySystemComponent* AbilityComp = LocalDebugActor ? LocalDebugActor->FindComponentByClass<UDNAAbilitySystemComponent>() : nullptr;
	if (AbilityComp)
	{
		static FDNATagContainer OwnerTags;
		OwnerTags.Reset();
		AbilityComp->GetOwnedDNATags(OwnerTags);

		CanvasContext.Printf(TEXT("Local Tags: {cyan}%s"), *OwnerTags.ToStringSimple());
	}

	CanvasContext.Printf(TEXT("DNA Effects: {yellow}%d"), DataPack.DNAEffects.Num());
	for (int32 Idx = 0; Idx < DataPack.DNAEffects.Num(); Idx++)
	{
		const FRepData::FDNAEffectDebug& ItemData = DataPack.DNAEffects[Idx];

		FString Desc = FString::Printf(TEXT("\t{yellow}%s {grey}source:{white}%s {grey}duration:{white}"), *ItemData.Effect, *ItemData.Context);
		Desc += (ItemData.Duration > 0.0f) ? FString::Printf(TEXT("%.2f"), ItemData.Duration) : FString(TEXT("INF"));

		if (ItemData.Period > 0.0f)
		{
			Desc += FString::Printf(TEXT(" {grey}period:{white}%.2f"), ItemData.Period);
		}

		if (ItemData.Stacks > 1)
		{
			Desc += FString::Printf(TEXT(" {grey}stacks:{white}%d"), ItemData.Stacks);
		}

		if (ItemData.Level > 1.0f)
		{
			Desc += FString::Printf(TEXT(" {grey}level:{white}%.2f"), ItemData.Level);
		}

		CanvasContext.Print(Desc);
	}

	CanvasContext.Printf(TEXT("DNA Abilities: {yellow}%d"), DataPack.Abilities.Num());
	for (int32 Idx = 0; Idx < DataPack.Abilities.Num(); Idx++)
	{
		const FRepData::FDNAAbilityDebug& ItemData = DataPack.Abilities[Idx];

		CanvasContext.Printf(TEXT("\t{yellow}%s {grey}source:{white}%s {grey}level:{white}%d {grey}active:{white}%s"),
			*ItemData.Ability, *ItemData.Source, ItemData.Level, ItemData.bIsActive ? TEXT("YES") : TEXT("no"));
	}
}

#endif // WITH_DNA_DEBUGGER
