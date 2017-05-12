// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UnrealType.h"
#include "DNATagContainer.h"
#include "DNAEffectTypes.h"
#include "DNAEffectAggregator.h"
#include "DNAEffect.h"
#include "DNAEffectCalculation.h"
#include "DNAEffectExecutionCalculation.generated.h"

class UDNAAbilitySystemComponent;

/** Struct representing parameters for a custom DNA effect execution. Should not be held onto via reference, used just for the scope of the execution */
USTRUCT()
struct DNAABILITIES_API FDNAEffectCustomExecutionParameters
{
	GENERATED_USTRUCT_BODY()

public:

	// Constructors
	FDNAEffectCustomExecutionParameters();
	FDNAEffectCustomExecutionParameters(FDNAEffectSpec& InOwningSpec, const TArray<FDNAEffectExecutionScopedModifierInfo>& InScopedMods, UDNAAbilitySystemComponent* InTargetAbilityComponent, const FDNATagContainer& InPassedIntags, const FPredictionKey& InPredictionKey);
	FDNAEffectCustomExecutionParameters(FDNAEffectSpec& InOwningSpec, const TArray<FDNAEffectExecutionScopedModifierInfo>& InScopedMods, UDNAAbilitySystemComponent* InTargetAbilityComponent, const FDNATagContainer& InPassedIntags, const FPredictionKey& InPredictionKey, const TArray<FActiveDNAEffectHandle>& InIgnoreHandles);

	/** Simple accessor to owning DNA spec */
	const FDNAEffectSpec& GetOwningSpec() const;

	/** Non const access. Be careful with this, especially when modifying a spec after attribute capture. */
	FDNAEffectSpec* GetOwningSpecForPreExecuteMod() const;

	/** Simple accessor to target ability system component */
	UDNAAbilitySystemComponent* GetTargetDNAAbilitySystemComponent() const;

	/** Simple accessor to source ability system component (could be null!) */
	UDNAAbilitySystemComponent* GetSourceDNAAbilitySystemComponent() const;
	
	/** Simple accessor to the Passed In Tags to this execution */
	const FDNATagContainer& GetPassedInTags() const;

	TArray<FActiveDNAEffectHandle> GetIgnoreHandles() const;
	
	FPredictionKey GetPredictionKey() const;

	/**
	 * Attempts to calculate the magnitude of a captured attribute given the specified parameters. Can fail if the DNA spec doesn't have
	 * a valid capture for the attribute.
	 * 
	 * @param InCaptureDef	Attribute definition to attempt to calculate the magnitude of
	 * @param InEvalParams	Parameters to evaluate the attribute under
	 * @param OutMagnitude	[OUT] Computed magnitude
	 * 
	 * @return True if the magnitude was successfully calculated, false if it was not
	 */
	bool AttemptCalculateCapturedAttributeMagnitude(const FDNAEffectAttributeCaptureDefinition& InCaptureDef, const FAggregatorEvaluateParameters& InEvalParams, OUT float& OutMagnitude) const;
	
	/**
	 * Attempts to calculate the magnitude of a captured attribute given the specified parameters, including a starting base value. 
	 * Can fail if the DNA spec doesn't have a valid capture for the attribute.
	 * 
	 * @param InCaptureDef	Attribute definition to attempt to calculate the magnitude of
	 * @param InEvalParams	Parameters to evaluate the attribute under
	 * @param InBaseValue	Base value to evaluate the attribute under
	 * @param OutMagnitude	[OUT] Computed magnitude
	 * 
	 * @return True if the magnitude was successfully calculated, false if it was not
	 */
	bool AttemptCalculateCapturedAttributeMagnitudeWithBase(const FDNAEffectAttributeCaptureDefinition& InCaptureDef, const FAggregatorEvaluateParameters& InEvalParams, float InBaseValue, OUT float& OutMagnitude) const;

