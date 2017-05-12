// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Templates/SubclassOf.h"
#include "Engine/NetSerialization.h"
#include "Engine/EngineTypes.h"
#include "DNATagContainer.h"
#include "Engine/CurveTable.h"
#include "AttributeSet.h"
#include "EngineDefines.h"
#include "DNAEffectTypes.h"
#include "DNAEffectAggregator.h"
#include "DNAPrediction.h"
#include "DNATagAssetInterface.h"
#include "DNAAbilitySpec.h"
#include "ActiveDNAEffectIterator.h"
#include "UObject/ObjectKey.h"
#include "DNAEffect.generated.h"

class UDNAAbilitySystemComponent;
class UDNAEffect;
class UDNAEffectCustomApplicationRequirement;
class UDNAEffectExecutionCalculation;
class UDNAEffectTemplate;
class UDNAModMagnitudeCalculation;
struct FActiveDNAEffectsContainer;
struct FDNAEffectModCallbackData;
struct FDNAEffectSpec;

/** Enumeration outlining the possible DNA effect magnitude calculation policies. */
UENUM()
enum class EDNAEffectMagnitudeCalculation : uint8
{
	/** Use a simple, scalable float for the calculation. */
	ScalableFloat,
	/** Perform a calculation based upon an attribute. */
	AttributeBased,
	/** Perform a custom calculation, capable of capturing and acting on multiple attributes, in either BP or native. */
	CustomCalculationClass,	
	/** This magnitude will be set explicitly by the code/blueprint that creates the spec. */
	SetByCaller,
};

/** Enumeration outlining the possible attribute based float calculation policies. */
UENUM()
enum class EAttributeBasedFloatCalculationType : uint8
{
	/** Use the final evaluated magnitude of the attribute. */
	AttributeMagnitude,
	/** Use the base value of the attribute. */
	AttributeBaseValue,
	/** Use the "bonus" evaluated magnitude of the attribute: Equivalent to (FinalMag - BaseValue). */
	AttributeBonusMagnitude,
	/** Use a calculated magnitude stopping with the evaluation of the specified "Final Channel" */
	AttributeMagnitudeEvaluatedUpToChannel
};

struct DNAABILITIES_API FDNAEffectConstants
{
	/** Infinite duration */
	static const float INFINITE_DURATION;

	/** No duration; Time specifying instant application of an effect */
	static const float INSTANT_APPLICATION;

	/** Constant specifying that the combat effect has no period and doesn't check for over time application */
	static const float NO_PERIOD;

	/** No Level/Level not set */
	static const float INVALID_LEVEL;
};

/** 
 * Struct representing a float whose magnitude is dictated by a backing attribute and a calculation policy, follows basic form of:
 * (Coefficient * (PreMultiplyAdditiveValue + [Eval'd Attribute Value According to Policy])) + PostMultiplyAdditiveValue
 */
USTRUCT()
struct FAttributeBasedFloat
{
	GENERATED_USTRUCT_BODY()

public:

	/** Constructor */
	FAttributeBasedFloat()
		: Coefficient(1.f)
		, PreMultiplyAdditiveValue(0.f)
		, PostMultiplyAdditiveValue(0.f)
		, BackingAttribute()
		, AttributeCalculationType(EAttributeBasedFloatCalculationType::AttributeMagnitude)
		, FinalChannel(EDNAModEvaluationChannel::Channel0)
	{}

	/**
	 * Calculate and return the magnitude of the float given the specified DNA effect spec.
	 * 
	 * @note:	This function assumes (and asserts on) the existence of the required captured attribute within the spec.
	 *			It is the responsibility of the caller to verify that the spec is properly setup before calling this function.
	 *			
	 *	@param InRelevantSpec	DNA effect spec providing the backing attribute capture
	 *	
	 *	@return Evaluated magnitude based upon the spec & calculation policy
	 */
	float CalculateMagnitude(const FDNAEffectSpec& InRelevantSpec) const;

	/** Coefficient to the attribute calculation */
	UPROPERTY(EditDefaultsOnly, Category=AttributeFloat)
	FScalableFloat Coefficient;

	/** Additive value to the attribute calculation, added in before the coefficient applies */
	UPROPERTY(EditDefaultsOnly, Category=AttributeFloat)
	FScalableFloat PreMultiplyAdditiveValue;

	/** Additive value to the attribute calculation, added in after the coefficient applies */
	UPROPERTY(EditDefaultsOnly, Category=AttributeFloat)
	FScalableFloat PostMultiplyAdditiveValue;

	/** Attribute backing the calculation */
	UPROPERTY(EditDefaultsOnly, Category=AttributeFloat)
	FDNAEffectAttributeCaptureDefinition BackingAttribute;

	/** If a curve table entry is specified, the attribute will be used as a lookup into the curve instead of using the attribute directly. */
	UPROPERTY(EditDefaultsOnly, Category=AttributeFloat)
	FCurveTableRowHandle AttributeCurve;

	/** Calculation policy in regards to the attribute */
	UPROPERTY(EditDefaultsOnly, Category=AttributeFloat)
	EAttributeBasedFloatCalculationType AttributeCalculationType;

	/** Channel to terminate evaluation on when using AttributeEvaluatedUpToChannel calculation type */
	UPROPERTY(EditDefaultsOnly, Category=AttributeFloat)
	EDNAModEvaluationChannel FinalChannel;

	/** Filter to use on source tags; If specified, only modifiers applied with all of these tags will factor into the calculation */
	UPROPERTY(EditDefaultsOnly, Category=AttributeFloat)
	FDNATagContainer SourceTagFilter;

	/** Filter to use on target tags; If specified, only modifiers applied with all of these tags will factor into the calculation */
	UPROPERTY(EditDefaultsOnly, Category=AttributeFloat)
	FDNATagContainer TargetTagFilter;

	/** Equality/Inequality operators */
	bool operator==(const FAttributeBasedFloat& Other) const;
	bool operator!=(const FAttributeBasedFloat& Other) const;
};

/** Structure to encapsulate magnitudes that are calculated via custom calculation */
USTRUCT()
struct FCustomCalculationBasedFloat
{
	GENERATED_USTRUCT_BODY()

	FCustomCalculationBasedFloat()
		: CalculationClassMagnitude(nullptr)
		, Coefficient(1.f)
		, PreMultiplyAdditiveValue(0.f)
		, PostMultiplyAdditiveValue(0.f)
	{}

public:

	/**
	 * Calculate and return the magnitude of the float given the specified DNA effect spec.
	 * 
	 * @note:	This function assumes (and asserts on) the existence of the required captured attribute within the spec.
	 *			It is the responsibility of the caller to verify that the spec is properly setup before calling this function.
	 *			
	 *	@param InRelevantSpec	DNA effect spec providing the backing attribute capture
	 *	
	 *	@return Evaluated magnitude based upon the spec & calculation policy
	 */
	float CalculateMagnitude(const FDNAEffectSpec& InRelevantSpec) const;

	UPROPERTY(EditDefaultsOnly, Category=CustomCalculation, DisplayName="Calculation Class")
	TSubclassOf<UDNAModMagnitudeCalculation> CalculationClassMagnitude;

	/** Coefficient to the custom calculation */
	UPROPERTY(EditDefaultsOnly, Category=CustomCalculation)
	FScalableFloat Coefficient;

	/** Additive value to the attribute calculation, added in before the coefficient applies */
	UPROPERTY(EditDefaultsOnly, Category=AttributeFloat)
	FScalableFloat PreMultiplyAdditiveValue;

	/** Additive value to the attribute calculation, added in after the coefficient applies */
	UPROPERTY(EditDefaultsOnly, Category=AttributeFloat)
	FScalableFloat PostMultiplyAdditiveValue;

	/** Equality/Inequality operators */
	bool operator==(const FCustomCalculationBasedFloat& Other) const;
	bool operator!=(const FCustomCalculationBasedFloat& Other) const;
};

/** Struct for holding SetBytCaller data */
USTRUCT()
struct FSetByCallerFloat
{
	GENERATED_USTRUCT_BODY()

	FSetByCallerFloat()
	: DataName(NAME_None)
	{}

	/** The Name the caller (code or blueprint) will use to set this magnitude by. */
	UPROPERTY(EditDefaultsOnly, Category=SetByCaller)
	FName	DataName;

	/** Equality/Inequality operators */
	bool operator==(const FSetByCallerFloat& Other) const;
	bool operator!=(const FSetByCallerFloat& Other) const;
};

/** Struct representing the magnitude of a DNA effect modifier, potentially calculated in numerous different ways */
USTRUCT()
struct DNAABILITIES_API FDNAEffectModifierMagnitude
{
	GENERATED_USTRUCT_BODY()

public:

	/** Default Constructor */
	FDNAEffectModifierMagnitude()
		: MagnitudeCalculationType(EDNAEffectMagnitudeCalculation::ScalableFloat)
	{
	}

	/** Constructors for setting value in code (for automation tests) */
	FDNAEffectModifierMagnitude(const FScalableFloat& Value)
		: MagnitudeCalculationType(EDNAEffectMagnitudeCalculation::ScalableFloat)
		, ScalableFloatMagnitude(Value)
	{
	}
	FDNAEffectModifierMagnitude(const FAttributeBasedFloat& Value)
		: MagnitudeCalculationType(EDNAEffectMagnitudeCalculation::AttributeBased)
		, AttributeBasedMagnitude(Value)
	{
	}
	FDNAEffectModifierMagnitude(const FCustomCalculationBasedFloat& Value)
		: MagnitudeCalculationType(EDNAEffectMagnitudeCalculation::CustomCalculationClass)
		, CustomMagnitude(Value)
	{
	}
	FDNAEffectModifierMagnitude(const FSetByCallerFloat& Value)
		: MagnitudeCalculationType(EDNAEffectMagnitudeCalculation::SetByCaller)
		, SetByCallerMagnitude(Value)
	{
	}
 
	/**
	 * Determines if the magnitude can be properly calculated with the specified DNA effect spec (could fail if relying on an attribute not present, etc.)
	 * 
	 * @param InRelevantSpec	DNA effect spec to check for magnitude calculation
	 * 
	 * @return Whether or not the magnitude can be properly calculated
	 */
	bool CanCalculateMagnitude(const FDNAEffectSpec& InRelevantSpec) const;

