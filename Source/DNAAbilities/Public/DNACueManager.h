// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "Misc/StringAssetReference.h"
#include "DNATagContainer.h"
#include "Engine/DataAsset.h"
#include "DNAEffectTypes.h"
#include "DNAPrediction.h"
#include "Engine/World.h"
#include "AssetData.h"
#include "Engine/StreamableManager.h"
#include "DNACue_Types.h"
#include "DNACueTranslator.h"
#include "DNACueManager.generated.h"

#define DNACUE_DEBUG 0

class ADNACueNotify_Actor;
class UDNAAbilitySystemComponent;
class UObjectLibrary;

DECLARE_DELEGATE_OneParam(FOnDNACueNotifySetLoaded, TArray<FStringAssetReference>);
DECLARE_DELEGATE_OneParam(FDNACueProxyTick, float);
DECLARE_DELEGATE_RetVal_TwoParams(bool, FShouldLoadGCNotifyDelegate, const FAssetData&, FName);

/** An ObjectLibrary for the DNACue Notifies. Wraps 2 underlying UObjectLibraries plus options/delegates for how they are loaded */ 
USTRUCT()
struct FDNACueObjectLibrary
{
	GENERATED_BODY()
	FDNACueObjectLibrary()
	{
		bHasBeenInitialized = false;
	}

	// Paths to search for
	UPROPERTY()
	TArray<FString> Paths;

	// Callback for when load finishes
	FOnDNACueNotifySetLoaded OnLoaded;

	// Callback for "should I add this FAssetData to the set"
	FShouldLoadGCNotifyDelegate ShouldLoad;

	// Object library for actor based notifies
	UPROPERTY()
	UObjectLibrary* ActorObjectLibrary;

	// Object library for object based notifies
	UPROPERTY()
	UObjectLibrary* StaticObjectLibrary;

	// Priority to use if async loading
	TAsyncLoadPriority AsyncPriority;

	// Should we force a sync scan on the asset registry in order to discover asset data, or just use what is there?
	UPROPERTY()
	bool bShouldSyncScan;

	// Should we start async loading everything that we find (that passes ShouldLoad delegate check)
	UPROPERTY()
	bool bShouldAsyncLoad;

	// Should we sync load everything that we find (that passes ShouldLoad delegate check)
	UPROPERTY()
	bool bShouldSyncLoad;

	// Set to put the loaded asset data into. If null we will use the global set (RuntimeDNACueObjectLibrary.CueSet)
	UPROPERTY()
	UDNACueSet* CueSet;

	UPROPERTY()
	bool bHasBeenInitialized;
};

UCLASS()
class DNAABILITIES_API UDNACueManager : public UDataAsset
{
	GENERATED_UCLASS_BODY()

	// -------------------------------------------------------------
	// Wrappers to handle replicating executed cues
	// -------------------------------------------------------------

	virtual void InvokeDNACueExecuted_FromSpec(UDNAAbilitySystemComponent* OwningComponent, const FDNAEffectSpec& Spec, FPredictionKey PredictionKey);
	virtual void InvokeDNACueExecuted(UDNAAbilitySystemComponent* OwningComponent, const FDNATag DNACueTag, FPredictionKey PredictionKey, FDNAEffectContextHandle EffectContext);
	virtual void InvokeDNACueExecuted_WithParams(UDNAAbilitySystemComponent* OwningComponent, const FDNATag DNACueTag, FPredictionKey PredictionKey, FDNACueParameters DNACueParameters);

	virtual void InvokeDNACueAddedAndWhileActive_FromSpec(UDNAAbilitySystemComponent* OwningComponent, const FDNAEffectSpec& Spec, FPredictionKey PredictionKey);

	/** Start or stop a DNA cue send context. Used by FScopedDNACueSendContext above, when all contexts are removed the cues are flushed */
	void StartDNACueSendContext();
	void EndDNACueSendContext();

	/** Send out any pending cues */
	virtual void FlushPendingCues();

	virtual void OnCreated();

	virtual void OnEngineInitComplete();

	/** Process a pending cue, return false if the cue should be rejected. */
	virtual bool ProcessPendingCueExecute(FDNACuePendingExecute& PendingCue);

	/** Returns true if two pending cues match, can be overridden in game */
	virtual bool DoesPendingCueExecuteMatch(FDNACuePendingExecute& PendingCue, FDNACuePendingExecute& ExistingCue);

	// -------------------------------------------------------------
	// Handling DNACues at runtime:
	// -------------------------------------------------------------

