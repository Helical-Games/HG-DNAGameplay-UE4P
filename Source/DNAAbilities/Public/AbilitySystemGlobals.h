// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Misc/StringAssetReference.h"
#include "Misc/StringClassReference.h"
#include "DNATagContainer.h"
#include "DNAEffectTypes.h"
#include "DNAAbilitiesModule.h"
#include "AbilitySystemGlobals.generated.h"

class UDNAAbilitySystemComponent;
class UDNACueManager;
class UDNATagReponseTable;
struct FDNAAbilityActorInfo;
struct FDNAEffectSpec;
struct FDNAEffectSpecForRPC;

/** Called when ability fails to activate, passes along the failed ability and a tag explaining why */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDNAAbilitySystemAssetOpenedDelegate, FString , int );
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDNAAbilitySystemAssetFoundDelegate, FString, int);


/** Holds global data for the ability system. Can be configured per project via config file */
UCLASS(config=Game)
class DNAABILITIES_API UDNAAbilitySystemGlobals : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Gets the single instance of the globals object, will create it as necessary */
	static UDNAAbilitySystemGlobals& Get()
	{
		return *IDNAAbilitiesModule::Get().GetDNAAbilitySystemGlobals();
	}

	/** Should be called once as part of project setup to load global data tables and tags */
	virtual void InitGlobalData();

	/** Returns true if InitGlobalData has been called */
	bool IsDNAAbilitySystemGlobalsInitialized()
	{
		return GlobalAttributeSetInitter.IsValid();
	}

	/** Returns the curvetable used as the default for scalable floats that don't specify a curve table */
	UCurveTable* GetGlobalCurveTable();

	/** Returns the data table defining attribute metadata (NOTE: Currently not in use) */
	UDataTable* GetGlobalAttributeMetaDataTable();

	/** Returns data used to initialize attributes to their default values */
	FAttributeSetInitter* GetAttributeSetInitter() const;

	/** Searches the passed in actor for an ability system component, will use the DNAAbilitySystemInterface */
	static UDNAAbilitySystemComponent* GetDNAAbilitySystemComponentFromActor(const AActor* Actor, bool LookForComponent=false);

	/** Should allocate a project specific AbilityActorInfo struct. Caller is responsible for deallocation */
	virtual FDNAAbilityActorInfo* AllocAbilityActorInfo() const;

	/** Should allocate a project specific DNAEffectContext struct. Caller is responsible for deallocation */
	virtual FDNAEffectContext* AllocDNAEffectContext() const;

	/** Global callback that can handle game-specific code that needs to run before applying a DNA effect spec */
	virtual void GlobalPreDNAEffectSpecApply(FDNAEffectSpec& Spec, UDNAAbilitySystemComponent* DNAAbilitySystemComponent);

	/** Returns true if the ability system should try to predict DNA effects applied to non local targets */
	bool ShouldPredictTargetDNAEffects() const
	{
		return PredictTargetDNAEffects;
	}

	/** Searches the passed in class to look for a UFunction implementing the DNA cue tag, sets MatchedTag to the exact tag found */
	UFunction* GetDNACueFunction(const FDNATag &Tag, UClass* Class, FName &MatchedTag);

	/** Returns the DNA cue manager singleton object, creating if necessary */
	virtual UDNACueManager* GetDNACueManager();

	/** Returns the DNA tag response object, creating if necessary */
	UDNATagReponseTable* GetDNATagResponseTable();

	/** Sets a default DNA cue tag using the asset's name. Returns true if it changed the tag. */
	static bool DeriveDNACueTagFromAssetName(FString AssetName, FDNATag& DNACueTag, FName& DNACueName);

	template<class T>
	static void DeriveDNACueTagFromClass(T* CDO)
	{
#if WITH_EDITOR
		UClass* ParentClass = CDO->GetClass()->GetSuperClass();
		if (T* ParentCDO = Cast<T>(ParentClass->GetDefaultObject()))
		{
			if (ParentCDO->DNACueTag.IsValid() && (ParentCDO->DNACueTag == CDO->DNACueTag))
			{
				// Parente has a valid tag. But maybe there is a better one for this class to use.
				// Reset our DNACueTag and see if we find one.
				FDNATag ParentTag = ParentCDO->DNACueTag;
				CDO->DNACueTag = FDNATag();
				if (UDNAAbilitySystemGlobals::DeriveDNACueTagFromAssetName(CDO->GetName(), CDO->DNACueTag, CDO->DNACueName) == false)
				{
					// We did not find one, so parent tag it is.
					CDO->DNACueTag = ParentTag;
				}
				return;
			}
		}
		UDNAAbilitySystemGlobals::DeriveDNACueTagFromAssetName(CDO->GetName(), CDO->DNACueTag, CDO->DNACueName);
#endif
	}

	/** The class to instantiate as the globals object. Defaults to this class but can be overridden */
	UPROPERTY(config)
	FStringClassReference DNAAbilitySystemGlobalsClassName;

	void AutomationTestOnly_SetGlobalCurveTable(UCurveTable *InTable)
	{
		GlobalCurveTable = InTable;
	}

	void AutomationTestOnly_SetGlobalAttributeDataTable(UDataTable *InTable)
	{
		GlobalAttributeMetaDataTable = InTable;
	}

	// Cheat functions

	/** Toggles whether we should ignore ability cooldowns. Does nothing in shipping builds */
	UFUNCTION(exec)
	virtual void ToggleIgnoreDNAAbilitySystemCooldowns();

	/** Toggles whether we should ignore ability costs. Does nothing in shipping builds */
	UFUNCTION(exec)
	virtual void ToggleIgnoreDNAAbilitySystemCosts();

	/** Returns true if ability cooldowns are ignored, returns false otherwise. Always returns false in shipping builds. */
	bool ShouldIgnoreCooldowns() const;

	/** Returns true if ability costs are ignored, returns false otherwise. Always returns false in shipping builds. */
	bool ShouldIgnoreCosts() const;

	DECLARE_MULTICAST_DELEGATE(FOnClientServerDebugAvailable);
	FOnClientServerDebugAvailable OnClientServerDebugAvailable;

	/** Global place to accumulate debug strings for ability system component. Used when we fill up client side debug string immediately, and then wait for server to send server strings */
	TArray<FString>	DNAAbilitySystemDebugStrings;


	// Helper functions for applying global scaling to various ability system tasks. This isn't meant to be a shipping feature, but to help with debugging and interation via cvar DNAAbilitySystem.GlobalAbilityScale.
	static void NonShipping_ApplyGlobalAbilityScaler_Rate(float& Rate);
	static void NonShipping_ApplyGlobalAbilityScaler_Duration(float& Duration);

	// Global Tags

	UPROPERTY()
	FDNATag ActivateFailCooldownTag; // TryActivate failed due to being on cooldown
	UPROPERTY(config)
	FName ActivateFailCooldownName;

	UPROPERTY()
	FDNATag ActivateFailCostTag; // TryActivate failed due to not being able to spend costs
	UPROPERTY(config)
	FName ActivateFailCostName;

	UPROPERTY()
	FDNATag ActivateFailTagsBlockedTag; // TryActivate failed due to being blocked by other abilities
	UPROPERTY(config)
	FName ActivateFailTagsBlockedName;

	UPROPERTY()
	FDNATag ActivateFailTagsMissingTag; // TryActivate failed due to missing required tags
	UPROPERTY(config)
	FName ActivateFailTagsMissingName;

	UPROPERTY()
	FDNATag ActivateFailNetworkingTag; // Failed to activate due to invalid networking settings, this is designer error
	UPROPERTY(config)
	FName ActivateFailNetworkingName;

	/** How many bits to use for "number of tags" in FMinimapReplicationTagCountMap::NetSerialize.  */
	UPROPERTY(config)
	int32	MinimalReplicationTagCountBits;

	virtual void InitGlobalTags()
	{
		if (ActivateFailCooldownName != NAME_None)
		{
			ActivateFailCooldownTag = FDNATag::RequestDNATag(ActivateFailCooldownName);
		}

		if (ActivateFailCostName != NAME_None)
		{
			ActivateFailCostTag = FDNATag::RequestDNATag(ActivateFailCostName);
		}

		if (ActivateFailTagsBlockedName != NAME_None)
		{
			ActivateFailTagsBlockedTag = FDNATag::RequestDNATag(ActivateFailTagsBlockedName);
		}

		if (ActivateFailTagsMissingName != NAME_None)
		{
			ActivateFailTagsMissingTag = FDNATag::RequestDNATag(ActivateFailTagsMissingName);
		}

		if (ActivateFailNetworkingName != NAME_None)
		{
			ActivateFailNetworkingTag = FDNATag::RequestDNATag(ActivateFailNetworkingName);
		}
	}

	// DNACue Parameters
	virtual void InitDNACueParameters(FDNACueParameters& CueParameters, const FDNAEffectSpecForRPC &Spec);
	virtual void InitDNACueParameters_GESpec(FDNACueParameters& CueParameters, const FDNAEffectSpec &Spec);
	virtual void InitDNACueParameters(FDNACueParameters& CueParameters, const FDNAEffectContextHandle& EffectContext);

	// Trigger async loading of the DNA cue object libraries. By default, the manager will do this on creation,
	// but that behavior can be changed by a derived class overriding ShouldAsyncLoadObjectLibrariesAtStart and returning false.
	// In that case, this function must be called to begin the load
	virtual void StartAsyncLoadingObjectLibraries();

	/** Simple accessor to whether DNA modifier evaluation channels should be allowed or not */
	bool ShouldAllowDNAModEvaluationChannels() const;

	/**
	 * Returns whether the specified DNA mod evaluation channel is valid for use or not.
	 * Considers whether channel usage is allowed at all, as well as if the specified channel has a valid alias for the game.
	 */
	bool IsDNAModEvaluationChannelValid(EDNAModEvaluationChannel Channel) const;

	/** Simple channel-based accessor to the alias name for the specified DNA mod evaluation channel, if any */
	const FName& GetDNAModEvaluationChannelAlias(EDNAModEvaluationChannel Channel) const;

	/** Simple index-based accessor to the alias name for the specified DNA mod evaluation channel, if any */
	const FName& GetDNAModEvaluationChannelAlias(int32 Index) const;

	virtual TArray<FString> GetDNACueNotifyPaths() { return DNACueNotifyPaths; }