	/**
	 * Attempts to calculate the magnitude given the provided spec. May fail if necessary information (such as captured attributes) is missing from
	 * the spec.
	 * 
	 * @param InRelevantSpec			DNA effect spec to use to calculate the magnitude with
	 * @param OutCalculatedMagnitude	[OUT] Calculated value of the magnitude, will be set to 0.f in the event of failure
	 * 
	 * @return True if the calculation was successful, false if it was not
	 */
	bool AttemptCalculateMagnitude(const FDNAEffectSpec& InRelevantSpec, OUT float& OutCalculatedMagnitude, bool WarnIfSetByCallerFail=true, float DefaultSetbyCaller=0.f) const;

	/** Attempts to recalculate the magnitude given a changed aggregator. This will only recalculate if we are a modifier that is linked (non snapshot) to the given aggregator. */
	bool AttemptRecalculateMagnitudeFromDependentAggregatorChange(const FDNAEffectSpec& InRelevantSpec, OUT float& OutCalculatedMagnitude, const FAggregator* ChangedAggregator) const;

	/**
	 * Gather all of the attribute capture definitions necessary to compute the magnitude and place them into the provided array
	 * 
	 * @param OutCaptureDefs	[OUT] Array populated with necessary attribute capture definitions
	 */
	void GetAttributeCaptureDefinitions(OUT TArray<FDNAEffectAttributeCaptureDefinition>& OutCaptureDefs) const;

	EDNAEffectMagnitudeCalculation GetMagnitudeCalculationType() const { return MagnitudeCalculationType; }

	/** Returns the magnitude as it was entered in data. Only applies to ScalableFloat or any other type that can return data without context */
	bool GetStaticMagnitudeIfPossible(float InLevel, float& OutMagnitude, const FString* ContextString = nullptr) const;

	/** Returns the DataName associated with this magnitude if it is set by caller */
	bool GetSetByCallerDataNameIfPossible(FName& OutDataName) const;

	/** Returns the custom magnitude calculation class, if any, for this magnitude. Only applies to CustomMagnitudes */
	TSubclassOf<UDNAModMagnitudeCalculation> GetCustomMagnitudeCalculationClass() const;

	bool operator==(const FDNAEffectModifierMagnitude& Other) const;
	bool operator!=(const FDNAEffectModifierMagnitude& Other) const;

#if WITH_EDITOR
	FText GetValueForEditorDisplay() const;
	void ReportErrors(const FString& PathName) const;
#endif

protected:

	/** Type of calculation to perform to derive the magnitude */
	UPROPERTY(EditDefaultsOnly, Category=Magnitude)
	EDNAEffectMagnitudeCalculation MagnitudeCalculationType;

	/** Magnitude value represented by a scalable float */
	UPROPERTY(EditDefaultsOnly, Category=Magnitude)
	FScalableFloat ScalableFloatMagnitude;

	/** Magnitude value represented by an attribute-based float
	(Coefficient * (PreMultiplyAdditiveValue + [Eval'd Attribute Value According to Policy])) + PostMultiplyAdditiveValue */
	UPROPERTY(EditDefaultsOnly, Category=Magnitude)
	FAttributeBasedFloat AttributeBasedMagnitude;

	/** Magnitude value represented by a custom calculation class */
	UPROPERTY(EditDefaultsOnly, Category=Magnitude)
	FCustomCalculationBasedFloat CustomMagnitude;

	/** Magnitude value represented by a SetByCaller magnitude */
	UPROPERTY(EditDefaultsOnly, Category=Magnitude)
	FSetByCallerFloat SetByCallerMagnitude;

	// @hack: @todo: This is temporary to aid in post-load fix-up w/o exposing members publicly
	friend class UDNAEffect;
	friend class FDNAEffectModifierMagnitudeDetails;
};

/** 
 * Struct representing modifier info used exclusively for "scoped" executions that happen instantaneously. These are
 * folded into a calculation only for the extent of the calculation and never permanently added to an aggregator.
 */
USTRUCT()
struct FDNAEffectExecutionScopedModifierInfo
{
	GENERATED_USTRUCT_BODY()

	// Constructors
	FDNAEffectExecutionScopedModifierInfo()
		: ModifierOp(EDNAModOp::Additive)
	{}

	FDNAEffectExecutionScopedModifierInfo(const FDNAEffectAttributeCaptureDefinition& InCaptureDef)
		: CapturedAttribute(InCaptureDef)
		, ModifierOp(EDNAModOp::Additive)
	{
	}

	/** Backing attribute that the scoped modifier is for */
	UPROPERTY(VisibleDefaultsOnly, Category=Execution)
	FDNAEffectAttributeCaptureDefinition CapturedAttribute;

	/** Modifier operation to perform */
	UPROPERTY(EditDefaultsOnly, Category=Execution)
	TEnumAsByte<EDNAModOp::Type> ModifierOp;

	/** Magnitude of the scoped modifier */
	UPROPERTY(EditDefaultsOnly, Category=Execution)
	FDNAEffectModifierMagnitude ModifierMagnitude;

	/** Evaluation channel settings of the scoped modifier */
	UPROPERTY(EditDefaultsOnly, Category=Execution)
	FDNAModEvaluationChannelSettings EvaluationChannelSettings;

	/** Source tag requirements for the modifier to apply */
	UPROPERTY(EditDefaultsOnly, Category=Execution)
	FDNATagRequirements SourceTags;

	/** Target tag requirements for the modifier to apply */
	UPROPERTY(EditDefaultsOnly, Category=Execution)
	FDNATagRequirements TargetTags;
};

/**
 * Struct for DNA effects that apply only if another DNA effect (or execution) was successfully applied.
 */
USTRUCT()
struct DNAABILITIES_API FConditionalDNAEffect
{
	GENERATED_USTRUCT_BODY()

	bool CanApply(const FDNATagContainer& SourceTags, float SourceLevel) const;

	FDNAEffectSpecHandle CreateSpec(FDNAEffectContextHandle EffectContext, float SourceLevel) const;

	/** DNA effect that will be applied to the target */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = DNAEffect)
	TSubclassOf<UDNAEffect> EffectClass;

	/** Tags that the source must have for this GE to apply */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = DNAEffect)
	FDNATagContainer RequiredSourceTags;
};

/** 
 * Struct representing the definition of a custom execution for a DNA effect.
 * Custom executions run special logic from an outside class each time the DNA effect executes.
 */
USTRUCT()
struct DNAABILITIES_API FDNAEffectExecutionDefinition
{
	GENERATED_USTRUCT_BODY()

	/**
	 * Gathers and populates the specified array with the capture definitions that the execution would like in order
	 * to perform its custom calculation. Up to the individual execution calculation to handle if some of them are missing
	 * or not.
	 * 
	 * @param OutCaptureDefs	[OUT] Capture definitions requested by the execution
	 */
	void GetAttributeCaptureDefinitions(OUT TArray<FDNAEffectAttributeCaptureDefinition>& OutCaptureDefs) const;

	/** Custom execution calculation class to run when the DNA effect executes */
	UPROPERTY(EditDefaultsOnly, Category=Execution)
	TSubclassOf<UDNAEffectExecutionCalculation> CalculationClass;
	
	/** These tags are passed into the execution as is, and may be used to do conditional logic */
	UPROPERTY(EditDefaultsOnly, Category = Execution)
	FDNATagContainer PassedInTags;

	/** Modifiers that are applied "in place" during the execution calculation */
	UPROPERTY(EditDefaultsOnly, Category = Execution)
	TArray<FDNAEffectExecutionScopedModifierInfo> CalculationModifiers;

	/** Deprecated. */
	UPROPERTY()
	TArray<TSubclassOf<UDNAEffect>> ConditionalDNAEffectClasses_DEPRECATED;

	/** Other DNA Effects that will be applied to the target of this execution if the execution is successful. Note if no execution class is selected, these will always apply. */
	UPROPERTY(EditDefaultsOnly, Category = Execution)
	TArray<FConditionalDNAEffect> ConditionalDNAEffects;
};

/**
 * FDNAModifierInfo
 *	Tells us "Who/What we" modify
 *	Does not tell us how exactly
 *
 */
USTRUCT()
struct DNAABILITIES_API FDNAModifierInfo
{
	GENERATED_USTRUCT_BODY()

	FDNAModifierInfo()	
	: ModifierOp(EDNAModOp::Additive)
	{

	}

	/** The Attribute we modify or the GE we modify modifies. */
	UPROPERTY(EditDefaultsOnly, Category=DNAModifier, meta=(FilterMetaTag="HideFromModifiers"))
	FDNAAttribute Attribute;

	/** The numeric operation of this modifier: Override, Add, Multiply, etc  */
	UPROPERTY(EditDefaultsOnly, Category=DNAModifier)
	TEnumAsByte<EDNAModOp::Type> ModifierOp;

	// @todo: Remove this after content resave
	/** Now "deprecated," though being handled in a custom manner to avoid engine version bump. */
	UPROPERTY()
	FScalableFloat Magnitude;

	/** Magnitude of the modifier */
	UPROPERTY(EditDefaultsOnly, Category=DNAModifier)
	FDNAEffectModifierMagnitude ModifierMagnitude;

	/** Evaluation channel settings of the modifier */
	UPROPERTY(EditDefaultsOnly, Category=DNAModifier)
	FDNAModEvaluationChannelSettings EvaluationChannelSettings;

	UPROPERTY(EditDefaultsOnly, Category=DNAModifier)
	FDNATagRequirements	SourceTags;

	UPROPERTY(EditDefaultsOnly, Category=DNAModifier)
	FDNATagRequirements	TargetTags;

	/** Equality/Inequality operators */
	bool operator==(const FDNAModifierInfo& Other) const;
	bool operator!=(const FDNAModifierInfo& Other) const;
};

/**
 * FDNAEffectCue
 *	This is a cosmetic cue that can be tied to a UDNAEffect. 
 *  This is essentially a DNATag + a Min/Max level range that is used to map the level of a DNAEffect to a normalized value used by the DNACue system.
 */
USTRUCT()
struct FDNAEffectCue
{
	GENERATED_USTRUCT_BODY()

	FDNAEffectCue()
		: MinLevel(0.f)
		, MaxLevel(0.f)
	{
	}

	FDNAEffectCue(const FDNATag& InTag, float InMinLevel, float InMaxLevel)
		: MinLevel(InMinLevel)
		, MaxLevel(InMaxLevel)
	{
		DNACueTags.AddTag(InTag);
	}