	/** Main entry point for handling a DNAcue event. These functions will call the 3 functions below to handle DNA cues */
	virtual void HandleDNACues(AActor* TargetActor, const FDNATagContainer& DNACueTags, EDNACueEvent::Type EventType, const FDNACueParameters& Parameters);
	virtual void HandleDNACue(AActor* TargetActor, FDNATag DNACueTag, EDNACueEvent::Type EventType, const FDNACueParameters& Parameters);

	/** 1. returns true to ignore DNA cues */
	virtual bool ShouldSuppressDNACues(AActor* TargetActor);

	/** 2. Allows Tag to be translated in place to a different Tag. See FDNACueTranslorManager */
	void TranslateDNACue(FDNATag& Tag, AActor* TargetActor, const FDNACueParameters& Parameters);

	/** 3. Actually routes the DNAcue event to the right place.  */
	virtual void RouteDNACue(AActor* TargetActor, FDNATag DNACueTag, EDNACueEvent::Type EventType, const FDNACueParameters& Parameters);

	// -------------------------------------------------------------

	/** Force any instanced DNACueNotifies to stop */
	virtual void EndDNACuesFor(AActor* TargetActor);

	/** Returns the cached instance cue. Creates it if it doesn't exist */
	virtual ADNACueNotify_Actor* GetInstancedCueActor(AActor* TargetActor, UClass* CueClass, const FDNACueParameters& Parameters);

	/** Notify that this actor is finished and should be destroyed or recycled */
	virtual void NotifyDNACueActorFinished(ADNACueNotify_Actor* Actor);

	/** Notify to say the actor is about to be destroyed and the GC manager needs to remove references to it. This should not happen in normal play with recycling enabled, but could happen in replays. */
	virtual void NotifyDNACueActorEndPlay(ADNACueNotify_Actor* Actor);

	/** Resets preallocation for a given world */
	void ResetPreallocation(UWorld* World);

	/** Prespawns a single actor for DNAcue notify actor classes that need prespawning (should be called by outside gamecode, such as gamestate) */
	void UpdatePreallocation(UWorld* World);

	void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);

	void OnPreReplayScrub(UWorld* World);

	/** Prints what classess exceeded their preallocation sizes during runtime */
	void DumpPreallocationStats(UWorld* World);

	// -------------------------------------------------------------
	//  Loading DNACueNotifies from ObjectLibraries
	//
	//	There are two libraries in the DNACueManager:
	//	1. The RunTime object library, which is initialized with the "always loaded" paths, via UDNAAbilitySystemGlobals::GetDNACueNotifyPaths()
	//		-GC Notifies in this path are loaded at startup
	//		-Everything loaded will go into the global DNAcue set, which is where GC events will be routed to by default
	//
	//	2. The Editor object library, which is initialized with the "all valid" paths, via UDNACueManager::GetValidDNACuePaths()
	//		-Only used in the editor.
	//		-Never loads clases or scans directly itself. 
	//		-Just reflect asset registry to show about "all DNA cue notifies the game could know about"
	//
	// -------------------------------------------------------------

	/** Returns the Ryntime cueset, which is the global cueset used in runtime, as opposed to the editor cue set which is used only when running the editor */
	UDNACueSet* GetRuntimeCueSet();

	/** Called to setup and initialize the runtime library. The passed in paths will be scanned and added to the global DNA cue set where appropriate */
	void InitializeRuntimeObjectLibrary();

	// Will return the Runtime cue set and the EditorCueSet, if the EditorCueSet is available. This is used mainly for handling asset created/deleted in the editor
	TArray<UDNACueSet*> GetGlobalCueSets();
	

#if WITH_EDITOR
	/** Called from editor to soft load all DNA cue notifies for the DNACueEditor */
	void InitializeEditorObjectLibrary();

	/** Calling this will make the GC manager periodically refresh the EditorObjectLibrary until the asset registry is finished scanning */
	void RequestPeriodicUpdateOfEditorObjectLibraryWhileWaitingOnAssetRegistry();

	/** Get filenames of all GC notifies we know about (loaded or not). Useful for cooking */
	void GetEditorObjectLibraryDNACueNotifyFilenames(TArray<FString>& Filenames) const;

	/** Looks in the EditorObjectLibrary for a notify for this tag, if it finds it, it loads it and puts it in the RuntimeObjectLibrary so that it can be previewed in the editor */
	void LoadNotifyForEditorPreview(FDNATag DNACueTag);

	UDNACueSet* GetEditorCueSet();

	FSimpleMulticastDelegate OnEditorObjectLibraryUpdated;
	bool EditorObjectLibraryFullyInitialized;

	FTimerHandle EditorPeriodicUpdateHandle;
#endif

