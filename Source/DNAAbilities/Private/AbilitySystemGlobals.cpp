// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/DNAAbilityTypes.h"
#include "AbilitySystemStats.h"
#include "DNACueInterface.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "DNACueManager.h"
#include "DNATagResponseTable.h"
#include "DNATagsManager.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

UDNAAbilitySystemGlobals::UDNAAbilitySystemGlobals(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	DNAAbilitySystemGlobalsClassName = FStringClassReference(TEXT("/Script/DNAAbilities.DNAAbilitySystemGlobals"));

	PredictTargetDNAEffects = true;

	MinimalReplicationTagCountBits = 5;

	bAllowDNAModEvaluationChannels = false;

#if WITH_EDITORONLY_DATA
	RegisteredReimportCallback = false;
#endif // #if WITH_EDITORONLY_DATA

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	bIgnoreDNAAbilitySystemCooldowns = false;
	bIgnoreDNAAbilitySystemCosts = false;
#endif // #if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UDNAAbilitySystemGlobals::InitGlobalData()
{
	GetGlobalCurveTable();
	GetGlobalAttributeMetaDataTable();
	
	InitAttributeDefaults();

	GetDNACueManager();
	GetDNATagResponseTable();
	InitGlobalTags();

	// Register for PreloadMap so cleanup can occur on map transitions
	FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UDNAAbilitySystemGlobals::HandlePreLoadMap);

#if WITH_EDITOR
	// Register in editor for PreBeginPlay so cleanup can occur when we start a PIE session
	if (GIsEditor)
	{
		FEditorDelegates::PreBeginPIE.AddUObject(this, &UDNAAbilitySystemGlobals::OnPreBeginPIE);
	}
#endif
}


UCurveTable * UDNAAbilitySystemGlobals::GetGlobalCurveTable()
{
	if (!GlobalCurveTable && GlobalCurveTableName.IsValid())
	{
		GlobalCurveTable = Cast<UCurveTable>(GlobalCurveTableName.TryLoad());
	}
	return GlobalCurveTable;
}

UDataTable * UDNAAbilitySystemGlobals::GetGlobalAttributeMetaDataTable()
{
	if (!GlobalAttributeMetaDataTable && GlobalAttributeMetaDataTableName.IsValid())
	{
		GlobalAttributeMetaDataTable = Cast<UDataTable>(GlobalAttributeMetaDataTableName.TryLoad());
	}
	return GlobalAttributeMetaDataTable;
}

bool UDNAAbilitySystemGlobals::DeriveDNACueTagFromAssetName(FString AssetName, FDNATag& DNACueTag, FName& DNACueName)
{
	FDNATag OriginalTag = DNACueTag;
	
	// In the editor, attempt to infer DNACueTag from our asset name (if there is no valid DNACueTag already).
#if WITH_EDITOR
	if (GIsEditor)
	{
		if (DNACueTag.IsValid() == false)
		{
			AssetName.RemoveFromStart(TEXT("Default__"));
			AssetName.RemoveFromStart(TEXT("REINST_"));
			AssetName.RemoveFromStart(TEXT("SKEL_"));
			AssetName.RemoveFromStart(TEXT("GC_"));		// allow GC_ prefix in asset name
			AssetName.RemoveFromEnd(TEXT("_c"));

			AssetName.ReplaceInline(TEXT("_"), TEXT("."), ESearchCase::CaseSensitive);

			if (!AssetName.Contains(TEXT("DNACue")))
			{
				AssetName = FString(TEXT("DNACue.")) + AssetName;
			}

			DNACueTag = UDNATagsManager::Get().RequestDNATag(FName(*AssetName), false);
		}
		DNACueName = DNACueTag.GetTagName();
	}
#endif
	return (OriginalTag != DNACueTag);
}

bool UDNAAbilitySystemGlobals::ShouldAllowDNAModEvaluationChannels() const
{
	return bAllowDNAModEvaluationChannels;
}

bool UDNAAbilitySystemGlobals::IsDNAModEvaluationChannelValid(EDNAModEvaluationChannel Channel) const
{
	// Only valid if channels are allowed and the channel has a game-specific alias specified or if not using channels and the channel is Channel0
	const bool bAllowChannels = ShouldAllowDNAModEvaluationChannels();
	return bAllowChannels ? (!GetDNAModEvaluationChannelAlias(Channel).IsNone()) : (Channel == EDNAModEvaluationChannel::Channel0);
}

const FName& UDNAAbilitySystemGlobals::GetDNAModEvaluationChannelAlias(EDNAModEvaluationChannel Channel) const
{
	return GetDNAModEvaluationChannelAlias(static_cast<int32>(Channel));
}