	/** The attribute to use as the source for cue magnitude. If none use level */
	UPROPERTY(EditDefaultsOnly, Category = DNACue)
	FDNAAttribute MagnitudeAttribute;

	/** The minimum level that this Cue supports */
	UPROPERTY(EditDefaultsOnly, Category = DNACue)
	float	MinLevel;

	/** The maximum level that this Cue supports */
	UPROPERTY(EditDefaultsOnly, Category = DNACue)
	float	MaxLevel;

	/** Tags passed to the DNA cue handler when this cue is activated */
	UPROPERTY(EditDefaultsOnly, Category = DNACue, meta = (Categories="DNACue"))
	FDNATagContainer DNACueTags;

	float NormalizeLevel(float InLevel)
	{
		float Range = MaxLevel - MinLevel;
		if (Range <= KINDA_SMALL_NUMBER)
		{
			return 1.f;
		}

		return FMath::Clamp((InLevel - MinLevel) / Range, 0.f, 1.0f);
	}
};

USTRUCT(BlueprintType)
struct DNAABILITIES_API FInheritedTagContainer
{
	GENERATED_USTRUCT_BODY()

	/** Tags that I inherited and tags that I added minus tags that I removed*/
	UPROPERTY(VisibleAnywhere, Transient, BlueprintReadOnly, Category = Application)
	FDNATagContainer CombinedTags;

	/** Tags that I have in addition to my parent's tags */
	UPROPERTY(EditDefaultsOnly, Transient, BlueprintReadOnly, Category = Application)
	FDNATagContainer Added;

	/** Tags that should be removed if my parent had them */
	UPROPERTY(EditDefaultsOnly, Transient, BlueprintReadOnly, Category = Application)
	FDNATagContainer Removed;

	void UpdateInheritedTagProperties(const FInheritedTagContainer* Parent);

	void PostInitProperties();

	void AddTag(const FDNATag& TagToAdd);
	void RemoveTag(FDNATag TagToRemove);
};

/** DNA effect duration policies */
UENUM()
enum class EDNAEffectDurationType : uint8
{
	/** This effect applies instantly */
	Instant,
	/** This effect lasts forever */
	Infinite,
	/** The duration of this effect will be specified by a magnitude */
	HasDuration
};

/** Enumeration of policies for dealing with duration of a DNA effect while stacking */
UENUM()
enum class EDNAEffectStackingDurationPolicy : uint8
{
	/** The duration of the effect will be refreshed from any successful stack application */
	RefreshOnSuccessfulApplication,

	/** The duration of the effect will never be refreshed */
	NeverRefresh,
};

/** Enumeration of policies for dealing with the period of a DNA effect while stacking */
UENUM()
enum class EDNAEffectStackingPeriodPolicy : uint8
{
	/** Any progress toward the next tick of a periodic effect is discarded upon any successful stack application */
	ResetOnSuccessfulApplication,

	/** The progress toward the next tick of a periodic effect will never be reset, regardless of stack applications */
	NeverReset,
};

/** Enumeration of policies for dealing DNA effect stacks that expire (in duration based effects). */
UENUM()
enum class EDNAEffectStackingExpirationPolicy : uint8
{
	/** The entire stack is cleared when the active DNA effect expires  */
	ClearEntireStack,

	/** The current stack count will be decremented by 1 and the duration refreshed. The GE is not "reapplied", just continues to exist with one less stacks. */
	RemoveSingleStackAndRefreshDuration,

	/** The duration of the DNA effect is refreshed. This essentially makes the effect infinite in duration. This can be used to manually handle stack decrements via XXX callback */
	RefreshDuration,
};

/** Holds evaluated magnitude from a DNAEffect modifier */
USTRUCT()
struct FModifierSpec
{
	GENERATED_USTRUCT_BODY()

	FModifierSpec() : EvaluatedMagnitude(0.f) { }

	float GetEvaluatedMagnitude() const { return EvaluatedMagnitude; }

private:

	// @todo: Probably need to make the modifier info private so people don't go gunking around in the magnitude
	/** In the event that the modifier spec requires custom magnitude calculations, this is the authoritative, last evaluated value of the magnitude */
	UPROPERTY()
	float EvaluatedMagnitude;

	/** These structures are the only ones that should internally be able to update the EvaluatedMagnitude. Any gamecode that gets its hands on FModifierSpec should never be setting EvaluatedMagnitude manually */
	friend struct FDNAEffectSpec;
	friend struct FActiveDNAEffectsContainer;
};

/** Saves list of modified attributes, to use for DNA cues or later processing */
USTRUCT()
struct DNAABILITIES_API FDNAEffectModifiedAttribute
{
	GENERATED_USTRUCT_BODY()

	/** The attribute that has been modified */
	UPROPERTY()
	FDNAAttribute Attribute;

	/** Total magnitude applied to that attribute */
	UPROPERTY()
	float TotalMagnitude;

	FDNAEffectModifiedAttribute() : TotalMagnitude(0.0f) {}
};

/** Struct used to hold the result of a DNA attribute capture; Initially seeded by definition data, but then populated by ability system component when appropriate */
USTRUCT()
struct DNAABILITIES_API FDNAEffectAttributeCaptureSpec
{
	// Allow these as friends so they can seed the aggregator, which we don't otherwise want exposed
	friend struct FActiveDNAEffectsContainer;
	friend class UDNAAbilitySystemComponent;

	GENERATED_USTRUCT_BODY()

	// Constructors
	FDNAEffectAttributeCaptureSpec();
	FDNAEffectAttributeCaptureSpec(const FDNAEffectAttributeCaptureDefinition& InDefinition);

	/**
	 * Returns whether the spec actually has a valid capture yet or not
	 * 
	 * @return True if the spec has a valid attribute capture, false if it does not
	 */
	bool HasValidCapture() const;

	/**
	 * Attempts to calculate the magnitude of the captured attribute given the specified parameters. Can fail if the spec doesn't have
	 * a valid capture yet.
	 * 
	 * @param InEvalParams	Parameters to evaluate the attribute under
	 * @param OutMagnitude	[OUT] Computed magnitude
	 * 
	 * @return True if the magnitude was successfully calculated, false if it was not
	 */
	bool AttemptCalculateAttributeMagnitude(const FAggregatorEvaluateParameters& InEvalParams, OUT float& OutMagnitude) const;

	/**
	 * Attempts to calculate the magnitude of the captured attribute given the specified parameters, up to the specified evaluation channel (inclusive).
	 * Can fail if the spec doesn't have a valid capture yet.
	 * 
	 * @param InEvalParams	Parameters to evaluate the attribute under
	 * @param FinalChannel	Evaluation channel to terminate the calculation at
	 * @param OutMagnitude	[OUT] Computed magnitude
	 * 
	 * @return True if the magnitude was successfully calculated, false if it was not
	 */
	bool AttemptCalculateAttributeMagnitudeUpToChannel(const FAggregatorEvaluateParameters& InEvalParams, EDNAModEvaluationChannel FinalChannel, OUT float& OutMagnitude) const;

	/**
	 * Attempts to calculate the magnitude of the captured attribute given the specified parameters, including a starting base value. 
	 * Can fail if the spec doesn't have a valid capture yet.
	 * 
	 * @param InEvalParams	Parameters to evaluate the attribute under
	 * @param InBaseValue	Base value to evaluate the attribute under
	 * @param OutMagnitude	[OUT] Computed magnitude
	 * 
	 * @return True if the magnitude was successfully calculated, false if it was not
	 */
	bool AttemptCalculateAttributeMagnitudeWithBase(const FAggregatorEvaluateParameters& InEvalParams, float InBaseValue, OUT float& OutMagnitude) const;

	/**
	 * Attempts to calculate the base value of the captured attribute given the specified parameters. Can fail if the spec doesn't have
	 * a valid capture yet.
	 * 
	 * @param OutBaseValue	[OUT] Computed base value
	 * 
	 * @return True if the base value was successfully calculated, false if it was not
	 */
	bool AttemptCalculateAttributeBaseValue(OUT float& OutBaseValue) const;

	/**
	 * Attempts to calculate the "bonus" magnitude (final - base value) of the captured attribute given the specified parameters. Can fail if the spec doesn't have
	 * a valid capture yet.
	 * 
	 * @param InEvalParams		Parameters to evaluate the attribute under
	 * @param OutBonusMagnitude	[OUT] Computed bonus magnitude
	 * 
	 * @return True if the bonus magnitude was successfully calculated, false if it was not
	 */
	bool AttemptCalculateAttributeBonusMagnitude(const FAggregatorEvaluateParameters& InEvalParams, OUT float& OutBonusMagnitude) const;

	/**
	 * Attempts to calculate the contribution of the specified GE to the captured attribute given the specified parameters. Can fail if the spec doesn't have
	 * a valid capture yet.
	 *
	 * @param InEvalParams		Parameters to evaluate the attribute under
	 * @param ActiveHandle		Handle of the DNA effect to query about
	 * @param OutBonusMagnitude	[OUT] Computed bonus magnitude
	 *
	 * @return True if the bonus magnitude was successfully calculated, false if it was not
	 */
	bool AttemptCalculateAttributeContributionMagnitude(const FAggregatorEvaluateParameters& InEvalParams, FActiveDNAEffectHandle ActiveHandle, OUT float& OutBonusMagnitude) const;

	/**
	 * Attempts to populate the specified aggregator with a snapshot of the backing captured aggregator. Can fail if the spec doesn't have
	 * a valid capture yet.
	 *
	 * @param OutAggregatorSnapshot	[OUT] Snapshotted aggregator, if possible
	 *
	 * @return True if the aggregator was successfully snapshotted, false if it was not
	 */
	bool AttemptGetAttributeAggregatorSnapshot(OUT FAggregator& OutAggregatorSnapshot) const;

	/**
	 * Attempts to populate the specified aggregator with all of the mods of the backing captured aggregator. Can fail if the spec doesn't have
	 * a valid capture yet.
	 *
	 * @param OutAggregatorToAddTo	[OUT] Aggregator with mods appended, if possible
	 *
	 * @return True if the aggregator had mods successfully added to it, false if it did not
	 */
	bool AttemptAddAggregatorModsToAggregator(OUT FAggregator& OutAggregatorToAddTo) const;
	
	/** Simple accessor to backing capture definition */
	const FDNAEffectAttributeCaptureDefinition& GetBackingDefinition() const;