protected:

	virtual bool ShouldSyncScanRuntimeObjectLibraries() const;
	virtual bool ShouldSyncLoadRuntimeObjectLibraries() const;
	virtual bool ShouldAsyncLoadRuntimeObjectLibraries() const;

	/** Refreshes the existing, already initialized, object libraries. */
	void RefreshObjectLibraries();

	/** Internal function to actually init the FDNACueObjectLibrary */
	void InitObjectLibrary(FDNACueObjectLibrary& Library);

	virtual TArray<FString> GetAlwaysLoadedDNACuePaths();

	/** returns list of valid DNA cue paths. Subclasses may override this to specify locations that aren't part of the "always loaded" LoadedPaths array */
	virtual TArray<FString>	GetValidDNACuePaths() { return GetAlwaysLoadedDNACuePaths(); }

	UPROPERTY(transient)
	FDNACueObjectLibrary RuntimeDNACueObjectLibrary;

	UPROPERTY(transient)
	FDNACueObjectLibrary EditorDNACueObjectLibrary;

public:		

	/** Called before loading any DNA cue notifies from object libraries. Allows subclasses to skip notifies. */
	virtual bool ShouldLoadDNACueAssetData(const FAssetData& Data) const { return true; }
	
	int32 FinishLoadingDNACueNotifies();

	UPROPERTY(transient)
	FStreamableManager	StreamableManager;
	
	TMap<FGCNotifyActorKey, TWeakObjectPtr<ADNACueNotify_Actor> > NotifyMapActor;

	void PrintDNACueNotifyMap();

	virtual void PrintLoadedDNACueNotifyClasses();

	virtual class UWorld* GetWorld() const override;

#if WITH_EDITOR

	/** Handles updating an object library when a new asset is created */
	void HandleAssetAdded(UObject *Object);

	/** Handles cleaning up an object library if it matches the passed in object */
	void HandleAssetDeleted(UObject *Object);

	/** Warns if we move a DNACue notify out of the valid search paths */
	void HandleAssetRenamed(const FAssetData& Data, const FString& String);

	bool VerifyNotifyAssetIsInValidPath(FString Path);

	bool bAccelerationMapOutdated;

	FOnDNACueNotifyChange	OnDNACueNotifyAddOrRemove;

	/** Animation Preview Hacks */
	static class USceneComponent* PreviewComponent;
	static UWorld* PreviewWorld;
	static FDNACueProxyTick PreviewProxyTick;
#endif

	static bool IsDNACueRecylingEnabled();
	
	virtual bool ShouldAsyncLoadObjectLibrariesAtStart() const { return true; }

	FDNACueTranslationManager	TranslationManager;

	// -------------------------------------------------------------
	//  Debugging Help
	// -------------------------------------------------------------

#if DNACUE_DEBUG
	virtual FDNACueDebugInfo* GetDebugInfo(int32 Handle, bool Reset=false);
#endif

protected:

#if WITH_EDITOR
	//This handles the case where DNACueNotifications have changed between sessions, which is possible in editor.
	virtual void ReloadObjectLibrary(UWorld* World, const UWorld::InitializationValues IVS);
#endif

	void BuildCuesToAddToGlobalSet(const TArray<FAssetData>& AssetDataList, FName TagPropertyName, TArray<struct FDNACueReferencePair>& OutCuesToAdd, TArray<FStringAssetReference>& OutAssetsToLoad, FShouldLoadGCNotifyDelegate = FShouldLoadGCNotifyDelegate());

	/** The cue manager has a tendency to produce a lot of RPCs. This logs out when we are attempting to fire more RPCs than will actually go off */
	void CheckForTooManyRPCs(FName FuncName, const FDNACuePendingExecute& PendingCue, const FString& CueID, const FDNAEffectContext* EffectContext);

	void OnDNACueNotifyAsyncLoadComplete(TArray<FStringAssetReference> StringRef);

	void CheckForPreallocation(UClass* GCClass);

	/** Hardref to the DNAcue notify classes we have async loaded*/
	UPROPERTY(transient)
	TArray<UClass*> LoadedDNACueNotifyClasses;

	/** Classes that we need to preallocate instances for */
	UPROPERTY(transient)
	TArray<ADNACueNotify_Actor*> DNACueClassesForPreallocation;

	/** List of DNA cue executes that haven't been processed yet */
	UPROPERTY(transient)
	TArray<FDNACuePendingExecute> PendingExecuteCues;

	/** Number of active DNA cue send contexts, when it goes to 0 cues are flushed */
	UPROPERTY(transient)
	int32 DNACueSendContextCount;

	/** Cached world we are currently handling cues for. Used for non instanced GC Notifies that need world. */
	UWorld* CurrentWorld;

	FPreallocationInfo& GetPreallocationInfo(UWorld* World);

	UPROPERTY(transient)
	TArray<FPreallocationInfo>	PreallocationInfoList_Internal;
};