const FName& UDNAAbilitySystemGlobals::GetDNAModEvaluationChannelAlias(int32 Index) const
{
	check(Index >= 0 && Index < ARRAY_COUNT(DNAModEvaluationChannelAliases));
	return DNAModEvaluationChannelAliases[Index];
}

#if WITH_EDITOR

void UDNAAbilitySystemGlobals::OnTableReimported(UObject* InObject)
{
	if (GIsEditor && !IsRunningCommandlet() && InObject)
	{
		UCurveTable* ReimportedCurveTable = Cast<UCurveTable>(InObject);
		if (ReimportedCurveTable && GlobalAttributeDefaultsTables.Contains(ReimportedCurveTable))
		{
			ReloadAttributeDefaults();
		}
	}	
}

#endif

FDNAAbilityActorInfo * UDNAAbilitySystemGlobals::AllocAbilityActorInfo() const
{
	return new FDNAAbilityActorInfo();
}

FDNAEffectContext* UDNAAbilitySystemGlobals::AllocDNAEffectContext() const
{
	return new FDNAEffectContext();
}

/** Helping function to avoid having to manually cast */
UDNAAbilitySystemComponent* UDNAAbilitySystemGlobals::GetDNAAbilitySystemComponentFromActor(const AActor* Actor, bool LookForComponent)
{
	if (Actor == nullptr)
	{
		return nullptr;
	}

	const IDNAAbilitySystemInterface* ASI = Cast<IDNAAbilitySystemInterface>(Actor);
	if (ASI)
	{
		return ASI->GetDNAAbilitySystemComponent();
	}

	if (LookForComponent)
	{
		/** This is slow and not desirable */
		ABILITY_LOG(Warning, TEXT("GetDNAAbilitySystemComponentFromActor called on %s that is not IDNAAbilitySystemInterface. This slow!"), *Actor->GetName());

		return Actor->FindComponentByClass<UDNAAbilitySystemComponent>();
	}

	return nullptr;
}

// --------------------------------------------------------------------

UFunction* UDNAAbilitySystemGlobals::GetDNACueFunction(const FDNATag& ChildTag, UClass* Class, FName &MatchedTag)
{
	SCOPE_CYCLE_COUNTER(STAT_GetDNACueFunction);

	// A global cached map to lookup these functions might be a good idea. Keep in mind though that FindFunctionByName
	// is fast and already gives us a reliable map lookup.
	// 
	// We would get some speed by caching off the 'fully qualified name' to 'best match' lookup. E.g. we can directly map
	// 'DNACue.X.Y' to the function 'DNACue.X' without having to look for DNACue.X.Y first.
	// 
	// The native remapping (DNA.X.Y to DNA_X_Y) is also annoying and slow and could be fixed by this as well.
	// 
	// Keep in mind that any UFunction* cacheing is pretty unsafe. Classes can be loaded (and unloaded) during runtime
	// and will be regenerated all the time in the editor. Just doing a single pass at startup won't be enough,
	// we'll need a mechanism for registering classes when they are loaded on demand.
	
	FDNATagContainer TagAndParentsContainer = ChildTag.GetDNATagParents();

	for (auto InnerTagIt = TagAndParentsContainer.CreateConstIterator(); InnerTagIt; ++InnerTagIt)
	{
		FName CueName = InnerTagIt->GetTagName();
		if (UFunction* Func = Class->FindFunctionByName(CueName, EIncludeSuperFlag::IncludeSuper))
		{
			MatchedTag = CueName;
			return Func;
		}

		// Native functions cant be named with ".", so look for them with _. 
		FName NativeCueFuncName = *CueName.ToString().Replace(TEXT("."), TEXT("_"));
		if (UFunction* Func = Class->FindFunctionByName(NativeCueFuncName, EIncludeSuperFlag::IncludeSuper))
		{
			MatchedTag = CueName; // purposefully returning the . qualified name.
			return Func;
		}
	}

	return nullptr;
}

// --------------------------------------------------------------------

void UDNAAbilitySystemGlobals::InitDNACueParameters(FDNACueParameters& CueParameters, const FDNAEffectSpecForRPC& Spec)
{
	CueParameters.AggregatedSourceTags = Spec.AggregatedSourceTags;
	CueParameters.AggregatedTargetTags = Spec.AggregatedTargetTags;
	CueParameters.DNAEffectLevel = Spec.GetLevel();
	CueParameters.AbilityLevel = Spec.GetAbilityLevel();
	InitDNACueParameters(CueParameters, Spec.GetContext());
}