	/** Register this handle with linked aggregators */
	void RegisterLinkedAggregatorCallback(FActiveDNAEffectHandle Handle) const;

	/** Unregister this handle with linked aggregators */
	void UnregisterLinkedAggregatorCallback(FActiveDNAEffectHandle Handle) const;
	
	/** Return true if this capture should be recalculated if the given aggregator has changed */
	bool ShouldRefreshLinkedAggregator(const FAggregator* ChangedAggregator) const;

	/** Swaps any internal references From aggregator To aggregator. Used when cloning */
	void SwapAggregator(FAggregatorRef From, FAggregatorRef To);
		
private:

	/** Copy of the definition the spec should adhere to for capturing */
	UPROPERTY()
	FDNAEffectAttributeCaptureDefinition BackingDefinition;

	/** Ref to the aggregator for the captured attribute */
	FAggregatorRef AttributeAggregator;
};

/** Struct used to handle a collection of captured source and target attributes */
USTRUCT()
struct DNAABILITIES_API FDNAEffectAttributeCaptureSpecContainer
{
	GENERATED_USTRUCT_BODY()

public:

	FDNAEffectAttributeCaptureSpecContainer();

	FDNAEffectAttributeCaptureSpecContainer(FDNAEffectAttributeCaptureSpecContainer&& Other);

	FDNAEffectAttributeCaptureSpecContainer(const FDNAEffectAttributeCaptureSpecContainer& Other);

	FDNAEffectAttributeCaptureSpecContainer& operator=(FDNAEffectAttributeCaptureSpecContainer&& Other);

	FDNAEffectAttributeCaptureSpecContainer& operator=(const FDNAEffectAttributeCaptureSpecContainer& Other);

	/**
	 * Add a definition to be captured by the owner of the container. Will not add the definition if its exact
	 * match already exists within the container.
	 * 
	 * @param InCaptureDefinition	Definition to capture with
	 */
	void AddCaptureDefinition(const FDNAEffectAttributeCaptureDefinition& InCaptureDefinition);

	/**
	 * Capture source or target attributes from the specified component. Should be called by the container's owner.
	 * 
	 * @param InDNAAbilitySystemComponent	Component to capture attributes from
	 * @param InCaptureSource			Whether to capture attributes as source or target
	 */
	void CaptureAttributes(class UDNAAbilitySystemComponent* InDNAAbilitySystemComponent, EDNAEffectAttributeCaptureSource InCaptureSource);

	/**
	 * Find a capture spec within the container matching the specified capture definition, if possible.
	 * 
	 * @param InDefinition				Capture definition to use as the search basis
	 * @param bOnlyIncludeValidCapture	If true, even if a spec is found, it won't be returned if it doesn't also have a valid capture already
	 * 
	 * @return The found attribute spec matching the specified search params, if any
	 */
	const FDNAEffectAttributeCaptureSpec* FindCaptureSpecByDefinition(const FDNAEffectAttributeCaptureDefinition& InDefinition, bool bOnlyIncludeValidCapture) const;

	/**
	 * Determines if the container has specs with valid captures for all of the specified definitions.
	 * 
	 * @param InCaptureDefsToCheck	Capture definitions to check for
	 * 
	 * @return True if the container has valid capture attributes for all of the specified definitions, false if it does not
	 */
	bool HasValidCapturedAttributes(const TArray<FDNAEffectAttributeCaptureDefinition>& InCaptureDefsToCheck) const;

	/** Returns whether the container has at least one spec w/o snapshotted attributes */
	bool HasNonSnapshottedAttributes() const;

	/** Registers any linked aggregators to notify this active handle if they are dirtied */
	void RegisterLinkedAggregatorCallbacks(FActiveDNAEffectHandle Handle) const;

	/** Unregisters any linked aggregators from notifying this active handle if they are dirtied */
	void UnregisterLinkedAggregatorCallbacks(FActiveDNAEffectHandle Handle) const;

	/** Swaps any internal references From aggregator To aggregator. Used when cloning */
	void SwapAggregator(FAggregatorRef From, FAggregatorRef To);

private:

	/** Captured attributes from the source of a DNA effect */
	UPROPERTY()
	TArray<FDNAEffectAttributeCaptureSpec> SourceAttributes;

	/** Captured attributes from the target of a DNA effect */
	UPROPERTY()
	TArray<FDNAEffectAttributeCaptureSpec> TargetAttributes;

	/** If true, has at least one capture spec that did not request a snapshot */
	UPROPERTY()
	bool bHasNonSnapshottedAttributes;
};

/**
 * DNAEffect Specification. Tells us:
 *	-What UDNAEffect (const data)
 *	-What Level
 *  -Who instigated
 *  
 * FDNAEffectSpec is modifiable. We start with initial conditions and modifications be applied to it. In this sense, it is stateful/mutable but it
 * is still distinct from an FActiveDNAEffect which in an applied instance of an FDNAEffectSpec.
 */
USTRUCT()
struct DNAABILITIES_API FDNAEffectSpec
{
	GENERATED_USTRUCT_BODY()

	// --------------------------------------------------------------------------------------------------------------------------
	//	IMPORTANT: Any state added to FDNAEffectSpec must be handled in the move/copy constructor/operator!
	//	(When VS2012/2013 support is dropped, we can use compiler generated operators, but until then these need to be maintained manually!)
	// --------------------------------------------------------------------------------------------------------------------------

	FDNAEffectSpec();

	FDNAEffectSpec(const UDNAEffect* InDef, const FDNAEffectContextHandle& InEffectContext, float Level = FDNAEffectConstants::INVALID_LEVEL);

	FDNAEffectSpec(const FDNAEffectSpec& Other);

	FDNAEffectSpec(FDNAEffectSpec&& Other);

	FDNAEffectSpec& operator=(FDNAEffectSpec&& Other);

	FDNAEffectSpec& operator=(const FDNAEffectSpec& Other);

	// Can be called manually but it is  preferred to use the 3 parameter constructor
	void Initialize(const UDNAEffect* InDef, const FDNAEffectContextHandle& InEffectContext, float Level = FDNAEffectConstants::INVALID_LEVEL);

	/**
	 * Determines if the spec has capture specs with valid captures for all of the specified definitions.
	 * 
	 * @param InCaptureDefsToCheck	Capture definitions to check for
	 * 
	 * @return True if the container has valid capture attributes for all of the specified definitions, false if it does not
	 */
	bool HasValidCapturedAttributes(const TArray<FDNAEffectAttributeCaptureDefinition>& InCaptureDefsToCheck) const;

	/** Looks for an existing modified attribute struct, may return NULL */
	const FDNAEffectModifiedAttribute* GetModifiedAttribute(const FDNAAttribute& Attribute) const;
	FDNAEffectModifiedAttribute* GetModifiedAttribute(const FDNAAttribute& Attribute);

	/** Adds a new modified attribute struct, will always add so check to see if it exists first */
	FDNAEffectModifiedAttribute* AddModifiedAttribute(const FDNAAttribute& Attribute);

	/**
	 * Helper function to attempt to calculate the duration of the spec from its GE definition
	 * 
	 * @param OutDefDuration	Computed duration of the spec from its GE definition; Not the actual duration of the spec
	 * 
	 * @return True if the calculation was successful, false if it was not
	 */
	bool AttemptCalculateDurationFromDef(OUT float& OutDefDuration) const;

	/** Sets duration. This should only be called as the DNAEffect is being created and applied; Ignores calls after attribute capture */
	void SetDuration(float NewDuration, bool bLockDuration);

	float GetDuration() const;
	float GetPeriod() const;
	float GetChanceToApplyToTarget() const;

	/** Set the context info: who and where this spec came from. */
	void SetContext(FDNAEffectContextHandle NewEffectContext);

	FDNAEffectContextHandle GetContext() const
	{
		return EffectContext;
	}

	// Appends all tags granted by this DNA effect spec
	void GetAllGrantedTags(OUT FDNATagContainer& Container) const;

	// Appends all tags that apply to this DNA effect spec
	void GetAllAssetTags(OUT FDNATagContainer& Container) const;

	/** Sets the magnitude of a SetByCaller modifier */
	void SetSetByCallerMagnitude(FName DataName, float Magnitude);

	/** Returns the magnitude of a SetByCaller modifier. Will return 0.f and Warn if the magnitude has not been set. */
	float GetSetByCallerMagnitude(FName DataName, bool WarnIfNotFound=true, float DefaultIfNotFound=0.f) const;

	void SetLevel(float InLevel);

	float GetLevel() const;

	void PrintAll() const;

	FString ToSimpleString() const;

	const FDNAEffectContextHandle& GetEffectContext() const
	{
		return EffectContext;
	}

	void DuplicateEffectContext()
	{
		EffectContext = EffectContext.Duplicate();
	}

	void CaptureAttributeDataFromTarget(UDNAAbilitySystemComponent* TargetDNAAbilitySystemComponent);

	/**
	 * Get the computed magnitude of the modifier on the spec with the specified index
	 * 
	 * @param ModifierIndx			Modifier to get
	 * @param bFactorInStackCount	If true, the calculation will include the stack count
	 * 
	 * @return Computed magnitude
	 */
	float GetModifierMagnitude(int32 ModifierIdx, bool bFactorInStackCount) const;

	void CalculateModifierMagnitudes();

	/** Recapture attributes from source and target for cloning */
	void RecaptureAttributeDataForClone(UDNAAbilitySystemComponent* OriginalASC, UDNAAbilitySystemComponent* NewASC);

	/** Recaptures source actor tags of this spec without modifying anything else */
	void RecaptureSourceActorTags();

	/** Helper function to initialize all of the capture definitions required by the spec */
	void SetupAttributeCaptureDefinitions();

	/** Helper function that returns the duration after applying relevant modifiers from the source and target ability system components */
	float CalculateModifiedDuration() const;

private:

	void CaptureDataFromSource();

public:

	// -----------------------------------------------------------------------

	/** DNAEfect definition. The static data that this spec points to. */
	UPROPERTY()
	const UDNAEffect* Def;
	
	/** A list of attributes that were modified during the application of this spec */
	UPROPERTY()
	TArray<FDNAEffectModifiedAttribute> ModifiedAttributes;
	
	/** Attributes captured by the spec that are relevant to custom calculations, potentially in owned modifiers, etc.; NOT replicated to clients */
	UPROPERTY(NotReplicated)
	FDNAEffectAttributeCaptureSpecContainer CapturedRelevantAttributes;

