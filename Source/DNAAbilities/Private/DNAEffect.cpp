// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNAEffect.h"
#include "TimerManager.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/PackageMapClient.h"
#include "Engine/NetConnection.h"
#include "AbilitySystemStats.h"
#include "DNATagsModule.h"
#include "AbilitySystemGlobals.h"
#include "DNAEffectExtension.h"
#include "AbilitySystemComponent.h"
#include "DNAModMagnitudeCalculation.h"
#include "DNAEffectExecutionCalculation.h"
#include "DNACueManager.h"

#if ENABLE_VISUAL_LOG
//#include "VisualLogger/VisualLogger.h"
#endif // ENABLE_VISUAL_LOG

const float FDNAEffectConstants::INFINITE_DURATION = -1.f;
const float FDNAEffectConstants::INSTANT_APPLICATION = 0.f;
const float FDNAEffectConstants::NO_PERIOD = 0.f;
const float FDNAEffectConstants::INVALID_LEVEL = -1.f;

const float UDNAEffect::INFINITE_DURATION = FDNAEffectConstants::INFINITE_DURATION;
const float UDNAEffect::INSTANT_APPLICATION =FDNAEffectConstants::INSTANT_APPLICATION;
const float UDNAEffect::NO_PERIOD = FDNAEffectConstants::NO_PERIOD;
const float UDNAEffect::INVALID_LEVEL = FDNAEffectConstants::INVALID_LEVEL;

DECLARE_CYCLE_STAT(TEXT("MakeQuery"), STAT_MakeDNAEffectQuery, STATGROUP_DNAAbilitySystem);

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	UDNAEffect
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------


UDNAEffect::UDNAEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EDNAEffectDurationType::Instant;
	bExecutePeriodicEffectOnApplication = true;
	ChanceToApplyToTarget.SetValue(1.f);
	StackingType = EDNAEffectStackingType::None;
	StackLimitCount = 0;
	StackDurationRefreshPolicy = EDNAEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	StackPeriodResetPolicy = EDNAEffectStackingPeriodPolicy::ResetOnSuccessfulApplication;
	bRequireModifierSuccessToTriggerCues = true;

#if WITH_EDITORONLY_DATA
	ShowAllProperties = true;
	Template = nullptr;
#endif

}

void UDNAEffect::GetOwnedDNATags(FDNATagContainer& TagContainer) const
{
	TagContainer.AppendTags(InheritableOwnedTagsContainer.CombinedTags);
}

void UDNAEffect::PostLoad()
{
	Super::PostLoad();

	// Temporary post-load fix-up to preserve magnitude data
	for (FDNAModifierInfo& CurModInfo : Modifiers)
	{
		// If the old magnitude actually had some value in it, copy it over and then clear out the old data
		static const FString DNAEffectPostLoadContext(TEXT("UDNAEffect::PostLoad"));
		if (CurModInfo.Magnitude.Value != 0.f || CurModInfo.Magnitude.Curve.IsValid(DNAEffectPostLoadContext))
		{
			CurModInfo.ModifierMagnitude.ScalableFloatMagnitude = CurModInfo.Magnitude;
			CurModInfo.Magnitude = FScalableFloat();
		}

#if WITH_EDITOR
		CurModInfo.ModifierMagnitude.ReportErrors(GetPathName());
#endif // WITH_EDITOR
	}

	// We need to update when we first load to override values coming in from the superclass
	// We also copy the tags from the old tag containers into the inheritable tag containers

	UpdateInheritedTagProperties();

	for (FDNAAbilitySpecDef& Def : GrantedAbilities)
	{
		if (Def.Level != INDEX_NONE)
		{
			Def.LevelScalableFloat.SetValue(Def.Level);
			Def.Level = INDEX_NONE;
		}
	}

	HasGrantedApplicationImmunityQuery = !GrantedApplicationImmunityQuery.IsEmpty();

#if WITH_EDITOR
	GETCURVE_REPORTERROR(Period.Curve);
	GETCURVE_REPORTERROR(ChanceToApplyToTarget.Curve);
	DurationMagnitude.ReportErrors(GetPathName());
#endif // WITH_EDITOR


	for (const TSubclassOf<UDNAEffect>& ConditionalEffectClass : TargetEffectClasses_DEPRECATED)
	{
		FConditionalDNAEffect ConditionalDNAEffect;
		ConditionalDNAEffect.EffectClass = ConditionalEffectClass;
		ConditionalDNAEffects.Add(ConditionalDNAEffect);
	}
	TargetEffectClasses_DEPRECATED.Empty();

	for (FDNAEffectExecutionDefinition& Execution : Executions)
	{
		for (const TSubclassOf<UDNAEffect>& ConditionalEffectClass : Execution.ConditionalDNAEffectClasses_DEPRECATED)
		{
			FConditionalDNAEffect ConditionalDNAEffect;
			ConditionalDNAEffect.EffectClass = ConditionalEffectClass;
			Execution.ConditionalDNAEffects.Add(ConditionalDNAEffect);
		}

		Execution.ConditionalDNAEffectClasses_DEPRECATED.Empty();
	}
}

void UDNAEffect::PostInitProperties()
{
	Super::PostInitProperties();

	InheritableDNAEffectTags.PostInitProperties();
	InheritableOwnedTagsContainer.PostInitProperties();
	RemoveDNAEffectsWithTags.PostInitProperties();
}

#if WITH_EDITOR

void UDNAEffect::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const UProperty* PropertyThatChanged = PropertyChangedEvent.MemberProperty;
	if (PropertyThatChanged)
	{
		UDNAEffect* Parent = Cast<UDNAEffect>(GetClass()->GetSuperClass()->GetDefaultObject());
		FName PropName = PropertyThatChanged->GetFName();
		if (PropName == GET_MEMBER_NAME_CHECKED(UDNAEffect, InheritableDNAEffectTags))
		{
			InheritableDNAEffectTags.UpdateInheritedTagProperties(Parent ? &Parent->InheritableDNAEffectTags : NULL);
		}
		else if (PropName == GET_MEMBER_NAME_CHECKED(UDNAEffect, InheritableOwnedTagsContainer))
		{
			InheritableOwnedTagsContainer.UpdateInheritedTagProperties(Parent ? &Parent->InheritableOwnedTagsContainer : NULL);
		}
		else if (PropName == GET_MEMBER_NAME_CHECKED(UDNAEffect, RemoveDNAEffectsWithTags))
		{
			RemoveDNAEffectsWithTags.UpdateInheritedTagProperties(Parent ? &Parent->RemoveDNAEffectsWithTags : NULL);
		}
	}

	HasGrantedApplicationImmunityQuery = !GrantedApplicationImmunityQuery.IsEmpty();
}

#endif // #if WITH_EDITOR

void UDNAEffect::PreSave(const class ITargetPlatform* TargetPlatform)
{
	Super::PreSave(TargetPlatform);
	HasGrantedApplicationImmunityQuery = !GrantedApplicationImmunityQuery.IsEmpty();
}

void UDNAEffect::UpdateInheritedTagProperties()
{
	UDNAEffect* Parent = Cast<UDNAEffect>(GetClass()->GetSuperClass()->GetDefaultObject());

	InheritableDNAEffectTags.UpdateInheritedTagProperties(Parent ? &Parent->InheritableDNAEffectTags : NULL);
	InheritableOwnedTagsContainer.UpdateInheritedTagProperties(Parent ? &Parent->InheritableOwnedTagsContainer : NULL);
	RemoveDNAEffectsWithTags.UpdateInheritedTagProperties(Parent ? &Parent->RemoveDNAEffectsWithTags : NULL);
}