void UDNAAbilitySystemGlobals::InitDNACueParameters_GESpec(FDNACueParameters& CueParameters, const FDNAEffectSpec& Spec)
{
	CueParameters.AggregatedSourceTags = *Spec.CapturedSourceTags.GetAggregatedTags();
	CueParameters.AggregatedTargetTags = *Spec.CapturedTargetTags.GetAggregatedTags();

	// Look for a modified attribute magnitude to pass to the CueParameters
	for (const FDNAEffectCue& CueDef : Spec.Def->DNACues)
	{	
		bool FoundMatch = false;
		if (CueDef.MagnitudeAttribute.IsValid())
		{
			for (const FDNAEffectModifiedAttribute& ModifiedAttribute : Spec.ModifiedAttributes)
			{
				if (ModifiedAttribute.Attribute == CueDef.MagnitudeAttribute)
				{
					CueParameters.RawMagnitude = ModifiedAttribute.TotalMagnitude;
					FoundMatch = true;
					break;
				}
			}
			if (FoundMatch)
			{
				break;
			}
		}
	}

	CueParameters.DNAEffectLevel = Spec.GetLevel();
	CueParameters.AbilityLevel = Spec.GetEffectContext().GetAbilityLevel();

	InitDNACueParameters(CueParameters, Spec.GetContext());
}

void UDNAAbilitySystemGlobals::InitDNACueParameters(FDNACueParameters& CueParameters, const FDNAEffectContextHandle& EffectContext)
{
	if (EffectContext.IsValid())
	{
		// Copy Context over wholesale. Projects may want to override this and not copy over all data
		CueParameters.EffectContext = EffectContext;
	}
}

// --------------------------------------------------------------------

void UDNAAbilitySystemGlobals::StartAsyncLoadingObjectLibraries()
{
	if (GlobalDNACueManager != nullptr)
	{
		GlobalDNACueManager->InitializeRuntimeObjectLibrary();
	}
}

// --------------------------------------------------------------------

/** Initialize FAttributeSetInitter. This is virtual so projects can override what class they use */
void UDNAAbilitySystemGlobals::AllocAttributeSetInitter()
{
	GlobalAttributeSetInitter = TSharedPtr<FAttributeSetInitter>(new FAttributeSetInitterDiscreteLevels());
}

FAttributeSetInitter* UDNAAbilitySystemGlobals::GetAttributeSetInitter() const
{
	check(GlobalAttributeSetInitter.IsValid());
	return GlobalAttributeSetInitter.Get();
}

void UDNAAbilitySystemGlobals::InitAttributeDefaults()
{
 	bool bLoadedAnyDefaults = false;
 
	// Handle deprecated, single global table name
	if (GlobalAttributeSetDefaultsTableName.IsValid())
	{
		UCurveTable* AttribTable = Cast<UCurveTable>(GlobalAttributeSetDefaultsTableName.TryLoad());
		if (AttribTable)
		{
			GlobalAttributeDefaultsTables.Add(AttribTable);
			bLoadedAnyDefaults = true;
		}
	}

	// Handle array of global curve tables for attribute defaults
 	for (const FStringAssetReference& AttribDefaultTableName : GlobalAttributeSetDefaultsTableNames)
 	{
		if (AttribDefaultTableName.IsValid())
		{
			UCurveTable* AttribTable = Cast<UCurveTable>(AttribDefaultTableName.TryLoad());
			if (AttribTable)
			{
				GlobalAttributeDefaultsTables.Add(AttribTable);
				bLoadedAnyDefaults = true;
			}
		}
 	}
	
	if (bLoadedAnyDefaults)
	{
		// Subscribe for reimports if in the editor
#if WITH_EDITOR
		if (GIsEditor && !RegisteredReimportCallback)
		{
			GEditor->OnObjectReimported().AddUObject(this, &UDNAAbilitySystemGlobals::OnTableReimported);
			RegisteredReimportCallback = true;
		}
#endif


		ReloadAttributeDefaults();
	}
}

void UDNAAbilitySystemGlobals::ReloadAttributeDefaults()
{
	AllocAttributeSetInitter();
	GlobalAttributeSetInitter->PreloadAttributeSetData(GlobalAttributeDefaultsTables);
}

// --------------------------------------------------------------------