	/** other effects that need to be applied to the target if this effect is successful */
	TArray< FDNAEffectSpecHandle > TargetEffectSpecs;

	// The duration in seconds of this effect
	// instantaneous effects should have a duration of FDNAEffectConstants::INSTANT_APPLICATION
	// effects that last forever should have a duration of FDNAEffectConstants::INFINITE_DURATION
	UPROPERTY()
	float Duration;

	// The period in seconds of this effect.
	// Nonperiodic effects should have a period of FDNAEffectConstants::NO_PERIOD
	UPROPERTY()
	float Period;

	// The chance, in a 0.0-1.0 range, that this DNAEffect will be applied to the target Attribute or DNAEffect.
	UPROPERTY()
	float ChanceToApplyToTarget;

	// Captured Source Tags on DNAEffectSpec creation.	
	UPROPERTY(NotReplicated)
	FTagContainerAggregator	CapturedSourceTags;

	// Tags from the target, captured during execute	
	UPROPERTY(NotReplicated)
	FTagContainerAggregator	CapturedTargetTags;

	/** Tags that are granted and that did not come from the UDNAEffect def. These are replicated. */
	UPROPERTY()
	FDNATagContainer DynamicGrantedTags;

	/** Tags that are on this effect spec and that did not come from the UDNAEffect def. These are replicated. */
	UPROPERTY()
	FDNATagContainer DynamicAssetTags;
	
	UPROPERTY()
	TArray<FModifierSpec> Modifiers;

	UPROPERTY()
	int32 StackCount;

	/** Whether the spec has had its source attribute capture completed or not yet */
	UPROPERTY(NotReplicated)
	uint32 bCompletedSourceAttributeCapture : 1;

	/** Whether the spec has had its target attribute capture completed or not yet */
	UPROPERTY(NotReplicated)
	uint32 bCompletedTargetAttributeCapture : 1;

	/** Whether the duration of the spec is locked or not; If it is, attempts to set it will fail */
	UPROPERTY(NotReplicated)
	uint32 bDurationLocked : 1;

	UPROPERTY()
	TArray<FDNAAbilitySpecDef> GrantedAbilitySpecs;

private:

	/** Map of set by caller magnitudes */
	TMap<FName, float>	SetByCallerMagnitudes;
	
	UPROPERTY()
	FDNAEffectContextHandle EffectContext; // This tells us how we got here (who / what applied us)
	
	UPROPERTY()
	float Level;	
};


/** This is a cut down version of the DNA effect spec used for RPCs. */
USTRUCT()
struct DNAABILITIES_API FDNAEffectSpecForRPC
{
	GENERATED_USTRUCT_BODY()

	FDNAEffectSpecForRPC();

	FDNAEffectSpecForRPC(const FDNAEffectSpec& InSpec);

	/** DNAEfect definition. The static data that this spec points to. */
	UPROPERTY()
	const UDNAEffect* Def;

	UPROPERTY()
	TArray<FDNAEffectModifiedAttribute> ModifiedAttributes;

	UPROPERTY()
	FDNAEffectContextHandle EffectContext; // This tells us how we got here (who / what applied us)

	UPROPERTY()
	FDNATagContainer AggregatedSourceTags;

	UPROPERTY()
	FDNATagContainer AggregatedTargetTags;

	UPROPERTY()
	float Level;

	UPROPERTY()
	float AbilityLevel;

	FDNAEffectContextHandle GetContext() const
	{
		return EffectContext;
	}

	float GetLevel() const
	{
		return Level;
	}

	float GetAbilityLevel() const
	{
		return AbilityLevel;
	}

	FString ToSimpleString() const;

	const FDNAEffectModifiedAttribute* GetModifiedAttribute(const FDNAAttribute& Attribute) const;
};


/**
 * Active DNAEffect instance
 *	-What DNAEffect Spec
 *	-Start time
 *  -When to execute next
 *  -Replication callbacks
 *
 */
USTRUCT(BlueprintType)
struct DNAABILITIES_API FActiveDNAEffect : public FFastArraySerializerItem
{
	GENERATED_USTRUCT_BODY()

	// ---------------------------------------------------------------------------------------------------------------------------------
	//  IMPORTANT: Any new state added to FActiveDNAEffect must be handled in the copy/move constructor/operator
	//	(When VS2012/2013 support is dropped, we can use compiler generated operators, but until then these need to be maintained manually)
	// ---------------------------------------------------------------------------------------------------------------------------------

	FActiveDNAEffect();

	FActiveDNAEffect(const FActiveDNAEffect& Other);

	FActiveDNAEffect(FActiveDNAEffectHandle InHandle, const FDNAEffectSpec &InSpec, float CurrentWorldTime, float InStartServerWorldTime, FPredictionKey InPredictionKey);
	
	FActiveDNAEffect(FActiveDNAEffect&& Other);

	FActiveDNAEffect& operator=(FActiveDNAEffect&& other);

	FActiveDNAEffect& operator=(const FActiveDNAEffect& other);

	float GetTimeRemaining(float WorldTime) const
	{
		float Duration = GetDuration();		
		return (Duration == FDNAEffectConstants::INFINITE_DURATION ? -1.f : Duration - (WorldTime - StartWorldTime));
	}
	
	float GetDuration() const
	{
		return Spec.GetDuration();
	}

	float GetPeriod() const
	{
		return Spec.GetPeriod();
	}

	float GetEndTime() const
	{
		float Duration = GetDuration();		
		return (Duration == FDNAEffectConstants::INFINITE_DURATION ? -1.f : Duration + StartWorldTime);
	}

	void CheckOngoingTagRequirements(const FDNATagContainer& OwnerTags, struct FActiveDNAEffectsContainer& OwningContainer, bool bInvokeDNACueEvents = false);

	void PrintAll() const;

	void PreReplicatedRemove(const struct FActiveDNAEffectsContainer &InArray);
	void PostReplicatedAdd(const struct FActiveDNAEffectsContainer &InArray);
	void PostReplicatedChange(const struct FActiveDNAEffectsContainer &InArray);

	/** Refreshes the cached StartWorldTime for this effect. To be used when the server/client world time delta changes significantly to keep the start time in sync. */
	void RecomputeStartWorldTime(const FActiveDNAEffectsContainer& InArray);

	bool operator==(const FActiveDNAEffect& Other)
	{
		return Handle == Other.Handle;
	}

	// ---------------------------------------------------------------------------------------------------------------------------------

	/** Globally unique ID for identify this active DNA effect. Can be used to look up owner. Not networked. */
	FActiveDNAEffectHandle Handle;

	UPROPERTY()
	FDNAEffectSpec Spec;

	UPROPERTY()
	FPredictionKey	PredictionKey;

	/** Server time this started */
	UPROPERTY()
	float StartServerWorldTime;

	/** Used for handling duration modifications being replicated */
	UPROPERTY(NotReplicated)
	float CachedStartServerWorldTime;

	UPROPERTY(NotReplicated)
	float StartWorldTime;

	// Not sure if this should replicate or not. If replicated, we may have trouble where IsInhibited doesn't appear to change when we do tag checks (because it was previously inhibited, but replication made it inhibited).
	UPROPERTY()
	bool bIsInhibited;

	/** When replicated down, we cue the GC events until the entire list of active DNA effects has been received */
	mutable bool bPendingRepOnActiveGC;
	mutable bool bPendingRepWhileActiveGC;

	bool IsPendingRemove;

	/** Last StackCount that the client had. Used to tell if the stackcount has changed in PostReplicatedChange */
	int32 ClientCachedStackCount;

	FOnActiveDNAEffectRemoved OnRemovedDelegate;

	FOnActiveDNAEffectStackChange OnStackChangeDelegate;

	FOnActiveDNAEffectTimeChange OnTimeChangeDelegate;

	FTimerHandle PeriodHandle;

	FTimerHandle DurationHandle;

	FActiveDNAEffect* PendingNext;
};

DECLARE_DELEGATE_RetVal_OneParam(bool, FActiveDNAEffectQueryCustomMatch, const FActiveDNAEffect&);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FActiveDNAEffectQueryCustomMatch_Dynamic, FActiveDNAEffect, Effect, bool&, bMatches);

/** Every set condition within this query must match in order for the query to match. i.e. individual query elements are ANDed together. */
USTRUCT(BlueprintType)
struct DNAABILITIES_API FDNAEffectQuery
{
	GENERATED_USTRUCT_BODY()

public:
	// ctors and operators
	FDNAEffectQuery();
	FDNAEffectQuery(const FDNAEffectQuery& Other);
	FDNAEffectQuery(FActiveDNAEffectQueryCustomMatch InCustomMatchDelegate);
	FDNAEffectQuery(FDNAEffectQuery&& Other);
	FDNAEffectQuery& operator=(FDNAEffectQuery&& Other);
	FDNAEffectQuery& operator=(const FDNAEffectQuery& Other);

	/** Native delegate for providing custom matching conditions. */
	FActiveDNAEffectQueryCustomMatch CustomMatchDelegate;