void UDNAEffect::ValidateDNAEffect()
{
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	FAttributeBasedFloat
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

float FAttributeBasedFloat::CalculateMagnitude(const FDNAEffectSpec& InRelevantSpec) const
{
	const FDNAEffectAttributeCaptureSpec* CaptureSpec = InRelevantSpec.CapturedRelevantAttributes.FindCaptureSpecByDefinition(BackingAttribute, true);
	checkf(CaptureSpec, TEXT("Attempted to calculate an attribute-based float from spec: %s that did not have the required captured attribute: %s"), 
		*InRelevantSpec.ToSimpleString(), *BackingAttribute.ToSimpleString());

	float AttribValue = 0.f;

	// Base value can be calculated w/o evaluation parameters
	if (AttributeCalculationType == EAttributeBasedFloatCalculationType::AttributeBaseValue)
	{
		CaptureSpec->AttemptCalculateAttributeBaseValue(AttribValue);
	}
	// Set up eval params to handle magnitude or bonus magnitude calculations
	else
	{
		FAggregatorEvaluateParameters EvaluationParameters;
		EvaluationParameters.SourceTags = InRelevantSpec.CapturedSourceTags.GetAggregatedTags();
		EvaluationParameters.TargetTags = InRelevantSpec.CapturedTargetTags.GetAggregatedTags();
		EvaluationParameters.AppliedSourceTagFilter = SourceTagFilter;
		EvaluationParameters.AppliedTargetTagFilter = TargetTagFilter;

		if (AttributeCalculationType == EAttributeBasedFloatCalculationType::AttributeMagnitude)
		{
			CaptureSpec->AttemptCalculateAttributeMagnitude(EvaluationParameters, AttribValue);
		}
		else if (AttributeCalculationType == EAttributeBasedFloatCalculationType::AttributeBonusMagnitude)
		{
			CaptureSpec->AttemptCalculateAttributeBonusMagnitude(EvaluationParameters, AttribValue);
		}
		else if (AttributeCalculationType == EAttributeBasedFloatCalculationType::AttributeMagnitudeEvaluatedUpToChannel)
		{
			const bool bRequestingValidChannel = UDNAAbilitySystemGlobals::Get().IsDNAModEvaluationChannelValid(FinalChannel);
			ensure(bRequestingValidChannel);
			const EDNAModEvaluationChannel ChannelToUse = bRequestingValidChannel ? FinalChannel : EDNAModEvaluationChannel::Channel0;

			CaptureSpec->AttemptCalculateAttributeMagnitudeUpToChannel(EvaluationParameters, ChannelToUse, AttribValue);
		}
	}

	// if a curve table entry is specified, use the attribute value as a lookup into the curve instead of using it directly
	static const FString CalculateMagnitudeContext(TEXT("FAttributeBasedFloat::CalculateMagnitude"));
	if (AttributeCurve.IsValid(CalculateMagnitudeContext))
	{
		AttributeCurve.Eval(AttribValue, &AttribValue, CalculateMagnitudeContext);
	}

	const float SpecLvl = InRelevantSpec.GetLevel();
	FString ContextString = FString::Printf(TEXT("FAttributeBasedFloat::CalculateMagnitude from spec %s"), *InRelevantSpec.ToSimpleString());
	return ((Coefficient.GetValueAtLevel(SpecLvl, &ContextString) * (AttribValue + PreMultiplyAdditiveValue.GetValueAtLevel(SpecLvl, &ContextString))) + PostMultiplyAdditiveValue.GetValueAtLevel(SpecLvl, &ContextString));
}

bool FAttributeBasedFloat::operator==(const FAttributeBasedFloat& Other) const
{
	if (Coefficient != Other.Coefficient ||
		PreMultiplyAdditiveValue != Other.PreMultiplyAdditiveValue ||
		PostMultiplyAdditiveValue != Other.PostMultiplyAdditiveValue ||
		BackingAttribute != Other.BackingAttribute ||
		AttributeCurve != Other.AttributeCurve ||
		AttributeCalculationType != Other.AttributeCalculationType)
	{
		return false;
	}
	if (SourceTagFilter.Num() != Other.SourceTagFilter.Num() ||
		!SourceTagFilter.HasAll(Other.SourceTagFilter))
	{
		return false;
	}
	if (TargetTagFilter.Num() != Other.TargetTagFilter.Num() ||
		!TargetTagFilter.HasAll(Other.TargetTagFilter))
	{
		return false;
	}

	return true;
}

bool FAttributeBasedFloat::operator!=(const FAttributeBasedFloat& Other) const
{
	return !(*this == Other);
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	FCustomCalculationBasedFloat
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

float FCustomCalculationBasedFloat::CalculateMagnitude(const FDNAEffectSpec& InRelevantSpec) const
{
	const UDNAModMagnitudeCalculation* CalcCDO = CalculationClassMagnitude->GetDefaultObject<UDNAModMagnitudeCalculation>();
	check(CalcCDO);

	float CustomBaseValue = CalcCDO->CalculateBaseMagnitude(InRelevantSpec);

	const float SpecLvl = InRelevantSpec.GetLevel();
	FString ContextString = FString::Printf(TEXT("FCustomCalculationBasedFloat::CalculateMagnitude from effect %s"), *CalcCDO->GetName());
	return ((Coefficient.GetValueAtLevel(SpecLvl, &ContextString) * (CustomBaseValue + PreMultiplyAdditiveValue.GetValueAtLevel(SpecLvl, &ContextString))) + PostMultiplyAdditiveValue.GetValueAtLevel(SpecLvl, &ContextString));
}

/** Equality/Inequality operators */
bool FCustomCalculationBasedFloat::operator==(const FCustomCalculationBasedFloat& Other) const
{
	if (CalculationClassMagnitude != Other.CalculationClassMagnitude)
	{
		return false;
	}
	if (Coefficient != Other.Coefficient ||
		PreMultiplyAdditiveValue != Other.PreMultiplyAdditiveValue ||
		PostMultiplyAdditiveValue != Other.PostMultiplyAdditiveValue)
	{
		return false;
	}

	return true;
}

bool FCustomCalculationBasedFloat::operator!=(const FCustomCalculationBasedFloat& Other) const
{
	return !(*this == Other);
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	FDNAEffectMagnitude
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

bool FDNAEffectModifierMagnitude::CanCalculateMagnitude(const FDNAEffectSpec& InRelevantSpec) const
{
	// Only can calculate magnitude properly if all required capture definitions are fulfilled by the spec
	TArray<FDNAEffectAttributeCaptureDefinition> ReqCaptureDefs;
	ReqCaptureDefs.Reset();

	GetAttributeCaptureDefinitions(ReqCaptureDefs);

	return InRelevantSpec.HasValidCapturedAttributes(ReqCaptureDefs);
}

bool FDNAEffectModifierMagnitude::AttemptCalculateMagnitude(const FDNAEffectSpec& InRelevantSpec, OUT float& OutCalculatedMagnitude, bool WarnIfSetByCallerFail, float DefaultSetbyCaller) const
{
	const bool bCanCalc = CanCalculateMagnitude(InRelevantSpec);
	if (bCanCalc)
	{
		FString ContextString = FString::Printf(TEXT("FDNAEffectModifierMagnitude::AttemptCalculateMagnitude from effect %s"), *InRelevantSpec.ToSimpleString());

		switch (MagnitudeCalculationType)
		{
			case EDNAEffectMagnitudeCalculation::ScalableFloat:
			{
				OutCalculatedMagnitude = ScalableFloatMagnitude.GetValueAtLevel(InRelevantSpec.GetLevel(), &ContextString);
			}
			break;

			case EDNAEffectMagnitudeCalculation::AttributeBased:
			{
				OutCalculatedMagnitude = AttributeBasedMagnitude.CalculateMagnitude(InRelevantSpec);
			}
			break;

			case EDNAEffectMagnitudeCalculation::CustomCalculationClass:
			{
				OutCalculatedMagnitude = CustomMagnitude.CalculateMagnitude(InRelevantSpec);
			}
			break;

			case EDNAEffectMagnitudeCalculation::SetByCaller:
				OutCalculatedMagnitude = InRelevantSpec.GetSetByCallerMagnitude(SetByCallerMagnitude.DataName, WarnIfSetByCallerFail, DefaultSetbyCaller);
				break;
			default:
				ABILITY_LOG(Error, TEXT("Unknown MagnitudeCalculationType %d in AttemptCalculateMagnitude"), (int32)MagnitudeCalculationType);
				OutCalculatedMagnitude = 0.f;
				break;
		}
	}
	else
	{
		OutCalculatedMagnitude = 0.f;
	}

	return bCanCalc;
}

bool FDNAEffectModifierMagnitude::AttemptRecalculateMagnitudeFromDependentAggregatorChange(const FDNAEffectSpec& InRelevantSpec, OUT float& OutCalculatedMagnitude, const FAggregator* ChangedAggregator) const
{
	TArray<FDNAEffectAttributeCaptureDefinition > ReqCaptureDefs;
	ReqCaptureDefs.Reset();

	GetAttributeCaptureDefinitions(ReqCaptureDefs);

	// We could have many potential captures. If a single one matches our criteria, then we call AttemptCalculateMagnitude once and return.
	for (const FDNAEffectAttributeCaptureDefinition& CaptureDef : ReqCaptureDefs)
	{
		if (CaptureDef.bSnapshot == false)
		{
			const FDNAEffectAttributeCaptureSpec* CapturedSpec = InRelevantSpec.CapturedRelevantAttributes.FindCaptureSpecByDefinition(CaptureDef, true);
			if (CapturedSpec && CapturedSpec->ShouldRefreshLinkedAggregator(ChangedAggregator))
			{
				return AttemptCalculateMagnitude(InRelevantSpec, OutCalculatedMagnitude);
			}
		}
	}

	return false;
}

void FDNAEffectModifierMagnitude::GetAttributeCaptureDefinitions(OUT TArray<FDNAEffectAttributeCaptureDefinition>& OutCaptureDefs) const
{
	OutCaptureDefs.Empty();

	switch (MagnitudeCalculationType)
	{
		case EDNAEffectMagnitudeCalculation::AttributeBased:
		{
			OutCaptureDefs.Add(AttributeBasedMagnitude.BackingAttribute);
		}
		break;

		case EDNAEffectMagnitudeCalculation::CustomCalculationClass:
		{
			if (CustomMagnitude.CalculationClassMagnitude)
			{
				const UDNAModMagnitudeCalculation* CalcCDO = CustomMagnitude.CalculationClassMagnitude->GetDefaultObject<UDNAModMagnitudeCalculation>();
				check(CalcCDO);

				OutCaptureDefs.Append(CalcCDO->GetAttributeCaptureDefinitions());
			}
		}
		break;
	}
}

bool FDNAEffectModifierMagnitude::GetStaticMagnitudeIfPossible(float InLevel, float& OutMagnitude, const FString* ContextString) const
{
	if (MagnitudeCalculationType == EDNAEffectMagnitudeCalculation::ScalableFloat)
	{
		OutMagnitude = ScalableFloatMagnitude.GetValueAtLevel(InLevel, ContextString);
		return true;
	}

	return false;
}

bool FDNAEffectModifierMagnitude::GetSetByCallerDataNameIfPossible(FName& OutDataName) const
{
	if (MagnitudeCalculationType == EDNAEffectMagnitudeCalculation::SetByCaller)
	{
		OutDataName = SetByCallerMagnitude.DataName;
		return true;
	}

	return false;
}

TSubclassOf<UDNAModMagnitudeCalculation> FDNAEffectModifierMagnitude::GetCustomMagnitudeCalculationClass() const
{
	TSubclassOf<UDNAModMagnitudeCalculation> CustomCalcClass = nullptr;

	if (MagnitudeCalculationType == EDNAEffectMagnitudeCalculation::CustomCalculationClass)
	{
		CustomCalcClass = CustomMagnitude.CalculationClassMagnitude;
	}

	return CustomCalcClass;
}

bool FDNAEffectModifierMagnitude::operator==(const FDNAEffectModifierMagnitude& Other) const
{
	if (MagnitudeCalculationType != Other.MagnitudeCalculationType)
	{
		return false;
	}

	switch (MagnitudeCalculationType)
	{
	case EDNAEffectMagnitudeCalculation::ScalableFloat:
		if (ScalableFloatMagnitude != Other.ScalableFloatMagnitude)
		{
			return false;
		}
		break;
	case EDNAEffectMagnitudeCalculation::AttributeBased:
		if (AttributeBasedMagnitude != Other.AttributeBasedMagnitude)
		{
			return false;
		}
		break;
	case EDNAEffectMagnitudeCalculation::CustomCalculationClass:
		if (CustomMagnitude != Other.CustomMagnitude)
		{
			return false;
		}
		break;
	case EDNAEffectMagnitudeCalculation::SetByCaller:
		if (SetByCallerMagnitude.DataName != Other.SetByCallerMagnitude.DataName)
		{
			return false;
		}
		break;
	}

	return true;
}

bool FDNAEffectModifierMagnitude::operator!=(const FDNAEffectModifierMagnitude& Other) const
{
	return !(*this == Other);
}

#if WITH_EDITOR
FText FDNAEffectModifierMagnitude::GetValueForEditorDisplay() const
{
	switch (MagnitudeCalculationType)
	{
		case EDNAEffectMagnitudeCalculation::ScalableFloat:
			return FText::Format(NSLOCTEXT("DNAEffect", "ScalableFloatModifierMagnitude", "{0} s"), FText::AsNumber(ScalableFloatMagnitude.Value));
			
		case EDNAEffectMagnitudeCalculation::AttributeBased:
			return NSLOCTEXT("DNAEffect", "AttributeBasedModifierMagnitude", "Attribute Based");

		case EDNAEffectMagnitudeCalculation::CustomCalculationClass:
			return NSLOCTEXT("DNAEffect", "CustomCalculationClassModifierMagnitude", "Custom Calculation");

		case EDNAEffectMagnitudeCalculation::SetByCaller:
			return NSLOCTEXT("DNAEffect", "SetByCallerModifierMagnitude", "Set by Caller");
	}

	return NSLOCTEXT("DNAEffect", "UnknownModifierMagnitude", "Unknown");
}

void FDNAEffectModifierMagnitude::ReportErrors(const FString& PathName) const
{
	GETCURVE_REPORTERROR_WITHPATHNAME(ScalableFloatMagnitude.Curve, PathName);

	GETCURVE_REPORTERROR_WITHPATHNAME(AttributeBasedMagnitude.Coefficient.Curve, PathName);
	GETCURVE_REPORTERROR_WITHPATHNAME(AttributeBasedMagnitude.PreMultiplyAdditiveValue.Curve, PathName);
	GETCURVE_REPORTERROR_WITHPATHNAME(AttributeBasedMagnitude.PostMultiplyAdditiveValue.Curve, PathName);

	GETCURVE_REPORTERROR_WITHPATHNAME(CustomMagnitude.Coefficient.Curve, PathName);
	GETCURVE_REPORTERROR_WITHPATHNAME(CustomMagnitude.PreMultiplyAdditiveValue.Curve, PathName);
	GETCURVE_REPORTERROR_WITHPATHNAME(CustomMagnitude.PostMultiplyAdditiveValue.Curve, PathName);
}
#endif // WITH_EDITOR


// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	FDNAEffectExecutionDefinition
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

void FDNAEffectExecutionDefinition::GetAttributeCaptureDefinitions(OUT TArray<FDNAEffectAttributeCaptureDefinition>& OutCaptureDefs) const
{
	OutCaptureDefs.Empty();

	if (CalculationClass)
	{
		const UDNAEffectExecutionCalculation* CalculationCDO = Cast<UDNAEffectExecutionCalculation>(CalculationClass->ClassDefaultObject);
		check(CalculationCDO);

		OutCaptureDefs.Append(CalculationCDO->GetAttributeCaptureDefinitions());
	}

	// Scoped modifiers might have custom magnitude calculations, requiring additional captured attributes
	for (const FDNAEffectExecutionScopedModifierInfo& CurScopedMod : CalculationModifiers)
	{
		TArray<FDNAEffectAttributeCaptureDefinition> ScopedModMagDefs;
		CurScopedMod.ModifierMagnitude.GetAttributeCaptureDefinitions(ScopedModMagDefs);

		OutCaptureDefs.Append(ScopedModMagDefs);
	}
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	FConditionalDNAEffect
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

bool FConditionalDNAEffect::CanApply(const FDNATagContainer& SourceTags, float SourceLevel) const
{
	// Right now we're just using the tags but in the future we may gate this by source level as well
	return SourceTags.HasAll(RequiredSourceTags);
}

FDNAEffectSpecHandle FConditionalDNAEffect::CreateSpec(FDNAEffectContextHandle EffectContext, float SourceLevel) const
{
	const UDNAEffect* EffectCDO = EffectClass ? EffectClass->GetDefaultObject<UDNAEffect>() : nullptr;
	return EffectCDO ? FDNAEffectSpecHandle(new FDNAEffectSpec(EffectCDO, EffectContext, SourceLevel)) : FDNAEffectSpecHandle();
}


// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	FDNAEffectSpec
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

FDNAEffectSpec::FDNAEffectSpec()
	: Def(nullptr)
	, Duration(UDNAEffect::INSTANT_APPLICATION)
	, Period(UDNAEffect::NO_PERIOD)
	, ChanceToApplyToTarget(1.f)
	, StackCount(1)
	, bCompletedSourceAttributeCapture(false)
	, bCompletedTargetAttributeCapture(false)
	, bDurationLocked(false)
	, Level(UDNAEffect::INVALID_LEVEL)
{
}

FDNAEffectSpec::FDNAEffectSpec(const UDNAEffect* InDef, const FDNAEffectContextHandle& InEffectContext, float InLevel)
	: Def(InDef)
	, Duration(UDNAEffect::INSTANT_APPLICATION)
	, Period(UDNAEffect::NO_PERIOD)
	, ChanceToApplyToTarget(1.f)
	, StackCount(1)
	, bCompletedSourceAttributeCapture(false)
	, bCompletedTargetAttributeCapture(false)
	, bDurationLocked(false)
{
	Initialize(InDef, InEffectContext, InLevel);
}

FDNAEffectSpec::FDNAEffectSpec(const FDNAEffectSpec& Other)
{
	*this = Other;
}

FDNAEffectSpec::FDNAEffectSpec(FDNAEffectSpec&& Other)
	: Def(Other.Def)
	, ModifiedAttributes(MoveTemp(Other.ModifiedAttributes))
	, CapturedRelevantAttributes(MoveTemp(Other.CapturedRelevantAttributes))
	, TargetEffectSpecs(MoveTemp(Other.TargetEffectSpecs))
	, Duration(Other.Duration)
	, Period(Other.Period)
	, ChanceToApplyToTarget(Other.ChanceToApplyToTarget)
	, CapturedSourceTags(MoveTemp(Other.CapturedSourceTags))
	, CapturedTargetTags(MoveTemp(Other.CapturedTargetTags))
	, DynamicGrantedTags(MoveTemp(Other.DynamicGrantedTags))
	, DynamicAssetTags(MoveTemp(Other.DynamicAssetTags))
	, Modifiers(MoveTemp(Other.Modifiers))
	, StackCount(Other.StackCount)
	, bCompletedSourceAttributeCapture(Other.bCompletedSourceAttributeCapture)
	, bCompletedTargetAttributeCapture(Other.bCompletedTargetAttributeCapture)
	, bDurationLocked(Other.bDurationLocked)
	, GrantedAbilitySpecs(MoveTemp(Other.GrantedAbilitySpecs))
	, SetByCallerMagnitudes(MoveTemp(Other.SetByCallerMagnitudes))
	, EffectContext(Other.EffectContext)
	, Level(Other.Level)
{

}

FDNAEffectSpec& FDNAEffectSpec::operator=(FDNAEffectSpec&& Other)
{
	Def = Other.Def;
	ModifiedAttributes = MoveTemp(Other.ModifiedAttributes);
	CapturedRelevantAttributes = MoveTemp(Other.CapturedRelevantAttributes);
	TargetEffectSpecs = MoveTemp(Other.TargetEffectSpecs);
	Duration = Other.Duration;
	Period = Other.Period;
	ChanceToApplyToTarget = Other.ChanceToApplyToTarget;
	CapturedSourceTags = MoveTemp(Other.CapturedSourceTags);
	CapturedTargetTags = MoveTemp(Other.CapturedTargetTags);
	DynamicGrantedTags = MoveTemp(Other.DynamicGrantedTags);
	DynamicAssetTags = MoveTemp(Other.DynamicAssetTags);
	Modifiers = MoveTemp(Other.Modifiers);
	StackCount = Other.StackCount;
	bCompletedSourceAttributeCapture = Other.bCompletedSourceAttributeCapture;
	bCompletedTargetAttributeCapture = Other.bCompletedTargetAttributeCapture;
	bDurationLocked = Other.bDurationLocked;
	GrantedAbilitySpecs = MoveTemp(Other.GrantedAbilitySpecs);
	SetByCallerMagnitudes = MoveTemp(Other.SetByCallerMagnitudes);
	EffectContext = Other.EffectContext;
	Level = Other.Level;
	return *this;
}

FDNAEffectSpec& FDNAEffectSpec::operator=(const FDNAEffectSpec& Other)
{
	Def = Other.Def;
	ModifiedAttributes = Other.ModifiedAttributes;
	CapturedRelevantAttributes = Other.CapturedRelevantAttributes;
	TargetEffectSpecs = Other.TargetEffectSpecs;
	Duration = Other.Duration;
	Period = Other.Period;
	ChanceToApplyToTarget = Other.ChanceToApplyToTarget;
	CapturedSourceTags = Other.CapturedSourceTags;
	CapturedTargetTags = Other.CapturedTargetTags;
	DynamicGrantedTags = Other.DynamicGrantedTags;
	DynamicAssetTags = Other.DynamicAssetTags;
	Modifiers = Other.Modifiers;
	StackCount = Other.StackCount;
	bCompletedSourceAttributeCapture = Other.bCompletedSourceAttributeCapture;
	bCompletedTargetAttributeCapture = Other.bCompletedTargetAttributeCapture;
	bDurationLocked = Other.bDurationLocked;
	GrantedAbilitySpecs =  Other.GrantedAbilitySpecs;
	SetByCallerMagnitudes = Other.SetByCallerMagnitudes;
	EffectContext = Other.EffectContext;
	Level = Other.Level;
	return *this;
}

FDNAEffectSpecForRPC::FDNAEffectSpecForRPC()
	: Def(nullptr)
	, Level(UDNAEffect::INVALID_LEVEL)
	, AbilityLevel(1)
{
}

FDNAEffectSpecForRPC::FDNAEffectSpecForRPC(const FDNAEffectSpec& InSpec)
	: Def(InSpec.Def)
	, ModifiedAttributes()
	, EffectContext(InSpec.GetEffectContext())
	, AggregatedSourceTags(*InSpec.CapturedSourceTags.GetAggregatedTags())
	, AggregatedTargetTags(*InSpec.CapturedTargetTags.GetAggregatedTags())
	, Level(InSpec.GetLevel())
	, AbilityLevel(InSpec.GetEffectContext().GetAbilityLevel())
{
	// Only copy attributes that are in the DNA cue info
	for (int32 i = InSpec.ModifiedAttributes.Num() - 1; i >= 0; i--)
	{
		for (const FDNAEffectCue& CueInfo : Def->DNACues)
		{
			if (CueInfo.MagnitudeAttribute == InSpec.ModifiedAttributes[i].Attribute)
			{
				ModifiedAttributes.Add(InSpec.ModifiedAttributes[i]);
			}
		}
	}
}

void FDNAEffectSpec::Initialize(const UDNAEffect* InDef, const FDNAEffectContextHandle& InEffectContext, float InLevel)
{
	Def = InDef;
	check(Def);	
	SetLevel(InLevel);
	SetContext(InEffectContext);

	// Init our ModifierSpecs
	Modifiers.SetNum(Def->Modifiers.Num());

	// Prep the spec with all of the attribute captures it will need to perform
	SetupAttributeCaptureDefinitions();
	
	// Add the DNAEffect asset tags to the source Spec tags
	CapturedSourceTags.GetSpecTags().AppendTags(InDef->InheritableDNAEffectTags.CombinedTags);

	// Make TargetEffectSpecs too

	for (const FConditionalDNAEffect& ConditionalEffect : InDef->ConditionalDNAEffects)
	{
		if (ConditionalEffect.CanApply(CapturedSourceTags.GetActorTags(), InLevel))
		{
			FDNAEffectSpecHandle SpecHandle = ConditionalEffect.CreateSpec(EffectContext, InLevel);
			if (SpecHandle.IsValid())
			{
				TargetEffectSpecs.Add(SpecHandle);
			}
		}
	}

	// Make Granted AbilitySpecs (caller may modify these specs after creating spec, which is why we dont just reference them from the def)
	GrantedAbilitySpecs = InDef->GrantedAbilities;

	// if we're granting abilities and they don't specify a source object use the source of this GE
	for (FDNAAbilitySpecDef& AbilitySpecDef : GrantedAbilitySpecs)
	{
		if (AbilitySpecDef.SourceObject == nullptr)
		{
			AbilitySpecDef.SourceObject = InEffectContext.GetSourceObject();
		}
	}

	// Everything is setup now, capture data from our source
	CaptureDataFromSource();
}

void FDNAEffectSpec::SetupAttributeCaptureDefinitions()
{
	// Add duration if required
	if (Def->DurationPolicy == EDNAEffectDurationType::HasDuration)
	{
		CapturedRelevantAttributes.AddCaptureDefinition(UDNAAbilitySystemComponent::GetOutgoingDurationCapture());
		CapturedRelevantAttributes.AddCaptureDefinition(UDNAAbilitySystemComponent::GetIncomingDurationCapture());
	}

	TArray<FDNAEffectAttributeCaptureDefinition> CaptureDefs;

	// Gather capture definitions from duration	
	{
		CaptureDefs.Reset();
		Def->DurationMagnitude.GetAttributeCaptureDefinitions(CaptureDefs);
		for (const FDNAEffectAttributeCaptureDefinition& CurDurationCaptureDef : CaptureDefs)
		{
			CapturedRelevantAttributes.AddCaptureDefinition(CurDurationCaptureDef);
		}
	}

	// Gather all capture definitions from modifiers
	for (int32 ModIdx = 0; ModIdx < Modifiers.Num(); ++ModIdx)
	{
		const FDNAModifierInfo& ModDef = Def->Modifiers[ModIdx];
		const FModifierSpec& ModSpec = Modifiers[ModIdx];

		CaptureDefs.Reset();
		ModDef.ModifierMagnitude.GetAttributeCaptureDefinitions(CaptureDefs);
		
		for (const FDNAEffectAttributeCaptureDefinition& CurCaptureDef : CaptureDefs)
		{
			CapturedRelevantAttributes.AddCaptureDefinition(CurCaptureDef);
		}
	}

	// Gather all capture definitions from executions
	for (const FDNAEffectExecutionDefinition& Exec : Def->Executions)
	{
		CaptureDefs.Reset();
		Exec.GetAttributeCaptureDefinitions(CaptureDefs);
		for (const FDNAEffectAttributeCaptureDefinition& CurExecCaptureDef : CaptureDefs)
		{
			CapturedRelevantAttributes.AddCaptureDefinition(CurExecCaptureDef);
		}
	}
}

void FDNAEffectSpec::CaptureAttributeDataFromTarget(UDNAAbilitySystemComponent* TargetDNAAbilitySystemComponent)
{
	CapturedRelevantAttributes.CaptureAttributes(TargetDNAAbilitySystemComponent, EDNAEffectAttributeCaptureSource::Target);
	bCompletedTargetAttributeCapture = true;
}

void FDNAEffectSpec::CaptureDataFromSource()
{
	// Capture source actor tags
	RecaptureSourceActorTags();

	// Capture source Attributes
	// Is this the right place to do it? Do we ever need to create spec and capture attributes at a later time? If so, this will need to move.
	CapturedRelevantAttributes.CaptureAttributes(EffectContext.GetInstigatorDNAAbilitySystemComponent(), EDNAEffectAttributeCaptureSource::Source);

	// Now that we have source attributes captured, re-evaluate the duration since it could be based on the captured attributes.
	float DefCalcDuration = 0.f;
	if (AttemptCalculateDurationFromDef(DefCalcDuration))
	{
		SetDuration(DefCalcDuration, false);
	}

	bCompletedSourceAttributeCapture = true;
}

void FDNAEffectSpec::RecaptureSourceActorTags()
{
	CapturedSourceTags.GetActorTags().Reset();
	EffectContext.GetOwnedDNATags(CapturedSourceTags.GetActorTags(), CapturedSourceTags.GetSpecTags());
}

bool FDNAEffectSpec::AttemptCalculateDurationFromDef(OUT float& OutDefDuration) const
{
	check(Def);

	bool bCalculatedDuration = true;

	const EDNAEffectDurationType DurType = Def->DurationPolicy;
	if (DurType == EDNAEffectDurationType::Infinite)
	{
		OutDefDuration = UDNAEffect::INFINITE_DURATION;
	}
	else if (DurType == EDNAEffectDurationType::Instant)
	{
		OutDefDuration = UDNAEffect::INSTANT_APPLICATION;
	}
	else
	{
		// The last parameters (false, 1.f) are so that if SetByCaller hasn't been set yet, we don't warn and default
		// to 1.f. This is so that the rest of the system doesn't treat the effect as an instant effect. 1.f is arbitrary
		// and this makes it illegal to SetByCaller something into an instant effect.
		bCalculatedDuration = Def->DurationMagnitude.AttemptCalculateMagnitude(*this, OutDefDuration, false, 1.f);
	}

	return bCalculatedDuration;
}

void FDNAEffectSpec::SetLevel(float InLevel)
{
	Level = InLevel;
	if (Def)
	{
		float DefCalcDuration = 0.f;
		if (AttemptCalculateDurationFromDef(DefCalcDuration))
		{
			SetDuration(DefCalcDuration, false);
		}

		FString ContextString = FString::Printf(TEXT("FDNAEffectSpec::SetLevel from effect %s"), *Def->GetName());
		Period = Def->Period.GetValueAtLevel(InLevel, &ContextString);
		ChanceToApplyToTarget = Def->ChanceToApplyToTarget.GetValueAtLevel(InLevel, &ContextString);
	}
}

float FDNAEffectSpec::GetLevel() const
{
	return Level;
}

float FDNAEffectSpec::GetDuration() const
{
	return Duration;
}

void FDNAEffectSpec::SetDuration(float NewDuration, bool bLockDuration)
{
	if (!bDurationLocked)
	{
		Duration = NewDuration;
		bDurationLocked = bLockDuration;
		if (Duration > 0.f)
		{
			// We may have potential problems one day if a game is applying duration based DNA effects from instantaneous effects
			// (E.g., every time fire damage is applied, a DOT is also applied). We may need to for Duration to always be captured.
			CapturedRelevantAttributes.AddCaptureDefinition(UDNAAbilitySystemComponent::GetOutgoingDurationCapture());
		}
	}
}

float FDNAEffectSpec::CalculateModifiedDuration() const
{
	FAggregator DurationAgg;

	const FDNAEffectAttributeCaptureSpec* OutgoingCaptureSpec = CapturedRelevantAttributes.FindCaptureSpecByDefinition(UDNAAbilitySystemComponent::GetOutgoingDurationCapture(), true);
	if (OutgoingCaptureSpec)
	{
		OutgoingCaptureSpec->AttemptAddAggregatorModsToAggregator(DurationAgg);
	}

	const FDNAEffectAttributeCaptureSpec* IncomingCaptureSpec = CapturedRelevantAttributes.FindCaptureSpecByDefinition(UDNAAbilitySystemComponent::GetIncomingDurationCapture(), true);
	if (IncomingCaptureSpec)
	{
		IncomingCaptureSpec->AttemptAddAggregatorModsToAggregator(DurationAgg);
	}

	FAggregatorEvaluateParameters Params;
	Params.SourceTags = CapturedSourceTags.GetAggregatedTags();
	Params.TargetTags = CapturedTargetTags.GetAggregatedTags();

	return DurationAgg.EvaluateWithBase(GetDuration(), Params);
}

float FDNAEffectSpec::GetPeriod() const
{
	return Period;
}

float FDNAEffectSpec::GetChanceToApplyToTarget() const
{
	return ChanceToApplyToTarget;
}

float FDNAEffectSpec::GetModifierMagnitude(int32 ModifierIdx, bool bFactorInStackCount) const
{
	check(Modifiers.IsValidIndex(ModifierIdx) && Def && Def->Modifiers.IsValidIndex(ModifierIdx));

	const float SingleEvaluatedMagnitude = Modifiers[ModifierIdx].GetEvaluatedMagnitude();

	float ModMagnitude = SingleEvaluatedMagnitude;
	if (bFactorInStackCount)
	{
		ModMagnitude = DNAEffectUtilities::ComputeStackedModifierMagnitude(SingleEvaluatedMagnitude, StackCount, Def->Modifiers[ModifierIdx].ModifierOp);
	}

	return ModMagnitude;
}

void FDNAEffectSpec::CalculateModifierMagnitudes()
{
	for(int32 ModIdx = 0; ModIdx < Modifiers.Num(); ++ModIdx)
	{
		const FDNAModifierInfo& ModDef = Def->Modifiers[ModIdx];
		FModifierSpec& ModSpec = Modifiers[ModIdx];

		if (ModDef.ModifierMagnitude.AttemptCalculateMagnitude(*this, ModSpec.EvaluatedMagnitude) == false)
		{
			ModSpec.EvaluatedMagnitude = 0.f;
			ABILITY_LOG(Warning, TEXT("Modifier on spec: %s was asked to CalculateMagnitude and failed, falling back to 0."), *ToSimpleString());
		}
	}
}

bool FDNAEffectSpec::HasValidCapturedAttributes(const TArray<FDNAEffectAttributeCaptureDefinition>& InCaptureDefsToCheck) const
{
	return CapturedRelevantAttributes.HasValidCapturedAttributes(InCaptureDefsToCheck);
}

void FDNAEffectSpec::RecaptureAttributeDataForClone(UDNAAbilitySystemComponent* OriginalASC, UDNAAbilitySystemComponent* NewASC)
{
	if (bCompletedSourceAttributeCapture == false)
	{
		// Only do this if we are the source
		if (EffectContext.GetInstigatorDNAAbilitySystemComponent() == OriginalASC)
		{
			// Flip the effect context
			EffectContext.AddInstigator(NewASC->GetOwner(), EffectContext.GetEffectCauser());
			CaptureDataFromSource();
		}
	}

	if (bCompletedTargetAttributeCapture == false)
	{
		CaptureAttributeDataFromTarget(NewASC);
	}
}

const FDNAEffectModifiedAttribute* FDNAEffectSpec::GetModifiedAttribute(const FDNAAttribute& Attribute) const
{
	for (const FDNAEffectModifiedAttribute& ModifiedAttribute : ModifiedAttributes)
	{
		if (ModifiedAttribute.Attribute == Attribute)
		{
			return &ModifiedAttribute;
		}
	}
	return NULL;
}

FDNAEffectModifiedAttribute* FDNAEffectSpec::GetModifiedAttribute(const FDNAAttribute& Attribute)
{
	for (FDNAEffectModifiedAttribute& ModifiedAttribute : ModifiedAttributes)
	{
		if (ModifiedAttribute.Attribute == Attribute)
		{
			return &ModifiedAttribute;
		}
	}
	return NULL;
}

const FDNAEffectModifiedAttribute* FDNAEffectSpecForRPC::GetModifiedAttribute(const FDNAAttribute& Attribute) const
{
	for (const FDNAEffectModifiedAttribute& ModifiedAttribute : ModifiedAttributes)
	{
		if (ModifiedAttribute.Attribute == Attribute)
		{
			return &ModifiedAttribute;
		}
	}
	return NULL;
}

FString FDNAEffectSpecForRPC::ToSimpleString() const
{
	return FString::Printf(TEXT("%s"), *Def->GetName());
}

FDNAEffectModifiedAttribute* FDNAEffectSpec::AddModifiedAttribute(const FDNAAttribute& Attribute)
{
	FDNAEffectModifiedAttribute NewAttribute;
	NewAttribute.Attribute = Attribute;
	return &ModifiedAttributes[ModifiedAttributes.Add(NewAttribute)];
}

void FDNAEffectSpec::SetContext(FDNAEffectContextHandle NewEffectContext)
{
	bool bWasAlreadyInit = EffectContext.IsValid();
	EffectContext = NewEffectContext;	
	if (bWasAlreadyInit)
	{
		CaptureDataFromSource();
	}
}

void FDNAEffectSpec::GetAllGrantedTags(OUT FDNATagContainer& Container) const
{
	Container.AppendTags(DynamicGrantedTags);
	Container.AppendTags(Def->InheritableOwnedTagsContainer.CombinedTags);
}

void FDNAEffectSpec::GetAllAssetTags(OUT FDNATagContainer& Container) const
{
	Container.AppendTags(DynamicAssetTags);
	if (ensure(Def))
	{
		Container.AppendTags(Def->InheritableDNAEffectTags.CombinedTags);
	}
}

void FDNAEffectSpec::SetSetByCallerMagnitude(FName DataName, float Magnitude)
{
	SetByCallerMagnitudes.FindOrAdd(DataName) = Magnitude;
}

float FDNAEffectSpec::GetSetByCallerMagnitude(FName DataName, bool WarnIfNotFound, float DefaultIfNotFound) const
{
	float Magnitude = DefaultIfNotFound;
	const float* Ptr = SetByCallerMagnitudes.Find(DataName);
	if (Ptr)
	{
		Magnitude = *Ptr;
	}
	else if (WarnIfNotFound)
	{
		ABILITY_LOG(Error, TEXT("FDNAEffectSpec::GetMagnitude called for Data %s on Def %s when magnitude had not yet been set by caller."), *DataName.ToString(), *Def->GetName());
	}

	return Magnitude;
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	FDNAEffectAttributeCaptureSpec
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

FDNAEffectAttributeCaptureSpec::FDNAEffectAttributeCaptureSpec()
{
}

FDNAEffectAttributeCaptureSpec::FDNAEffectAttributeCaptureSpec(const FDNAEffectAttributeCaptureDefinition& InDefinition)
	: BackingDefinition(InDefinition)
{
}

bool FDNAEffectAttributeCaptureSpec::HasValidCapture() const
{
	return (AttributeAggregator.Get() != nullptr);
}

bool FDNAEffectAttributeCaptureSpec::AttemptCalculateAttributeMagnitude(const FAggregatorEvaluateParameters& InEvalParams, OUT float& OutMagnitude) const
{
	FAggregator* Agg = AttributeAggregator.Get();
	if (Agg)
	{
		OutMagnitude = Agg->Evaluate(InEvalParams);
		return true;
	}

	return false;
}

bool FDNAEffectAttributeCaptureSpec::AttemptCalculateAttributeMagnitudeUpToChannel(const FAggregatorEvaluateParameters& InEvalParams, EDNAModEvaluationChannel FinalChannel, OUT float& OutMagnitude) const
{
	FAggregator* Agg = AttributeAggregator.Get();
	if (Agg)
	{
		OutMagnitude = Agg->EvaluateToChannel(InEvalParams, FinalChannel);
		return true;
	}

	return false;
}

bool FDNAEffectAttributeCaptureSpec::AttemptCalculateAttributeMagnitudeWithBase(const FAggregatorEvaluateParameters& InEvalParams, float InBaseValue, OUT float& OutMagnitude) const
{
	FAggregator* Agg = AttributeAggregator.Get();
	if (Agg)
	{
		OutMagnitude = Agg->EvaluateWithBase(InBaseValue, InEvalParams);
		return true;
	}

	return false;
}

bool FDNAEffectAttributeCaptureSpec::AttemptCalculateAttributeBaseValue(OUT float& OutBaseValue) const
{
	FAggregator* Agg = AttributeAggregator.Get();
	if (Agg)
	{
		OutBaseValue = Agg->GetBaseValue();
		return true;
	}

	return false;
}

bool FDNAEffectAttributeCaptureSpec::AttemptCalculateAttributeBonusMagnitude(const FAggregatorEvaluateParameters& InEvalParams, OUT float& OutBonusMagnitude) const
{
	FAggregator* Agg = AttributeAggregator.Get();
	if (Agg)
	{
		OutBonusMagnitude = Agg->EvaluateBonus(InEvalParams);
		return true;
	}

	return false;
}

bool FDNAEffectAttributeCaptureSpec::AttemptCalculateAttributeContributionMagnitude(const FAggregatorEvaluateParameters& InEvalParams, FActiveDNAEffectHandle ActiveHandle, OUT float& OutBonusMagnitude) const
{
	FAggregator* Agg = AttributeAggregator.Get();
	if (Agg && ActiveHandle.IsValid())
	{
		OutBonusMagnitude = Agg->EvaluateContribution(InEvalParams, ActiveHandle);
		return true;
	}

	return false;
}

bool FDNAEffectAttributeCaptureSpec::AttemptGetAttributeAggregatorSnapshot(OUT FAggregator& OutAggregatorSnapshot) const
{
	FAggregator* Agg = AttributeAggregator.Get();
	if (Agg)
	{
		OutAggregatorSnapshot.TakeSnapshotOf(*Agg);
		return true;
	}

	return false;
}

bool FDNAEffectAttributeCaptureSpec::AttemptAddAggregatorModsToAggregator(OUT FAggregator& OutAggregatorToAddTo) const
{
	FAggregator* Agg = AttributeAggregator.Get();
	if (Agg)
	{
		OutAggregatorToAddTo.AddModsFrom(*Agg);
		return true;
	}

	return false;
}

void FDNAEffectAttributeCaptureSpec::RegisterLinkedAggregatorCallback(FActiveDNAEffectHandle Handle) const
{
	if (BackingDefinition.bSnapshot == false)
	{
		// Its possible the linked Aggregator is already gone.
		FAggregator* Agg = AttributeAggregator.Get();
		if (Agg)
		{
			Agg->AddDependent(Handle);
		}
	}
}

void FDNAEffectAttributeCaptureSpec::UnregisterLinkedAggregatorCallback(FActiveDNAEffectHandle Handle) const
{
	FAggregator* Agg = AttributeAggregator.Get();
	if (Agg)
	{
		Agg->RemoveDependent(Handle);
	}
}

bool FDNAEffectAttributeCaptureSpec::ShouldRefreshLinkedAggregator(const FAggregator* ChangedAggregator) const
{
	return (BackingDefinition.bSnapshot == false && (ChangedAggregator == nullptr || AttributeAggregator.Get() == ChangedAggregator));
}

void FDNAEffectAttributeCaptureSpec::SwapAggregator(FAggregatorRef From, FAggregatorRef To)
{
	if (AttributeAggregator.Get() == From.Get())
	{
		AttributeAggregator = To;
	}
}

const FDNAEffectAttributeCaptureDefinition& FDNAEffectAttributeCaptureSpec::GetBackingDefinition() const
{
	return BackingDefinition;
}

FDNAEffectAttributeCaptureSpecContainer::FDNAEffectAttributeCaptureSpecContainer()
	: bHasNonSnapshottedAttributes(false)
{
}

FDNAEffectAttributeCaptureSpecContainer::FDNAEffectAttributeCaptureSpecContainer(FDNAEffectAttributeCaptureSpecContainer&& Other)
	: SourceAttributes(MoveTemp(Other.SourceAttributes))
	, TargetAttributes(MoveTemp(Other.TargetAttributes))
	, bHasNonSnapshottedAttributes(Other.bHasNonSnapshottedAttributes)
{
}

FDNAEffectAttributeCaptureSpecContainer::FDNAEffectAttributeCaptureSpecContainer(const FDNAEffectAttributeCaptureSpecContainer& Other)
	: SourceAttributes(Other.SourceAttributes)
	, TargetAttributes(Other.TargetAttributes)
	, bHasNonSnapshottedAttributes(Other.bHasNonSnapshottedAttributes)
{
}

FDNAEffectAttributeCaptureSpecContainer& FDNAEffectAttributeCaptureSpecContainer::operator=(FDNAEffectAttributeCaptureSpecContainer&& Other)
{
	SourceAttributes = MoveTemp(Other.SourceAttributes);
	TargetAttributes = MoveTemp(Other.TargetAttributes);
	bHasNonSnapshottedAttributes = Other.bHasNonSnapshottedAttributes;
	return *this;
}

FDNAEffectAttributeCaptureSpecContainer& FDNAEffectAttributeCaptureSpecContainer::operator=(const FDNAEffectAttributeCaptureSpecContainer& Other)
{
	SourceAttributes = Other.SourceAttributes;
	TargetAttributes = Other.TargetAttributes;
	bHasNonSnapshottedAttributes = Other.bHasNonSnapshottedAttributes;
	return *this;
}

void FDNAEffectAttributeCaptureSpecContainer::AddCaptureDefinition(const FDNAEffectAttributeCaptureDefinition& InCaptureDefinition)
{
	const bool bSourceAttribute = (InCaptureDefinition.AttributeSource == EDNAEffectAttributeCaptureSource::Source);
	TArray<FDNAEffectAttributeCaptureSpec>& AttributeArray = (bSourceAttribute ? SourceAttributes : TargetAttributes);

	// Only add additional captures if this exact capture definition isn't already being handled
	if (!AttributeArray.ContainsByPredicate(
			[&](const FDNAEffectAttributeCaptureSpec& Element)
			{
				return Element.GetBackingDefinition() == InCaptureDefinition;
			}))
	{
		AttributeArray.Add(FDNAEffectAttributeCaptureSpec(InCaptureDefinition));

		if (!InCaptureDefinition.bSnapshot)
		{
			bHasNonSnapshottedAttributes = true;
		}
	}
}

void FDNAEffectAttributeCaptureSpecContainer::CaptureAttributes(UDNAAbilitySystemComponent* InDNAAbilitySystemComponent, EDNAEffectAttributeCaptureSource InCaptureSource)
{
	if (InDNAAbilitySystemComponent)
	{
		const bool bSourceComponent = (InCaptureSource == EDNAEffectAttributeCaptureSource::Source);
		TArray<FDNAEffectAttributeCaptureSpec>& AttributeArray = (bSourceComponent ? SourceAttributes : TargetAttributes);

		// Capture every spec's requirements from the specified component
		for (FDNAEffectAttributeCaptureSpec& CurCaptureSpec : AttributeArray)
		{
			InDNAAbilitySystemComponent->CaptureAttributeForDNAEffect(CurCaptureSpec);
		}
	}
}

const FDNAEffectAttributeCaptureSpec* FDNAEffectAttributeCaptureSpecContainer::FindCaptureSpecByDefinition(const FDNAEffectAttributeCaptureDefinition& InDefinition, bool bOnlyIncludeValidCapture) const
{
	const bool bSourceAttribute = (InDefinition.AttributeSource == EDNAEffectAttributeCaptureSource::Source);
	const TArray<FDNAEffectAttributeCaptureSpec>& AttributeArray = (bSourceAttribute ? SourceAttributes : TargetAttributes);

	const FDNAEffectAttributeCaptureSpec* MatchingSpec = AttributeArray.FindByPredicate(
		[&](const FDNAEffectAttributeCaptureSpec& Element)
	{
		return Element.GetBackingDefinition() == InDefinition;
	});

	// Null out the found results if the caller only wants valid captures and we don't have one yet
	if (MatchingSpec && bOnlyIncludeValidCapture && !MatchingSpec->HasValidCapture())
	{
		MatchingSpec = nullptr;
	}

	return MatchingSpec;
}

bool FDNAEffectAttributeCaptureSpecContainer::HasValidCapturedAttributes(const TArray<FDNAEffectAttributeCaptureDefinition>& InCaptureDefsToCheck) const
{
	bool bHasValid = true;

	for (const FDNAEffectAttributeCaptureDefinition& CurDef : InCaptureDefsToCheck)
	{
		const FDNAEffectAttributeCaptureSpec* CaptureSpec = FindCaptureSpecByDefinition(CurDef, true);
		if (!CaptureSpec)
		{
			bHasValid = false;
			break;
		}
	}

	return bHasValid;
}

bool FDNAEffectAttributeCaptureSpecContainer::HasNonSnapshottedAttributes() const
{
	return bHasNonSnapshottedAttributes;
}

void FDNAEffectAttributeCaptureSpecContainer::RegisterLinkedAggregatorCallbacks(FActiveDNAEffectHandle Handle) const
{
	for (const FDNAEffectAttributeCaptureSpec& CaptureSpec : SourceAttributes)
	{
		CaptureSpec.RegisterLinkedAggregatorCallback(Handle);
	}

	for (const FDNAEffectAttributeCaptureSpec& CaptureSpec : TargetAttributes)
	{
		CaptureSpec.RegisterLinkedAggregatorCallback(Handle);
	}
}

void FDNAEffectAttributeCaptureSpecContainer::UnregisterLinkedAggregatorCallbacks(FActiveDNAEffectHandle Handle) const
{
	for (const FDNAEffectAttributeCaptureSpec& CaptureSpec : SourceAttributes)
	{
		CaptureSpec.UnregisterLinkedAggregatorCallback(Handle);
	}

	for (const FDNAEffectAttributeCaptureSpec& CaptureSpec : TargetAttributes)
	{
		CaptureSpec.UnregisterLinkedAggregatorCallback(Handle);
	}
}

void FDNAEffectAttributeCaptureSpecContainer::SwapAggregator(FAggregatorRef From, FAggregatorRef To)
{
	for (FDNAEffectAttributeCaptureSpec& CaptureSpec : SourceAttributes)
	{
		CaptureSpec.SwapAggregator(From, To);
	}

	for (FDNAEffectAttributeCaptureSpec& CaptureSpec : TargetAttributes)
	{
		CaptureSpec.SwapAggregator(From, To);
	}
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	FActiveDNAEffect
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

FActiveDNAEffect::FActiveDNAEffect()
	: StartServerWorldTime(0)
	, CachedStartServerWorldTime(0)
	, StartWorldTime(0.f)
	, bIsInhibited(true)
	, bPendingRepOnActiveGC(false)
	, bPendingRepWhileActiveGC(false)
	, IsPendingRemove(false)
	, ClientCachedStackCount(0)
	, PendingNext(nullptr)
{
}

FActiveDNAEffect::FActiveDNAEffect(const FActiveDNAEffect& Other)
{
	*this = Other;
}

FActiveDNAEffect::FActiveDNAEffect(FActiveDNAEffectHandle InHandle, const FDNAEffectSpec &InSpec, float CurrentWorldTime, float InStartServerWorldTime, FPredictionKey InPredictionKey)
	: Handle(InHandle)
	, Spec(InSpec)
	, PredictionKey(InPredictionKey)
	, StartServerWorldTime(InStartServerWorldTime)
	, CachedStartServerWorldTime(InStartServerWorldTime)
	, StartWorldTime(CurrentWorldTime)
	, bIsInhibited(true)
	, bPendingRepOnActiveGC(false)
	, bPendingRepWhileActiveGC(false)
	, IsPendingRemove(false)
	, ClientCachedStackCount(0)
	, PendingNext(nullptr)
{
}

FActiveDNAEffect::FActiveDNAEffect(FActiveDNAEffect&& Other)
	:Handle(Other.Handle)
	,Spec(MoveTemp(Other.Spec))
	,PredictionKey(Other.PredictionKey)
	,StartServerWorldTime(Other.StartServerWorldTime)
	,CachedStartServerWorldTime(Other.CachedStartServerWorldTime)
	,StartWorldTime(Other.StartWorldTime)
	,bIsInhibited(Other.bIsInhibited)
	,bPendingRepOnActiveGC(Other.bPendingRepOnActiveGC)
	,bPendingRepWhileActiveGC(Other.bPendingRepWhileActiveGC)
	,IsPendingRemove(Other.IsPendingRemove)
	,ClientCachedStackCount(0)
	,OnRemovedDelegate(Other.OnRemovedDelegate)
	,PeriodHandle(Other.PeriodHandle)
	,DurationHandle(Other.DurationHandle)
{

	ReplicationID = Other.ReplicationID;
	ReplicationKey = Other.ReplicationKey;

	// Note: purposefully not copying PendingNext pointer.
}

FActiveDNAEffect& FActiveDNAEffect::operator=(FActiveDNAEffect&& Other)
{
	Handle = Other.Handle;
	Spec = MoveTemp(Other.Spec);
	PredictionKey = Other.PredictionKey;
	StartServerWorldTime = Other.StartServerWorldTime;
	CachedStartServerWorldTime = Other.CachedStartServerWorldTime;
	StartWorldTime = Other.StartWorldTime;
	bIsInhibited = Other.bIsInhibited;
	bPendingRepOnActiveGC = Other.bPendingRepOnActiveGC;
	bPendingRepWhileActiveGC = Other.bPendingRepWhileActiveGC;
	IsPendingRemove = Other.IsPendingRemove;
	ClientCachedStackCount = Other.ClientCachedStackCount;
	OnRemovedDelegate = Other.OnRemovedDelegate;
	PeriodHandle = Other.PeriodHandle;
	DurationHandle = Other.DurationHandle;
	// Note: purposefully not copying PendingNext pointer.

	ReplicationID = Other.ReplicationID;
	ReplicationKey = Other.ReplicationKey;
	return *this;
}

FActiveDNAEffect& FActiveDNAEffect::operator=(const FActiveDNAEffect& Other)
{
	Handle = Other.Handle;
	Spec = Other.Spec;
	PredictionKey = Other.PredictionKey;
	StartServerWorldTime = Other.StartServerWorldTime;
	CachedStartServerWorldTime = Other.CachedStartServerWorldTime;
	StartWorldTime = Other.StartWorldTime;
	bIsInhibited = Other.bIsInhibited;
	bPendingRepOnActiveGC = Other.bPendingRepOnActiveGC;
	bPendingRepWhileActiveGC = Other.bPendingRepWhileActiveGC;
	IsPendingRemove = Other.IsPendingRemove;
	ClientCachedStackCount = Other.ClientCachedStackCount;
	OnRemovedDelegate = Other.OnRemovedDelegate;
	PeriodHandle = Other.PeriodHandle;
	DurationHandle = Other.DurationHandle;
	PendingNext = Other.PendingNext;

	ReplicationID = Other.ReplicationID;
	ReplicationKey = Other.ReplicationKey;
	return *this;
}

/** This is the core function that turns the ActiveGE 'on' or 'off */
void FActiveDNAEffect::CheckOngoingTagRequirements(const FDNATagContainer& OwnerTags, FActiveDNAEffectsContainer& OwningContainer, bool bInvokeDNACueEvents)
{
	bool bShouldBeInhibited = !Spec.Def->OngoingTagRequirements.RequirementsMet(OwnerTags);

	if (bIsInhibited != bShouldBeInhibited)
	{
		// All OnDirty callbacks must be inhibited until we update this entire DNAEffect.
		FScopedAggregatorOnDirtyBatch	AggregatorOnDirtyBatcher;

		// Important to set this prior to adding or removing, so that any delegates that are triggered can query accurately against this GE
		bIsInhibited = bShouldBeInhibited;

		if (bShouldBeInhibited)
		{
			// Remove our ActiveDNAEffects modifiers with our Attribute Aggregators
			OwningContainer.RemoveActiveDNAEffectGrantedTagsAndModifiers(*this, bInvokeDNACueEvents);
		}
		else
		{
			OwningContainer.AddActiveDNAEffectGrantedTagsAndModifiers(*this, bInvokeDNACueEvents);
		}
	}
}

void FActiveDNAEffect::PreReplicatedRemove(const struct FActiveDNAEffectsContainer &InArray)
{
	if (Spec.Def == nullptr)
	{
		ABILITY_LOG(Error, TEXT("Received PreReplicatedRemove with no UDNAEffect def."));
		return;
	}

	ABILITY_LOG(Verbose, TEXT("PreReplicatedRemove: %s %s Marked as Pending Remove: %s"), *Handle.ToString(), *Spec.Def->GetName(), IsPendingRemove ? TEXT("TRUE") : TEXT("FALSE"));

	const_cast<FActiveDNAEffectsContainer&>(InArray).InternalOnActiveDNAEffectRemoved(*this, !bIsInhibited);	// Const cast is ok. It is there to prevent mutation of the DNAEffects array, which this wont do.
}

void FActiveDNAEffect::PostReplicatedAdd(const struct FActiveDNAEffectsContainer &InArray)
{
	if (Spec.Def == nullptr)
	{
		ABILITY_LOG(Error, TEXT("Received ReplicatedDNAEffect with no UDNAEffect def."));
		return;
	}

	if (Spec.Def->Modifiers.Num() != Spec.Modifiers.Num())
	{
		// This can happen with older replays, where the replicated Spec.Modifiers size changed in the newer Spec.Def
		ABILITY_LOG(Error, TEXT("FActiveDNAEffect::PostReplicatedAdd: Spec.Def->Modifiers.Num() != Spec.Modifiers.Num()"));
		return;
	}

	bool ShouldInvokeDNACueEvents = true;
	if (PredictionKey.IsLocalClientKey())
	{
		// PredictionKey will only be valid on the client that predicted it. So if this has a valid PredictionKey, we can assume we already predicted it and shouldn't invoke DNACues.
		// We may need to do more bookkeeping here in the future. Possibly give the predicted DNAeffect a chance to pass something off to the new replicated DNA effect.
		
		if (InArray.HasPredictedEffectWithPredictedKey(PredictionKey))
		{
			ShouldInvokeDNACueEvents = false;
		}
	}

	// Adjust start time for local clock
	{
		static const float MAX_DELTA_TIME = 3.f;

		// Was this actually just activated, or are we just finding out about it due to relevancy/join in progress?
		float WorldTimeSeconds = InArray.GetWorldTime();
		float ServerWorldTime = InArray.GetServerWorldTime();

		float DeltaServerWorldTime = ServerWorldTime - StartServerWorldTime;	// How long we think the effect has been playing

		// Set our local start time accordingly
		StartWorldTime = WorldTimeSeconds - DeltaServerWorldTime;
		CachedStartServerWorldTime = StartServerWorldTime;

		// Determine if we should invoke the OnActive DNACue event
		if (ShouldInvokeDNACueEvents)
		{
			// These events will get invoked if, after the parent array has been completely updated, this GE is still not inhibited
			bPendingRepOnActiveGC = (ServerWorldTime > 0 && FMath::Abs(DeltaServerWorldTime) < MAX_DELTA_TIME);
			bPendingRepWhileActiveGC = true;
		}
	}

	// Cache off StackCount
	ClientCachedStackCount = Spec.StackCount;

	// Handles are not replicated, so create a new one.
	Handle = FActiveDNAEffectHandle::GenerateNewHandle(InArray.Owner);

	// Do stuff for adding GEs (add mods, tags, *invoke callbacks*
	const_cast<FActiveDNAEffectsContainer&>(InArray).InternalOnActiveDNAEffectAdded(*this);	// Const cast is ok. It is there to prevent mutation of the DNAEffects array, which this wont do.
	
}

void FActiveDNAEffect::PostReplicatedChange(const struct FActiveDNAEffectsContainer &InArray)
{
	if (Spec.Def == nullptr)
	{
		ABILITY_LOG(Error, TEXT("Received ReplicatedDNAEffect with no UDNAEffect def."));
		return;
	}

	if (Spec.Def->Modifiers.Num() != Spec.Modifiers.Num())
	{
		// This can happen with older replays, where the replicated Spec.Modifiers size changed in the newer Spec.Def
		ABILITY_LOG(Error, TEXT("FActiveDNAEffect::PostReplicatedChange: Spec.Def->Modifiers.Num() != Spec.Modifiers.Num()"));
		return;
	}

	// Handle potential duration refresh
	if (CachedStartServerWorldTime != StartServerWorldTime)
	{
		StartWorldTime = InArray.GetWorldTime() - static_cast<float>(InArray.GetServerWorldTime() - StartServerWorldTime);
		CachedStartServerWorldTime = StartServerWorldTime;

		const_cast<FActiveDNAEffectsContainer&>(InArray).OnDurationChange(*this);
	}
	
	if (ClientCachedStackCount != Spec.StackCount)
	{
		// If its a stack count change, we just call OnStackCountChange and it will broadcast delegates and update attribute aggregators
		// Const cast is ok. It is there to prevent mutation of the DNAEffects array, which this wont do.
		const_cast<FActiveDNAEffectsContainer&>(InArray).OnStackCountChange(*this, ClientCachedStackCount, Spec.StackCount);
		ClientCachedStackCount = Spec.StackCount;
	}
	else
	{
		// Stack count didn't change, but something did (like a modifier magnitude). We need to update our attribute aggregators
		// Const cast is ok. It is there to prevent mutation of the DNAEffects array, which this wont do.
		const_cast<FActiveDNAEffectsContainer&>(InArray).UpdateAllAggregatorModMagnitudes(*this);
	}
}

void FActiveDNAEffect::RecomputeStartWorldTime(const FActiveDNAEffectsContainer& InArray)
{
	StartWorldTime = InArray.GetWorldTime() - static_cast<float>(InArray.GetServerWorldTime() - StartServerWorldTime);
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	FActiveDNAEffectsContainer
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

FActiveDNAEffectsContainer::FActiveDNAEffectsContainer()
	: Owner(nullptr)
	, OwnerIsNetAuthority(false)
	, ScopedLockCount(0)
	, PendingRemoves(0)
	, PendingDNAEffectHead(nullptr)
{
	PendingDNAEffectNext = &PendingDNAEffectHead;
}

FActiveDNAEffectsContainer::~FActiveDNAEffectsContainer()
{
	FActiveDNAEffect* PendingDNAEffect = PendingDNAEffectHead;
	if (PendingDNAEffectHead)
	{
		FActiveDNAEffect* Next = PendingDNAEffectHead->PendingNext;
		delete PendingDNAEffectHead;
		PendingDNAEffectHead =	Next;
	}
}

void FActiveDNAEffectsContainer::RegisterWithOwner(UDNAAbilitySystemComponent* InOwner)
{
	if (Owner != InOwner)
	{
		Owner = InOwner;
		OwnerIsNetAuthority = Owner->IsOwnerActorAuthoritative();

		// Binding raw is ok here, since the owner is literally the UObject that owns us. If we are destroyed, its because that uobject is destroyed,
		// and if that is destroyed, the delegate wont be able to fire.
		Owner->RegisterGenericDNATagEvent().AddRaw(this, &FActiveDNAEffectsContainer::OnOwnerTagChange);
	}
}

/** This is the main function that executes a DNAEffect on Attributes and ActiveDNAEffects */
void FActiveDNAEffectsContainer::ExecuteActiveEffectsFrom(FDNAEffectSpec &Spec, FPredictionKey PredictionKey)
{
	FDNAEffectSpec& SpecToUse = Spec;

	// Capture our own tags.
	// TODO: We should only capture them if we need to. We may have snapshotted target tags (?) (in the case of dots with exotic setups?)

	SpecToUse.CapturedTargetTags.GetActorTags().Reset();
	Owner->GetOwnedDNATags(SpecToUse.CapturedTargetTags.GetActorTags());

	SpecToUse.CalculateModifierMagnitudes();

	// ------------------------------------------------------
	//	Modifiers
	//		These will modify the base value of attributes
	// ------------------------------------------------------
	
	bool ModifierSuccessfullyExecuted = false;

	for (int32 ModIdx = 0; ModIdx < SpecToUse.Modifiers.Num(); ++ModIdx)
	{
		const FDNAModifierInfo& ModDef = SpecToUse.Def->Modifiers[ModIdx];
		
		FDNAModifierEvaluatedData EvalData(ModDef.Attribute, ModDef.ModifierOp, SpecToUse.GetModifierMagnitude(ModIdx, true));
		ModifierSuccessfullyExecuted |= InternalExecuteMod(SpecToUse, EvalData);
	}

	// ------------------------------------------------------
	//	Executions
	//		This will run custom code to 'do stuff'
	// ------------------------------------------------------
	
	TArray< FDNAEffectSpecHandle, TInlineAllocator<4> > ConditionalEffectSpecs;

	bool DNACuesWereManuallyHandled = false;

	for (const FDNAEffectExecutionDefinition& CurExecDef : SpecToUse.Def->Executions)
	{
		bool bRunConditionalEffects = true; // Default to true if there is no CalculationClass specified.

		if (CurExecDef.CalculationClass)
		{
			const UDNAEffectExecutionCalculation* ExecCDO = CurExecDef.CalculationClass->GetDefaultObject<UDNAEffectExecutionCalculation>();
			check(ExecCDO);

			// Run the custom execution
			FDNAEffectCustomExecutionParameters ExecutionParams(SpecToUse, CurExecDef.CalculationModifiers, Owner, CurExecDef.PassedInTags, PredictionKey);
			FDNAEffectCustomExecutionOutput ExecutionOutput;
			ExecCDO->Execute(ExecutionParams, ExecutionOutput);

			bRunConditionalEffects = ExecutionOutput.ShouldTriggerConditionalDNAEffects();

			// Execute any mods the custom execution yielded
			TArray<FDNAModifierEvaluatedData>& OutModifiers = ExecutionOutput.GetOutputModifiersRef();

			const bool bApplyStackCountToEmittedMods = !ExecutionOutput.IsStackCountHandledManually();
			const int32 SpecStackCount = SpecToUse.StackCount;

			for (FDNAModifierEvaluatedData& CurExecMod : OutModifiers)
			{
				// If the execution didn't manually handle the stack count, automatically apply it here
				if (bApplyStackCountToEmittedMods && SpecStackCount > 1)
				{
					CurExecMod.Magnitude = DNAEffectUtilities::ComputeStackedModifierMagnitude(CurExecMod.Magnitude, SpecStackCount, CurExecMod.ModifierOp);
				}
				ModifierSuccessfullyExecuted |= InternalExecuteMod(SpecToUse, CurExecMod);
			}

			// If execution handled DNACues, we dont have to.
			if (ExecutionOutput.AreDNACuesHandledManually())
			{
				DNACuesWereManuallyHandled = true;
			}
		}

		if (bRunConditionalEffects)
		{
			// If successful, apply conditional specs
			for (const FConditionalDNAEffect& ConditionalEffect : CurExecDef.ConditionalDNAEffects)
			{
				if (ConditionalEffect.CanApply(SpecToUse.CapturedSourceTags.GetActorTags(), SpecToUse.GetLevel()))
				{
					FDNAEffectSpecHandle SpecHandle = ConditionalEffect.CreateSpec(SpecToUse.GetEffectContext(), SpecToUse.GetLevel());
					if (SpecHandle.IsValid())
					{
						ConditionalEffectSpecs.Add(SpecHandle);
					}
				}
			}
		}
	}

	// ------------------------------------------------------
	//	Invoke DNACue events
	// ------------------------------------------------------
	
	// If there are no modifiers or we don't require modifier success to trigger, we apply the DNACue. 
	bool InvokeDNACueExecute = (SpecToUse.Modifiers.Num() == 0) || !Spec.Def->bRequireModifierSuccessToTriggerCues;

	// If there are modifiers, we only want to invoke the DNACue if one of them went through (could be blocked by immunity or % chance roll)
	if (SpecToUse.Modifiers.Num() > 0 && ModifierSuccessfullyExecuted)
	{
		InvokeDNACueExecute = true;
	}

	// Don't trigger DNA cues if one of the executions says it manually handled them
	if (DNACuesWereManuallyHandled)
	{
		InvokeDNACueExecute = false;
	}

	if (InvokeDNACueExecute && SpecToUse.Def->DNACues.Num())
	{
		// TODO: check replication policy. Right now we will replicate every execute via a multicast RPC

		ABILITY_LOG(Log, TEXT("Invoking Execute DNACue for %s"), *SpecToUse.ToSimpleString());

		UDNAAbilitySystemGlobals::Get().GetDNACueManager()->InvokeDNACueExecuted_FromSpec(Owner, SpecToUse, PredictionKey);
	}

	// Apply any conditional linked effects

	for (const FDNAEffectSpecHandle TargetSpec : ConditionalEffectSpecs)
	{
		if (TargetSpec.IsValid())
		{
			Owner->ApplyDNAEffectSpecToSelf(*TargetSpec.Data.Get(), PredictionKey);
		}
	}
}

void FActiveDNAEffectsContainer::ExecutePeriodicDNAEffect(FActiveDNAEffectHandle Handle)
{
	DNAEFFECT_SCOPE_LOCK();
	FActiveDNAEffect* ActiveEffect = GetActiveDNAEffect(Handle);
	if (ActiveEffect && !ActiveEffect->bIsInhibited)
	{
		if (UE_LOG_ACTIVE(VLogDNAAbilitySystem, Log))
		{
			ABILITY_VLOG(Owner->OwnerActor, Log, TEXT("Executed Periodic Effect %s"), *ActiveEffect->Spec.Def->GetFName().ToString());
			for (FDNAModifierInfo Modifier : ActiveEffect->Spec.Def->Modifiers)
			{
				float Magnitude = 0.f;
				Modifier.ModifierMagnitude.AttemptCalculateMagnitude(ActiveEffect->Spec, Magnitude);
				ABILITY_VLOG(Owner->OwnerActor, Log, TEXT("         %s: %s %f"), *Modifier.Attribute.GetName(), *EDNAModOpToString(Modifier.ModifierOp), Magnitude);
			}
		}

		// Clear modified attributes before each periodic execution
		ActiveEffect->Spec.ModifiedAttributes.Empty();

		// Execute
		ExecuteActiveEffectsFrom(ActiveEffect->Spec);

		// Invoke Delegates for periodic effects being executed
		UDNAAbilitySystemComponent* SourceASC = ActiveEffect->Spec.GetContext().GetInstigatorDNAAbilitySystemComponent();
		Owner->OnPeriodicDNAEffectExecuteOnSelf(SourceASC, ActiveEffect->Spec, Handle);
		if (SourceASC)
		{
			SourceASC->OnPeriodicDNAEffectExecuteOnTarget(Owner, ActiveEffect->Spec, Handle);
		}
	}

}

FActiveDNAEffect* FActiveDNAEffectsContainer::GetActiveDNAEffect(const FActiveDNAEffectHandle Handle)
{
	for (FActiveDNAEffect& Effect : this)
	{
		if (Effect.Handle == Handle)
		{
			return &Effect;
		}
	}
	return nullptr;
}

const FActiveDNAEffect* FActiveDNAEffectsContainer::GetActiveDNAEffect(const FActiveDNAEffectHandle Handle) const
{
	for (const FActiveDNAEffect& Effect : this)
	{
		if (Effect.Handle == Handle)
		{
			return &Effect;
		}
	}
	return nullptr;
}

FAggregatorRef& FActiveDNAEffectsContainer::FindOrCreateAttributeAggregator(FDNAAttribute Attribute)
{
	FAggregatorRef* RefPtr = AttributeAggregatorMap.Find(Attribute);
	if (RefPtr)
	{
		return *RefPtr;
	}

	// Create a new aggregator for this attribute.
	float CurrentBaseValueOfProperty = Owner->GetNumericAttributeBase(Attribute);
	ABILITY_LOG(Log, TEXT("Creating new entry in AttributeAggregatorMap for %s. CurrentValue: %.2f"), *Attribute.GetName(), CurrentBaseValueOfProperty);

	FAggregator* NewAttributeAggregator = new FAggregator(CurrentBaseValueOfProperty);
	
	if (Attribute.IsSystemAttribute() == false)
	{
		NewAttributeAggregator->OnDirty.AddUObject(Owner, &UDNAAbilitySystemComponent::OnAttributeAggregatorDirty, Attribute);
	}

	return AttributeAggregatorMap.Add(Attribute, FAggregatorRef(NewAttributeAggregator));
}

void FActiveDNAEffectsContainer::OnAttributeAggregatorDirty(FAggregator* Aggregator, FDNAAttribute Attribute)
{
	check(AttributeAggregatorMap.FindChecked(Attribute).Get() == Aggregator);

	// Our Aggregator has changed, we need to reevaluate this aggregator and update the current value of the attribute.
	// Note that this is not an execution, so there are no 'source' and 'target' tags to fill out in the FAggregatorEvaluateParameters.
	// ActiveDNAEffects that have required owned tags will be turned on/off via delegates, and will add/remove themselves from attribute
	// aggregators when that happens.
	
	FAggregatorEvaluateParameters EvaluationParameters;

	if (Owner->IsNetSimulating())
	{
		if (FScopedAggregatorOnDirtyBatch::GlobalFromNetworkUpdate && Aggregator->NetUpdateID != FScopedAggregatorOnDirtyBatch::NetUpdateID)
		{
			// We are a client. The current value of this attribute is the replicated server's "final" value. We dont actually know what the 
			// server's base value is. But we can calculate it with ReverseEvaluate(). Then, we can call Evaluate with IncludePredictiveMods=true
			// to apply our mods and get an accurate predicted value.
			// 
			// It is very important that we only do this exactly one time when we get a new value from the server. Once we set the new local value for this
			// attribute below, recalculating the base would give us the wrong server value. We should only do this when we are coming directly from a network update.
			// 
			// Unfortunately there are two ways we could get here from a network update: from the ActiveDNAEffect container being updated or from a traditional
			// OnRep on the actual attribute uproperty. Both of these could happen in a single network update, or potentially only one could happen
			// (and in fact it could be either one! the AGE container could change in a way that doesnt change the final attribute value, or we could have the base value
			// of the attribute actually be modified (e.g,. losing health or mana which only results in an OnRep and not in a AGE being applied).
			// 
			// So both paths need to lead to this function, but we should only do it one time per update. Once we update the base value, we need to make sure we dont do it again
			// until we get a new network update. GlobalFromNetworkUpdate and NetUpdateID are what do this.
			// 
			// GlobalFromNetworkUpdate - only set to true when we are coming from an OnRep or when we are coming from an ActiveDNAEffect container net update.
			// NetUpdateID - updated once whenever an AttributeSet is received over the network. It will be incremented one time per actor that gets an update.
					
			float BaseValue = 0.f;
			if (!FDNAAttribute::IsDNAAttributeDataProperty(Attribute.GetUProperty()))
			{
				// Legacy float attribute case requires the base value to be deduced from the final value, as it is not replicated
				float FinalValue = Owner->GetNumericAttribute(Attribute);
				BaseValue = Aggregator->ReverseEvaluate(FinalValue, EvaluationParameters);
				ABILITY_LOG(Log, TEXT("Reverse Evaluated %s. FinalValue: %.2f  BaseValue: %.2f "), *Attribute.GetName(), FinalValue, BaseValue);
			}
			else
			{
				BaseValue = Owner->GetNumericAttributeBase(Attribute);
			}
			
			Aggregator->SetBaseValue(BaseValue, false);
			Aggregator->NetUpdateID = FScopedAggregatorOnDirtyBatch::NetUpdateID;
		}

		EvaluationParameters.IncludePredictiveMods = true;
	}

	float NewValue = Aggregator->Evaluate(EvaluationParameters);

	if (EvaluationParameters.IncludePredictiveMods)
	{
		ABILITY_LOG(Log, TEXT("After Prediction, FinalValue: %.2f"), NewValue);
	}

	InternalUpdateNumericalAttribute(Attribute, NewValue, nullptr);
}

void FActiveDNAEffectsContainer::OnMagnitudeDependencyChange(FActiveDNAEffectHandle Handle, const FAggregator* ChangedAgg)
{
	if (Handle.IsValid())
	{
		DNAEFFECT_SCOPE_LOCK();
		FActiveDNAEffect* ActiveEffect = GetActiveDNAEffect(Handle);
		if (ActiveEffect)
		{
			// This handle registered with the ChangedAgg to be notified when the aggregator changed.
			// At this point we don't know what actually needs to be updated inside this active DNA effect.
			FDNAEffectSpec& Spec = ActiveEffect->Spec;

			// We must update attribute aggregators only if we are actually 'on' right now, and if we are non periodic (periodic effects do their thing on execute callbacks)
			const bool MustUpdateAttributeAggregators = (ActiveEffect->bIsInhibited == false && (Spec.GetPeriod() <= UDNAEffect::NO_PERIOD));

			// As we update our modifier magnitudes, we will update our owner's attribute aggregators. When we do this, we have to clear them first of all of our (Handle's) previous mods.
			// Since we could potentially have two mods to the same attribute, one that gets updated, and one that doesnt - we need to do this in two passes.
			TSet<FDNAAttribute> AttributesToUpdate;

			bool bMarkedDirty = false;

			// First pass: update magnitudes of our modifiers that changed
			for(int32 ModIdx = 0; ModIdx < Spec.Modifiers.Num(); ++ModIdx)
			{
				const FDNAModifierInfo& ModDef = Spec.Def->Modifiers[ModIdx];
				FModifierSpec& ModSpec = Spec.Modifiers[ModIdx];

				float RecalculatedMagnitude = 0.f;
				if (ModDef.ModifierMagnitude.AttemptRecalculateMagnitudeFromDependentAggregatorChange(Spec, RecalculatedMagnitude, ChangedAgg))
				{
					// If this is the first pending magnitude change, need to mark the container item dirty as well as
					// wake the owner actor from dormancy so replication works properly
					if (!bMarkedDirty)
					{
						bMarkedDirty = true;
						if (Owner && Owner->OwnerActor && IsNetAuthority())
						{
							Owner->OwnerActor->FlushNetDormancy();
						}
						MarkItemDirty(*ActiveEffect);
					}

					ModSpec.EvaluatedMagnitude = RecalculatedMagnitude;

					// We changed, so we need to reapply/update our spot in the attribute aggregator map
					if (MustUpdateAttributeAggregators)
					{
						AttributesToUpdate.Add(ModDef.Attribute);
					}
				}
			}

			// Second pass, update the aggregators that we need to
			UpdateAggregatorModMagnitudes(AttributesToUpdate, *ActiveEffect);
		}
	}
}

void FActiveDNAEffectsContainer::OnStackCountChange(FActiveDNAEffect& ActiveEffect, int32 OldStackCount, int32 NewStackCount)
{
	MarkItemDirty(ActiveEffect);
	if (OldStackCount != NewStackCount)
	{
		// Only update attributes if stack count actually changed.
		UpdateAllAggregatorModMagnitudes(ActiveEffect);
	}

	if (ActiveEffect.Spec.Def != nullptr)
	{
		Owner->NotifyTagMap_StackCountChange(ActiveEffect.Spec.Def->InheritableOwnedTagsContainer.CombinedTags);
	}

	Owner->NotifyTagMap_StackCountChange(ActiveEffect.Spec.DynamicGrantedTags);

	ActiveEffect.OnStackChangeDelegate.Broadcast(ActiveEffect.Handle, ActiveEffect.Spec.StackCount, OldStackCount);
}

/** Called when the duration or starttime of an AGE has changed */
void FActiveDNAEffectsContainer::OnDurationChange(FActiveDNAEffect& Effect)
{
	Effect.OnTimeChangeDelegate.Broadcast(Effect.Handle, Effect.StartWorldTime, Effect.GetDuration());
	Owner->OnDNAEffectDurationChange(Effect);
}

void FActiveDNAEffectsContainer::UpdateAllAggregatorModMagnitudes(FActiveDNAEffect& ActiveEffect)
{
	// We should never be doing this for periodic effects since their mods are not persistent on attribute aggregators
	if (ActiveEffect.Spec.GetPeriod() > UDNAEffect::NO_PERIOD)
	{
		return;
	}

	// we don't need to update inhibited effects
	if (ActiveEffect.bIsInhibited)
	{
		return;
	}

	const FDNAEffectSpec& Spec = ActiveEffect.Spec;

	if (Spec.Def == nullptr)
	{
		ABILITY_LOG(Error, TEXT("UpdateAllAggregatorModMagnitudes called with no UDNAEffect def."));
		return;
	}

	TSet<FDNAAttribute> AttributesToUpdate;

	for (int32 ModIdx = 0; ModIdx < Spec.Modifiers.Num(); ++ModIdx)
	{
		const FDNAModifierInfo& ModDef = Spec.Def->Modifiers[ModIdx];
		AttributesToUpdate.Add(ModDef.Attribute);
	}

	UpdateAggregatorModMagnitudes(AttributesToUpdate, ActiveEffect);
}

void FActiveDNAEffectsContainer::UpdateAggregatorModMagnitudes(const TSet<FDNAAttribute>& AttributesToUpdate, FActiveDNAEffect& ActiveEffect)
{
	const FDNAEffectSpec& Spec = ActiveEffect.Spec;
	for (const FDNAAttribute& Attribute : AttributesToUpdate)
	{
		// skip over any modifiers for attributes that we don't have
		if (!Owner || Owner->HasAttributeSetForAttribute(Attribute) == false)
		{
			continue;
		}

		FAggregator* Aggregator = FindOrCreateAttributeAggregator(Attribute).Get();
		check(Aggregator);

		// Update the aggregator Mods.
		Aggregator->UpdateAggregatorMod(ActiveEffect.Handle, Attribute, Spec, ActiveEffect.PredictionKey.WasLocallyGenerated(), ActiveEffect.Handle);
	}
}

FActiveDNAEffect* FActiveDNAEffectsContainer::FindStackableActiveDNAEffect(const FDNAEffectSpec& Spec)
{
	FActiveDNAEffect* StackableGE = nullptr;
	const UDNAEffect* GEDef = Spec.Def;
	EDNAEffectStackingType StackingType = GEDef->StackingType;

	if (StackingType != EDNAEffectStackingType::None && Spec.GetDuration() != UDNAEffect::INSTANT_APPLICATION)
	{
		// Iterate through DNAEffects to see if we find a match. Note that we could cache off a handle in a map but we would still
		// do a linear search through DNAEffects to find the actual FActiveDNAEffect (due to unstable nature of the DNAEffects array).
		// If this becomes a slow point in the profiler, the map may still be useful as an early out to avoid an unnecessary sweep.
		UDNAAbilitySystemComponent* SourceASC = Spec.GetContext().GetInstigatorDNAAbilitySystemComponent();
		for (FActiveDNAEffect& ActiveEffect: this)
		{
			// Aggregate by source stacking additionally requires the source ability component to match
			if (ActiveEffect.Spec.Def == Spec.Def && ((StackingType == EDNAEffectStackingType::AggregateByTarget) || (SourceASC && SourceASC == ActiveEffect.Spec.GetContext().GetInstigatorDNAAbilitySystemComponent())))
			{
				StackableGE = &ActiveEffect;
				break;
			}
		}
	}

	return StackableGE;
}

bool FActiveDNAEffectsContainer::HandleActiveDNAEffectStackOverflow(const FActiveDNAEffect& ActiveStackableGE, const FDNAEffectSpec& OldSpec, const FDNAEffectSpec& OverflowingSpec)
{
	const UDNAEffect* StackedGE = OldSpec.Def;
	
	const bool bAllowOverflowApplication = !(StackedGE->bDenyOverflowApplication);

	FPredictionKey PredictionKey;
	for (TSubclassOf<UDNAEffect> OverflowEffect : StackedGE->OverflowEffects)
	{
		if (OverflowEffect)
		{
			FDNAEffectSpec NewGESpec(OverflowEffect->GetDefaultObject<UDNAEffect>(), OverflowingSpec.GetContext(), OverflowingSpec.GetLevel());
			// @todo: copy over source tags
			// @todo: scope lock
			Owner->ApplyDNAEffectSpecToSelf(NewGESpec, PredictionKey);
		}
	}
	// @todo: Scope lock
	if (!bAllowOverflowApplication && StackedGE->bClearStackOnOverflow)
	{
		Owner->RemoveActiveDNAEffect(ActiveStackableGE.Handle);
	}

	return bAllowOverflowApplication;
}

bool FActiveDNAEffectsContainer::ShouldUseMinimalReplication()
{
	return IsNetAuthority() && (Owner->ReplicationMode == EReplicationMode::Minimal || Owner->ReplicationMode == EReplicationMode::Mixed);
}

void FActiveDNAEffectsContainer::SetBaseAttributeValueFromReplication(FDNAAttribute Attribute, float ServerValue)
{
	FAggregatorRef* RefPtr = AttributeAggregatorMap.Find(Attribute);
	if (RefPtr && RefPtr->Get())
	{
		FAggregator* Aggregator = RefPtr->Get();
		
		FScopedAggregatorOnDirtyBatch::GlobalFromNetworkUpdate = true;
		OnAttributeAggregatorDirty(Aggregator, Attribute);
		FScopedAggregatorOnDirtyBatch::GlobalFromNetworkUpdate = false;
	}
	else
	{
		// No aggregators on the client but still broadcast the dirty delegate
		if (FOnDNAAttributeChange* Delegate = AttributeChangeDelegates.Find(Attribute))
		{
			Delegate->Broadcast(ServerValue, nullptr);
		}
	}
}

void FActiveDNAEffectsContainer::GetAllActiveDNAEffectSpecs(TArray<FDNAEffectSpec>& OutSpecCopies) const
{
	for (const FActiveDNAEffect& ActiveEffect : this)	
	{
		OutSpecCopies.Add(ActiveEffect.Spec);
	}
}

void FActiveDNAEffectsContainer::GetDNAEffectStartTimeAndDuration(FActiveDNAEffectHandle Handle, float& EffectStartTime, float& EffectDuration) const
{
	EffectStartTime = UDNAEffect::INFINITE_DURATION;
	EffectDuration = UDNAEffect::INFINITE_DURATION;

	if (Handle.IsValid())
	{
		for (const FActiveDNAEffect& ActiveEffect : this)
		{
			if (ActiveEffect.Handle == Handle)
			{
				EffectStartTime = ActiveEffect.StartWorldTime;
				EffectDuration = ActiveEffect.GetDuration();
				return;
			}
		}
	}

	ABILITY_LOG(Warning, TEXT("GetDNAEffectStartTimeAndDuration called with invalid Handle: %s"), *Handle.ToString());
}

float FActiveDNAEffectsContainer::GetDNAEffectMagnitude(FActiveDNAEffectHandle Handle, FDNAAttribute Attribute) const
{
	for (const FActiveDNAEffect& Effect : this)
	{
		if (Effect.Handle == Handle)
		{
			for(int32 ModIdx = 0; ModIdx < Effect.Spec.Modifiers.Num(); ++ModIdx)
			{
				const FDNAModifierInfo& ModDef = Effect.Spec.Def->Modifiers[ModIdx];
				const FModifierSpec& ModSpec = Effect.Spec.Modifiers[ModIdx];
			
				if (ModDef.Attribute == Attribute)
				{
					return ModSpec.GetEvaluatedMagnitude();
				}
			}
		}
	}

	ABILITY_LOG(Warning, TEXT("GetDNAEffectMagnitude called with invalid Handle: %s"), *Handle.ToString());
	return -1.f;
}

void FActiveDNAEffectsContainer::SetActiveDNAEffectLevel(FActiveDNAEffectHandle ActiveHandle, int32 NewLevel)
{
	for (FActiveDNAEffect& Effect : this)
	{
		if (Effect.Handle == ActiveHandle)
		{
			Effect.Spec.SetLevel(NewLevel);
			MarkItemDirty(Effect);
			Effect.Spec.CalculateModifierMagnitudes();
			UpdateAllAggregatorModMagnitudes(Effect);
			break;
		}
	}
}

const FDNATagContainer* FActiveDNAEffectsContainer::GetDNAEffectSourceTagsFromHandle(FActiveDNAEffectHandle Handle) const
{
	// @todo: Need to consider this with tag changes
	for (const FActiveDNAEffect& Effect : this)
	{
		if (Effect.Handle == Handle)
		{
			return Effect.Spec.CapturedSourceTags.GetAggregatedTags();
		}
	}

	return nullptr;
}

const FDNATagContainer* FActiveDNAEffectsContainer::GetDNAEffectTargetTagsFromHandle(FActiveDNAEffectHandle Handle) const
{
	// @todo: Need to consider this with tag changes
	const FActiveDNAEffect* Effect = GetActiveDNAEffect(Handle);
	if (Effect)
	{
		return Effect->Spec.CapturedTargetTags.GetAggregatedTags();
	}

	return nullptr;
}

void FActiveDNAEffectsContainer::CaptureAttributeForDNAEffect(OUT FDNAEffectAttributeCaptureSpec& OutCaptureSpec)
{
	FAggregatorRef& AttributeAggregator = FindOrCreateAttributeAggregator(OutCaptureSpec.BackingDefinition.AttributeToCapture);
	
	if (OutCaptureSpec.BackingDefinition.bSnapshot)
	{
		OutCaptureSpec.AttributeAggregator.TakeSnapshotOf(AttributeAggregator);
	}
	else
	{
		OutCaptureSpec.AttributeAggregator = AttributeAggregator;
	}
}

void FActiveDNAEffectsContainer::InternalUpdateNumericalAttribute(FDNAAttribute Attribute, float NewValue, const FDNAEffectModCallbackData* ModData)
{
	ABILITY_LOG(Log, TEXT("Property %s new value is: %.2f"), *Attribute.GetName(), NewValue);
	Owner->SetNumericAttribute_Internal(Attribute, NewValue);

	FOnDNAAttributeChange* Delegate = AttributeChangeDelegates.Find(Attribute);
	if (Delegate)
	{
		// We should only have one: either cached CurrentModcallbackData, or explicit callback data passed directly in.
		if (ModData && CurrentModcallbackData)
		{
			ABILITY_LOG(Warning, TEXT("Had passed in ModData and cached CurrentModcallbackData in FActiveDNAEffectsContainer::InternalUpdateNumericalAttribute. For attribute %s on %s."), *Attribute.GetName(), *Owner->GetFullName() );
		}

		// Broadcast dirty delegate. If we were given explicit mod data then pass it. 
		Delegate->Broadcast(NewValue, ModData ? ModData : CurrentModcallbackData);
	}
	CurrentModcallbackData = nullptr;
}

void FActiveDNAEffectsContainer::SetAttributeBaseValue(FDNAAttribute Attribute, float NewBaseValue)
{
	// if we're using the new attributes we should always update their base value
	bool bIsDNAAttributeDataProperty = FDNAAttribute::IsDNAAttributeDataProperty(Attribute.GetUProperty());
	if (bIsDNAAttributeDataProperty)
	{
		const UStructProperty* StructProperty = Cast<UStructProperty>(Attribute.GetUProperty());
		check(StructProperty);
		UAttributeSet* AttributeSet = const_cast<UAttributeSet*>(Owner->GetAttributeSubobject(Attribute.GetAttributeSetClass()));
		ensure(AttributeSet);
		FDNAAttributeData* DataPtr = StructProperty->ContainerPtrToValuePtr<FDNAAttributeData>(AttributeSet);
		if (ensure(DataPtr))
		{
			DataPtr->SetBaseValue(NewBaseValue);
		}
	}

	FAggregatorRef* RefPtr = AttributeAggregatorMap.Find(Attribute);
	if (RefPtr)
	{
		// There is an aggregator for this attribute, so set the base value. The dirty callback chain
		// will update the actual AttributeSet property value for us.

		const UAttributeSet* Set = Owner->GetAttributeSubobject(Attribute.GetAttributeSetClass());
		check(Set);

		Set->PreAttributeBaseChange(Attribute, NewBaseValue);
		RefPtr->Get()->SetBaseValue(NewBaseValue);
	}
	// if there is no aggregator set the current value (base == current in this case)
	else
	{
		InternalUpdateNumericalAttribute(Attribute, NewBaseValue, nullptr);
	}
}

float FActiveDNAEffectsContainer::GetAttributeBaseValue(FDNAAttribute Attribute) const
{
	float BaseValue = 0.f;
	const FAggregatorRef* RefPtr = AttributeAggregatorMap.Find(Attribute);
	// if this attribute is of type FDNAAttributeData then use the base value stored there
	if (FDNAAttribute::IsDNAAttributeDataProperty(Attribute.GetUProperty()))
	{
		const UStructProperty* StructProperty = Cast<UStructProperty>(Attribute.GetUProperty());
		check(StructProperty);
		const UAttributeSet* AttributeSet = Owner->GetAttributeSubobject(Attribute.GetAttributeSetClass());
		ensure(AttributeSet);
		const FDNAAttributeData* DataPtr = StructProperty->ContainerPtrToValuePtr<FDNAAttributeData>(AttributeSet);
		if (DataPtr)
		{
			BaseValue = DataPtr->GetBaseValue();
		}
	}
	// otherwise, if we have an aggregator use the base value in the aggregator
	else if (RefPtr)
	{
		BaseValue = RefPtr->Get()->GetBaseValue();
	}
	// if the attribute is just a float and there is no aggregator then the base value is the current value
	else
	{
		BaseValue = Owner->GetNumericAttribute(Attribute);
	}

	return BaseValue;
}

float FActiveDNAEffectsContainer::GetEffectContribution(const FAggregatorEvaluateParameters& Parameters, FActiveDNAEffectHandle ActiveHandle, FDNAAttribute Attribute)
{
	FAggregatorRef Aggregator = FindOrCreateAttributeAggregator(Attribute);
	return Aggregator.Get()->EvaluateContribution(Parameters, ActiveHandle);
}

bool FActiveDNAEffectsContainer::InternalExecuteMod(FDNAEffectSpec& Spec, FDNAModifierEvaluatedData& ModEvalData)
{
	check(Owner);

	bool bExecuted = false;

	UAttributeSet* AttributeSet = nullptr;
	UClass* AttributeSetClass = ModEvalData.Attribute.GetAttributeSetClass();
	if (AttributeSetClass && AttributeSetClass->IsChildOf(UAttributeSet::StaticClass()))
	{
		AttributeSet = const_cast<UAttributeSet*>(Owner->GetAttributeSubobject(AttributeSetClass));
	}

	if (AttributeSet)
	{
		FDNAEffectModCallbackData ExecuteData(Spec, ModEvalData, *Owner);

		/**
		 *  This should apply 'gamewide' rules. Such as clamping Health to MaxHealth or granting +3 health for every point of strength, etc
		 *	PreAttributeModify can return false to 'throw out' this modification.
		 */
		if (AttributeSet->PreDNAEffectExecute(ExecuteData))
		{
			float OldValueOfProperty = Owner->GetNumericAttribute(ModEvalData.Attribute);
			ApplyModToAttribute(ModEvalData.Attribute, ModEvalData.ModifierOp, ModEvalData.Magnitude, &ExecuteData);

			FDNAEffectModifiedAttribute* ModifiedAttribute = Spec.GetModifiedAttribute(ModEvalData.Attribute);
			if (!ModifiedAttribute)
			{
				// If we haven't already created a modified attribute holder, create it
				ModifiedAttribute = Spec.AddModifiedAttribute(ModEvalData.Attribute);
			}
			ModifiedAttribute->TotalMagnitude += ModEvalData.Magnitude;

			/** This should apply 'gamewide' rules. Such as clamping Health to MaxHealth or granting +3 health for every point of strength, etc */
			AttributeSet->PostDNAEffectExecute(ExecuteData);

#if ENABLE_VISUAL_LOG
			DebugExecutedDNAEffectData DebugData;
			DebugData.DNAEffectName = Spec.Def->GetName();
			DebugData.ActivationState = "INSTANT";
			DebugData.Attribute = ModEvalData.Attribute;
			DebugData.Magnitude = Owner->GetNumericAttribute(ModEvalData.Attribute) - OldValueOfProperty;
			DebugExecutedDNAEffects.Add(DebugData);
#endif // ENABLE_VISUAL_LOG

			bExecuted = true;
		}
	}
	else
	{
		// Our owner doesn't have this attribute, so we can't do anything
		ABILITY_LOG(Log, TEXT("%s does not have attribute %s. Skipping modifier"), *Owner->GetPathName(), *ModEvalData.Attribute.GetName());
	}

	return bExecuted;
}

void FActiveDNAEffectsContainer::ApplyModToAttribute(const FDNAAttribute &Attribute, TEnumAsByte<EDNAModOp::Type> ModifierOp, float ModifierMagnitude, const FDNAEffectModCallbackData* ModData)
{
	CurrentModcallbackData = ModData;
	float CurrentBase = GetAttributeBaseValue(Attribute);
	float NewBase = FAggregator::StaticExecModOnBaseValue(CurrentBase, ModifierOp, ModifierMagnitude);

	SetAttributeBaseValue(Attribute, NewBase);

	if (CurrentModcallbackData)
	{
		// We expect this to be cleared for us in InternalUpdateNumericalAttribute
		ABILITY_LOG(Warning, TEXT("FActiveDNAEffectsContainer::ApplyModToAttribute CurrentModcallbackData was not consumed For attribute %s on %s."), *Attribute.GetName(), *Owner->GetFullName());
		CurrentModcallbackData = nullptr;
	}
}

FActiveDNAEffect* FActiveDNAEffectsContainer::ApplyDNAEffectSpec(const FDNAEffectSpec& Spec, FPredictionKey& InPredictionKey, bool& bFoundExistingStackableGE)
{
	SCOPE_CYCLE_COUNTER(STAT_ApplyDNAEffectSpec);

	DNAEFFECT_SCOPE_LOCK();

	bFoundExistingStackableGE = false;

	if (Owner && Owner->OwnerActor && IsNetAuthority())
	{
		Owner->OwnerActor->FlushNetDormancy();
	}

	FActiveDNAEffect* AppliedActiveGE = nullptr;
	FActiveDNAEffect* ExistingStackableGE = FindStackableActiveDNAEffect(Spec);

	bool bSetDuration = true;
	bool bSetPeriod = true;
	int32 StartingStackCount = 0;
	int32 NewStackCount = 0;

	// Check if there's an active GE this application should stack upon
	if (ExistingStackableGE)
	{
		if (!IsNetAuthority())
		{
			// Don't allow prediction of stacking for now
			return nullptr;
		}
		else
		{
			// Server invalidates the prediction key for this GE since client is not predicting it
			InPredictionKey = FPredictionKey();
		}

		bFoundExistingStackableGE = true;

		FDNAEffectSpec& ExistingSpec = ExistingStackableGE->Spec;
		StartingStackCount = ExistingSpec.StackCount;
		
		// How to apply multiple stacks at once? What if we trigger an overflow which can reject the application?
		// We still want to apply the stacks that didnt push us over, but we also want to call HandleActiveDNAEffectStackOverflow.
		
		// For now: call HandleActiveDNAEffectStackOverflow only if we are ALREADY at the limit. Else we just clamp stack limit to max.
		if (ExistingSpec.StackCount == ExistingSpec.Def->StackLimitCount)
		{
			if (!HandleActiveDNAEffectStackOverflow(*ExistingStackableGE, ExistingSpec, Spec))
			{
				return nullptr;
			}
		}
		
		NewStackCount = ExistingSpec.StackCount + Spec.StackCount;
		if (ExistingSpec.Def->StackLimitCount > 0)
		{
			NewStackCount = FMath::Min(NewStackCount, ExistingSpec.Def->StackLimitCount);
		}

		// Need to unregister callbacks because the source aggregators could potentially be different with the new application. They will be
		// re-registered later below, as necessary.
		ExistingSpec.CapturedRelevantAttributes.UnregisterLinkedAggregatorCallbacks(ExistingStackableGE->Handle);

		// @todo: If dynamically granted tags differ (which they shouldn't), we'll actually have to diff them
		// and cause a removal and add of only the ones that have changed. For now, ensure on this happening and come
		// back to this later.
		ensureMsgf(ExistingSpec.DynamicGrantedTags == Spec.DynamicGrantedTags, TEXT("While adding a stack of the DNA effect: %s, the old stack and the new application had different dynamically granted tags, which is currently not resolved properly!"), *Spec.Def->GetName());
		
		// We only grant abilities on the first apply. So we *dont* want the new spec's GrantedAbilitySpecs list
		TArray<FDNAAbilitySpecDef>	GrantedSpecTempArray(MoveTemp(ExistingStackableGE->Spec.GrantedAbilitySpecs));

		// @todo: If dynamic asset tags differ (which they shouldn't), we'll actually have to diff them
		// and cause a removal and add of only the ones that have changed. For now, ensure on this happening and come
		// back to this later.
		ensureMsgf(ExistingSpec.DynamicAssetTags == Spec.DynamicAssetTags, TEXT("While adding a stack of the DNA effect: %s, the old stack and the new application had different dynamic asset tags, which is currently not resolved properly!"), *Spec.Def->GetName());

		ExistingStackableGE->Spec = Spec;
		ExistingStackableGE->Spec.StackCount = NewStackCount;

		// Swap in old granted ability spec
		ExistingStackableGE->Spec.GrantedAbilitySpecs = MoveTemp(GrantedSpecTempArray);
		
		AppliedActiveGE = ExistingStackableGE;

		const UDNAEffect* GEDef = ExistingSpec.Def;

		// Make sure the GE actually wants to refresh its duration
		if (GEDef->StackDurationRefreshPolicy == EDNAEffectStackingDurationPolicy::NeverRefresh)
		{
			bSetDuration = false;
		}
		else
		{
			RestartActiveDNAEffectDuration(*ExistingStackableGE);
		}

		// Make sure the GE actually wants to reset its period
		if (GEDef->StackPeriodResetPolicy == EDNAEffectStackingPeriodPolicy::NeverReset)
		{
			bSetPeriod = false;
		}
	}
	else
	{
		FActiveDNAEffectHandle NewHandle = FActiveDNAEffectHandle::GenerateNewHandle(Owner);

		if (ScopedLockCount > 0 && DNAEffects_Internal.GetSlack() <= 0)
		{
			/**
			 *	If we have no more slack and we are scope locked, we need to put this addition on our pending GE list, which will be moved
			 *	onto the real active GE list once the scope lock is over.
			 *	
			 *	To avoid extra heap allocations, each active DNAeffect container keeps a linked list of pending GEs. This list is allocated
			 *	on demand and re-used in subsequent pending adds. The code below will either 1) Alloc a new pending GE 2) reuse an existing pending GE.
			 *	The move operator is used to move stuff to and from these pending GEs to avoid deep copies.
			 */

			check(PendingDNAEffectNext);
			if (*PendingDNAEffectNext == nullptr)
			{
				// We have no memory allocated to put our next pending GE, so make a new one.
				// [#1] If you change this, please change #1-3!!!
				AppliedActiveGE = new FActiveDNAEffect(NewHandle, Spec, GetWorldTime(), GetServerWorldTime(), InPredictionKey);
				*PendingDNAEffectNext = AppliedActiveGE;
			}
			else
			{
				// We already had memory allocated to put a pending GE, move in.
				// [#2] If you change this, please change #1-3!!!
				**PendingDNAEffectNext = MoveTemp(FActiveDNAEffect(NewHandle, Spec, GetWorldTime(), GetServerWorldTime(), InPredictionKey));
				AppliedActiveGE = *PendingDNAEffectNext;
			}

			// The next pending DNAEffect goes to where our PendingNext points
			PendingDNAEffectNext = &AppliedActiveGE->PendingNext;
		}
		else
		{

			// [#3] If you change this, please change #1-3!!!
			AppliedActiveGE = new(DNAEffects_Internal) FActiveDNAEffect(NewHandle, Spec, GetWorldTime(), GetServerWorldTime(), InPredictionKey);
		}
	}

	FDNAEffectSpec& AppliedEffectSpec = AppliedActiveGE->Spec;
	UDNAAbilitySystemGlobals::Get().GlobalPreDNAEffectSpecApply(AppliedEffectSpec, Owner);

	// Make sure our target's tags are collected, so we can properly filter infinite effects
	AppliedEffectSpec.CapturedTargetTags.GetActorTags().Reset();
	Owner->GetOwnedDNATags(AppliedEffectSpec.CapturedTargetTags.GetActorTags());

	// Calc all of our modifier magnitudes now. Some may need to update later based on attributes changing, etc, but those should
	// be done through delegate callbacks.
	AppliedEffectSpec.CaptureAttributeDataFromTarget(Owner);
	AppliedEffectSpec.CalculateModifierMagnitudes();

	// Build ModifiedAttribute list so GCs can have magnitude info if non-period effect
	// Note: One day may want to not call DNA cues unless ongoing tag requirements are met (will need to move this there)
	{
		const bool HasModifiedAttributes = AppliedEffectSpec.ModifiedAttributes.Num() > 0;
		const bool HasDurationAndNoPeriod = AppliedEffectSpec.Def->DurationPolicy == EDNAEffectDurationType::HasDuration && AppliedEffectSpec.GetPeriod() == UDNAEffect::NO_PERIOD;
		const bool HasPeriodAndNoDuration = AppliedEffectSpec.Def->DurationPolicy == EDNAEffectDurationType::Instant &&
											AppliedEffectSpec.GetPeriod() != UDNAEffect::NO_PERIOD;
		const bool ShouldBuildModifiedAttributeList = !HasModifiedAttributes && (HasDurationAndNoPeriod || HasPeriodAndNoDuration);
		if (ShouldBuildModifiedAttributeList)
		{
			int32 ModifierIndex = -1;
			for (const FDNAModifierInfo& Mod : AppliedEffectSpec.Def->Modifiers)
			{
				++ModifierIndex;

				// Take magnitude from evaluated magnitudes
				float Magnitude = 0.0f;
				if (AppliedEffectSpec.Modifiers.IsValidIndex(ModifierIndex))
				{
					const FModifierSpec& ModSpec = AppliedEffectSpec.Modifiers[ModifierIndex];
					Magnitude = ModSpec.GetEvaluatedMagnitude();
				}
				
				// Add to ModifiedAttribute list if it doesn't exist already
				FDNAEffectModifiedAttribute* ModifiedAttribute = AppliedEffectSpec.GetModifiedAttribute(Mod.Attribute);
				if (!ModifiedAttribute)
				{
					ModifiedAttribute = AppliedEffectSpec.AddModifiedAttribute(Mod.Attribute);
				}
				ModifiedAttribute->TotalMagnitude += Magnitude;
			}
		}
	}

	// Register Source and Target non snapshot capture delegates here
	AppliedEffectSpec.CapturedRelevantAttributes.RegisterLinkedAggregatorCallbacks(AppliedActiveGE->Handle);
	
	if (bSetDuration)
	{
		// Re-calculate the duration, as it could rely on target captured attributes
		float DefCalcDuration = 0.f;
		if (AppliedEffectSpec.AttemptCalculateDurationFromDef(DefCalcDuration))
		{
			AppliedEffectSpec.SetDuration(DefCalcDuration, false);
		}
		else if (AppliedEffectSpec.Def->DurationMagnitude.GetMagnitudeCalculationType() == EDNAEffectMagnitudeCalculation::SetByCaller)
		{
			AppliedEffectSpec.Def->DurationMagnitude.AttemptCalculateMagnitude(AppliedEffectSpec, AppliedEffectSpec.Duration);
		}

		const float DurationBaseValue = AppliedEffectSpec.GetDuration();

		// Calculate Duration mods if we have a real duration
		if (DurationBaseValue > 0.f)
		{
			float FinalDuration = AppliedEffectSpec.CalculateModifiedDuration();

			// We cannot mod ourselves into an instant or infinite duration effect
			if (FinalDuration <= 0.f)
			{
				ABILITY_LOG(Error, TEXT("DNAEffect %s Duration was modified to %.2f. Clamping to 0.1s duration."), *AppliedEffectSpec.Def->GetName(), FinalDuration);
				FinalDuration = 0.1f;
			}

			AppliedEffectSpec.SetDuration(FinalDuration, true);

			// ABILITY_LOG(Warning, TEXT("SetDuration for %s. Base: %.2f, Final: %.2f"), *NewEffect.Spec.Def->GetName(), DurationBaseValue, FinalDuration);

			// Register duration callbacks with the timer manager
			if (Owner)
			{
				FTimerManager& TimerManager = Owner->GetWorld()->GetTimerManager();
				FTimerDelegate Delegate = FTimerDelegate::CreateUObject(Owner, &UDNAAbilitySystemComponent::CheckDurationExpired, AppliedActiveGE->Handle);
				TimerManager.SetTimer(AppliedActiveGE->DurationHandle, Delegate, FinalDuration, false);
			}
		}
	}
	
	// Register period callbacks with the timer manager
	if (Owner && (AppliedEffectSpec.GetPeriod() != UDNAEffect::NO_PERIOD))
	{
		FTimerManager& TimerManager = Owner->GetWorld()->GetTimerManager();
		FTimerDelegate Delegate = FTimerDelegate::CreateUObject(Owner, &UDNAAbilitySystemComponent::ExecutePeriodicEffect, AppliedActiveGE->Handle);
			
		// The timer manager moves things from the pending list to the active list after checking the active list on the first tick so we need to execute here
		if (AppliedEffectSpec.Def->bExecutePeriodicEffectOnApplication)
		{
			TimerManager.SetTimerForNextTick(Delegate);
		}

		if (bSetPeriod)
		{
			TimerManager.SetTimer(AppliedActiveGE->PeriodHandle, Delegate, AppliedEffectSpec.GetPeriod(), true);
		}
	}

	if (InPredictionKey.IsLocalClientKey() == false || IsNetAuthority())	// Clients predicting a DNAEffect must not call MarkItemDirty
	{
		MarkItemDirty(*AppliedActiveGE);

		ABILITY_LOG(Verbose, TEXT("Added GE: %s. ReplicationID: %d. Key: %d. PredictionLey: %d"), *AppliedActiveGE->Spec.Def->GetName(), AppliedActiveGE->ReplicationID, AppliedActiveGE->ReplicationKey, InPredictionKey.Current);
	}
	else
	{
		// Clients predicting should call MarkArrayDirty to force the internal replication map to be rebuilt.
		MarkArrayDirty();

		// Once replicated state has caught up to this prediction key, we must remove this DNA effect.
		InPredictionKey.NewRejectOrCaughtUpDelegate(FPredictionKeyEvent::CreateUObject(Owner, &UDNAAbilitySystemComponent::RemoveActiveDNAEffect_NoReturn, AppliedActiveGE->Handle, -1));
		
	}

	// @note @todo: This is currently assuming (potentially incorrectly) that the inhibition state of a stacked GE won't change
	// as a result of stacking. In reality it could in complicated cases with differing sets of dynamically-granted tags.
	if (ExistingStackableGE)
	{
		OnStackCountChange(*ExistingStackableGE, StartingStackCount, NewStackCount);
		
	}
	else
	{
		InternalOnActiveDNAEffectAdded(*AppliedActiveGE);
	}

	return AppliedActiveGE;
}

/** This is called anytime a new ActiveDNAEffect is added, on both client and server in all cases */
void FActiveDNAEffectsContainer::InternalOnActiveDNAEffectAdded(FActiveDNAEffect& Effect)
{
	SCOPE_CYCLE_COUNTER(STAT_OnActiveDNAEffectAdded);

	const UDNAEffect* EffectDef = Effect.Spec.Def;

	if (EffectDef == nullptr)
	{
		ABILITY_LOG(Error, TEXT("FActiveDNAEffectsContainer serialized new DNAEffect with NULL Def!"));
		return;
	}

	DNAEFFECT_SCOPE_LOCK();
	UE_VLOG(Owner->OwnerActor ? Owner->OwnerActor : Owner->GetOuter(), LogDNAEffects, Log, TEXT("Added: %s"), *GetNameSafe(EffectDef->GetClass()));

	// Add our ongoing tag requirements to the dependency map. We will actually check for these tags below.
	for (const FDNATag& Tag : EffectDef->OngoingTagRequirements.IgnoreTags)
	{
		ActiveEffectTagDependencies.FindOrAdd(Tag).Add(Effect.Handle);
	}

	for (const FDNATag& Tag : EffectDef->OngoingTagRequirements.RequireTags)
	{
		ActiveEffectTagDependencies.FindOrAdd(Tag).Add(Effect.Handle);
	}

	// Add any external dependencies that might dirty the effect, if necessary
	AddCustomMagnitudeExternalDependencies(Effect);

	// Check if we should actually be turned on or not (this will turn us on for the first time)
	static FDNATagContainer OwnerTags;
	OwnerTags.Reset();

	Owner->GetOwnedDNATags(OwnerTags);
	
	Effect.bIsInhibited = true; // Effect has to start inhibited, if it should be uninhibited, CheckOnGoingTagRequirements will handle that state change
	Effect.CheckOngoingTagRequirements(OwnerTags, *this);
}

void FActiveDNAEffectsContainer::AddActiveDNAEffectGrantedTagsAndModifiers(FActiveDNAEffect& Effect, bool bInvokeDNACueEvents)
{
	if (Effect.Spec.Def == nullptr)
	{
		ABILITY_LOG(Error, TEXT("AddActiveDNAEffectGrantedTagsAndModifiers called with null Def!"));
		return;
	}

	DNAEFFECT_SCOPE_LOCK();

	// Register this ActiveDNAEffects modifiers with our Attribute Aggregators
	if (Effect.Spec.GetPeriod() <= UDNAEffect::NO_PERIOD)
	{
		for (int32 ModIdx = 0; ModIdx < Effect.Spec.Modifiers.Num(); ++ModIdx)
		{
			if (Effect.Spec.Def->Modifiers.IsValidIndex(ModIdx) == false)
			{
				// This should not be possible but is happening for us in some replay scenerios. Possibly a backward compat issue: GE def has changed and removed modifiers, but replicated data sends the old number of mods
				ensureMsgf(false, TEXT("Spec Modifiers[%d] (max %d) is invalid with Def (%s) modifiers (max %d)"), ModIdx, Effect.Spec.Modifiers.Num(), *GetNameSafe(Effect.Spec.Def), Effect.Spec.Def ? Effect.Spec.Def->Modifiers.Num() : -1);
				continue;
			}

			const FDNAModifierInfo &ModInfo = Effect.Spec.Def->Modifiers[ModIdx];

			// skip over any modifiers for attributes that we don't have
			if (!Owner || Owner->HasAttributeSetForAttribute(ModInfo.Attribute) == false)
			{
				continue;
			}

			// Note we assume the EvaluatedMagnitude is up to do. There is no case currently where we should recalculate magnitude based on
			// Ongoing tags being met. We either calculate magnitude one time, or its done via OnDirty calls (or potentially a frequency timer one day)

			float EvaluatedMagnitude = Effect.Spec.GetModifierMagnitude(ModIdx, true);	// Note this could cause an attribute aggregator to be created, so must do this before calling/caching the Aggregator below!

			FAggregator* Aggregator = FindOrCreateAttributeAggregator(Effect.Spec.Def->Modifiers[ModIdx].Attribute).Get();
			if (ensure(Aggregator))
			{
				Aggregator->AddAggregatorMod(EvaluatedMagnitude, ModInfo.ModifierOp, ModInfo.EvaluationChannelSettings.GetEvaluationChannel(), &ModInfo.SourceTags, &ModInfo.TargetTags, Effect.PredictionKey.WasLocallyGenerated(), Effect.Handle);
			}
		}
	}

	// Update our owner with the tags this DNAEffect grants them
	Owner->UpdateTagMap(Effect.Spec.Def->InheritableOwnedTagsContainer.CombinedTags, 1);
	Owner->UpdateTagMap(Effect.Spec.DynamicGrantedTags, 1);
	if (ShouldUseMinimalReplication())
	{
		Owner->AddMinimalReplicationDNATags(Effect.Spec.Def->InheritableOwnedTagsContainer.CombinedTags);
		Owner->AddMinimalReplicationDNATags(Effect.Spec.DynamicGrantedTags);
	}

	// Immunity
	ApplicationImmunityDNATagCountContainer.UpdateTagCount(Effect.Spec.Def->GrantedApplicationImmunityTags.RequireTags, 1);
	ApplicationImmunityDNATagCountContainer.UpdateTagCount(Effect.Spec.Def->GrantedApplicationImmunityTags.IgnoreTags, 1);

	if (Effect.Spec.Def->HasGrantedApplicationImmunityQuery)
	{	
		ApplicationImmunityQueryEffects.Add(Effect.Spec.Def);
	}

	// Grant abilities
	if (IsNetAuthority() && !Owner->bSuppressGrantAbility)
	{
		for (FDNAAbilitySpecDef& AbilitySpecDef : Effect.Spec.GrantedAbilitySpecs)
		{
			// Only do this if we haven't assigned the ability yet! This prevents cases where stacking GEs
			// would regrant the ability every time the stack was applied
			if (AbilitySpecDef.AssignedHandle.IsValid() == false)
			{
				Owner->GiveAbility( FDNAAbilitySpec(AbilitySpecDef, Effect.Spec.GetLevel(), Effect.Handle) );
			}
		}	
	}

	// Update DNACue tags and events
	if (!Owner->bSuppressDNACues)
	{
		for (const FDNAEffectCue& Cue : Effect.Spec.Def->DNACues)
		{
			Owner->UpdateTagMap(Cue.DNACueTags, 1);

			if (bInvokeDNACueEvents)
			{
				Owner->InvokeDNACueEvent(Effect.Spec, EDNACueEvent::OnActive);
				Owner->InvokeDNACueEvent(Effect.Spec, EDNACueEvent::WhileActive);
			}

			if (ShouldUseMinimalReplication())
			{
				for (const FDNATag& CueTag : Cue.DNACueTags)
				{
					// Note: minimal replication does not replicate the effect context with the DNA cue parameters.
					// This is just a choice right now. If needed, it may be better to convert the effect context to GC parameters *here*
					// and pass those into this function
					Owner->AddDNACue_MinimalReplication(CueTag);
				}
			}
		}
	}

	// Generic notify for anyone listening
	Owner->OnActiveDNAEffectAddedDelegateToSelf.Broadcast(Owner, Effect.Spec, Effect.Handle);
}

/** Called on server to remove a DNAEffect */
bool FActiveDNAEffectsContainer::RemoveActiveDNAEffect(FActiveDNAEffectHandle Handle, int32 StacksToRemove)
{
	// Iterating through manually since this is a removal operation and we need to pass the index into InternalRemoveActiveDNAEffect
	int32 NumDNAEffects = GetNumDNAEffects();
	for (int32 ActiveGEIdx = 0; ActiveGEIdx < NumDNAEffects; ++ActiveGEIdx)
	{
		FActiveDNAEffect& Effect = *GetActiveDNAEffect(ActiveGEIdx);
		if (Effect.Handle == Handle && Effect.IsPendingRemove == false)
		{
			UE_VLOG(Owner->OwnerActor, LogDNAEffects, Log, TEXT("Removed: %s"), *GetNameSafe(Effect.Spec.Def->GetClass()));
			if (UE_LOG_ACTIVE(VLogDNAAbilitySystem, Log))
			{
				ABILITY_VLOG(Owner->OwnerActor, Log, TEXT("Removed %s"), *Effect.Spec.Def->GetFName().ToString());
				for (FDNAModifierInfo Modifier : Effect.Spec.Def->Modifiers)
				{
					float Magnitude = 0.f;
					Modifier.ModifierMagnitude.AttemptCalculateMagnitude(Effect.Spec, Magnitude);
					ABILITY_VLOG(Owner->OwnerActor, Log, TEXT("         %s: %s %f"), *Modifier.Attribute.GetName(), *EDNAModOpToString(Modifier.ModifierOp), Magnitude);
				}
			}

			InternalRemoveActiveDNAEffect(ActiveGEIdx, StacksToRemove, true);
			return true;
		}
	}
	ABILITY_LOG(Log, TEXT("RemoveActiveDNAEffect called with invalid Handle: %s"), *Handle.ToString());
	return false;
}

/** Called by server to actually remove a DNAEffect */
bool FActiveDNAEffectsContainer::InternalRemoveActiveDNAEffect(int32 Idx, int32 StacksToRemove, bool bPrematureRemoval)
{
	SCOPE_CYCLE_COUNTER(STAT_RemoveActiveDNAEffect);

	bool IsLocked = (ScopedLockCount > 0);	// Cache off whether we were previously locked
	DNAEFFECT_SCOPE_LOCK();			// Apply lock so no one else can change the AGE list (we may still change it if IsLocked is false)
	
	if (ensure(Idx < GetNumDNAEffects()))
	{
		FActiveDNAEffect& Effect = *GetActiveDNAEffect(Idx);
		if (!ensure(!Effect.IsPendingRemove))
		{
			// This effect is already being removed. This probably means a bug at the callsite, but we can handle it gracefully here by earlying out and pretending the effect was removed.
			return true;
		}

		ABILITY_LOG(Verbose, TEXT("InternalRemoveActiveDNAEffect: Auth: %s Handle: %s Def: %s"), IsNetAuthority() ? TEXT("TRUE") : TEXT("FALSE"), *Effect.Handle.ToString(), Effect.Spec.Def ? *Effect.Spec.Def->GetName() : TEXT("NONE"));

		if (StacksToRemove > 0 && Effect.Spec.StackCount > StacksToRemove)
		{
			// This won't be a full remove, only a change in StackCount.
			int32 StartingStackCount = Effect.Spec.StackCount;
			Effect.Spec.StackCount -= StacksToRemove;
			OnStackCountChange(Effect, StartingStackCount, Effect.Spec.StackCount);
			return false;
		}

		// Invoke Remove DNACue event
		bool ShouldInvokeDNACueEvent = true;
		const bool bIsNetAuthority = IsNetAuthority();
		if (!bIsNetAuthority && Effect.PredictionKey.IsLocalClientKey() && Effect.PredictionKey.WasReceived() == false)
		{
			// This was an effect that we predicted. Don't invoke DNACue event if we have another DNAEffect that shares the same predictionkey and was received from the server
			if (HasReceivedEffectWithPredictedKey(Effect.PredictionKey))
			{
				ShouldInvokeDNACueEvent = false;
			}
		}

		// Don't invoke the GC event if the effect is inhibited, and thus the GC is already not active
		ShouldInvokeDNACueEvent &= !Effect.bIsInhibited;

		// Mark the effect pending remove, and remove all side effects from the effect
		InternalOnActiveDNAEffectRemoved(Effect, ShouldInvokeDNACueEvent);

		if (Effect.DurationHandle.IsValid())
		{
			Owner->GetWorld()->GetTimerManager().ClearTimer(Effect.DurationHandle);
		}
		if (Effect.PeriodHandle.IsValid())
		{
			Owner->GetWorld()->GetTimerManager().ClearTimer(Effect.PeriodHandle);
		}

		if (bIsNetAuthority && Owner->OwnerActor)
		{
			Owner->OwnerActor->FlushNetDormancy();
		}

		// Remove this handle from the global map
		Effect.Handle.RemoveFromGlobalMap();

		// Attempt to apply expiration effects, if necessary
		InternalApplyExpirationEffects(Effect.Spec, bPrematureRemoval);

		bool ModifiedArray = false;

		// Finally remove the ActiveDNAEffect
		if (IsLocked)
		{
			// We are locked, so this removal is now pending.
			PendingRemoves++;

			ABILITY_LOG(Verbose, TEXT("InternalRemoveActiveDNAEffect while locked; Counting as a Pending Remove: Auth: %s Handle: %s Def: %s"), IsNetAuthority() ? TEXT("TRUE") : TEXT("FALSE"), *Effect.Handle.ToString(), Effect.Spec.Def ? *Effect.Spec.Def->GetName() : TEXT("NONE"));
		}
		else
		{
			// Not locked, so do the removal right away.

			// If we are not scope locked, then there is no way this idx should be referring to something on the pending add list.
			// It is possible to remove a GE that is pending add, but it would happen while the scope lock is still in effect, resulting
			// in a pending remove being set.
			check(Idx < DNAEffects_Internal.Num());

			DNAEffects_Internal.RemoveAtSwap(Idx);
			ModifiedArray = true;
		}

		MarkArrayDirty();

		// Hack: force netupdate on owner. This isn't really necessary in real DNA but is nice
		// during debugging where breakpoints or pausing can mess up network update times. Open issue
		// with network team.
		Owner->GetOwner()->ForceNetUpdate();
		
		return ModifiedArray;
	}

	ABILITY_LOG(Warning, TEXT("InternalRemoveActiveDNAEffect called with invalid index: %d"), Idx);
	return false;
}

/** Called by client and server: This does cleanup that has to happen whether the effect is being removed locally or due to replication */
void FActiveDNAEffectsContainer::InternalOnActiveDNAEffectRemoved(FActiveDNAEffect& Effect, bool bInvokeDNACueEvents)
{
	SCOPE_CYCLE_COUNTER(STAT_OnActiveDNAEffectRemoved);

	// Mark the effect as pending removal
	Effect.IsPendingRemove = true;

	if (Effect.Spec.Def)
	{
		// Remove our tag requirements from the dependency map
		RemoveActiveEffectTagDependency(Effect.Spec.Def->OngoingTagRequirements.IgnoreTags, Effect.Handle);
		RemoveActiveEffectTagDependency(Effect.Spec.Def->OngoingTagRequirements.RequireTags, Effect.Handle);

		// Only Need to update tags and modifiers if the DNA effect is active.
		if (!Effect.bIsInhibited)
		{
			RemoveActiveDNAEffectGrantedTagsAndModifiers(Effect, bInvokeDNACueEvents);
		}

		RemoveCustomMagnitudeExternalDependencies(Effect);
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("InternalOnActiveDNAEffectRemoved called with no DNAEffect: %s"), *Effect.Handle.ToString());
	}

	Effect.OnRemovedDelegate.Broadcast();
	OnActiveDNAEffectRemovedDelegate.Broadcast(Effect);
}

void FActiveDNAEffectsContainer::RemoveActiveDNAEffectGrantedTagsAndModifiers(const FActiveDNAEffect& Effect, bool bInvokeDNACueEvents)
{
	// Update AttributeAggregators: remove mods from this ActiveGE Handle
	if (Effect.Spec.GetPeriod() <= UDNAEffect::NO_PERIOD)
	{
		for(const FDNAModifierInfo& Mod : Effect.Spec.Def->Modifiers)
		{
			if(Mod.Attribute.IsValid())
			{
				FAggregatorRef* RefPtr = AttributeAggregatorMap.Find(Mod.Attribute);
				if(RefPtr)
				{
					RefPtr->Get()->RemoveAggregatorMod(Effect.Handle);
				}
			}
		}
	}

	// Update DNAtag count and broadcast delegate if we are at 0
	Owner->UpdateTagMap(Effect.Spec.Def->InheritableOwnedTagsContainer.CombinedTags, -1);
	Owner->UpdateTagMap(Effect.Spec.DynamicGrantedTags, -1);

	if (ShouldUseMinimalReplication())
	{
		Owner->RemoveMinimalReplicationDNATags(Effect.Spec.Def->InheritableOwnedTagsContainer.CombinedTags);
		Owner->RemoveMinimalReplicationDNATags(Effect.Spec.DynamicGrantedTags);
	}

	// Immunity
	ApplicationImmunityDNATagCountContainer.UpdateTagCount(Effect.Spec.Def->GrantedApplicationImmunityTags.RequireTags, -1);
	ApplicationImmunityDNATagCountContainer.UpdateTagCount(Effect.Spec.Def->GrantedApplicationImmunityTags.IgnoreTags, -1);

	if (Effect.Spec.Def->HasGrantedApplicationImmunityQuery)
	{
		ApplicationImmunityQueryEffects.Remove(Effect.Spec.Def);
	}

	// Cancel/remove granted abilities
	if (IsNetAuthority())
	{
		for (const FDNAAbilitySpecDef& AbilitySpecDef : Effect.Spec.GrantedAbilitySpecs)
		{
			if (AbilitySpecDef.AssignedHandle.IsValid())
			{
				switch(AbilitySpecDef.RemovalPolicy)
				{
				case EDNAEffectGrantedAbilityRemovePolicy::CancelAbilityImmediately:
					{
						Owner->ClearAbility(AbilitySpecDef.AssignedHandle);
						break;
					}
				case EDNAEffectGrantedAbilityRemovePolicy::RemoveAbilityOnEnd:
					{
						Owner->SetRemoveAbilityOnEnd(AbilitySpecDef.AssignedHandle);
						break;
					}
				default:
					{
						// Do nothing to granted ability
						break;
					}
				}
			}
		}
	}

	// Update DNACue tags and events
	if (!Owner->bSuppressDNACues)
	{
		for (const FDNAEffectCue& Cue : Effect.Spec.Def->DNACues)
		{
			Owner->UpdateTagMap(Cue.DNACueTags, -1);

			if (bInvokeDNACueEvents)
			{
				Owner->InvokeDNACueEvent(Effect.Spec, EDNACueEvent::Removed);
			}

			if (ShouldUseMinimalReplication())
			{
				for (const FDNATag& CueTag : Cue.DNACueTags)
				{
					Owner->RemoveDNACue_MinimalReplication(CueTag);
				}
			}
		}
	}
}

void FActiveDNAEffectsContainer::RemoveActiveEffectTagDependency(const FDNATagContainer& Tags, FActiveDNAEffectHandle Handle)
{
	for(const FDNATag& Tag : Tags)
	{
		auto Ptr = ActiveEffectTagDependencies.Find(Tag);
		if (Ptr)
		{
			Ptr->Remove(Handle);
			if (Ptr->Num() <= 0)
			{
				ActiveEffectTagDependencies.Remove(Tag);
			}
		}
	}
}

void FActiveDNAEffectsContainer::AddCustomMagnitudeExternalDependencies(FActiveDNAEffect& Effect)
{
	const UDNAEffect* GEDef = Effect.Spec.Def;
	if (GEDef)
	{
		const bool bIsNetAuthority = IsNetAuthority();

		// Check each modifier to see if it has a custom external dependency
		for (const FDNAModifierInfo& CurMod : GEDef->Modifiers)
		{
			TSubclassOf<UDNAModMagnitudeCalculation> ModCalcClass = CurMod.ModifierMagnitude.GetCustomMagnitudeCalculationClass();
			if (ModCalcClass)
			{
				const UDNAModMagnitudeCalculation* ModCalcClassCDO = ModCalcClass->GetDefaultObject<UDNAModMagnitudeCalculation>();
				if (ModCalcClassCDO)
				{
					// Only register the dependency if acting as net authority or if the calculation class has indicated it wants non-net authorities
					// to be allowed to perform the calculation as well
					UWorld* World = Owner ? Owner->GetWorld() : nullptr;
					FOnExternalDNAModifierDependencyChange* ExternalDelegate = ModCalcClassCDO->GetExternalModifierDependencyMulticast(Effect.Spec, World);
					if (ExternalDelegate && (bIsNetAuthority || ModCalcClassCDO->ShouldAllowNonNetAuthorityDependencyRegistration()))
					{
						FObjectKey ModCalcClassKey(*ModCalcClass);
						FCustomModifierDependencyHandle* ExistingDependencyHandle = CustomMagnitudeClassDependencies.Find(ModCalcClassKey);
						
						// If the dependency has already been registered for this container, just add the handle of the effect to the existing list
						if (ExistingDependencyHandle)
						{
							ExistingDependencyHandle->ActiveEffectHandles.Add(Effect.Handle);
						}
						// If the dependency is brand new, bind an update to the delegate and cache off the handle
						else
						{
							FCustomModifierDependencyHandle& NewDependencyHandle = CustomMagnitudeClassDependencies.Add(ModCalcClassKey);
							NewDependencyHandle.ActiveDelegateHandle = ExternalDelegate->AddRaw(this, &FActiveDNAEffectsContainer::OnCustomMagnitudeExternalDependencyFired, ModCalcClass);
							NewDependencyHandle.ActiveEffectHandles.Add(Effect.Handle);
						}
					}
				}
			}
		}
	}
}

void FActiveDNAEffectsContainer::RemoveCustomMagnitudeExternalDependencies(FActiveDNAEffect& Effect)
{
	const UDNAEffect* GEDef = Effect.Spec.Def;
	if (GEDef && CustomMagnitudeClassDependencies.Num() > 0)
	{
		const bool bIsNetAuthority = IsNetAuthority();
		for (const FDNAModifierInfo& CurMod : GEDef->Modifiers)
		{
			TSubclassOf<UDNAModMagnitudeCalculation> ModCalcClass = CurMod.ModifierMagnitude.GetCustomMagnitudeCalculationClass();
			if (ModCalcClass)
			{
				const UDNAModMagnitudeCalculation* ModCalcClassCDO = ModCalcClass->GetDefaultObject<UDNAModMagnitudeCalculation>();
				if (ModCalcClassCDO)
				{
					UWorld* World = Owner ? Owner->GetWorld() : nullptr;
					FOnExternalDNAModifierDependencyChange* ExternalDelegate = ModCalcClassCDO->GetExternalModifierDependencyMulticast(Effect.Spec, World);
					if (ExternalDelegate && (bIsNetAuthority || ModCalcClassCDO->ShouldAllowNonNetAuthorityDependencyRegistration()))
					{
						FObjectKey ModCalcClassKey(*ModCalcClass);
						FCustomModifierDependencyHandle* ExistingDependencyHandle = CustomMagnitudeClassDependencies.Find(ModCalcClassKey);
						
						// If this dependency was bound for this effect, remove it
						if (ExistingDependencyHandle)
						{
							ExistingDependencyHandle->ActiveEffectHandles.Remove(Effect.Handle);

							// If this was the last effect for this dependency, unbind the delegate and remove the dependency entirely
							if (ExistingDependencyHandle->ActiveEffectHandles.Num() == 0)
							{
								ExternalDelegate->Remove(ExistingDependencyHandle->ActiveDelegateHandle);
								CustomMagnitudeClassDependencies.Remove(ModCalcClassKey);
							}
						}
					}
				}
			}
		}
	}
}

void FActiveDNAEffectsContainer::OnCustomMagnitudeExternalDependencyFired(TSubclassOf<UDNAModMagnitudeCalculation> MagnitudeCalculationClass)
{
	if (MagnitudeCalculationClass)
	{
		FObjectKey ModCalcClassKey(*MagnitudeCalculationClass);
		FCustomModifierDependencyHandle* ExistingDependencyHandle = CustomMagnitudeClassDependencies.Find(ModCalcClassKey);
		if (ExistingDependencyHandle)
		{
			const bool bIsNetAuthority = IsNetAuthority();
			const UDNAModMagnitudeCalculation* CalcClassCDO = MagnitudeCalculationClass->GetDefaultObject<UDNAModMagnitudeCalculation>();
			const bool bRequiresDormancyFlush = CalcClassCDO ? !CalcClassCDO->ShouldAllowNonNetAuthorityDependencyRegistration() : false;

			const TSet<FActiveDNAEffectHandle>& HandlesNeedingUpdate = ExistingDependencyHandle->ActiveEffectHandles;

			// Iterate through all effects, updating the ones that specifically respond to the external dependency being updated
			for (FActiveDNAEffect& Effect : this)
			{
				if (HandlesNeedingUpdate.Contains(Effect.Handle))
				{
					if (bIsNetAuthority)
					{
						// By default, a dormancy flush should be required here. If a calculation class has requested that
						// non-net authorities can respond to external dependencies, the dormancy flush is skipped as a desired optimization
						if (bRequiresDormancyFlush && Owner && Owner->OwnerActor)
						{
							Owner->OwnerActor->FlushNetDormancy();
						}

						MarkItemDirty(Effect);
					}

					Effect.Spec.CalculateModifierMagnitudes();
					UpdateAllAggregatorModMagnitudes(Effect);
				}
			}
		}
	}
}

void FActiveDNAEffectsContainer::InternalApplyExpirationEffects(const FDNAEffectSpec& ExpiringSpec, bool bPrematureRemoval)
{
	DNAEFFECT_SCOPE_LOCK();

	check(Owner);

	// Don't allow prediction of expiration effects
	if (IsNetAuthority())
	{
		const UDNAEffect* ExpiringGE = ExpiringSpec.Def;
		if (ExpiringGE)
		{
			// Determine the appropriate type of effect to apply depending on whether the effect is being prematurely removed or not
			const TArray<TSubclassOf<UDNAEffect>>& ExpiryEffects = (bPrematureRemoval ? ExpiringGE->PrematureExpirationEffectClasses : ExpiringGE->RoutineExpirationEffectClasses);

			for (const TSubclassOf<UDNAEffect>& CurExpiryEffect : ExpiryEffects)
			{
				if (CurExpiryEffect)
				{
					const UDNAEffect* CurExpiryCDO = CurExpiryEffect->GetDefaultObject<UDNAEffect>();
					check(CurExpiryCDO);
									
					// Duplicate GE context
					const FDNAEffectContextHandle& ExpiringSpecContextHandle = ExpiringSpec.GetEffectContext();
					FDNAEffectContextHandle NewContextHandle = ExpiringSpecContextHandle.Duplicate();
					
					// We need to manually initialize the new GE spec. We want to pass on all of the tags from the originating GE *Except* for that GE's asset tags. (InheritableDNAEffectTags).
					// But its very important that the ability tags and anything else that was added to the source tags in the originating GE carries over
					FDNAEffectSpec NewExpirySpec;

					// Make a full copy
					NewExpirySpec.CapturedSourceTags = ExpiringSpec.CapturedSourceTags;

					// But then remove the tags the originating GE added
					NewExpirySpec.CapturedSourceTags.GetSpecTags().RemoveTags( ExpiringGE->InheritableDNAEffectTags.CombinedTags );

					// Now initialize like the normal cstor would have. Note that this will add the new GE's asset tags (in case they were removed in the line above / e.g., shared asset tags with the originating GE)					
					NewExpirySpec.Initialize(CurExpiryCDO, NewContextHandle, ExpiringSpec.GetLevel());

					Owner->ApplyDNAEffectSpecToSelf(NewExpirySpec);
				}
			}
		}
	}
}

void FActiveDNAEffectsContainer::RestartActiveDNAEffectDuration(FActiveDNAEffect& ActiveDNAEffect)
{
	ActiveDNAEffect.StartServerWorldTime = GetServerWorldTime();
	ActiveDNAEffect.CachedStartServerWorldTime = ActiveDNAEffect.StartServerWorldTime;
	ActiveDNAEffect.StartWorldTime = GetWorldTime();
	MarkItemDirty(ActiveDNAEffect);

	OnDurationChange(ActiveDNAEffect);
}

void FActiveDNAEffectsContainer::OnOwnerTagChange(FDNATag TagChange, int32 NewCount)
{
	// It may be beneficial to do a scoped lock on attribute re-evaluation during this function

	auto Ptr = ActiveEffectTagDependencies.Find(TagChange);
	if (Ptr)
	{
		DNAEFFECT_SCOPE_LOCK();

		FDNATagContainer OwnerTags;
		Owner->GetOwnedDNATags(OwnerTags);

		TSet<FActiveDNAEffectHandle>& Handles = *Ptr;
		for (const FActiveDNAEffectHandle& Handle : Handles)
		{
			FActiveDNAEffect* ActiveEffect = GetActiveDNAEffect(Handle);
			if (ActiveEffect)
			{
				ActiveEffect->CheckOngoingTagRequirements(OwnerTags, *this, true);
			}
		}
	}
}

bool FActiveDNAEffectsContainer::HasApplicationImmunityToSpec(const FDNAEffectSpec& SpecToApply, const FActiveDNAEffect*& OutGEThatProvidedImmunity) const
{
	SCOPE_CYCLE_COUNTER(STAT_HasApplicationImmunityToSpec)

	const FDNATagContainer* AggregatedSourceTags = SpecToApply.CapturedSourceTags.GetAggregatedTags();
	if (!ensure(AggregatedSourceTags))
	{
		return false;
	}

	// Query
	for (const UDNAEffect* EffectDef : ApplicationImmunityQueryEffects)
	{
		if (EffectDef->GrantedApplicationImmunityQuery.Matches(SpecToApply))
		{
			// This is blocked, but who blocked? Search for that Active GE
			for (const FActiveDNAEffect& Effect : this)
			{
				if (Effect.Spec.Def == EffectDef)
				{
					OutGEThatProvidedImmunity = &Effect;
					return true;
				}
			}
			ABILITY_LOG(Error, TEXT("Application Immunity was triggered for Applied GE: %s by Granted GE: %s. But this GE was not found in the Active DNAEffects list!"), *GetNameSafe(SpecToApply.Def), *GetNameSafe( EffectDef));
			break;
		}
	}

	// Quick map test
	if (!AggregatedSourceTags->HasAny(ApplicationImmunityDNATagCountContainer.GetExplicitDNATags()))
	{
		return false;
	}

	for (const FActiveDNAEffect& Effect : this)
	{
		if (Effect.Spec.Def->GrantedApplicationImmunityTags.IsEmpty() == false && Effect.Spec.Def->GrantedApplicationImmunityTags.RequirementsMet( *AggregatedSourceTags ))
		{
			OutGEThatProvidedImmunity = &Effect;
			return true;
		}
	}

	return false;
}

bool FActiveDNAEffectsContainer::NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
{
	if (Owner)
	{
		EReplicationMode ReplicationMode = Owner->ReplicationMode;
		if (ReplicationMode == EReplicationMode::Minimal)
		{
			return false;
		}
		else if (ReplicationMode == EReplicationMode::Mixed)
		{
			if (UPackageMapClient* Client = Cast<UPackageMapClient>(DeltaParms.Map))
			{
				UNetConnection* Connection = Client->GetConnection();

				// Even in mixed mode, we should always replicate out to replays so it has all information.
				if (Connection->GetDriver()->NetDriverName != NAME_DemoNetDriver)
				{
					// In mixed mode, we only want to replicate to the owner of this channel, minimal replication
					// data will go to everyone else.
					if (!Owner->GetOwner()->IsOwnedBy(Connection->OwningActor))
					{
						return false;
					}
				}
			}
		}
	}

	bool RetVal = FastArrayDeltaSerialize<FActiveDNAEffect>(DNAEffects_Internal, DeltaParms, *this);

	// After the array has been replicated, invoke GC events ONLY if the effect is not inhibited
	// We postpone this check because in the same net update we could receive multiple GEs that affect if one another is inhibited
	
	if (DeltaParms.Writer == nullptr && Owner != nullptr)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_ActiveDNAEffectsContainer_NetDeltaSerialize_CheckRepDNACues);

		if (!DeltaParms.bOutHasMoreUnmapped) // Do not invoke GCs when we have missing information (like AActor*s in EffectContext)
		{
			for (const FActiveDNAEffect& Effect : this)
			{
			
				if (Effect.bIsInhibited == false)
				{
					if (Effect.bPendingRepOnActiveGC)
					{
						Owner->InvokeDNACueEvent(Effect.Spec, EDNACueEvent::OnActive);
						
					}
					if (Effect.bPendingRepWhileActiveGC)
					{
						Owner->InvokeDNACueEvent(Effect.Spec, EDNACueEvent::WhileActive);
					}
				}

				Effect.bPendingRepOnActiveGC = false;
				Effect.bPendingRepWhileActiveGC = false;
			}
		}
	}

	return RetVal;
}

void FActiveDNAEffectsContainer::Uninitialize()
{
	for (FActiveDNAEffect& CurEffect : this)
	{
		RemoveCustomMagnitudeExternalDependencies(CurEffect);
	}
	ensure(CustomMagnitudeClassDependencies.Num() == 0);
}

float FActiveDNAEffectsContainer::GetServerWorldTime() const
{
	UWorld* World = Owner->GetWorld();
	AGameStateBase* GameState = World->GetGameState();
	if (GameState)
	{
		return GameState->GetServerWorldTimeSeconds();
	}

	return World->GetTimeSeconds();
}

float FActiveDNAEffectsContainer::GetWorldTime() const
{
	UWorld *World = Owner->GetWorld();
	return World->GetTimeSeconds();
}

void FActiveDNAEffectsContainer::CheckDuration(FActiveDNAEffectHandle Handle)
{
	DNAEFFECT_SCOPE_LOCK();
	// Intentionally iterating through only the internal list since we need to pass the index for removal
	// and pending effects will never need to be checked for duration expiration (They will be added to the real list first)
	for (int32 ActiveGEIdx = 0; ActiveGEIdx < DNAEffects_Internal.Num(); ++ActiveGEIdx)
	{
		FActiveDNAEffect& Effect = DNAEffects_Internal[ActiveGEIdx];
		if (Effect.Handle == Handle)
		{
			if (Effect.IsPendingRemove)
			{
				// break is this effect is pending remove. 
				// (Note: don't combine this with the above if statement that is looking for the effect via handle, since we want to stop iteration if we find a matching handle but are pending remove).
				break;
			}

			FTimerManager& TimerManager = Owner->GetWorld()->GetTimerManager();

			// The duration may have changed since we registered this callback with the timer manager.
			// Make sure that this effect should really be destroyed now
			float Duration = Effect.GetDuration();
			float CurrentTime = GetWorldTime();

			int32 StacksToRemove = -2;
			bool RefreshStartTime = false;
			bool RefreshDurationTimer = false;
			bool CheckForFinalPeriodicExec = false;

			if (Duration > 0.f && (((Effect.StartWorldTime + Duration) < CurrentTime) || FMath::IsNearlyZero(CurrentTime - Duration - Effect.StartWorldTime, KINDA_SMALL_NUMBER)))
			{
				// Figure out what to do based on the expiration policy
				switch(Effect.Spec.Def->StackExpirationPolicy)
				{
				case EDNAEffectStackingExpirationPolicy::ClearEntireStack:
					StacksToRemove = -1; // Remove all stacks
					CheckForFinalPeriodicExec = true;					
					break;

				case EDNAEffectStackingExpirationPolicy::RemoveSingleStackAndRefreshDuration:
					StacksToRemove = 1;
					CheckForFinalPeriodicExec = (Effect.Spec.StackCount == 1);
					RefreshStartTime = true;
					RefreshDurationTimer = true;
					break;
				case EDNAEffectStackingExpirationPolicy::RefreshDuration:
					RefreshStartTime = true;
					RefreshDurationTimer = true;
					break;
				};					
			}
			else
			{
				// Effect isn't finished, just refresh its duration timer
				RefreshDurationTimer = true;
			}

			if (CheckForFinalPeriodicExec)
			{
				// This DNA effect has hit its duration. Check if it needs to execute one last time before removing it.
				if (Effect.PeriodHandle.IsValid() && TimerManager.TimerExists(Effect.PeriodHandle))
				{
					float PeriodTimeRemaining = TimerManager.GetTimerRemaining(Effect.PeriodHandle);
					if (PeriodTimeRemaining <= KINDA_SMALL_NUMBER && !Effect.bIsInhibited)
					{
						ExecuteActiveEffectsFrom(Effect.Spec);

						// The above call to ExecuteActiveEffectsFrom could cause this effect to be explicitly removed
						// (for example it could kill the owner and cause the effect to be wiped via death).
						// In that case, we need to early out instead of possibly continueing to the below calls to InternalRemoveActiveDNAEffect
						if ( Effect.IsPendingRemove )
						{
							break;
						}
					}

					// Forcibly clear the periodic ticks because this effect is going to be removed
					TimerManager.ClearTimer(Effect.PeriodHandle);
				}
			}

			if (StacksToRemove >= -1)
			{
				InternalRemoveActiveDNAEffect(ActiveGEIdx, StacksToRemove, false);
			}

			if (RefreshStartTime)
			{
				RestartActiveDNAEffectDuration(Effect);
			}

			if (RefreshDurationTimer)
			{
				// Always reset the timer, since the duration might have been modified
				FTimerDelegate Delegate = FTimerDelegate::CreateUObject(Owner, &UDNAAbilitySystemComponent::CheckDurationExpired, Effect.Handle);
				TimerManager.SetTimer(Effect.DurationHandle, Delegate, (Effect.StartWorldTime + Duration) - CurrentTime, false);
			}

			break;
		}
	}
}

bool FActiveDNAEffectsContainer::CanApplyAttributeModifiers(const UDNAEffect* DNAEffect, float Level, const FDNAEffectContextHandle& EffectContext)
{
	SCOPE_CYCLE_COUNTER(STAT_DNAEffectsCanApplyAttributeModifiers);

	FDNAEffectSpec	Spec(DNAEffect, EffectContext, Level);

	Spec.CalculateModifierMagnitudes();
	
	for(int32 ModIdx = 0; ModIdx < Spec.Modifiers.Num(); ++ModIdx)
	{
		const FDNAModifierInfo& ModDef = Spec.Def->Modifiers[ModIdx];
		const FModifierSpec& ModSpec = Spec.Modifiers[ModIdx];
	
		// It only makes sense to check additive operators
		if (ModDef.ModifierOp == EDNAModOp::Additive)
		{
			if (!ModDef.Attribute.IsValid())
			{
				continue;
			}
			const UAttributeSet* Set = Owner->GetAttributeSubobject(ModDef.Attribute.GetAttributeSetClass());
			float CurrentValue = ModDef.Attribute.GetNumericValueChecked(Set);
			float CostValue = ModSpec.GetEvaluatedMagnitude();

			if (CurrentValue + CostValue < 0.f)
			{
				return false;
			}
		}
	}
	return true;
}

TArray<float> FActiveDNAEffectsContainer::GetActiveEffectsTimeRemaining(const FDNAEffectQuery& Query) const
{
	SCOPE_CYCLE_COUNTER(STAT_DNAEffectsGetActiveEffectsTimeRemaining);

	float CurrentTime = GetWorldTime();

	TArray<float>	ReturnList;

	for (const FActiveDNAEffect& Effect : this)
	{
		if (!Query.Matches(Effect))
		{
			continue;
		}

		float Elapsed = CurrentTime - Effect.StartWorldTime;
		float Duration = Effect.GetDuration();

		ReturnList.Add(Duration - Elapsed);
	}

	// Note: keep one return location to avoid copy operation.
	return ReturnList;
}

TArray<float> FActiveDNAEffectsContainer::GetActiveEffectsDuration(const FDNAEffectQuery& Query) const
{
	SCOPE_CYCLE_COUNTER(STAT_DNAEffectsGetActiveEffectsDuration);

	TArray<float>	ReturnList;

	for (const FActiveDNAEffect& Effect : this)
	{
		if (!Query.Matches(Effect))
		{
			continue;
		}

		ReturnList.Add(Effect.GetDuration());
	}

	// Note: keep one return location to avoid copy operation.
	return ReturnList;
}

TArray<TPair<float,float>> FActiveDNAEffectsContainer::GetActiveEffectsTimeRemainingAndDuration(const FDNAEffectQuery& Query) const
{
	SCOPE_CYCLE_COUNTER(STAT_DNAEffectsGetActiveEffectsTimeRemainingAndDuration);

	TArray<TPair<float,float>> ReturnList;

	float CurrentTime = GetWorldTime();

	for (const FActiveDNAEffect& Effect : this)
	{
		if (!Query.Matches(Effect))
		{
			continue;
		}

		float Elapsed = CurrentTime - Effect.StartWorldTime;
		float Duration = Effect.GetDuration();

		ReturnList.Add(TPairInitializer<float, float>(Duration - Elapsed, Duration));
	}

	// Note: keep one return location to avoid copy operation.
	return ReturnList;
}

TArray<FActiveDNAEffectHandle> FActiveDNAEffectsContainer::GetActiveEffects(const FDNAEffectQuery& Query) const
{
	SCOPE_CYCLE_COUNTER(STAT_DNAEffectsGetActiveEffects);

	TArray<FActiveDNAEffectHandle> ReturnList;

	for (const FActiveDNAEffect& Effect : this)
	{
		if (!Query.Matches(Effect))
		{
			continue;
		}

		ReturnList.Add(Effect.Handle);
	}

	return ReturnList;
}

float FActiveDNAEffectsContainer::GetActiveEffectsEndTime(const FDNAEffectQuery& Query) const
{
	float EndTime = 0.f;
	float Duration = 0.f;
	GetActiveEffectsEndTimeAndDuration(Query, EndTime, Duration);
	return EndTime;
}

bool FActiveDNAEffectsContainer::GetActiveEffectsEndTimeAndDuration(const FDNAEffectQuery& Query, float& EndTime, float& Duration) const
{
	bool FoundSomething = false;
	
	for (const FActiveDNAEffect& Effect : this)
	{
		if (!Query.Matches(Effect))
		{
			continue;
		}
		
		FoundSomething = true;

		float ThisEndTime = Effect.GetEndTime();
		if (ThisEndTime <= UDNAEffect::INFINITE_DURATION)
		{
			// This is an infinite duration effect, so this end time is indeterminate
			EndTime = -1.f;
			Duration = -1.f;
			return true;
		}

		if (ThisEndTime > EndTime)
		{
			EndTime = ThisEndTime;
			Duration = Effect.GetDuration();
		}
	}
	return FoundSomething;
}

TArray<FActiveDNAEffectHandle> FActiveDNAEffectsContainer::GetAllActiveEffectHandles() const
{
	SCOPE_CYCLE_COUNTER(STAT_DNAEffectsGetAllActiveEffectHandles);

	TArray<FActiveDNAEffectHandle> ReturnList;

	for (const FActiveDNAEffect& Effect : this)
	{
		ReturnList.Add(Effect.Handle);
	}

	return ReturnList;
}

void FActiveDNAEffectsContainer::ModifyActiveEffectStartTime(FActiveDNAEffectHandle Handle, float StartTimeDiff)
{
	SCOPE_CYCLE_COUNTER(STAT_DNAEffectsModifyActiveEffectStartTime);

	FActiveDNAEffect* Effect = GetActiveDNAEffect(Handle);

	if (Effect)
	{
		Effect->StartWorldTime += StartTimeDiff;
		Effect->StartServerWorldTime += StartTimeDiff;

		// Check if we are now expired
		CheckDuration(Handle);

		// Broadcast to anyone listening
		OnDurationChange(*Effect);

		MarkItemDirty(*Effect);
	}
}

int32 FActiveDNAEffectsContainer::RemoveActiveEffects(const FDNAEffectQuery& Query, int32 StacksToRemove)
{
	// Force a lock because the removals could cause other removals earlier in the array, so iterating backwards is not safe all by itself
	DNAEFFECT_SCOPE_LOCK();
	int32 NumRemoved = 0;

	// Manually iterating through in reverse because this is a removal operation
	for (int32 idx = GetNumDNAEffects() - 1; idx >= 0; --idx)
	{
		const FActiveDNAEffect& Effect = *GetActiveDNAEffect(idx);
		if (Effect.IsPendingRemove == false && Query.Matches(Effect))
		{
			InternalRemoveActiveDNAEffect(idx, StacksToRemove, true);
			++NumRemoved;
		}
	}
	return NumRemoved;
}

int32 FActiveDNAEffectsContainer::GetActiveEffectCount(const FDNAEffectQuery& Query, bool bEnforceOnGoingCheck) const
{
	int32 Count = 0;

	for (const FActiveDNAEffect& Effect : this)
	{
		if (!Effect.bIsInhibited || !bEnforceOnGoingCheck)
		{
			if (Query.Matches(Effect))
			{
				Count += Effect.Spec.StackCount;
			}
		}
	}

	return Count;
}

FOnDNAAttributeChange& FActiveDNAEffectsContainer::RegisterDNAAttributeEvent(FDNAAttribute Attribute)
{
	return AttributeChangeDelegates.FindOrAdd(Attribute);
}

bool FActiveDNAEffectsContainer::HasReceivedEffectWithPredictedKey(FPredictionKey PredictionKey) const
{
	for (const FActiveDNAEffect& Effect : this)
	{
		if (Effect.PredictionKey == PredictionKey && Effect.PredictionKey.WasReceived() == true)
		{
			return true;
		}
	}

	return false;
}

bool FActiveDNAEffectsContainer::HasPredictedEffectWithPredictedKey(FPredictionKey PredictionKey) const
{
	for (const FActiveDNAEffect& Effect : this)
	{
		if (Effect.PredictionKey == PredictionKey && Effect.PredictionKey.WasReceived() == false)
		{
			return true;
		}
	}

	return false;
}

void FActiveDNAEffectsContainer::GetActiveDNAEffectDataByAttribute(TMultiMap<FDNAAttribute, FActiveDNAEffectsContainer::DebugExecutedDNAEffectData>& EffectMap) const
{
	EffectMap.Empty();

	// Add all of the active DNA effects
	for (const FActiveDNAEffect& Effect : this)
	{
		if (Effect.Spec.Modifiers.Num() == Effect.Spec.Def->Modifiers.Num())
		{
			for (int32 Idx = 0; Idx < Effect.Spec.Modifiers.Num(); ++Idx)
			{
				FActiveDNAEffectsContainer::DebugExecutedDNAEffectData Data;
				Data.Attribute = Effect.Spec.Def->Modifiers[Idx].Attribute;
				Data.ActivationState = Effect.bIsInhibited ? TEXT("INHIBITED") : TEXT("ACTIVE");
				Data.DNAEffectName = Effect.Spec.Def->GetName();
				Data.ModifierOp = Effect.Spec.Def->Modifiers[Idx].ModifierOp;
				Data.Magnitude = Effect.Spec.Modifiers[Idx].GetEvaluatedMagnitude();
				if (Effect.Spec.StackCount > 1)
				{
					Data.Magnitude = DNAEffectUtilities::ComputeStackedModifierMagnitude(Data.Magnitude, Effect.Spec.StackCount, Data.ModifierOp);
				}
				Data.StackCount = Effect.Spec.StackCount;

				EffectMap.Add(Data.Attribute, Data);
			}
		}
	}
#if ENABLE_VISUAL_LOG
	// Add the executed DNA effects if we recorded them
	for (FActiveDNAEffectsContainer::DebugExecutedDNAEffectData Data : DebugExecutedDNAEffects)
	{
		EffectMap.Add(Data.Attribute, Data);
	}
#endif // ENABLE_VISUAL_LOG
}

#if ENABLE_VISUAL_LOG
void FActiveDNAEffectsContainer::GrabDebugSnapshot(FVisualLogEntry* Snapshot) const
{
	FVisualLogStatusCategory ActiveEffectsCategory;
	ActiveEffectsCategory.Category = TEXT("Effects");

	TMultiMap<FDNAAttribute, FActiveDNAEffectsContainer::DebugExecutedDNAEffectData> EffectMap;

	GetActiveDNAEffectDataByAttribute(EffectMap);

	// For each attribute that was modified go through all of its modifiers and list them
	TArray<FDNAAttribute> AttributeKeys;
	EffectMap.GetKeys(AttributeKeys);

	for (const FDNAAttribute& Attribute : AttributeKeys)
	{
		float CombinedModifierValue = 0.f;
		ActiveEffectsCategory.Add(TEXT(" --- Attribute --- "), Attribute.GetName());

		TArray<FActiveDNAEffectsContainer::DebugExecutedDNAEffectData> AttributeEffects;
		EffectMap.MultiFind(Attribute, AttributeEffects);

		for (const FActiveDNAEffectsContainer::DebugExecutedDNAEffectData& DebugData : AttributeEffects)
		{
			ActiveEffectsCategory.Add(DebugData.DNAEffectName, DebugData.ActivationState);
			ActiveEffectsCategory.Add(TEXT("Magnitude"), FString::Printf(TEXT("%f"), DebugData.Magnitude));

			if (DebugData.ActivationState != "INHIBITED")
			{
				CombinedModifierValue += DebugData.Magnitude;
			}
		}

		ActiveEffectsCategory.Add(TEXT("Total Modification"), FString::Printf(TEXT("%f"), CombinedModifierValue));
	}

	Snapshot->Status.Add(ActiveEffectsCategory);
}
#endif // ENABLE_VISUAL_LOG

void FActiveDNAEffectsContainer::DebugCyclicAggregatorBroadcasts(FAggregator* TriggeredAggregator)
{
	for (auto It = AttributeAggregatorMap.CreateIterator(); It; ++It)
	{
		FAggregatorRef AggregatorRef = It.Value();
		FDNAAttribute Attribute = It.Key();
		if (FAggregator* Aggregator = AggregatorRef.Get())
		{
			if (Aggregator == TriggeredAggregator)
			{
				ABILITY_LOG(Warning, TEXT(" Attribute %s was the triggered aggregator (%s)"), *Attribute.GetName(), *Owner->GetPathName());
			}
			else if (Aggregator->bIsBroadcastingDirty)
			{
				ABILITY_LOG(Warning, TEXT(" Attribute %s is broadcasting dirty (%s)"), *Attribute.GetName(), *Owner->GetPathName());
			}
			else
			{
				continue;
			}

			for (FActiveDNAEffectHandle Handle : Aggregator->Dependents)
			{
				UDNAAbilitySystemComponent* ASC = Handle.GetOwningDNAAbilitySystemComponent();
				if (ASC)
				{
					ABILITY_LOG(Warning, TEXT("  Dependant (%s) GE: %s"), *ASC->GetPathName(), *GetNameSafe(ASC->GetDNAEffectDefForHandle(Handle)));
				}
			}
		}
	}
}

void FActiveDNAEffectsContainer::CloneFrom(const FActiveDNAEffectsContainer& Source)
{
	// Make a full copy of the source's DNA effects
	DNAEffects_Internal = Source.DNAEffects_Internal;

	// Build our AttributeAggregatorMap by deep copying the source's
	AttributeAggregatorMap.Reset();

	TArray< TPair<FAggregatorRef, FAggregatorRef> >	SwappedAggregators;

	for (auto& It : Source.AttributeAggregatorMap)
	{
		const FDNAAttribute& Attribute = It.Key;
		const FAggregatorRef& SourceAggregatorRef = It.Value;

		FAggregatorRef& NewAggregatorRef = FindOrCreateAttributeAggregator(Attribute);
		FAggregator* NewAggregator = NewAggregatorRef.Get();
		FAggregator::FOnAggregatorDirty OnDirtyDelegate = NewAggregator->OnDirty;

		// Make full copy of the source aggregator
		*NewAggregator = *SourceAggregatorRef.Get();

		// But restore the OnDirty delegate to point to our proxy ASC
		NewAggregator->OnDirty = OnDirtyDelegate;

		TPair<FAggregatorRef, FAggregatorRef> SwappedPair;
		SwappedPair.Key = SourceAggregatorRef;
		SwappedPair.Value = NewAggregatorRef;

		SwappedAggregators.Add(SwappedPair);
	}

	// Make all of our copied GEs "unique" by giving them a new handle
	TMap<FActiveDNAEffectHandle, FActiveDNAEffectHandle> SwappedHandles;

	for (FActiveDNAEffect& Effect : this)
	{
		// Copy the Spec's context so we can modify it
		Effect.Spec.DuplicateEffectContext();
		Effect.Spec.SetupAttributeCaptureDefinitions();

		// For client only, capture attribute data since this data is constructed for replicated active DNA effects by default
		Effect.Spec.RecaptureAttributeDataForClone(Source.Owner, Owner);

		FActiveDNAEffectHandle& NewHandleRef = SwappedHandles.Add(Effect.Handle);
		Effect.Spec.CapturedRelevantAttributes.UnregisterLinkedAggregatorCallbacks(Effect.Handle);

		Effect.Handle = FActiveDNAEffectHandle::GenerateNewHandle(Owner);
		Effect.Spec.CapturedRelevantAttributes.RegisterLinkedAggregatorCallbacks(Effect.Handle);
		NewHandleRef = Effect.Handle;

		// Update any captured attribute references to the proxy source.
		for (TPair<FAggregatorRef, FAggregatorRef>& SwapAgg : SwappedAggregators)
		{
			Effect.Spec.CapturedRelevantAttributes.SwapAggregator( SwapAgg.Key, SwapAgg.Value );
		}
	}	

	// Now go through our aggregator map and replace dependency references to the source's GEs with our GEs.
	for (auto& It : AttributeAggregatorMap)
	{
		FDNAAttribute& Attribute = It.Key;
		FAggregatorRef& AggregatorRef = It.Value;
		FAggregator* Aggregator = AggregatorRef.Get();
		if (Aggregator)
		{
			Aggregator->OnActiveEffectDependenciesSwapped(SwappedHandles);
		}
	}

	// Broadcast dirty on everything so that the UAttributeSet properties get updated
	for (auto& It : AttributeAggregatorMap)
	{
		FAggregatorRef& AggregatorRef = It.Value;
		AggregatorRef.Get()->BroadcastOnDirty();
	}
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	Misc
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

namespace GlobalActiveDNAEffectHandles
{
	static TMap<FActiveDNAEffectHandle, TWeakObjectPtr<UDNAAbilitySystemComponent>>	Map;
}

void FActiveDNAEffectHandle::ResetGlobalHandleMap()
{
	GlobalActiveDNAEffectHandles::Map.Reset();
}

FActiveDNAEffectHandle FActiveDNAEffectHandle::GenerateNewHandle(UDNAAbilitySystemComponent* OwningComponent)
{
	static int32 GHandleID=0;
	FActiveDNAEffectHandle NewHandle(GHandleID++);

	TWeakObjectPtr<UDNAAbilitySystemComponent> WeakPtr(OwningComponent);

	GlobalActiveDNAEffectHandles::Map.Add(NewHandle, WeakPtr);

	return NewHandle;
}

UDNAAbilitySystemComponent* FActiveDNAEffectHandle::GetOwningDNAAbilitySystemComponent()
{
	TWeakObjectPtr<UDNAAbilitySystemComponent>* Ptr = GlobalActiveDNAEffectHandles::Map.Find(*this);
	if (Ptr)
	{
		return Ptr->Get();
	}

	return nullptr;	
}

const UDNAAbilitySystemComponent* FActiveDNAEffectHandle::GetOwningDNAAbilitySystemComponent() const
{
	TWeakObjectPtr<UDNAAbilitySystemComponent>* Ptr = GlobalActiveDNAEffectHandles::Map.Find(*this);
	if (Ptr)
	{
		return Ptr->Get();
	}

	return nullptr;
}

void FActiveDNAEffectHandle::RemoveFromGlobalMap()
{
	GlobalActiveDNAEffectHandles::Map.Remove(*this);
}

// -----------------------------------------------------------------

FDNAEffectQuery::FDNAEffectQuery()
	: EffectSource(nullptr),
	EffectDefinition(nullptr)
{
}

FDNAEffectQuery::FDNAEffectQuery(const FDNAEffectQuery& Other)
{
	*this = Other;
}


FDNAEffectQuery::FDNAEffectQuery(FActiveDNAEffectQueryCustomMatch InCustomMatchDelegate)
	: CustomMatchDelegate(InCustomMatchDelegate),
	EffectSource(nullptr),
	EffectDefinition(nullptr)
{
}

FDNAEffectQuery::FDNAEffectQuery(FDNAEffectQuery&& Other)
{
	*this = MoveTemp(Other);
}

FDNAEffectQuery& FDNAEffectQuery::operator=(FDNAEffectQuery&& Other)
{
	CustomMatchDelegate = MoveTemp(Other.CustomMatchDelegate);
	CustomMatchDelegate_BP = MoveTemp(Other.CustomMatchDelegate_BP);
	OwningTagQuery = MoveTemp(Other.OwningTagQuery);
	EffectTagQuery = MoveTemp(Other.EffectTagQuery);
	SourceTagQuery = MoveTemp(Other.SourceTagQuery);
	ModifyingAttribute = MoveTemp(Other.ModifyingAttribute);
	EffectSource = Other.EffectSource;
	EffectDefinition = Other.EffectDefinition;
	IgnoreHandles = MoveTemp(Other.IgnoreHandles);
	return *this;
}

FDNAEffectQuery& FDNAEffectQuery::operator=(const FDNAEffectQuery& Other)
{
	CustomMatchDelegate = Other.CustomMatchDelegate;
	CustomMatchDelegate_BP = Other.CustomMatchDelegate_BP;
	OwningTagQuery = Other.OwningTagQuery;
	EffectTagQuery = Other.EffectTagQuery;
	SourceTagQuery = Other.SourceTagQuery;
	ModifyingAttribute = Other.ModifyingAttribute;
	EffectSource = Other.EffectSource;
	EffectDefinition = Other.EffectDefinition;
	IgnoreHandles = Other.IgnoreHandles;
	return *this;
}


bool FDNAEffectQuery::Matches(const FActiveDNAEffect& Effect) const
{
	// since all of these query conditions must be met to be considered a match, failing
	// any one of them means we can return false

	// Anything in the ignore handle list is an immediate non-match
	if (IgnoreHandles.Contains(Effect.Handle))
	{
		return false;
	}

	if (CustomMatchDelegate.IsBound())
	{
		if (CustomMatchDelegate.Execute(Effect) == false)
		{
			return false;
		}
	}

	if (CustomMatchDelegate_BP.IsBound())
	{
		bool bDelegateMatches = false;
		CustomMatchDelegate_BP.Execute(Effect, bDelegateMatches);
		if (bDelegateMatches == false)
		{
			return false;
		}
	}

	return Matches(Effect.Spec);

}

bool FDNAEffectQuery::Matches(const FDNAEffectSpec& Spec) const
{
	if (Spec.Def == nullptr)
	{
		ABILITY_LOG(Error, TEXT("Matches called with no UDNAEffect def."));
		return false;
	}

	if (OwningTagQuery.IsEmpty() == false)
	{
		// Combine tags from the definition and the spec into one container to match queries that may span both
		// static to avoid memory allocations every time we do a query
		check(IsInGameThread());
		static FDNATagContainer TargetTags;
		TargetTags.Reset();
		if (Spec.Def->InheritableDNAEffectTags.CombinedTags.Num() > 0)
		{
			TargetTags.AppendTags(Spec.Def->InheritableDNAEffectTags.CombinedTags);
		}
		if (Spec.Def->InheritableOwnedTagsContainer.CombinedTags.Num() > 0)
		{
			TargetTags.AppendTags(Spec.Def->InheritableOwnedTagsContainer.CombinedTags);
		}
		if (Spec.DynamicGrantedTags.Num() > 0)
		{
			TargetTags.AppendTags(Spec.DynamicGrantedTags);
		}
		
		if (OwningTagQuery.Matches(TargetTags) == false)
		{
			return false;
		}
	}

	if (EffectTagQuery.IsEmpty() == false)
	{
		// Combine tags from the definition and the spec into one container to match queries that may span both
		// static to avoid memory allocations every time we do a query
		check(IsInGameThread());
		static FDNATagContainer GETags;
		GETags.Reset();
		if (Spec.Def->InheritableDNAEffectTags.CombinedTags.Num() > 0)
		{
			GETags.AppendTags(Spec.Def->InheritableDNAEffectTags.CombinedTags);
		}
		if (Spec.DynamicAssetTags.Num() > 0)
		{
			GETags.AppendTags(Spec.DynamicAssetTags);
		}

		if (EffectTagQuery.Matches(GETags) == false)
		{
			return false;
		}
	}

	if (SourceTagQuery.IsEmpty() == false)
	{
		FDNATagContainer const& SourceTags = Spec.CapturedSourceTags.GetSpecTags();
		if (SourceTagQuery.Matches(SourceTags) == false)
		{
			return false;
		}
	}

	// if we are looking for ModifyingAttribute go over each of the Spec Modifiers and check the Attributes
	if (ModifyingAttribute.IsValid())
	{
		bool bEffectModifiesThisAttribute = false;

		for (int32 ModIdx = 0; ModIdx < Spec.Modifiers.Num(); ++ModIdx)
		{
			const FDNAModifierInfo& ModDef = Spec.Def->Modifiers[ModIdx];
			const FModifierSpec& ModSpec = Spec.Modifiers[ModIdx];

			if (ModDef.Attribute == ModifyingAttribute)
			{
				bEffectModifiesThisAttribute = true;
				break;
			}
		}
		if (bEffectModifiesThisAttribute == false)
		{
			// effect doesn't modify the attribute we are looking for, no match
			return false;
		}
	}

	// check source object
	if (EffectSource != nullptr)
	{
		if (Spec.GetEffectContext().GetSourceObject() != EffectSource)
		{
			return false;
		}
	}

	// check definition
	if (EffectDefinition != nullptr)
	{
		if (Spec.Def != EffectDefinition.GetDefaultObject())
		{
			return false;
		}
	}

	return true;
}

bool FDNAEffectQuery::IsEmpty() const
{
	return 
	(
		OwningTagQuery.IsEmpty() &&
		EffectTagQuery.IsEmpty() &&
		SourceTagQuery.IsEmpty() &&
		!ModifyingAttribute.IsValid() &&
		!EffectSource &&
		!EffectDefinition
	);
}

// static
FDNAEffectQuery FDNAEffectQuery::MakeQuery_MatchAnyOwningTags(const FDNATagContainer& InTags)
{
	SCOPE_CYCLE_COUNTER(STAT_MakeDNAEffectQuery);
	FDNAEffectQuery OutQuery;
	OutQuery.OwningTagQuery = FDNATagQuery::MakeQuery_MatchAnyTags(InTags);
	return OutQuery;
}

// static
FDNAEffectQuery FDNAEffectQuery::MakeQuery_MatchAllOwningTags(const FDNATagContainer& InTags)
{
	SCOPE_CYCLE_COUNTER(STAT_MakeDNAEffectQuery);
	FDNAEffectQuery OutQuery;
	OutQuery.OwningTagQuery = FDNATagQuery::MakeQuery_MatchAllTags(InTags);
	return OutQuery;
}

// static
FDNAEffectQuery FDNAEffectQuery::MakeQuery_MatchNoOwningTags(const FDNATagContainer& InTags)
{
	SCOPE_CYCLE_COUNTER(STAT_MakeDNAEffectQuery);
	FDNAEffectQuery OutQuery;
	OutQuery.OwningTagQuery = FDNATagQuery::MakeQuery_MatchNoTags(InTags);
	return OutQuery;
}

// static
FDNAEffectQuery FDNAEffectQuery::MakeQuery_MatchAnyEffectTags(const FDNATagContainer& InTags)
{
	SCOPE_CYCLE_COUNTER(STAT_MakeDNAEffectQuery);
	FDNAEffectQuery OutQuery;
	OutQuery.EffectTagQuery = FDNATagQuery::MakeQuery_MatchAnyTags(InTags);
	return OutQuery;
}

// static
FDNAEffectQuery FDNAEffectQuery::MakeQuery_MatchAllEffectTags(const FDNATagContainer& InTags)
{
	SCOPE_CYCLE_COUNTER(STAT_MakeDNAEffectQuery);
	FDNAEffectQuery OutQuery;
	OutQuery.EffectTagQuery = FDNATagQuery::MakeQuery_MatchAllTags(InTags);
	return OutQuery;
}

// static
FDNAEffectQuery FDNAEffectQuery::MakeQuery_MatchNoEffectTags(const FDNATagContainer& InTags)
{
	SCOPE_CYCLE_COUNTER(STAT_MakeDNAEffectQuery);
	FDNAEffectQuery OutQuery;
	OutQuery.EffectTagQuery = FDNATagQuery::MakeQuery_MatchNoTags(InTags);
	return OutQuery;
}

// static
FDNAEffectQuery FDNAEffectQuery::MakeQuery_MatchAnySourceTags(const FDNATagContainer& InTags)
{
	SCOPE_CYCLE_COUNTER(STAT_MakeDNAEffectQuery);
	FDNAEffectQuery OutQuery;
	OutQuery.SourceTagQuery = FDNATagQuery::MakeQuery_MatchAnyTags(InTags);
	return OutQuery;
}

// static
FDNAEffectQuery FDNAEffectQuery::MakeQuery_MatchAllSourceTags(const FDNATagContainer& InTags)
{
	SCOPE_CYCLE_COUNTER(STAT_MakeDNAEffectQuery);
	FDNAEffectQuery OutQuery;
	OutQuery.SourceTagQuery = FDNATagQuery::MakeQuery_MatchAllTags(InTags);
	return OutQuery;
}

// static
FDNAEffectQuery FDNAEffectQuery::MakeQuery_MatchNoSourceTags(const FDNATagContainer& InTags)
{
	SCOPE_CYCLE_COUNTER(STAT_MakeDNAEffectQuery);
	FDNAEffectQuery OutQuery;
	OutQuery.SourceTagQuery = FDNATagQuery::MakeQuery_MatchNoTags(InTags);
	return OutQuery;
}

bool FDNAModifierInfo::operator==(const FDNAModifierInfo& Other) const
{
	if (Attribute != Other.Attribute)
	{
		return false;
	}

	if (ModifierOp != Other.ModifierOp)
	{
		return false;
	}

	if (ModifierMagnitude != Other.ModifierMagnitude)
	{
		return false;
	}

	if (SourceTags.RequireTags.Num() != Other.SourceTags.RequireTags.Num() || !SourceTags.RequireTags.HasAll(Other.SourceTags.RequireTags))
	{
		return false;
	}
	if (SourceTags.IgnoreTags.Num() != Other.SourceTags.IgnoreTags.Num() || !SourceTags.IgnoreTags.HasAll(Other.SourceTags.IgnoreTags))
	{
		return false;
	}

	if (TargetTags.RequireTags.Num() != Other.TargetTags.RequireTags.Num() || !TargetTags.RequireTags.HasAll(Other.TargetTags.RequireTags))
	{
		return false;
	}
	if (TargetTags.IgnoreTags.Num() != Other.TargetTags.IgnoreTags.Num() || !TargetTags.IgnoreTags.HasAll(Other.TargetTags.IgnoreTags))
	{
		return false;
	}

	return true;
}

bool FDNAModifierInfo::operator!=(const FDNAModifierInfo& Other) const
{
	return !(*this == Other);
}

void FInheritedTagContainer::UpdateInheritedTagProperties(const FInheritedTagContainer* Parent)
{
	// Make sure we've got a fresh start
	CombinedTags.Reset();

	// Re-add the Parent's tags except the one's we have removed
	if (Parent)
	{
		for (auto Itr = Parent->CombinedTags.CreateConstIterator(); Itr; ++Itr)
		{
			if (!Itr->MatchesAny(Removed))
			{
				CombinedTags.AddTag(*Itr);
			}
		}
	}

	// Add our own tags
	for (auto Itr = Added.CreateConstIterator(); Itr; ++Itr)
	{
		// Remove trumps add for explicit matches but not for parent tags.
		// This lets us remove all inherited tags starting with Foo but still add Foo.Bar
		if (!Removed.HasTagExact(*Itr))
		{
			CombinedTags.AddTag(*Itr);
		}
	}
}

void FInheritedTagContainer::PostInitProperties()
{
	// we shouldn't inherit the added and removed tags from our parents
	// make sure that these fields are clear
	Added.Reset();
	Removed.Reset();
}

void FInheritedTagContainer::AddTag(const FDNATag& TagToAdd)
{
	CombinedTags.AddTag(TagToAdd);
}

void FInheritedTagContainer::RemoveTag(FDNATag TagToRemove)
{
	CombinedTags.RemoveTag(TagToRemove);
}

void FActiveDNAEffectsContainer::IncrementLock()
{
	ScopedLockCount++;
}

void FActiveDNAEffectsContainer::DecrementLock()
{
	if (--ScopedLockCount == 0)
	{
		// ------------------------------------------
		// Move any pending effects onto the real list
		// ------------------------------------------
		FActiveDNAEffect* PendingDNAEffect = PendingDNAEffectHead;
		FActiveDNAEffect* Stop = *PendingDNAEffectNext;
		bool ModifiedArray = false;

		while (PendingDNAEffect != Stop)
		{
			if (!PendingDNAEffect->IsPendingRemove)
			{
				DNAEffects_Internal.Add(MoveTemp(*PendingDNAEffect));
				ModifiedArray = true;
			}
			else
			{
				PendingRemoves--;
			}
			PendingDNAEffect = PendingDNAEffect->PendingNext;
		}

		// Reset our pending DNAEffect linked list
		PendingDNAEffectNext = &PendingDNAEffectHead;

		// -----------------------------------------
		// Delete any pending remove effects
		// -----------------------------------------
		for (int32 idx=DNAEffects_Internal.Num()-1; idx >= 0 && PendingRemoves > 0; --idx)
		{
			FActiveDNAEffect& Effect = DNAEffects_Internal[idx];

			if (Effect.IsPendingRemove)
			{
				ABILITY_LOG(Verbose, TEXT("DecrementLock decrementing a pending remove: Auth: %s Handle: %s Def: %s"), IsNetAuthority() ? TEXT("TRUE") : TEXT("FALSE"), *Effect.Handle.ToString(), Effect.Spec.Def ? *Effect.Spec.Def->GetName() : TEXT("NONE"));
				DNAEffects_Internal.RemoveAtSwap(idx, 1, false);
				ModifiedArray = true;
				PendingRemoves--;
			}
		}

		if (!ensure(PendingRemoves == 0))
		{
			ABILITY_LOG(Error, TEXT("~FScopedActiveDNAEffectLock has %d pending removes after a scope lock removal"), PendingRemoves);
			PendingRemoves = 0;
		}

		if (ModifiedArray)
		{
			MarkArrayDirty();
		}
	}
}

FScopedActiveDNAEffectLock::FScopedActiveDNAEffectLock(FActiveDNAEffectsContainer& InContainer)
	: Container(InContainer)
{
	Container.IncrementLock();
}

FScopedActiveDNAEffectLock::~FScopedActiveDNAEffectLock()
{
	Container.DecrementLock();
}

