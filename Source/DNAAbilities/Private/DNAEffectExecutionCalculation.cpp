// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNAEffectExecutionCalculation.h"
#include "AbilitySystemComponent.h"

FDNAEffectCustomExecutionParameters::FDNAEffectCustomExecutionParameters()
	: OwningSpec(nullptr)
	, TargetDNAAbilitySystemComponent(nullptr)
	, PassedInTags(FDNATagContainer())
{
}

FDNAEffectCustomExecutionParameters::FDNAEffectCustomExecutionParameters(FDNAEffectSpec& InOwningSpec, const TArray<FDNAEffectExecutionScopedModifierInfo>& InScopedMods, UDNAAbilitySystemComponent* InTargetAbilityComponent, const FDNATagContainer& InPassedInTags, const FPredictionKey& InPredictionKey)
	: OwningSpec(&InOwningSpec)
	, TargetDNAAbilitySystemComponent(InTargetAbilityComponent)
	, PassedInTags(InPassedInTags)
	, PredictionKey(InPredictionKey)
{
	check(InOwningSpec.Def);

	FActiveDNAEffectHandle ModifierHandle = FActiveDNAEffectHandle::GenerateNewHandle(InTargetAbilityComponent);

	for (const FDNAEffectExecutionScopedModifierInfo& CurScopedMod : InScopedMods)
	{
		FAggregator* ScopedAggregator = ScopedModifierAggregators.Find(CurScopedMod.CapturedAttribute);
		if (!ScopedAggregator)
		{
			const FDNAEffectAttributeCaptureSpec* CaptureSpec = InOwningSpec.CapturedRelevantAttributes.FindCaptureSpecByDefinition(CurScopedMod.CapturedAttribute, true);

			FAggregator SnapshotAgg;
			if (CaptureSpec && CaptureSpec->AttemptGetAttributeAggregatorSnapshot(SnapshotAgg))
			{
				ScopedAggregator = &(ScopedModifierAggregators.Add(CurScopedMod.CapturedAttribute, SnapshotAgg));
			}
		}

		float ModEvalValue = 0.f;
		if (ScopedAggregator && CurScopedMod.ModifierMagnitude.AttemptCalculateMagnitude(InOwningSpec, ModEvalValue))
		{
			ScopedAggregator->AddAggregatorMod(ModEvalValue, CurScopedMod.ModifierOp, CurScopedMod.EvaluationChannelSettings.GetEvaluationChannel(), &CurScopedMod.SourceTags, &CurScopedMod.TargetTags, false, ModifierHandle);
		}
		else
		{
			ABILITY_LOG(Warning, TEXT("Attempted to apply a scoped modifier from %s's %s magnitude calculation that could not be properly calculated. Some attributes necessary for the calculation were missing."), *InOwningSpec.Def->GetName(), *CurScopedMod.CapturedAttribute.ToSimpleString());
		}
	}
}

FDNAEffectCustomExecutionParameters::FDNAEffectCustomExecutionParameters(FDNAEffectSpec& InOwningSpec, const TArray<FDNAEffectExecutionScopedModifierInfo>& InScopedMods, UDNAAbilitySystemComponent* InTargetAbilityComponent, const FDNATagContainer& InPassedInTags, const FPredictionKey& InPredictionKey, const TArray<FActiveDNAEffectHandle>& InIgnoreHandles)
	: FDNAEffectCustomExecutionParameters(InOwningSpec, InScopedMods, InTargetAbilityComponent, InPassedInTags, InPredictionKey)
{
	IgnoreHandles = InIgnoreHandles;
}

const FDNAEffectSpec& FDNAEffectCustomExecutionParameters::GetOwningSpec() const
{
	check(OwningSpec);
	return *OwningSpec;
}

FDNAEffectSpec* FDNAEffectCustomExecutionParameters::GetOwningSpecForPreExecuteMod() const
{
	check(OwningSpec);
	return OwningSpec;
}

UDNAAbilitySystemComponent* FDNAEffectCustomExecutionParameters::GetTargetDNAAbilitySystemComponent() const
{
	return TargetDNAAbilitySystemComponent.Get();
}