	/** BP-exposed delegate for providing custom matching conditions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Query)
	FActiveDNAEffectQueryCustomMatch_Dynamic CustomMatchDelegate_BP;

	/** Query that is matched against tags this GE gives */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Query)
	FDNATagQuery OwningTagQuery;

	/** Query that is matched against tags this GE has */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Query)
	FDNATagQuery EffectTagQuery;

	/** Query that is matched against tags the source of this GE has */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Query)
	FDNATagQuery SourceTagQuery;

	/** Matches on DNAEffects which modify given attribute. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Query)
	FDNAAttribute ModifyingAttribute;

	/** Matches on DNAEffects which come from this source */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Query)
	const UObject* EffectSource;

	/** Matches on DNAEffects with this definition */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Query)
	TSubclassOf<UDNAEffect> EffectDefinition;

	/** Handles to ignore as matches, even if other criteria is met */
	TArray<FActiveDNAEffectHandle> IgnoreHandles;

	/** Returns true if Effect matches all specified criteria of this query, including CustomMatch delegates if bound. Returns false otherwise. */
	bool Matches(const FActiveDNAEffect& Effect) const;

	/** Returns true if Effect matches all specified criteria of this query. This does NOT check FActiveDNAEffectQueryCustomMatch since this is performed on the spec (possibly prior to applying).
	 *	Note: it would be reasonable to support a custom delegate that operated on the FDNAEffectSpec itself.
	 */
	bool Matches(const FDNAEffectSpec& Effect) const;

	/** Returns true if the query is empty/default. E.g., it has no data set. */
	bool IsEmpty() const;

	/** 
	 * Shortcuts for easily creating common query types 
	 * @todo: add more as dictated by use cases
	 */

	/** Creates an effect query that will match if there are any common tags between the given tags and an ActiveDNAEffect's owning tags */
	static FDNAEffectQuery MakeQuery_MatchAnyOwningTags(const FDNATagContainer& InTags);
	/** Creates an effect query that will match if all of the given tags are in the ActiveDNAEffect's owning tags */
	static FDNAEffectQuery MakeQuery_MatchAllOwningTags(const FDNATagContainer& InTags);
	/** Creates an effect query that will match if there are no common tags between the given tags and an ActiveDNAEffect's owning tags */
	static FDNAEffectQuery MakeQuery_MatchNoOwningTags(const FDNATagContainer& InTags);
	
	/** Creates an effect query that will match if there are any common tags between the given tags and an ActiveDNAEffect's tags */
	static FDNAEffectQuery MakeQuery_MatchAnyEffectTags(const FDNATagContainer& InTags);
	/** Creates an effect query that will match if all of the given tags are in the ActiveDNAEffect's tags */
	static FDNAEffectQuery MakeQuery_MatchAllEffectTags(const FDNATagContainer& InTags);
	/** Creates an effect query that will match if there are no common tags between the given tags and an ActiveDNAEffect's tags */
	static FDNAEffectQuery MakeQuery_MatchNoEffectTags(const FDNATagContainer& InTags);

	/** Creates an effect query that will match if there are any common tags between the given tags and an ActiveDNAEffect's source tags */
	static FDNAEffectQuery MakeQuery_MatchAnySourceTags(const FDNATagContainer& InTags);
	/** Creates an effect query that will match if all of the given tags are in the ActiveDNAEffect's source tags */
	static FDNAEffectQuery MakeQuery_MatchAllSourceTags(const FDNATagContainer& InTags);
	/** Creates an effect query that will match if there are no common tags between the given tags and an ActiveDNAEffect's source tags */
	static FDNAEffectQuery MakeQuery_MatchNoSourceTags(const FDNATagContainer& InTags);
};

/**
 *	Generic querying data structure for active DNAEffects. Lets us ask things like:
 *		Give me duration/magnitude of active DNA effects with these tags
 *		Give me handles to all activate DNA effects modifying this attribute.
 *		
 *	Any requirements specified in the query are required: must meet "all" not "one".
 */
USTRUCT()
struct FActiveDNAEffectQuery
{
	GENERATED_USTRUCT_BODY()

	FActiveDNAEffectQuery()
		: OwningTagContainer(nullptr)
		, EffectTagContainer(nullptr)
		, OwningTagContainer_Rejection(nullptr)
		, EffectTagContainer_Rejection(nullptr)
		, EffectSource(nullptr)
		, EffectDef(nullptr)
	{
	}

	FActiveDNAEffectQuery(const FDNATagContainer* InOwningTagContainer)
		: OwningTagContainer(InOwningTagContainer)
		, EffectTagContainer(nullptr)
		, OwningTagContainer_Rejection(nullptr)
		, EffectTagContainer_Rejection(nullptr)
		, EffectSource(nullptr)
		, EffectDef(nullptr)
	{
	}

	/** Bind this to override the default query-matching code. */
	FActiveDNAEffectQueryCustomMatch CustomMatch;

	/** Returns true if Effect matches the criteria of this query, which will be overridden by CustomMatch if it is bound. Returns false otherwise. */
	bool Matches(const FActiveDNAEffect& Effect) const;

	/** used to match with InheritableOwnedTagsContainer */
	const FDNATagContainer* OwningTagContainer;

	/** used to match with InheritableDNAEffectTags */
	const FDNATagContainer* EffectTagContainer;

	/** used to reject matches with InheritableOwnedTagsContainer */
	const FDNATagContainer* OwningTagContainer_Rejection;

	/** used to reject matches with InheritableDNAEffectTags */
	const FDNATagContainer* EffectTagContainer_Rejection;

	// Matches on DNAEffects which modify given attribute
	FDNAAttribute ModifyingAttribute;

	// Matches on DNAEffects which come from this source
	const UObject* EffectSource;

	// Matches on DNAEffects with this definition
	const UDNAEffect* EffectDef;

	// Handles to ignore as matches, even if other criteria is met
	TArray<FActiveDNAEffectHandle> IgnoreHandles;
};

/** Helper struct to hold data about external dependencies for custom modifiers */
struct FCustomModifierDependencyHandle
{
	FCustomModifierDependencyHandle()
		: ActiveEffectHandles()
		, ActiveDelegateHandle()
	{}

	/** Set of handles of active DNA effects dependent upon a particular external dependency */
	TSet<FActiveDNAEffectHandle> ActiveEffectHandles;

	/** Delegate handle populated as a result of binding to an external dependency delegate */
	FDelegateHandle ActiveDelegateHandle;
};

/**
 * Active DNAEffects Container
 *	-Bucket of ActiveDNAEffects
 *	-Needed for FFastArraySerialization
 *  
 * This should only be used by UDNAAbilitySystemComponent. All of this could just live in UDNAAbilitySystemComponent except that we need a distinct USTRUCT to implement FFastArraySerializer.
 *
 * The preferred way to iterate through the ActiveDNAEffectContainer is with CreateConstITerator/CreateIterator or stl style range iteration:
 * 
 * 
 *	for (const FActiveDNAEffect& Effect : this)
 *	{
 *	}
 *
 *	for (auto It = CreateConstIterator(); It; ++It) 
 *	{
 *	}
 *
 */
USTRUCT()
struct DNAABILITIES_API FActiveDNAEffectsContainer : public FFastArraySerializer
{
	GENERATED_USTRUCT_BODY();

	friend struct FActiveDNAEffect;
	friend class UDNAAbilitySystemComponent;
	friend struct FScopedActiveDNAEffectLock;
	friend class ADNAAbilitySystemDebugHUD;
	friend class FActiveDNAEffectIterator<const FActiveDNAEffect, FActiveDNAEffectsContainer>;
	friend class FActiveDNAEffectIterator<FActiveDNAEffect, FActiveDNAEffectsContainer>;

	typedef FActiveDNAEffectIterator<const FActiveDNAEffect, FActiveDNAEffectsContainer> ConstIterator;
	typedef FActiveDNAEffectIterator<FActiveDNAEffect, FActiveDNAEffectsContainer> Iterator;

	FActiveDNAEffectsContainer();
	virtual ~FActiveDNAEffectsContainer();

	UDNAAbilitySystemComponent* Owner;
	bool OwnerIsNetAuthority;

	FOnGivenActiveDNAEffectRemoved	OnActiveDNAEffectRemovedDelegate;

	struct DebugExecutedDNAEffectData
	{
		FString DNAEffectName;
		FString ActivationState;
		FDNAAttribute Attribute;
		TEnumAsByte<EDNAModOp::Type> ModifierOp;
		float Magnitude;
		int32 StackCount;
	};

#if ENABLE_VISUAL_LOG
	// Stores a record of DNA effects that have executed and their results. Useful for debugging.
	TArray<DebugExecutedDNAEffectData> DebugExecutedDNAEffects;

	void GrabDebugSnapshot(FVisualLogEntry* Snapshot) const;
#endif // ENABLE_VISUAL_LOG

	void GetActiveDNAEffectDataByAttribute(TMultiMap<FDNAAttribute, FActiveDNAEffectsContainer::DebugExecutedDNAEffectData>& EffectMap) const;

	void RegisterWithOwner(UDNAAbilitySystemComponent* Owner);	
	
	FActiveDNAEffect* ApplyDNAEffectSpec(const FDNAEffectSpec& Spec, FPredictionKey& InPredictionKey, bool& bFoundExistingStackableGE);

	FActiveDNAEffect* GetActiveDNAEffect(const FActiveDNAEffectHandle Handle);

	const FActiveDNAEffect* GetActiveDNAEffect(const FActiveDNAEffectHandle Handle) const;

	void ExecuteActiveEffectsFrom(FDNAEffectSpec &Spec, FPredictionKey PredictionKey = FPredictionKey() );
	
	void ExecutePeriodicDNAEffect(FActiveDNAEffectHandle Handle);	// This should not be outward facing to the skill system API, should only be called by the owning DNAAbilitySystemComponent

	bool RemoveActiveDNAEffect(FActiveDNAEffectHandle Handle, int32 StacksToRemove);

	void GetDNAEffectStartTimeAndDuration(FActiveDNAEffectHandle Handle, float& EffectStartTime, float& EffectDuration) const;

	float GetDNAEffectMagnitude(FActiveDNAEffectHandle Handle, FDNAAttribute Attribute) const;

	void SetActiveDNAEffectLevel(FActiveDNAEffectHandle ActiveHandle, int32 NewLevel);

	void SetAttributeBaseValue(FDNAAttribute Attribute, float NewBaseValue);

	float GetAttributeBaseValue(FDNAAttribute Attribute) const;

	float GetEffectContribution(const FAggregatorEvaluateParameters& Parameters, FActiveDNAEffectHandle ActiveHandle, FDNAAttribute Attribute);

	/** Actually applies given mod to the attribute */
	void ApplyModToAttribute(const FDNAAttribute &Attribute, TEnumAsByte<EDNAModOp::Type> ModifierOp, float ModifierMagnitude, const FDNAEffectModCallbackData* ModData=nullptr);

	/**
	 * Get the source tags from the DNA spec represented by the specified handle, if possible
	 * 
	 * @param Handle	Handle of the DNA effect to retrieve source tags from
	 * 
	 * @return Source tags from the DNA spec represented by the handle, if possible
	 */
	const FDNATagContainer* GetDNAEffectSourceTagsFromHandle(FActiveDNAEffectHandle Handle) const;

	/**
	 * Get the target tags from the DNA spec represented by the specified handle, if possible
	 * 
	 * @param Handle	Handle of the DNA effect to retrieve target tags from
	 * 
	 * @return Target tags from the DNA spec represented by the handle, if possible
	 */
	const FDNATagContainer* GetDNAEffectTargetTagsFromHandle(FActiveDNAEffectHandle Handle) const;