	/**
	 * Attempts to calculate the base value of a captured attribute given the specified parameters. Can fail if the DNA spec doesn't have
	 * a valid capture for the attribute.
	 * 
	 * @param InCaptureDef	Attribute definition to attempt to calculate the base value of
	 * @param OutBaseValue	[OUT] Computed base value
	 * 
	 * @return True if the base value was successfully calculated, false if it was not
	 */
	bool AttemptCalculateCapturedAttributeBaseValue(const FDNAEffectAttributeCaptureDefinition& InCaptureDef, OUT float& OutBaseValue) const;

	/**
	 * Attempts to calculate the bonus magnitude of a captured attribute given the specified parameters. Can fail if the DNA spec doesn't have
	 * a valid capture for the attribute.
	 * 
	 * @param InCaptureDef		Attribute definition to attempt to calculate the bonus magnitude of
	 * @param InEvalParams		Parameters to evaluate the attribute under
	 * @param OutBonusMagnitude	[OUT] Computed bonus magnitude
	 * 
	 * @return True if the bonus magnitude was successfully calculated, false if it was not
	 */
	bool AttemptCalculateCapturedAttributeBonusMagnitude(const FDNAEffectAttributeCaptureDefinition& InCaptureDef, const FAggregatorEvaluateParameters& InEvalParams, OUT float& OutBonusMagnitude) const;
	
	/**
	 * Attempts to populate the specified aggregator with a snapshot of a backing captured aggregator. Can fail if the DNA spec doesn't have
	 * a valid capture for the attribute.
	 * 
	 * @param InCaptureDef				Attribute definition to attempt to snapshot
	 * @param OutSnapshottedAggregator	[OUT] Snapshotted aggregator, if possible
	 * 
	 * @return True if the aggregator was successfully snapshotted, false if it was not
	 */
	bool AttemptGetCapturedAttributeAggregatorSnapshot(const FDNAEffectAttributeCaptureDefinition& InCaptureDef, OUT FAggregator& OutSnapshottedAggregator) const;

private:

	/** Mapping of capture definition to aggregator with scoped modifiers added in; Used to process scoped modifiers w/o modifying underlying aggregators in the capture */
	TMap<FDNAEffectAttributeCaptureDefinition, FAggregator> ScopedModifierAggregators;

	/** Owning DNA effect spec */
	FDNAEffectSpec* OwningSpec;

	/** Target ability system component of the execution */
	TWeakObjectPtr<UDNAAbilitySystemComponent> TargetDNAAbilitySystemComponent;

	/** The extra tags that were passed in to this execution */
	FDNATagContainer PassedInTags;

	TArray<FActiveDNAEffectHandle> IgnoreHandles;
	
	FPredictionKey PredictionKey;
};

/** Struct representing the output of a custom DNA effect execution. */
USTRUCT()
struct DNAABILITIES_API FDNAEffectCustomExecutionOutput
{
	GENERATED_USTRUCT_BODY()

public:

	/** Constructor */
	FDNAEffectCustomExecutionOutput();

	/** Mark that the execution has manually handled the stack count and the GE system should not attempt to automatically act upon it for emitted modifiers */
	void MarkStackCountHandledManually();

	/** Simple accessor for determining whether the execution has manually handled the stack count or not */
	bool IsStackCountHandledManually() const;

	/** Accessor for determining if DNACue events have already been handled */
	bool AreDNACuesHandledManually() const;

	/** Mark that the execution wants conditional DNA effects to trigger */
	void MarkConditionalDNAEffectsToTrigger();

	/** Mark that the execution wants conditional DNA effects to trigger */
	void MarkDNACuesHandledManually();

	/** Simple accessor for determining whether the execution wants conditional DNA effects to trigger or not */
	bool ShouldTriggerConditionalDNAEffects() const;

	/** Add the specified evaluated data to the execution's output modifiers */
	void AddOutputModifier(const FDNAModifierEvaluatedData& InOutputMod);