UDNAAbilitySystemComponent* FDNAEffectCustomExecutionParameters::GetSourceDNAAbilitySystemComponent() const
{
	check(OwningSpec);
	return OwningSpec->GetContext().GetInstigatorDNAAbilitySystemComponent();
}

const FDNATagContainer& FDNAEffectCustomExecutionParameters::GetPassedInTags() const
{
	return PassedInTags;
}

TArray<FActiveDNAEffectHandle> FDNAEffectCustomExecutionParameters::GetIgnoreHandles() const
{
	return IgnoreHandles;
}
	
FPredictionKey FDNAEffectCustomExecutionParameters::GetPredictionKey() const
{
	return PredictionKey;
}

bool FDNAEffectCustomExecutionParameters::AttemptCalculateCapturedAttributeMagnitude(const FDNAEffectAttributeCaptureDefinition& InCaptureDef, const FAggregatorEvaluateParameters& InEvalParams, OUT float& OutMagnitude) const
{
	check(OwningSpec);

	const FAggregator* CalcAgg = ScopedModifierAggregators.Find(InCaptureDef);
	if (CalcAgg)
	{
		OutMagnitude = CalcAgg->Evaluate(InEvalParams);
		return true;
	}
	else
	{
		const FDNAEffectAttributeCaptureSpec* CaptureSpec = OwningSpec->CapturedRelevantAttributes.FindCaptureSpecByDefinition(InCaptureDef, true);
		if (CaptureSpec)
		{
			return CaptureSpec->AttemptCalculateAttributeMagnitude(InEvalParams, OutMagnitude);
		}
	}

	return false;
}

bool FDNAEffectCustomExecutionParameters::AttemptCalculateCapturedAttributeMagnitudeWithBase(const FDNAEffectAttributeCaptureDefinition& InCaptureDef, const FAggregatorEvaluateParameters& InEvalParams, float InBaseValue, OUT float& OutMagnitude) const
{
	check(OwningSpec);

	const FAggregator* CalcAgg = ScopedModifierAggregators.Find(InCaptureDef);
	if (CalcAgg)
	{
		OutMagnitude = CalcAgg->EvaluateWithBase(InBaseValue, InEvalParams);
		return true;
	}
	else
	{
		const FDNAEffectAttributeCaptureSpec* CaptureSpec = OwningSpec->CapturedRelevantAttributes.FindCaptureSpecByDefinition(InCaptureDef, true);
		if (CaptureSpec)
		{
			return CaptureSpec->AttemptCalculateAttributeMagnitudeWithBase(InEvalParams, InBaseValue, OutMagnitude);
		}
	}

	return false;
}

bool FDNAEffectCustomExecutionParameters::AttemptCalculateCapturedAttributeBaseValue(const FDNAEffectAttributeCaptureDefinition& InCaptureDef, OUT float& OutBaseValue) const
{
	check(OwningSpec);

	const FAggregator* CalcAgg = ScopedModifierAggregators.Find(InCaptureDef);
	if (CalcAgg)
	{
		OutBaseValue = CalcAgg->GetBaseValue();
		return true;
	}
	else
	{
		const FDNAEffectAttributeCaptureSpec* CaptureSpec = OwningSpec->CapturedRelevantAttributes.FindCaptureSpecByDefinition(InCaptureDef, true);
		if (CaptureSpec)
		{
			return CaptureSpec->AttemptCalculateAttributeBaseValue(OutBaseValue);
		}
	}

	return false;
}

bool FDNAEffectCustomExecutionParameters::AttemptCalculateCapturedAttributeBonusMagnitude(const FDNAEffectAttributeCaptureDefinition& InCaptureDef, const FAggregatorEvaluateParameters& InEvalParams, OUT float& OutBonusMagnitude) const
{
	check(OwningSpec);

	const FAggregator* CalcAgg = ScopedModifierAggregators.Find(InCaptureDef);
	if (CalcAgg)
	{
		OutBonusMagnitude = CalcAgg->EvaluateBonus(InEvalParams);
		return true;
	}
	else
	{
		const FDNAEffectAttributeCaptureSpec* CaptureSpec = OwningSpec->CapturedRelevantAttributes.FindCaptureSpecByDefinition(InCaptureDef, true);
		if (CaptureSpec)
		{
			return CaptureSpec->AttemptCalculateAttributeBonusMagnitude(InEvalParams, OutBonusMagnitude);
		}
	}

	return false;
}