	/**
	 * Populate the specified capture spec with the data necessary to capture an attribute from the container
	 * 
	 * @param OutCaptureSpec	[OUT] Capture spec to populate with captured data
	 */
	void CaptureAttributeForDNAEffect(OUT FDNAEffectAttributeCaptureSpec& OutCaptureSpec);

	void PrintAllDNAEffects() const;

	/**
	 *	Returns the total number of DNA effects.
	 *	NOTE this does include DNAEffects that pending removal.
	 *	Any pending remove DNA effects are deleted at the end of their scope lock
	 */
	FORCEINLINE int32 GetNumDNAEffects() const
	{
		int32 NumPending = 0;
		FActiveDNAEffect* PendingDNAEffect = PendingDNAEffectHead;
		FActiveDNAEffect* Stop = *PendingDNAEffectNext;
		while (PendingDNAEffect && PendingDNAEffect != Stop)
		{
			++NumPending;
			PendingDNAEffect = PendingDNAEffect->PendingNext;
		}

		return DNAEffects_Internal.Num() + NumPending;
	}

	void CheckDuration(FActiveDNAEffectHandle Handle);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms);

	void Uninitialize();	

	// ------------------------------------------------

	bool CanApplyAttributeModifiers(const UDNAEffect *DNAEffect, float Level, const FDNAEffectContextHandle& EffectContext);
	
	TArray<float> GetActiveEffectsTimeRemaining(const FDNAEffectQuery& Query) const;

	TArray<float> GetActiveEffectsDuration(const FDNAEffectQuery& Query) const;

	TArray<TPair<float,float>> GetActiveEffectsTimeRemainingAndDuration(const FDNAEffectQuery& Query) const;

	TArray<FActiveDNAEffectHandle> GetActiveEffects(const FDNAEffectQuery& Query) const;

	float GetActiveEffectsEndTime(const FDNAEffectQuery& Query) const;
	bool GetActiveEffectsEndTimeAndDuration(const FDNAEffectQuery& Query, float& EndTime, float& Duration) const;

	/** Returns an array of all of the active DNA effect handles */
	TArray<FActiveDNAEffectHandle> GetAllActiveEffectHandles() const;

	void ModifyActiveEffectStartTime(FActiveDNAEffectHandle Handle, float StartTimeDiff);

	int32 RemoveActiveEffects(const FDNAEffectQuery& Query, int32 StacksToRemove);

	/**
	 * Get the count of the effects matching the specified query (including stack count)
	 * 
	 * @return Count of the effects matching the specified query
	 */
	int32 GetActiveEffectCount(const FDNAEffectQuery& Query, bool bEnforceOnGoingCheck = true) const;

	float GetServerWorldTime() const;

	float GetWorldTime() const;

	bool HasReceivedEffectWithPredictedKey(FPredictionKey PredictionKey) const;

	bool HasPredictedEffectWithPredictedKey(FPredictionKey PredictionKey) const;
		
	void SetBaseAttributeValueFromReplication(FDNAAttribute Attribute, float BaseBalue);

	void GetAllActiveDNAEffectSpecs(TArray<FDNAEffectSpec>& OutSpecCopies) const;

	void DebugCyclicAggregatorBroadcasts(struct FAggregator* Aggregator);

	/** Performs a deep copy on the source container, duplicating all DNA effects and reconstructing the attribute aggregator map to match the passed in source. */
	void CloneFrom(const FActiveDNAEffectsContainer& Source);

	// -------------------------------------------------------------------------------------------

	FOnDNAAttributeChange& RegisterDNAAttributeEvent(FDNAAttribute Attribute);

	void OnOwnerTagChange(FDNATag TagChange, int32 NewCount);

	bool HasApplicationImmunityToSpec(const FDNAEffectSpec& SpecToApply, const FActiveDNAEffect*& OutGEThatProvidedImmunity) const;

	void IncrementLock();
	void DecrementLock();
	
	FORCEINLINE ConstIterator CreateConstIterator() const { return ConstIterator(*this);	}
	FORCEINLINE Iterator CreateIterator() { return Iterator(*this);	}

private:

	/**
	 *	Accessors for internal functions to get DNAEffects directly by index.
	 *	Note this will return DNAEffects that are pending removal!
	 *	
	 *	To iterate over all 'valid' DNA effects, use the CreateConstIterator/CreateIterator or the stl style range iterator
	 */
	FORCEINLINE const FActiveDNAEffect* GetActiveDNAEffect(int32 idx) const
	{
		return const_cast<FActiveDNAEffectsContainer*>(this)->GetActiveDNAEffect(idx);
	}

	FORCEINLINE FActiveDNAEffect* GetActiveDNAEffect(int32 idx)
	{
		if (idx < DNAEffects_Internal.Num())
		{
			return &DNAEffects_Internal[idx];
		}

		idx -= DNAEffects_Internal.Num();
		FActiveDNAEffect* Ptr = PendingDNAEffectHead;
		FActiveDNAEffect* Stop = *PendingDNAEffectNext;

		// Advance until the desired index or until hitting the actual end of the pending list currently in use (need to check both Ptr and Ptr->PendingNext to prevent hopping
		// the pointer too far along)
		while (idx-- > 0 && Ptr && Ptr != Stop && Ptr->PendingNext != Stop)
		{
			Ptr = Ptr->PendingNext;
		}

		return idx <= 0 ? Ptr : nullptr;
	}

	/** Our active list of Effects. Do not access this directly (Even from internal functions!) Use GetNumDNAEffect() / GetDNAEffect() ! */
	UPROPERTY()
	TArray<FActiveDNAEffect>	DNAEffects_Internal;

	void InternalUpdateNumericalAttribute(FDNAAttribute Attribute, float NewValue, const FDNAEffectModCallbackData* ModData);

	/** Cached pointer to current mod data needed for callbacks. We cache it in the AGE struct to avoid passing it through all the delegate/aggregator plumbing */
	const struct FDNAEffectModCallbackData* CurrentModcallbackData;
	
	/**
	 * Helper function to execute a mod on owned attributes
	 * 
	 * @param Spec			DNA effect spec executing the mod
	 * @param ModEvalData	Evaluated data for the mod
	 * 
	 * @return True if the mod successfully executed, false if it did not
	 */
	bool InternalExecuteMod(FDNAEffectSpec& Spec, FDNAModifierEvaluatedData& ModEvalData);

	bool IsNetAuthority() const
	{
		return OwnerIsNetAuthority;
	}

	/** Called internally to actually remove a DNAEffect or to reduce its StackCount. Returns true if we resized our internal DNAEffect array. */
	bool InternalRemoveActiveDNAEffect(int32 Idx, int32 StacksToRemove, bool bPrematureRemoval);
	
	/** Called both in server side creation and replication creation/deletion */
	void InternalOnActiveDNAEffectAdded(FActiveDNAEffect& Effect);
	void InternalOnActiveDNAEffectRemoved(FActiveDNAEffect& Effect, bool bInvokeDNACueEvents);

	void RemoveActiveDNAEffectGrantedTagsAndModifiers(const FActiveDNAEffect& Effect, bool bInvokeDNACueEvents);
	void AddActiveDNAEffectGrantedTagsAndModifiers(FActiveDNAEffect& Effect, bool bInvokeDNACueEvents);

	/** Updates tag dependency map when a DNAEffect is removed */
	void RemoveActiveEffectTagDependency(const FDNATagContainer& Tags, FActiveDNAEffectHandle Handle);

	/** Internal helper function to bind the active effect to all of the custom modifier magnitude external dependency delegates it contains, if any */
	void AddCustomMagnitudeExternalDependencies(FActiveDNAEffect& Effect);

	/** Internal helper function to unbind the active effect from all of the custom modifier magnitude external dependency delegates it may have bound to, if any */
	void RemoveCustomMagnitudeExternalDependencies(FActiveDNAEffect& Effect);

	/** Internal callback fired as a result of a custom modifier magnitude external dependency delegate firing; Updates affected active DNA effects as necessary */
	void OnCustomMagnitudeExternalDependencyFired(TSubclassOf<UDNAModMagnitudeCalculation> MagnitudeCalculationClass);

	/** Internal helper function to apply expiration effects from a removed/expired DNA effect spec */
	void InternalApplyExpirationEffects(const FDNAEffectSpec& ExpiringSpec, bool bPrematureRemoval);

	void RestartActiveDNAEffectDuration(FActiveDNAEffect& ActiveDNAEffect);

	// -------------------------------------------------------------------------------------------

	TMap<FDNAAttribute, FAggregatorRef>		AttributeAggregatorMap;

	TMap<FDNAAttribute, FOnDNAAttributeChange> AttributeChangeDelegates;

	TMap<FDNATag, TSet<FActiveDNAEffectHandle> >	ActiveEffectTagDependencies;

	/** Mapping of custom DNA modifier magnitude calculation class to dependency handles for triggering updates on external delegates firing */
	TMap<FObjectKey, FCustomModifierDependencyHandle> CustomMagnitudeClassDependencies;

	/** A map to manage stacking while we are the source */
	TMap<TWeakObjectPtr<UDNAEffect>, TArray<FActiveDNAEffectHandle> >	SourceStackingMap;
	
	/** Acceleration struct for immunity tests */
	FDNATagCountContainer ApplicationImmunityDNATagCountContainer;

	/** Active GEs that have immunity queries. This is an acceleration list to avoid searching through the Active DNAEffect list frequetly. (We only search for the active GE if immunity procs) */
	UPROPERTY()
	TArray<const UDNAEffect*> ApplicationImmunityQueryEffects;

	FAggregatorRef& FindOrCreateAttributeAggregator(FDNAAttribute Attribute);

	void OnAttributeAggregatorDirty(FAggregator* Aggregator, FDNAAttribute Attribute);

	void OnMagnitudeDependencyChange(FActiveDNAEffectHandle Handle, const FAggregator* ChangedAgg);

	void OnStackCountChange(FActiveDNAEffect& ActiveEffect, int32 OldStackCount, int32 NewStackCount);

	void OnDurationChange(FActiveDNAEffect& ActiveEffect);

	void UpdateAllAggregatorModMagnitudes(FActiveDNAEffect& ActiveEffect);

	void UpdateAggregatorModMagnitudes(const TSet<FDNAAttribute>& AttributesToUpdate, FActiveDNAEffect& ActiveEffect);

	/** Helper function to find the active GE that the specified spec can stack with, if any */
	FActiveDNAEffect* FindStackableActiveDNAEffect(const FDNAEffectSpec& Spec);
	
	/** Helper function to handle the case of same-effect stacking overflow; Returns true if the overflow application should apply, false if it should not */
	bool HandleActiveDNAEffectStackOverflow(const FActiveDNAEffect& ActiveStackableGE, const FDNAEffectSpec& OldSpec, const FDNAEffectSpec& OverflowingSpec);

	/** After application has gone through, give stacking rules a chance to do something as the source of the DNA effect (E.g., remove an old version) */
	virtual void ApplyStackingLogicPostApplyAsSource(UDNAAbilitySystemComponent* Target, const FDNAEffectSpec& SpecApplied, FActiveDNAEffectHandle ActiveHandle) { }

	bool ShouldUseMinimalReplication();

	mutable int32 ScopedLockCount;
	int32 PendingRemoves;

	FActiveDNAEffect*	PendingDNAEffectHead;	// Head of pending GE linked list
	FActiveDNAEffect** PendingDNAEffectNext;	// Points to the where to store the next pending GE (starts pointing at head, as more are added, points further down the list).

	/**
	 * DO NOT USE DIRECTLY
	 * STL-like iterators to enable range-based for loop support.
	 */

	FORCEINLINE friend Iterator begin(FActiveDNAEffectsContainer* Container) { return Container->CreateIterator(); }
	FORCEINLINE friend Iterator end(FActiveDNAEffectsContainer* Container) { return Iterator(*Container, -1); }

	FORCEINLINE friend ConstIterator begin(const FActiveDNAEffectsContainer* Container) { return Container->CreateConstIterator(); }
	FORCEINLINE friend ConstIterator end(const FActiveDNAEffectsContainer* Container) { return ConstIterator(*Container, -1); }
};