	/** Simple accessor to output modifiers of the execution */
	const TArray<FDNAModifierEvaluatedData>& GetOutputModifiers() const;

	/** Simple accessor to output modifiers of the execution */
	void GetOutputModifiers(OUT TArray<FDNAModifierEvaluatedData>& OutOutputModifiers) const;

	/** Returns direct access to output modifiers of the execution (avoid copy) */
	TArray<FDNAModifierEvaluatedData>& GetOutputModifiersRef() { return OutputModifiers; }

private:

	/** Modifiers emitted by the execution */
	UPROPERTY()
	TArray<FDNAModifierEvaluatedData> OutputModifiers;

	/** If true, the execution wants to trigger conditional DNA effects when it completes */
	UPROPERTY()
	uint32 bTriggerConditionalDNAEffects : 1;
	
	/** If true, the execution itself has manually handled the stack count of the effect and the GE system doesn't have to automatically handle it */
	UPROPERTY()
	uint32 bHandledStackCountManually : 1;

	/** If true, the execution itself has manually invoked all DNA cues and the GE system doesn't have to automatically handle them. */
	UPROPERTY()
	uint32 bHandledDNACuesManually : 1;
};

UCLASS(BlueprintType, Blueprintable, Abstract)
class DNAABILITIES_API UDNAEffectExecutionCalculation : public UDNAEffectCalculation
{
	GENERATED_UCLASS_BODY()

protected:

	/** Used to indicate if this execution uses Passed In Tags */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Attributes)
	bool bRequiresPassedInTags;

#if WITH_EDITORONLY_DATA

protected:

	/** Any attribute in this list will not show up as a valid option for scoped modifiers; Used to allow attribute capture for internal calculation while preventing modification */
	UPROPERTY(EditDefaultsOnly, Category=Attributes)
	TArray<FDNAEffectAttributeCaptureDefinition> InvalidScopedModifierAttributes;

public:
	/**
	 * Gets the collection of capture attribute definitions that the calculation class will accept as valid scoped modifiers
	 * 
	 * @param OutScopableModifiers	[OUT] Array to populate with definitions valid as scoped modifiers
	 */
	virtual void GetValidScopedModifierAttributeCaptureDefinitions(OUT TArray<FDNAEffectAttributeCaptureDefinition>& OutScopableModifiers) const;

	/** Returns if this execution requires passed in tags */
	virtual bool DoesRequirePassedInTags() const;

#endif // #if WITH_EDITORONLY_DATA

public:

	/**
	 * Called whenever the owning DNA effect is executed. Allowed to do essentially whatever is desired, including generating new
	 * modifiers to instantly execute as well.
	 * 
	 * @note: Native subclasses should override the auto-generated Execute_Implementation function and NOT this one.
	 * 
	 * @param ExecutionParams		Parameters for the custom execution calculation
	 * @param OutExecutionOutput	[OUT] Output data populated by the execution detailing further behavior or results of the execution
	 */
	UFUNCTION(BlueprintNativeEvent, Category="Calculation")
	void Execute(const FDNAEffectCustomExecutionParameters& ExecutionParams, FDNAEffectCustomExecutionOutput& OutExecutionOutput) const;
};



// -------------------------------------------------------------------------
//	Helper macros for declaring attribute captures 
// -------------------------------------------------------------------------

#define DECLARE_ATTRIBUTE_CAPTUREDEF(P) \
	UProperty* P##Property; \
	FDNAEffectAttributeCaptureDefinition P##Def; \

#define DEFINE_ATTRIBUTE_CAPTUREDEF(S, P, T, B) \
{ \
	P##Property = FindFieldChecked<UProperty>(S::StaticClass(), GET_MEMBER_NAME_CHECKED(S, P)); \
	P##Def = FDNAEffectAttributeCaptureDefinition(P##Property, EDNAEffectAttributeCaptureSource::T, B); \
}