bool FDNAEffectCustomExecutionParameters::AttemptGetCapturedAttributeAggregatorSnapshot(const FDNAEffectAttributeCaptureDefinition& InCaptureDef, OUT FAggregator& OutSnapshottedAggregator) const
{
	check(OwningSpec);

	const FAggregator* CalcAgg = ScopedModifierAggregators.Find(InCaptureDef);
	if (CalcAgg)
	{
		OutSnapshottedAggregator.TakeSnapshotOf(*CalcAgg);
		return true;
	}
	else
	{
		const FDNAEffectAttributeCaptureSpec* CaptureSpec = OwningSpec->CapturedRelevantAttributes.FindCaptureSpecByDefinition(InCaptureDef, true);
		if (CaptureSpec)
		{
			return CaptureSpec->AttemptGetAttributeAggregatorSnapshot(OutSnapshottedAggregator);
		}
	}

	return false;
}

FDNAEffectCustomExecutionOutput::FDNAEffectCustomExecutionOutput()
	: bTriggerConditionalDNAEffects(false)
	, bHandledStackCountManually(false)
	, bHandledDNACuesManually(false)
{
}

void FDNAEffectCustomExecutionOutput::MarkStackCountHandledManually()
{
	bHandledStackCountManually = true;
}

bool FDNAEffectCustomExecutionOutput::IsStackCountHandledManually() const
{
	return bHandledStackCountManually;
}

bool FDNAEffectCustomExecutionOutput::AreDNACuesHandledManually() const
{
	return bHandledDNACuesManually;
}

void FDNAEffectCustomExecutionOutput::MarkConditionalDNAEffectsToTrigger()
{
	bTriggerConditionalDNAEffects = true;
}

void FDNAEffectCustomExecutionOutput::MarkDNACuesHandledManually()
{
	bHandledDNACuesManually = true;
}

bool FDNAEffectCustomExecutionOutput::ShouldTriggerConditionalDNAEffects() const
{
	return bTriggerConditionalDNAEffects;
}

void FDNAEffectCustomExecutionOutput::AddOutputModifier(const FDNAModifierEvaluatedData& InOutputMod)
{
	OutputModifiers.Add(InOutputMod);
}

const TArray<FDNAModifierEvaluatedData>& FDNAEffectCustomExecutionOutput::GetOutputModifiers() const
{
	return OutputModifiers;
}

void FDNAEffectCustomExecutionOutput::GetOutputModifiers(OUT TArray<FDNAModifierEvaluatedData>& OutOutputModifiers) const
{
	OutOutputModifiers.Append(OutputModifiers);
}

UDNAEffectExecutionCalculation::UDNAEffectExecutionCalculation(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bRequiresPassedInTags = false;
}

#if WITH_EDITORONLY_DATA
void UDNAEffectExecutionCalculation::GetValidScopedModifierAttributeCaptureDefinitions(OUT TArray<FDNAEffectAttributeCaptureDefinition>& OutScopableModifiers) const
{
	OutScopableModifiers.Empty();

	const TArray<FDNAEffectAttributeCaptureDefinition>& DefaultCaptureDefs = GetAttributeCaptureDefinitions();
	for (const FDNAEffectAttributeCaptureDefinition& CurDef : DefaultCaptureDefs)
	{
		if (!InvalidScopedModifierAttributes.Contains(CurDef))
		{
			OutScopableModifiers.Add(CurDef);
		}
	}
}

bool UDNAEffectExecutionCalculation::DoesRequirePassedInTags() const
{
	return bRequiresPassedInTags;
}
#endif // #if WITH_EDITORONLY_DATA

void UDNAEffectExecutionCalculation::Execute_Implementation(const FDNAEffectCustomExecutionParameters& ExecutionParams, OUT FDNAEffectCustomExecutionOutput& OutExecutionOutput) const
{
}