template<>
struct TStructOpsTypeTraits< FActiveDNAEffectsContainer > : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};

/**
 *	FScopedActiveDNAEffectLock
 *	Provides a mechanism for locking the active DNA effect list while possibly invoking callbacks into gamecode.
 *	For example, if some internal code in FActiveDNAEffectsContainer is iterating through the active GE list
 *	or holding onto a pointer to something in that list, any changes to that list could cause memory the move out from underneath.
 *	
 *	This scope lock will queue deletions and additions until after the scope is over. The additions and deletions will actually 
 *	go through, but we will defer the memory operations to the active DNA effect list.
 */
struct DNAABILITIES_API FScopedActiveDNAEffectLock
{
	FScopedActiveDNAEffectLock(FActiveDNAEffectsContainer& InContainer);
	~FScopedActiveDNAEffectLock();

private:
	FActiveDNAEffectsContainer& Container;
};

#define DNAEFFECT_SCOPE_LOCK()	FScopedActiveDNAEffectLock ActiveScopeLock(*this);


// -------------------------------------------------------------------------------------

/**
 * UDNAEffect
 *	The DNAEffect definition. This is the data asset defined in the editor that drives everything.
 *  This is only blueprintable to allow for templating DNA effects. DNA effects should NOT contain blueprint graphs.
 */
UCLASS(Blueprintable, meta = (ShortTooltip="A DNAEffect modifies attributes and tags."))
class DNAABILITIES_API UDNAEffect : public UObject, public IDNATagAssetInterface
{

public:
	GENERATED_UCLASS_BODY()

	// These are deprecated but remain for backwards compat, please use FDNAEffectConstants:: instead.
	static const float INFINITE_DURATION;
	static const float INSTANT_APPLICATION;
	static const float NO_PERIOD;	
	static const float INVALID_LEVEL;

#if WITH_EDITORONLY_DATA
	/** Template to derive starting values and editing customization from */
	UPROPERTY()
	UDNAEffectTemplate*	Template;

	/** When false, show a limited set of properties for editing, based on the template we are derived from */
	UPROPERTY()
	bool ShowAllProperties;
#endif

	virtual void PostInitProperties() override;
#if WITH_EDITOR
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** Policy for the duration of this effect */
	UPROPERTY(EditDefaultsOnly, Category=DNAEffect)
	EDNAEffectDurationType DurationPolicy;

	/** Duration in seconds. 0.0 for instantaneous effects; -1.0 for infinite duration. */
	UPROPERTY(EditDefaultsOnly, Category=DNAEffect)
	FDNAEffectModifierMagnitude DurationMagnitude;

	/** Period in seconds. 0.0 for non-periodic effects */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Period)
	FScalableFloat	Period;
	
	/** If true, the effect executes on application and then at every period interval. If false, no execution occurs until the first period elapses. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Period)
	bool bExecutePeriodicEffectOnApplication;

	/** Array of modifiers that will affect the target of this effect */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=DNAEffect)
	TArray<FDNAModifierInfo> Modifiers;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = DNAEffect)
	TArray<FDNAEffectExecutionDefinition>	Executions;

	/** Probability that this DNA effect will be applied to the target actor (0.0 for never, 1.0 for always) */
	UPROPERTY(EditDefaultsOnly, Category=Application, meta=(DNAAttribute="True"))
	FScalableFloat	ChanceToApplyToTarget;

	UPROPERTY(EditDefaultsOnly, Category=Application, DisplayName="Application Requirement")
	TArray<TSubclassOf<UDNAEffectCustomApplicationRequirement> > ApplicationRequirements;

	/** Deprecated. Use ConditionalDNAEffects instead */
	UPROPERTY()
	TArray<TSubclassOf<UDNAEffect>> TargetEffectClasses_DEPRECATED;

	/** other DNA effects that will be applied to the target of this effect if this effect applies */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = DNAEffect)
	TArray<FConditionalDNAEffect> ConditionalDNAEffects;

	/** Effects to apply when a stacking effect "overflows" its stack count through another attempted application. Added whether the overflow application succeeds or not. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Overflow)
	TArray<TSubclassOf<UDNAEffect>> OverflowEffects;

	/** If true, stacking attempts made while at the stack count will fail, resulting in the duration and context not being refreshed */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Overflow)
	bool bDenyOverflowApplication;

	/** If true, the entire stack of the effect will be cleared once it overflows */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Overflow, meta=(EditCondition="bDenyOverflowApplication"))
	bool bClearStackOnOverflow;

	/** Effects to apply when this effect is made to expire prematurely (like via a forced removal, clear tags, etc.); Only works for effects with a duration */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Expiration)
	TArray<TSubclassOf<UDNAEffect>> PrematureExpirationEffectClasses;

	/** Effects to apply when this effect expires naturally via its duration; Only works for effects with a duration */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Expiration)
	TArray<TSubclassOf<UDNAEffect>> RoutineExpirationEffectClasses;

	// ------------------------------------------------
	// DNA tag interface
	// ------------------------------------------------

	/** Overridden to return requirements tags */
	virtual void GetOwnedDNATags(FDNATagContainer& TagContainer) const override;

	void UpdateInheritedTagProperties();
	void ValidateDNAEffect();

	virtual void PostLoad() override;
	virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;

	// ----------------------------------------------

	/** If true, cues will only trigger when GE modifiers succeed being applied (whether through modifiers or executions) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Display)
	bool bRequireModifierSuccessToTriggerCues;

	/** If true, DNACues will only be triggered for the first instance in a stacking DNAEffect. */
	UPROPERTY(EditDefaultsOnly, Category = Display)
	bool bSuppressStackingCues;

	/** Cues to trigger non-simulated reactions in response to this DNAEffect such as sounds, particle effects, etc */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Display)
	TArray<FDNAEffectCue>	DNACues;

	/** Data for the UI representation of this effect. This should include things like text, icons, etc. Not available in server-only builds. */
	UPROPERTY(EditDefaultsOnly, Instanced, BlueprintReadOnly, Category = Display)
	class UDNAEffectUIData* UIData;

	// ----------------------------------------------------------------------
	//	Tag Containers
	// ----------------------------------------------------------------------
	
	/** The DNAEffect's Tags: tags the the GE *has* and DOES NOT give to the actor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Tags, meta = (DisplayName = "DNAEffectAssetTag"))
	FInheritedTagContainer InheritableDNAEffectTags;
	
	/** "These tags are applied to the actor I am applied to" */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Tags, meta=(DisplayName="GrantedTags"))
	FInheritedTagContainer InheritableOwnedTagsContainer;
	
	/** Once Applied, these tags requirements are used to determined if the DNAEffect is "on" or "off". A DNAEffect can be off and do nothing, but still applied. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Tags)
	FDNATagRequirements OngoingTagRequirements;

	/** Tag requirements for this DNAEffect to be applied to a target. This is pass/fail at the time of application. If fail, this GE fails to apply. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Tags)
	FDNATagRequirements ApplicationTagRequirements;

	/** DNAEffects that *have* tags in this container will be cleared upon effect application. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Tags)
	FInheritedTagContainer RemoveDNAEffectsWithTags;

	/** Grants the owner immunity from these source tags.  */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Immunity, meta = (DisplayName = "GrantedApplicationImmunityTags"))
	FDNATagRequirements GrantedApplicationImmunityTags;

	/** Grants immunity to DNAEffects that match this query. Queries are more powerful but slightly slower than GrantedApplicationImmunityTags. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Immunity)
	FDNAEffectQuery GrantedApplicationImmunityQuery;

	/** Cached !GrantedApplicationImmunityQuery.IsEmpty(). Set on PostLoad. */
	bool HasGrantedApplicationImmunityQuery;

	// ----------------------------------------------------------------------
	//	Stacking
	// ----------------------------------------------------------------------
	
	/** How this DNAEffect stacks with other instances of this same DNAEffect */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Stacking)
	EDNAEffectStackingType	StackingType;

	/** Stack limit for StackingType */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Stacking)
	int32 StackLimitCount;

	/** Policy for how the effect duration should be refreshed while stacking */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Stacking)
	EDNAEffectStackingDurationPolicy StackDurationRefreshPolicy;

	/** Policy for how the effect period should be reset (or not) while stacking */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Stacking)
	EDNAEffectStackingPeriodPolicy StackPeriodResetPolicy;

	/** Policy for how to handle duration expiring on this DNA effect */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Stacking)
	EDNAEffectStackingExpirationPolicy StackExpirationPolicy;

	// ----------------------------------------------------------------------
	//	Granted abilities
	// ----------------------------------------------------------------------
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Granted Abilities")
	TArray<FDNAAbilitySpecDef>	GrantedAbilities;
};
