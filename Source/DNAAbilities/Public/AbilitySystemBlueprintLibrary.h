// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "UObject/ScriptInterface.h"
#include "DNATagContainer.h"
#include "AttributeSet.h"
#include "DNAEffectTypes.h"
#include "Abilities/DNAAbilityTargetTypes.h"
#include "DNACueInterface.h"
#include "Abilities/DNAAbilityTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Abilities/DNAAbilityTargetDataFilter.h"
#include "AbilitySystemBlueprintLibrary.generated.h"

class UDNAAbilitySystemComponent;
class UDNAEffect;

// meta =(RestrictedToClasses="DNAAbility")
UCLASS()
class DNAABILITIES_API UDNAAbilitySystemBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	UFUNCTION(BlueprintPure, Category = Ability)
	static UDNAAbilitySystemComponent* GetDNAAbilitySystemComponent(AActor *Actor);

	// NOTE: The Actor passed in must implement IDNAAbilitySystemInterface! or else this function will silently fail to
	// send the event.  The actor needs the interface to find the UDNAAbilitySystemComponent, and if the component isn't
	// found, the event will not be sent.
	UFUNCTION(BlueprintCallable, Category = Ability, Meta = (Tooltip = "This function can be used to trigger an ability on the actor in question with useful payload data."))
	static void SendDNAEventToActor(AActor* Actor, FDNATag EventTag, FDNAEventData Payload);
	
	// -------------------------------------------------------------------------------
	//		Attribute
	// -------------------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Ability|Attribute")
	static bool IsValid(FDNAAttribute Attribute);

	/** Returns the value of Attribute from the ability system component belonging to Actor. */
	UFUNCTION(BlueprintPure, Category = "Ability|Attribute")
	static float GetFloatAttribute(const class AActor* Actor, FDNAAttribute Attribute, bool& bSuccessfullyFoundAttribute);

	/** Returns the value of Attribute from the ability system component DNAAbilitySystem. */
	UFUNCTION(BlueprintPure, Category = "Ability|Attribute")
	static float GetFloatAttributeFromDNAAbilitySystemComponent(const class UDNAAbilitySystemComponent* DNAAbilitySystem, FDNAAttribute Attribute, bool& bSuccessfullyFoundAttribute);

	/** Returns the base value of Attribute from the ability system component belonging to Actor. */
	UFUNCTION(BlueprintPure, Category = "Ability|Attribute")
	static float GetFloatAttributeBase(const class AActor* Actor, FDNAAttribute Attribute, bool& bSuccessfullyFoundAttribute);

	/** Returns the base value of Attribute from the ability system component DNAAbilitySystemComponent. */
	UFUNCTION(BlueprintPure, Category = "Ability|Attribute")
	static float GetFloatAttributeBaseFromDNAAbilitySystemComponent(const class UDNAAbilitySystemComponent* DNAAbilitySystemComponent, FDNAAttribute Attribute, bool& bSuccessfullyFoundAttribute);

	/** Returns the value of Attribute from the ability system component DNAAbilitySystem after evaluating it with source and target tags. bSuccess indicates the success or failure of this operation. */
	UFUNCTION(BlueprintPure, Category = "Ability|Attribute")
	static float EvaluateAttributeValueWithTags(class UDNAAbilitySystemComponent* DNAAbilitySystem, FDNAAttribute Attribute, const FDNATagContainer& SourceTags, const FDNATagContainer& TargetTags, bool& bSuccess);

	/** Returns the value of Attribute from the ability system component DNAAbilitySystem after evaluating it with source and target tags using the passed in base value instead of the real base value. bSuccess indicates the success or failure of this operation. */
	UFUNCTION(BlueprintPure, Category = "Ability|Attribute")
	static float EvaluateAttributeValueWithTagsAndBase(class UDNAAbilitySystemComponent* DNAAbilitySystem, FDNAAttribute Attribute, const FDNATagContainer& SourceTags, const FDNATagContainer& TargetTags, float BaseValue, bool& bSuccess);

	/** Simple equality operator for DNA attributes */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Equal (DNA Attribute)", CompactNodeTitle = "==", Keywords = "== equal"), Category="Ability|Attribute")
	static bool EqualEqual_DNAAttributeDNAAttribute(FDNAAttribute AttributeA, FDNAAttribute AttributeB);

	/** Simple inequality operator for DNA attributes */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Not Equal (DNA Attribute)", CompactNodeTitle = "!=", Keywords = "!= not equal"), Category="Ability|Attribute")
	static bool NotEqual_DNAAttributeDNAAttribute(FDNAAttribute AttributeA, FDNAAttribute AttributeB);

	// -------------------------------------------------------------------------------
	//		TargetData
	// -------------------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Ability|TargetData")
	static FDNAAbilityTargetDataHandle AppendTargetDataHandle(FDNAAbilityTargetDataHandle TargetHandle, const FDNAAbilityTargetDataHandle& HandleToAdd);

	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static FDNAAbilityTargetDataHandle	AbilityTargetDataFromLocations(const FDNAAbilityTargetingLocationInfo& SourceLocation, const FDNAAbilityTargetingLocationInfo& TargetLocation);

	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static FDNAAbilityTargetDataHandle	AbilityTargetDataFromHitResult(const FHitResult& HitResult);

	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static int32 GetDataCountFromTargetData(const FDNAAbilityTargetDataHandle& TargetData);

	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static FDNAAbilityTargetDataHandle	AbilityTargetDataFromActor(AActor* Actor);

	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static FDNAAbilityTargetDataHandle	AbilityTargetDataFromActorArray(const TArray<AActor*>& ActorArray, bool OneTargetPerHandle);

	/** Create a new target data handle with filtration performed on the data */
	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static FDNAAbilityTargetDataHandle	FilterTargetData(const FDNAAbilityTargetDataHandle& TargetDataHandle, FDNATargetDataFilterHandle ActorFilterClass);

	/** Create a handle for filtering target data, filling out all fields */
	UFUNCTION(BlueprintPure, Category = "Filter")
	static FDNATargetDataFilterHandle MakeFilterHandle(FDNATargetDataFilter Filter, AActor* FilterActor);

	/** Create a spec handle, filling out all fields */
	UFUNCTION(BlueprintPure, Category = "Spec")
	static FDNAEffectSpecHandle MakeSpecHandle(UDNAEffect* InDNAEffect, AActor* InInstigator, AActor* InEffectCauser, float InLevel = 1.0f);

	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static TArray<AActor*> GetActorsFromTargetData(const FDNAAbilityTargetDataHandle& TargetData, int32 Index);

	/** Returns true if the given TargetData has the actor passed in targeted */
	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static bool DoesTargetDataContainActor(const FDNAAbilityTargetDataHandle& TargetData, int32 Index, AActor* Actor);

	/** Returns true if the given TargetData has at least 1 actor targeted */
	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static bool TargetDataHasActor(const FDNAAbilityTargetDataHandle& TargetData, int32 Index);

	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static bool TargetDataHasHitResult(const FDNAAbilityTargetDataHandle& HitResult, int32 Index);

	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static FHitResult GetHitResultFromTargetData(const FDNAAbilityTargetDataHandle& HitResult, int32 Index);

	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static bool TargetDataHasOrigin(const FDNAAbilityTargetDataHandle& TargetData, int32 Index);

	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static FTransform GetTargetDataOrigin(const FDNAAbilityTargetDataHandle& TargetData, int32 Index);

	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static bool TargetDataHasEndPoint(const FDNAAbilityTargetDataHandle& TargetData, int32 Index);

	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static FVector GetTargetDataEndPoint(const FDNAAbilityTargetDataHandle& TargetData, int32 Index);

	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static FTransform GetTargetDataEndPointTransform(const FDNAAbilityTargetDataHandle& TargetData, int32 Index);

	// -------------------------------------------------------------------------------
	//		DNAEffectContext
	// -------------------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Ability|EffectContext", Meta = (DisplayName = "IsValid"))
	static bool EffectContextIsValid(FDNAEffectContextHandle EffectContext);

	UFUNCTION(BlueprintPure, Category = "Ability|EffectContext", Meta = (DisplayName = "IsInstigatorLocallyControlled"))
	static bool EffectContextIsInstigatorLocallyControlled(FDNAEffectContextHandle EffectContext);

	UFUNCTION(BlueprintPure, Category = "Ability|EffectContext", Meta = (DisplayName = "GetHitResult"))
	static FHitResult EffectContextGetHitResult(FDNAEffectContextHandle EffectContext);

	UFUNCTION(BlueprintPure, Category = "Ability|EffectContext", Meta = (DisplayName = "HasHitResult"))
	static bool EffectContextHasHitResult(FDNAEffectContextHandle EffectContext);

	UFUNCTION(BlueprintCallable, Category = "Ability|EffectContext", Meta = (DisplayName = "AddHitResult"))
	static void EffectContextAddHitResult(FDNAEffectContextHandle EffectContext, FHitResult HitResult, bool bReset);

	/** Gets the location the effect originated from */
	UFUNCTION(BlueprintPure, Category = "Ability|EffectContext", Meta = (DisplayName = "GetOrigin"))
	static FVector EffectContextGetOrigin(FDNAEffectContextHandle EffectContext);

	/** Sets the location the effect originated from */
	UFUNCTION(BlueprintCallable, Category = "Ability|EffectContext", Meta = (DisplayName = "SetOrigin"))
	static void EffectContextSetOrigin(FDNAEffectContextHandle EffectContext, FVector Origin);

	/** Gets the instigating actor (that holds the ability system component) of the EffectContext */
	UFUNCTION(BlueprintPure, Category = "Ability|EffectContext", Meta = (DisplayName = "GetInstigatorActor"))
	static AActor* EffectContextGetInstigatorActor(FDNAEffectContextHandle EffectContext);

	/** Gets the original instigator actor that started the chain of events to cause this effect */
	UFUNCTION(BlueprintPure, Category = "Ability|EffectContext", Meta = (DisplayName = "GetOriginalInstigatorActor"))
	static AActor* EffectContextGetOriginalInstigatorActor(FDNAEffectContextHandle EffectContext);

	/** Gets the physical actor that caused the effect, possibly a projectile or weapon */
	UFUNCTION(BlueprintPure, Category = "Ability|EffectContext", Meta = (DisplayName = "GetEffectCauser"))
	static AActor* EffectContextGetEffectCauser(FDNAEffectContextHandle EffectContext);

	/** Gets the source object of the effect. */
	UFUNCTION(BlueprintPure, Category = "Ability|EffectContext", Meta = (DisplayName = "GetSourceObject"))
	static UObject* EffectContextGetSourceObject(FDNAEffectContextHandle EffectContext);

	// -------------------------------------------------------------------------------
	//		DNACue
	// -------------------------------------------------------------------------------
	
	UFUNCTION(BlueprintPure, Category="Ability|DNACue")
	static bool IsInstigatorLocallyControlled(FDNACueParameters Parameters);

	UFUNCTION(BlueprintPure, Category="Ability|DNACue")
	static bool IsInstigatorLocallyControlledPlayer(FDNACueParameters Parameters);

	UFUNCTION(BlueprintPure, Category = "Ability|DNACue")
	static int32 GetActorCount(FDNACueParameters Parameters);

	UFUNCTION(BlueprintPure, Category = "Ability|DNACue")
	static AActor* GetActorByIndex(FDNACueParameters Parameters, int32 Index);

	UFUNCTION(BlueprintPure, Category = "Ability|DNACue")
	static FHitResult GetHitResult(FDNACueParameters Parameters);

	UFUNCTION(BlueprintPure, Category = "Ability|DNACue")
	static bool HasHitResult(FDNACueParameters Parameters);

	/** Forwards the DNA cue to another DNA cue interface object */
	UFUNCTION(BlueprintCallable, Category = "Ability|DNACue")
	static void ForwardDNACueToTarget(TScriptInterface<IDNACueInterface> TargetCueInterface, EDNACueEvent::Type EventType, FDNACueParameters Parameters);

	/** Gets the instigating actor (that holds the ability system component) of the DNACue */
	UFUNCTION(BlueprintPure, Category = "Ability|DNACue")
	static AActor* GetInstigatorActor(FDNACueParameters Parameters);

	/** Gets instigating world location */
	UFUNCTION(BlueprintPure, Category = "Ability|DNACue")
	static FTransform GetInstigatorTransform(FDNACueParameters Parameters);

	/** Gets instigating world location */
	UFUNCTION(BlueprintPure, Category = "Ability|DNACue")
	static FVector GetOrigin(FDNACueParameters Parameters);

	/** Gets the best end location and normal for this DNA cue. If there is hit result data, it will return this. Otherwise it will return the target actor's location/rotation. If none of this is available, it will return false. */
	UFUNCTION(BlueprintPure, Category = "Ability|DNACue")
	static bool GetDNACueEndLocationAndNormal(AActor* TargetActor, FDNACueParameters Parameters, FVector& Location, FVector& Normal);

	/** Gets the best normalized effect direction for this DNA cue. This is useful for effects that require the direction of an enemy attack. Returns true if a valid direction could be calculated. */
	UFUNCTION(BlueprintPure, Category = "Ability|DNACue")
	static bool GetDNACueDirection(AActor* TargetActor, FDNACueParameters Parameters, FVector& Direction);

	/** Returns true if the aggregated source and target tags from the effect spec meets the tag requirements */
	UFUNCTION(BlueprintPure, Category = "Ability|DNACue")
	static bool DoesDNACueMeetTagRequirements(FDNACueParameters Parameters, UPARAM(ref) FDNATagRequirements& SourceTagReqs, UPARAM(ref) FDNATagRequirements& TargetTagReqs);

	// -------------------------------------------------------------------------------
	//		DNAEffectSpec
	// -------------------------------------------------------------------------------
	
	UFUNCTION(BlueprintCallable, Category = "Ability|DNAEffect")
	static FDNAEffectSpecHandle AssignSetByCallerMagnitude(FDNAEffectSpecHandle SpecHandle, FName DataName, float Magnitude);

	UFUNCTION(BlueprintCallable, Category = "Ability|DNAEffect")
	static FDNAEffectSpecHandle SetDuration(FDNAEffectSpecHandle SpecHandle, float Duration);

	// This instance of the effect will now grant NewDNATag to the object that this effect is applied to.
	UFUNCTION(BlueprintCallable, Category = "Ability|DNAEffect")
	static FDNAEffectSpecHandle AddGrantedTag(FDNAEffectSpecHandle SpecHandle, FDNATag NewDNATag);

	// This instance of the effect will now grant NewDNATags to the object that this effect is applied to.
	UFUNCTION(BlueprintCallable, Category = "Ability|DNAEffect")
	static FDNAEffectSpecHandle AddGrantedTags(FDNAEffectSpecHandle SpecHandle, FDNATagContainer NewDNATags);

	// Adds NewDNATag to this instance of the effect.
	UFUNCTION(BlueprintCallable, Category = "Ability|DNAEffect")
	static FDNAEffectSpecHandle AddAssetTag(FDNAEffectSpecHandle SpecHandle, FDNATag NewDNATag);

	// Adds NewDNATags to this instance of the effect.
	UFUNCTION(BlueprintCallable, Category = "Ability|DNAEffect")
	static FDNAEffectSpecHandle AddAssetTags(FDNAEffectSpecHandle SpecHandle, FDNATagContainer NewDNATags);

	UFUNCTION(BlueprintCallable, Category = "Ability|DNAEffect")
	static FDNAEffectSpecHandle AddLinkedDNAEffectSpec(FDNAEffectSpecHandle SpecHandle, FDNAEffectSpecHandle LinkedDNAEffectSpec);

	/** Sets the DNAEffectSpec's StackCount to the specified amount (prior to applying) */
	UFUNCTION(BlueprintCallable, Category = "Ability|DNAEffect")
	static FDNAEffectSpecHandle SetStackCount(FDNAEffectSpecHandle SpecHandle, int32 StackCount);

	/** Sets the DNAEffectSpec's StackCount to the max stack count defined in the DNAEffect definition */
	UFUNCTION(BlueprintCallable, Category = "Ability|DNAEffect")
	static FDNAEffectSpecHandle SetStackCountToMax(FDNAEffectSpecHandle SpecHandle);

	/** Gets the DNAEffectSpec's effect context handle */
	UFUNCTION(BlueprintCallable, Category = "Ability|DNAEffect")
	static FDNAEffectContextHandle GetEffectContext(FDNAEffectSpecHandle SpecHandle);

	// -------------------------------------------------------------------------------
	//		DNAEffectSpec
	// -------------------------------------------------------------------------------

	/** Gets the magnitude of change for an attribute on an APPLIED DNAEffectSpec. */
	UFUNCTION(BlueprintCallable, Category = "Ability|DNAEffect")
	static float GetModifiedAttributeMagnitude(FDNAEffectSpecHandle SpecHandle, FDNAAttribute Attribute);

	// -------------------------------------------------------------------------------
	//		FActiveDNAEffectHandle
	// -------------------------------------------------------------------------------
	
	/** Returns current stack count of an active DNA Effect. Will return 0 if the DNAEffect is no longer valid. */
	UFUNCTION(BlueprintCallable, Category = "Ability|DNAEffect")
	static int32 GetActiveDNAEffectStackCount(FActiveDNAEffectHandle ActiveHandle);

	/** Returns stack limit count of an active DNA Effect. Will return 0 if the DNAEffect is no longer valid. */
	UFUNCTION(BlueprintCallable, Category = "Ability|DNAEffect")
	static int32 GetActiveDNAEffectStackLimitCount(FActiveDNAEffectHandle ActiveHandle);

	UFUNCTION(BlueprintPure, Category = "Ability|DNAEffect", Meta = (DisplayName = "Get Active DNAEffect Debug String "))
	static FString GetActiveDNAEffectDebugString(FActiveDNAEffectHandle ActiveHandle);
};