protected:

	virtual void InitAttributeDefaults();
	virtual void ReloadAttributeDefaults();
	virtual void AllocAttributeSetInitter();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// data used for ability system cheat commands

	/** If we should ignore the cooldowns when activating abilities in the ability system. Set with ToggleIgnoreDNAAbilitySystemCooldowns() */
	bool bIgnoreDNAAbilitySystemCooldowns;

	/** If we should ignore the costs when activating abilities in the ability system. Set with ToggleIgnoreDNAAbilitySystemCosts() */
	bool bIgnoreDNAAbilitySystemCosts;
#endif // #if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	/** Whether the game should allow the usage of DNA mod evaluation channels or not */
	UPROPERTY(config)
	bool bAllowDNAModEvaluationChannels;

	/** The default mod evaluation channel for the game */
	UPROPERTY(config)
	EDNAModEvaluationChannel DefaultDNAModEvaluationChannel;

	/** Game-specified named aliases for DNA mod evaluation channels; Only those with valid aliases are eligible to be used in a game (except Channel0, which is always valid) */
	UPROPERTY(config)
	FName DNAModEvaluationChannelAliases[static_cast<int32>(EDNAModEvaluationChannel::Channel_MAX)];

	/** Name of global curve table to use as the default for scalable floats, etc. */
	UPROPERTY(config)
	FStringAssetReference GlobalCurveTableName;

	/** Holds information about the valid attributes' min and max values and stacking rules */
	UPROPERTY(config)
	FStringAssetReference GlobalAttributeMetaDataTableName;

	/** Holds default values for attribute sets, keyed off of Name/Levels. NOTE: Preserved for backwards compatibility, should use the array version below now */
	UPROPERTY(config)
	FStringAssetReference GlobalAttributeSetDefaultsTableName;

	/** Array of curve table names to use for default values for attribute sets, keyed off of Name/Levels */
	UPROPERTY(config)
	TArray<FStringAssetReference> GlobalAttributeSetDefaultsTableNames;

	/** Class reference to DNA cue manager. Use this if you want to just instantiate a class for your DNA cue manager without having to create an asset. */
	UPROPERTY(config)
	FStringAssetReference GlobalDNACueManagerClass;

	/** Object reference to DNA cue manager (E.g., reference to a specific blueprint of your DNACueManager class. This is not necessary unless you want to have data or blueprints in your DNA cue manager. */
	UPROPERTY(config)
	FStringAssetReference GlobalDNACueManagerName;

	/** Look in these paths for DNACueNotifies. These are your "always loaded" set. */
	UPROPERTY(config)
	TArray<FString>	DNACueNotifyPaths;

	/** The class to instantiate as the DNATagResponseTable. */
	UPROPERTY(config)
	FStringAssetReference DNATagResponseTableName;

	UPROPERTY()
	UDNATagReponseTable* DNATagResponseTable;

	/** Set to true if you want clients to try to predict DNA effects done to targets. If false it will only predict self effects */
	UPROPERTY(config)
	bool PredictTargetDNAEffects;

	UPROPERTY()
	UCurveTable* GlobalCurveTable;

	/** Curve tables containing default values for attribute sets, keyed off of Name/Levels */
	UPROPERTY()
	TArray<UCurveTable*> GlobalAttributeDefaultsTables;

	UPROPERTY()
	UDataTable* GlobalAttributeMetaDataTable;

	UPROPERTY()
	UDNACueManager* GlobalDNACueManager;

	TSharedPtr<FAttributeSetInitter> GlobalAttributeSetInitter;

	template <class T>
	T* InternalGetLoadTable(T*& Table, FString TableName);

#if WITH_EDITOR
	void OnTableReimported(UObject* InObject);

	void OnPreBeginPIE(const bool bIsSimulatingInEditor);
#endif

	void ResetCachedData();
	void HandlePreLoadMap(const FString& MapName);

#if WITH_EDITORONLY_DATA
	bool RegisteredReimportCallback;
#endif

public:
	//To add functionality for opening assets directly from the game.
	void Notify_OpenAssetInEditor(FString AssetName, int AssetType);
	FOnDNAAbilitySystemAssetOpenedDelegate AbilityOpenAssetInEditorCallbacks;

	//...for finding assets directly from the game.
	void Notify_FindAssetInEditor(FString AssetName, int AssetType);
	FOnDNAAbilitySystemAssetFoundDelegate AbilityFindAssetInEditorCallbacks;
};