UDNACueManager* UDNAAbilitySystemGlobals::GetDNACueManager()
{
	if (GlobalDNACueManager == nullptr)
	{
		// Load specific DNAcue manager object if specified
		if (GlobalDNACueManagerName.IsValid())
		{
			GlobalDNACueManager = LoadObject<UDNACueManager>(nullptr, *GlobalDNACueManagerName.ToString(), nullptr, LOAD_None, nullptr);
			if (GlobalDNACueManager == nullptr)
			{
				ABILITY_LOG(Error, TEXT("Unable to Load DNACueManager %s"), *GlobalDNACueManagerName.ToString() );
			}
		}

		// Load specific DNAcue manager class if specified
		if ( GlobalDNACueManager == nullptr && GlobalDNACueManagerClass.IsValid() )
		{
			UClass* GCMClass = LoadClass<UObject>(NULL, *GlobalDNACueManagerClass.ToString(), NULL, LOAD_None, NULL);
			if (GCMClass)
			{
				GlobalDNACueManager = NewObject<UDNACueManager>(this, GCMClass, NAME_None);
			}
		}

		if ( GlobalDNACueManager == nullptr)
		{
			// Fallback to CDO
			GlobalDNACueManager = UDNACueManager::StaticClass()->GetDefaultObject<UDNACueManager>();
		}

		GlobalDNACueManager->OnCreated();

		if (DNACueNotifyPaths.Num() == 0)
		{
			DNACueNotifyPaths.Add(TEXT("/Game"));
			ABILITY_LOG(Warning, TEXT("No DNACueNotifyPaths were specified in DefaultGame.ini under [/Script/DNAAbilities.DNAAbilitySystemGlobals]. Falling back to using all of /Game/. This may be slow on large projects. Consider specifying which paths are to be searched."));
		}
		
		if (GlobalDNACueManager->ShouldAsyncLoadObjectLibrariesAtStart())
		{
			StartAsyncLoadingObjectLibraries();
		}
	}

	check(GlobalDNACueManager);
	return GlobalDNACueManager;
}

UDNATagReponseTable* UDNAAbilitySystemGlobals::GetDNATagResponseTable()
{
	if (DNATagResponseTable == nullptr && DNATagResponseTableName.IsValid())
	{
		DNATagResponseTable = LoadObject<UDNATagReponseTable>(nullptr, *DNATagResponseTableName.ToString(), nullptr, LOAD_None, nullptr);
	}

	return DNATagResponseTable;
}

void UDNAAbilitySystemGlobals::GlobalPreDNAEffectSpecApply(FDNAEffectSpec& Spec, UDNAAbilitySystemComponent* DNAAbilitySystemComponent)
{

}

void UDNAAbilitySystemGlobals::ToggleIgnoreDNAAbilitySystemCooldowns()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	bIgnoreDNAAbilitySystemCooldowns = !bIgnoreDNAAbilitySystemCooldowns;
#endif // #if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UDNAAbilitySystemGlobals::ToggleIgnoreDNAAbilitySystemCosts()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	bIgnoreDNAAbilitySystemCosts = !bIgnoreDNAAbilitySystemCosts;
#endif // #if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

bool UDNAAbilitySystemGlobals::ShouldIgnoreCooldowns() const
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	return bIgnoreDNAAbilitySystemCooldowns;
#else
	return false;
#endif // #if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

bool UDNAAbilitySystemGlobals::ShouldIgnoreCosts() const
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	return bIgnoreDNAAbilitySystemCosts;
#else
	return false;
#endif // #if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

#if WITH_EDITOR
void UDNAAbilitySystemGlobals::OnPreBeginPIE(const bool bIsSimulatingInEditor)
{
	ResetCachedData();
}
#endif // WITH_EDITOR

void UDNAAbilitySystemGlobals::ResetCachedData()
{
	IDNACueInterface::ClearTagToFunctionMap();
	FActiveDNAEffectHandle::ResetGlobalHandleMap();
}

void UDNAAbilitySystemGlobals::HandlePreLoadMap(const FString& MapName)
{
	ResetCachedData();
}

void UDNAAbilitySystemGlobals::Notify_OpenAssetInEditor(FString AssetName, int AssetType)
{
	AbilityOpenAssetInEditorCallbacks.Broadcast(AssetName, AssetType);
}

void UDNAAbilitySystemGlobals::Notify_FindAssetInEditor(FString AssetName, int AssetType)
{
	AbilityFindAssetInEditorCallbacks.Broadcast(AssetName, AssetType);
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
float DNAAbilitySystemGlobalScaler = 1.f;
static FAutoConsoleVariableRef CVarOrionGlobalScaler(TEXT("DNAAbilitySystem.GlobalAbilityScale"), DNAAbilitySystemGlobalScaler, TEXT("Global rate for scaling ability stuff like montages and root motion tasks. Used only for testing/iteration, never for shipping."), ECVF_Cheat );
#endif

void UDNAAbilitySystemGlobals::NonShipping_ApplyGlobalAbilityScaler_Rate(float& Rate)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	Rate *= DNAAbilitySystemGlobalScaler;
#endif
}

void UDNAAbilitySystemGlobals::NonShipping_ApplyGlobalAbilityScaler_Duration(float& Duration)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (DNAAbilitySystemGlobalScaler > 0.f)
	{
		Duration /= DNAAbilitySystemGlobalScaler;
	}
#endif
}
