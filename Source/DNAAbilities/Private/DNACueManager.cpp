// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNACueManager.h"
#include "Engine/ObjectLibrary.h"
#include "DNACueNotify_Actor.h"
#include "Misc/MessageDialog.h"
#include "Stats/StatsMisc.h"
#include "Misc/ScopedSlowTask.h"
#include "Modules/ModuleManager.h"
#include "DrawDebugHelpers.h"
#include "DNATagsManager.h"
#include "DNATagsModule.h"
#include "AbilitySystemGlobals.h"
#include "AssetRegistryModule.h"
#include "DNACueInterface.h"
#include "DNACueSet.h"
#include "DNACueNotify_Static.h"
#include "AbilitySystemComponent.h"
#include "Net/DataReplication.h"
#include "Engine/ActorChannel.h"
#include "Engine/NetConnection.h"
#include "Net/UnrealNetwork.h"
#include "Misc/CoreDelegates.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/Engine.h"
#include "ISequenceRecorder.h"
#define LOCTEXT_NAMESPACE "DNACueManager"
#endif

int32 LogDNACueActorSpawning = 0;
static FAutoConsoleVariableRef CVarLogDNACueActorSpawning(TEXT("DNAAbilitySystem.LogDNACueActorSpawning"),	LogDNACueActorSpawning, TEXT("Log when we create DNACueNotify_Actors"), ECVF_Default	);

int32 DisplayDNACues = 0;
static FAutoConsoleVariableRef CVarDisplayDNACues(TEXT("DNAAbilitySystem.DisplayDNACues"),	DisplayDNACues, TEXT("Display DNACue events in world as text."), ECVF_Default	);

int32 DisableDNACues = 0;
static FAutoConsoleVariableRef CVarDisableDNACues(TEXT("DNAAbilitySystem.DisableDNACues"),	DisableDNACues, TEXT("Disables all DNACue events in the world."), ECVF_Default );

float DisplayDNACueDuration = 5.f;
static FAutoConsoleVariableRef CVarDurationeDNACues(TEXT("DNAAbilitySystem.DNACue.DisplayDuration"),	DisplayDNACueDuration, TEXT("Disables all DNACue events in the world."), ECVF_Default );

int32 DNACueRunOnDedicatedServer = 0;
static FAutoConsoleVariableRef CVarDedicatedServerDNACues(TEXT("DNAAbilitySystem.DNACue.RunOnDedicatedServer"), DNACueRunOnDedicatedServer, TEXT("Run DNA cue events on dedicated server"), ECVF_Default );

#if WITH_EDITOR
USceneComponent* UDNACueManager::PreviewComponent = nullptr;
UWorld* UDNACueManager::PreviewWorld = nullptr;
FDNACueProxyTick UDNACueManager::PreviewProxyTick;
#endif

UDNACueManager::UDNACueManager(const FObjectInitializer& PCIP)
: Super(PCIP)
{
#if WITH_EDITOR
	bAccelerationMapOutdated = true;
	EditorObjectLibraryFullyInitialized = false;
#endif

	CurrentWorld = nullptr;	
}

void UDNACueManager::OnCreated()
{
	FWorldDelegates::OnWorldCleanup.AddUObject(this, &UDNACueManager::OnWorldCleanup);
	FWorldDelegates::OnPreWorldFinishDestroy.AddUObject(this, &UDNACueManager::OnWorldCleanup, true, true);
	FNetworkReplayDelegates::OnPreScrub.AddUObject(this, &UDNACueManager::OnPreReplayScrub);
		
#if WITH_EDITOR
	FCoreDelegates::OnFEngineLoopInitComplete.AddUObject(this, &UDNACueManager::OnEngineInitComplete);
#endif
}

void UDNACueManager::OnEngineInitComplete()
{
#if WITH_EDITOR
	FCoreDelegates::OnFEngineLoopInitComplete.AddUObject(this, &UDNACueManager::OnEngineInitComplete);
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	AssetRegistryModule.Get().OnInMemoryAssetCreated().AddUObject(this, &UDNACueManager::HandleAssetAdded);
	AssetRegistryModule.Get().OnInMemoryAssetDeleted().AddUObject(this, &UDNACueManager::HandleAssetDeleted);
	AssetRegistryModule.Get().OnAssetRenamed().AddUObject(this, &UDNACueManager::HandleAssetRenamed);
	FWorldDelegates::OnPreWorldInitialization.AddUObject(this, &UDNACueManager::ReloadObjectLibrary);

	InitializeEditorObjectLibrary();
#endif
}

bool IsDedicatedServerForDNACue()
{
#if WITH_EDITOR
	// This will handle dedicated server PIE case properly
	return GEngine->ShouldAbsorbCosmeticOnlyEvent();
#else
	// When in standalone non editor, this is the fastest way to check
	return IsRunningDedicatedServer();
#endif
}


void UDNACueManager::HandleDNACues(AActor* TargetActor, const FDNATagContainer& DNACueTags, EDNACueEvent::Type EventType, const FDNACueParameters& Parameters)
{
#if WITH_EDITOR
	if (GIsEditor && TargetActor == nullptr && UDNACueManager::PreviewComponent)
	{
		TargetActor = Cast<AActor>(AActor::StaticClass()->GetDefaultObject());
	}
#endif

	if (ShouldSuppressDNACues(TargetActor))
	{
		return;
	}

	for (auto It = DNACueTags.CreateConstIterator(); It; ++It)
	{
		HandleDNACue(TargetActor, *It, EventType, Parameters);
	}
}

void UDNACueManager::HandleDNACue(AActor* TargetActor, FDNATag DNACueTag, EDNACueEvent::Type EventType, const FDNACueParameters& Parameters)
{
#if WITH_EDITOR
	if (GIsEditor && TargetActor == nullptr && UDNACueManager::PreviewComponent)
	{
		TargetActor = Cast<AActor>(AActor::StaticClass()->GetDefaultObject());
	}
#endif

	if (ShouldSuppressDNACues(TargetActor))
	{
		return;
	}

	TranslateDNACue(DNACueTag, TargetActor, Parameters);

	RouteDNACue(TargetActor, DNACueTag, EventType, Parameters);
}

bool UDNACueManager::ShouldSuppressDNACues(AActor* TargetActor)
{
	if (DisableDNACues)
	{
		return true;
	}

	if (DNACueRunOnDedicatedServer == 0 && IsDedicatedServerForDNACue())
	{
		return true;
	}

	if (TargetActor == nullptr)
	{
		return true;
	}

	return false;
}

void UDNACueManager::RouteDNACue(AActor* TargetActor, FDNATag DNACueTag, EDNACueEvent::Type EventType, const FDNACueParameters& Parameters)
{
	IDNACueInterface* DNACueInterface = Cast<IDNACueInterface>(TargetActor);
	bool bAcceptsCue = true;
	if (DNACueInterface)
	{
		bAcceptsCue = DNACueInterface->ShouldAcceptDNACue(TargetActor, DNACueTag, EventType, Parameters);
	}

#if ENABLE_DRAW_DEBUG
	if (DisplayDNACues)
	{
		FString DebugStr = FString::Printf(TEXT("%s - %s"), *DNACueTag.ToString(), *EDNACueEventToString(EventType) );
		FColor DebugColor = FColor::Green;
		DrawDebugString(TargetActor->GetWorld(), FVector(0.f, 0.f, 100.f), DebugStr, TargetActor, DebugColor, DisplayDNACueDuration);
	}
#endif // ENABLE_DRAW_DEBUG

	CurrentWorld = TargetActor->GetWorld();

	// Don't handle DNA cues when world is tearing down
	if (!GetWorld() || GetWorld()->bIsTearingDown)
	{
		return;
	}

	// Give the global set a chance
	check(RuntimeDNACueObjectLibrary.CueSet);
	if (bAcceptsCue)
	{
		RuntimeDNACueObjectLibrary.CueSet->HandleDNACue(TargetActor, DNACueTag, EventType, Parameters);
	}

	// Use the interface even if it's not in the map
	if (DNACueInterface && bAcceptsCue)
	{
		DNACueInterface->HandleDNACue(TargetActor, DNACueTag, EventType, Parameters);
	}

	CurrentWorld = nullptr;
}

void UDNACueManager::TranslateDNACue(FDNATag& Tag, AActor* TargetActor, const FDNACueParameters& Parameters)
{
	TranslationManager.TranslateTag(Tag, TargetActor, Parameters);
}

void UDNACueManager::EndDNACuesFor(AActor* TargetActor)
{
	for (auto It = NotifyMapActor.CreateIterator(); It; ++It)
	{
		FGCNotifyActorKey& Key = It.Key();
		if (Key.TargetActor == TargetActor)
		{
			ADNACueNotify_Actor* InstancedCue = It.Value().Get();
			if (InstancedCue)
			{
				InstancedCue->OnOwnerDestroyed(TargetActor);
			}
			It.RemoveCurrent();
		}
	}
}

int32 DNACueActorRecycle = 1;
static FAutoConsoleVariableRef CVarDNACueActorRecycle(TEXT("DNAAbilitySystem.DNACueActorRecycle"), DNACueActorRecycle, TEXT("Allow recycling of DNACue Actors"), ECVF_Default );

int32 DNACueActorRecycleDebug = 0;
static FAutoConsoleVariableRef CVarDNACueActorRecycleDebug(TEXT("DNAAbilitySystem.DNACueActorRecycleDebug"), DNACueActorRecycleDebug, TEXT("Prints logs for GC actor recycling debugging"), ECVF_Default );

bool UDNACueManager::IsDNACueRecylingEnabled()
{
	return DNACueActorRecycle > 0;
}

ADNACueNotify_Actor* UDNACueManager::GetInstancedCueActor(AActor* TargetActor, UClass* CueClass, const FDNACueParameters& Parameters)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_DNACueManager_GetInstancedCueActor);


	// First, see if this actor already have a DNACueNotifyActor already going for this CueClass
	ADNACueNotify_Actor* CDO = Cast<ADNACueNotify_Actor>(CueClass->ClassDefaultObject);
	FGCNotifyActorKey	NotifyKey(TargetActor, CueClass, 
							CDO->bUniqueInstancePerInstigator ? Parameters.GetInstigator() : nullptr, 
							CDO->bUniqueInstancePerSourceObject ? Parameters.GetSourceObject() : nullptr);

	ADNACueNotify_Actor* SpawnedCue = nullptr;
	if (TWeakObjectPtr<ADNACueNotify_Actor>* WeakPtrPtr = NotifyMapActor.Find(NotifyKey))
	{		
		SpawnedCue = WeakPtrPtr->Get();
		// If the cue is scheduled to be destroyed, don't reuse it, create a new one instead
		if (SpawnedCue && SpawnedCue->DNACuePendingRemove() == false)
		{
			if (SpawnedCue->GetOwner() != TargetActor)
			{
#if WITH_EDITOR	
				if (TargetActor && TargetActor->HasAnyFlags(RF_ClassDefaultObject))
				{
					// Animation preview hack, reuse this one even though the owner doesnt match the CDO
					return SpawnedCue;
				}
#endif

				// This should not happen. This means we think we can recycle and GC actor that is currently being used by someone else.
				ABILITY_LOG(Warning, TEXT("GetInstancedCueActor attempting to reuse GC Actor with a different owner! %s (Target: %s). Using GC Actor: %s. Current Owner: %s"), *GetNameSafe(CueClass), *GetNameSafe(TargetActor), *GetNameSafe(SpawnedCue), *GetNameSafe(SpawnedCue->GetOwner()));
			}
			else
			{
				UE_CLOG((DNACueActorRecycleDebug>0), LogDNAAbilitySystem, Display, TEXT("::GetInstancedCueActor Using Existing %s (Target: %s). Using GC Actor: %s"), *GetNameSafe(CueClass), *GetNameSafe(TargetActor), *GetNameSafe(SpawnedCue));
				return SpawnedCue;
			}
		}

		// We aren't going to use this existing cue notify actor, so clear it.
		SpawnedCue = nullptr;
	}

	UWorld* World = GetWorld();

	// We don't have an instance for this, and we need one, so make one
	if (ensure(TargetActor) && ensure(CueClass))
	{
		AActor* NewOwnerActor = TargetActor;
		bool UseActorRecycling = (DNACueActorRecycle > 0);
		
#if WITH_EDITOR	
		// Animtion preview hack. If we are trying to play the GC on a CDO, then don't use actor recycling and don't set the owner (to the CDO, which would cause problems)
		if (TargetActor && TargetActor->HasAnyFlags(RF_ClassDefaultObject))
		{
			NewOwnerActor = nullptr;
			UseActorRecycling = false;
		}
#endif
		// Look to reuse an existing one that is stored on the CDO:
		if (UseActorRecycling)
		{
			FPreallocationInfo& Info = GetPreallocationInfo(World);
			TArray<ADNACueNotify_Actor*>* PreallocatedList = Info.PreallocatedInstances.Find(CueClass);
			if (PreallocatedList && PreallocatedList->Num() > 0)
			{
				SpawnedCue = nullptr;
				while (true)
				{
					SpawnedCue = PreallocatedList->Pop(false);

					// Temp: tracking down possible memory corruption
					// null is maybe ok. But invalid low level is bad and we want to crash hard to find out who/why.
					if (SpawnedCue && (SpawnedCue->IsValidLowLevelFast() == false))
					{
						checkf(false, TEXT("UDNACueManager::GetInstancedCueActor found an invalid SpawnedCue for class %s"), *GetNameSafe(CueClass));
					}

					// Normal check: if cue was destroyed or is pending kill, then don't use it.
					if (SpawnedCue && SpawnedCue->IsPendingKill() == false)
					{
						break;
					}
					
					// outside of replays, this should not happen. GC Notifies should not be actually destroyed.
					checkf(World->DemoNetDriver, TEXT("Spawned Cue is pending kill or null: %s."), *GetNameSafe(SpawnedCue));

					if (PreallocatedList->Num() <= 0)
					{
						// Ran out of preallocated instances... break and create a new one.
						break;
					}
				}

				if (SpawnedCue)
				{
					SpawnedCue->bInRecycleQueue = false;
					SpawnedCue->SetOwner(NewOwnerActor);
					SpawnedCue->SetActorLocationAndRotation(TargetActor->GetActorLocation(), TargetActor->GetActorRotation());
					SpawnedCue->ReuseAfterRecycle();
				}

				UE_CLOG((DNACueActorRecycleDebug>0), LogDNAAbilitySystem, Display, TEXT("GetInstancedCueActor Popping Recycled %s (Target: %s). Using GC Actor: %s"), *GetNameSafe(CueClass), *GetNameSafe(TargetActor), *GetNameSafe(SpawnedCue));
#if WITH_EDITOR
				// let things know that we 'spawned'
				ISequenceRecorder& SequenceRecorder	= FModuleManager::LoadModuleChecked<ISequenceRecorder>("SequenceRecorder");
				SequenceRecorder.NotifyActorStartRecording(SpawnedCue);
#endif
			}
		}

		// If we can't reuse, then spawn a new one
		if (SpawnedCue == nullptr)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = NewOwnerActor;
			if (SpawnedCue == nullptr)
			{
				if (LogDNACueActorSpawning)
				{
					ABILITY_LOG(Warning, TEXT("Spawning DNAcueActor: %s"), *CueClass->GetName());
				}

				SpawnedCue = GetWorld()->SpawnActor<ADNACueNotify_Actor>(CueClass, TargetActor->GetActorLocation(), TargetActor->GetActorRotation(), SpawnParams);
			}
		}

		// Associate this DNACueNotifyActor with this target actor/key
		if (ensure(SpawnedCue))
		{
			SpawnedCue->NotifyKey = NotifyKey;
			NotifyMapActor.Add(NotifyKey, SpawnedCue);
		}
	}

	UE_CLOG((DNACueActorRecycleDebug>0), LogDNAAbilitySystem, Display, TEXT("GetInstancedCueActor  Returning %s (Target: %s). Using GC Actor: %s"), *GetNameSafe(CueClass), *GetNameSafe(TargetActor), *GetNameSafe(SpawnedCue));
	return SpawnedCue;
}

void UDNACueManager::NotifyDNACueActorFinished(ADNACueNotify_Actor* Actor)
{
	bool UseActorRecycling = (DNACueActorRecycle > 0);

#if WITH_EDITOR	
	// Don't recycle in preview worlds
	if (Actor->GetWorld()->IsPreviewWorld())
	{
		UseActorRecycling = false;
	}
#endif

	if (UseActorRecycling)
	{
		if (Actor->bInRecycleQueue)
		{
			// We are already in the recycle queue. This can happen normally
			// (For example the GC is removed and the owner is destroyed in the same frame)
			return;
		}

		ADNACueNotify_Actor* CDO = Actor->GetClass()->GetDefaultObject<ADNACueNotify_Actor>();
		if (CDO && Actor->Recycle())
		{
			if (Actor->IsPendingKill())
			{
				ensureMsgf(GetWorld()->DemoNetDriver, TEXT("DNACueNotify %s is pending kill in ::NotifyDNACueActorFinished (and not in network demo)"), *GetNameSafe(Actor));
				return;
			}
			Actor->bInRecycleQueue = true;

			// Remove this now from our internal map so that it doesn't get reused like a currently active cue would
			if (TWeakObjectPtr<ADNACueNotify_Actor>* WeakPtrPtr = NotifyMapActor.Find(Actor->NotifyKey))
			{
				// Only remove if this is the current actor in the map!
				// This could happen if a GC notify actor has a delayed removal and another GC event happens before the delayed removal happens (the old GC actor could replace the latest one in the map)
				if (WeakPtrPtr->Get() == Actor)
				{
					WeakPtrPtr->Reset();
				}
			}

			UE_CLOG((DNACueActorRecycleDebug>0), LogDNAAbilitySystem, Display, TEXT("NotifyDNACueActorFinished %s"), *GetNameSafe(Actor));

			FPreallocationInfo& Info = GetPreallocationInfo(Actor->GetWorld());
			TArray<ADNACueNotify_Actor*>& PreAllocatedList = Info.PreallocatedInstances.FindOrAdd(Actor->GetClass());

			// Put the actor back in the list
			if (ensureMsgf(PreAllocatedList.Contains(Actor)==false, TEXT("GC Actor PreallocationList already contains Actor %s"), *GetNameSafe(Actor)))
			{
				PreAllocatedList.Push(Actor);
			}
			
#if WITH_EDITOR
			// let things know that we 'de-spawned'
			ISequenceRecorder& SequenceRecorder	= FModuleManager::LoadModuleChecked<ISequenceRecorder>("SequenceRecorder");
			SequenceRecorder.NotifyActorStopRecording(Actor);
#endif
			return;
		}
	}	

	// We didn't recycle, so just destroy
	Actor->Destroy();
}

void UDNACueManager::NotifyDNACueActorEndPlay(ADNACueNotify_Actor* Actor)
{
	if (Actor && Actor->bInRecycleQueue)
	{
		FPreallocationInfo& Info = GetPreallocationInfo(Actor->GetWorld());
		TArray<ADNACueNotify_Actor*>& PreAllocatedList = Info.PreallocatedInstances.FindOrAdd(Actor->GetClass());
		PreAllocatedList.Remove(Actor);
	}
}

// ------------------------------------------------------------------------

bool UDNACueManager::ShouldSyncScanRuntimeObjectLibraries() const
{
	// Always sync scan the runtime object library
	return true;
}
bool UDNACueManager::ShouldSyncLoadRuntimeObjectLibraries() const
{
	// No real need to sync load it anymore
	return false;
}
bool UDNACueManager::ShouldAsyncLoadRuntimeObjectLibraries() const
{
	// Async load the run time library at startup
	return true;
}

void UDNACueManager::InitializeRuntimeObjectLibrary()
{
	RuntimeDNACueObjectLibrary.Paths = GetAlwaysLoadedDNACuePaths();
	if (RuntimeDNACueObjectLibrary.CueSet == nullptr)
	{
		RuntimeDNACueObjectLibrary.CueSet = NewObject<UDNACueSet>(this, TEXT("GlobalDNACueSet"));
	}

	RuntimeDNACueObjectLibrary.CueSet->Empty();
	RuntimeDNACueObjectLibrary.bHasBeenInitialized = true;
	
	RuntimeDNACueObjectLibrary.bShouldSyncScan = ShouldSyncScanRuntimeObjectLibraries();
	RuntimeDNACueObjectLibrary.bShouldAsyncLoad = ShouldSyncLoadRuntimeObjectLibraries();
	RuntimeDNACueObjectLibrary.bShouldSyncLoad = ShouldAsyncLoadRuntimeObjectLibraries();

	InitObjectLibrary(RuntimeDNACueObjectLibrary);
}

#if WITH_EDITOR
void UDNACueManager::InitializeEditorObjectLibrary()
{
	SCOPE_LOG_TIME_IN_SECONDS(*FString::Printf(TEXT("UDNACueManager::InitializeEditorObjectLibrary")), nullptr)

	EditorDNACueObjectLibrary.Paths = GetValidDNACuePaths();
	if (EditorDNACueObjectLibrary.CueSet == nullptr)
	{
		EditorDNACueObjectLibrary.CueSet = NewObject<UDNACueSet>(this, TEXT("EditorDNACueSet"));
	}

	EditorDNACueObjectLibrary.CueSet->Empty();
	EditorDNACueObjectLibrary.bHasBeenInitialized = true;

	// Don't load anything for the editor. Just read whatever the asset registry has.
	EditorDNACueObjectLibrary.bShouldSyncScan = IsRunningCommandlet();				// If we are cooking, then sync scan it right away so that we don't miss anything
	EditorDNACueObjectLibrary.bShouldAsyncLoad = false;
	EditorDNACueObjectLibrary.bShouldSyncLoad = false;

	InitObjectLibrary(EditorDNACueObjectLibrary);
	
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	if ( AssetRegistryModule.Get().IsLoadingAssets() )
	{
		// Let us know when we are done
		static FDelegateHandle DoOnce =
		AssetRegistryModule.Get().OnFilesLoaded().AddUObject(this, &UDNACueManager::InitializeEditorObjectLibrary);
	}
	else
	{
		EditorObjectLibraryFullyInitialized = true;
		if (EditorPeriodicUpdateHandle.IsValid())
		{
			GEditor->GetTimerManager()->ClearTimer(EditorPeriodicUpdateHandle);
			EditorPeriodicUpdateHandle.Invalidate();
		}
	}

	OnEditorObjectLibraryUpdated.Broadcast();
}

void UDNACueManager::RequestPeriodicUpdateOfEditorObjectLibraryWhileWaitingOnAssetRegistry()
{
	// Asset registry is still loading, so update every 15 seconds until its finished
	if (!EditorObjectLibraryFullyInitialized && !EditorPeriodicUpdateHandle.IsValid())
	{
		GEditor->GetTimerManager()->SetTimer( EditorPeriodicUpdateHandle, FTimerDelegate::CreateUObject(this, &UDNACueManager::InitializeEditorObjectLibrary), 15.f, true);
	}
}

void UDNACueManager::ReloadObjectLibrary(UWorld* World, const UWorld::InitializationValues IVS)
{
	if (bAccelerationMapOutdated)
	{
		RefreshObjectLibraries();
	}
}

void UDNACueManager::GetEditorObjectLibraryDNACueNotifyFilenames(TArray<FString>& Filenames) const
{
	if (ensure(EditorDNACueObjectLibrary.CueSet))
	{
		EditorDNACueObjectLibrary.CueSet->GetFilenames(Filenames);
	}
}

void UDNACueManager::LoadNotifyForEditorPreview(FDNATag DNACueTag)
{
	if (ensure(EditorDNACueObjectLibrary.CueSet) && ensure(RuntimeDNACueObjectLibrary.CueSet))
	{
		EditorDNACueObjectLibrary.CueSet->CopyCueDataToSetForEditorPreview(DNACueTag, RuntimeDNACueObjectLibrary.CueSet);
	}
}

#endif // WITH_EDITOR

TArray<FString> UDNACueManager::GetAlwaysLoadedDNACuePaths()
{
	return UDNAAbilitySystemGlobals::Get().GetDNACueNotifyPaths();
}

void UDNACueManager::RefreshObjectLibraries()
{
	if (RuntimeDNACueObjectLibrary.bHasBeenInitialized)
	{
		check(RuntimeDNACueObjectLibrary.CueSet);
		RuntimeDNACueObjectLibrary.CueSet->Empty();
		InitObjectLibrary(RuntimeDNACueObjectLibrary);
	}

	if (EditorDNACueObjectLibrary.bHasBeenInitialized)
	{
		check(EditorDNACueObjectLibrary.CueSet);
		EditorDNACueObjectLibrary.CueSet->Empty();
		InitObjectLibrary(EditorDNACueObjectLibrary);
	}
}

static void SearchDynamicClassCues(const FName PropertyName, const TArray<FString>& Paths, TArray<FDNACueReferencePair>& CuesToAdd, TArray<FStringAssetReference>& AssetsToLoad)
{
	// Iterate over all Dynamic Classes (nativized Blueprints). Search for ones with DNACueName tag.

	UDNATagsManager& Manager = UDNATagsManager::Get();
	TMap<FName, FDynamicClassStaticData>& DynamicClassMap = GetDynamicClassMap();
	for (auto PairIter : DynamicClassMap)
	{
		const FName* FoundDNATag = PairIter.Value.SelectedSearchableValues.Find(PropertyName);
		if (!FoundDNATag)
		{
			continue;
		}

		const FString ClassPath = PairIter.Key.ToString();
		for (const FString& Path : Paths)
		{
			const bool PathContainsClass = ClassPath.StartsWith(Path); // TODO: is it enough?
			if (!PathContainsClass)
			{
				continue;
			}

			ABILITY_LOG(Log, TEXT("DNACueManager Found a Dynamic Class: %s / %s"), *FoundDNATag->ToString(), *ClassPath);

			FDNATag DNACueTag = Manager.RequestDNATag(*FoundDNATag, false);
			if (DNACueTag.IsValid())
			{
				FStringAssetReference StringRef(ClassPath); // TODO: is there any translation needed?
				ensure(StringRef.IsValid());

				CuesToAdd.Add(FDNACueReferencePair(DNACueTag, StringRef));
				AssetsToLoad.Add(StringRef);
			}
			else
			{
				ABILITY_LOG(Warning, TEXT("Found DNACue tag %s in Dynamic Class %s but there is no corresponding tag in the DNATagManager."), *FoundDNATag->ToString(), *ClassPath);
			}

			break;
		}
	}
}

void UDNACueManager::InitObjectLibrary(FDNACueObjectLibrary& Lib)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Loading Library"), STAT_ObjectLibrary, STATGROUP_LoadTime);

	// Instantiate the UObjectLibraries if they aren't there already
	if (!Lib.StaticObjectLibrary)
	{
		Lib.StaticObjectLibrary = UObjectLibrary::CreateLibrary(ADNACueNotify_Actor::StaticClass(), true, GIsEditor && !IsRunningCommandlet());
		if (GIsEditor)
		{
			Lib.StaticObjectLibrary->bIncludeOnlyOnDiskAssets = false;
		}
	}
	if (!Lib.ActorObjectLibrary)
	{
		Lib.ActorObjectLibrary = UObjectLibrary::CreateLibrary(UDNACueNotify_Static::StaticClass(), true, GIsEditor && !IsRunningCommandlet());
		if (GIsEditor)
		{
			Lib.ActorObjectLibrary->bIncludeOnlyOnDiskAssets = false;
		}
	}	

	Lib.bHasBeenInitialized = true;

#if WITH_EDITOR
	bAccelerationMapOutdated = false;
#endif

	FScopeCycleCounterUObject PreloadScopeActor(Lib.ActorObjectLibrary);

	// ------------------------------------------------------------------------------------------------------------------
	//	Scan asset data. If bShouldSyncScan is false, whatever state the asset registry is in will be what is returned.
	// ------------------------------------------------------------------------------------------------------------------

	{
		//SCOPE_LOG_TIME_IN_SECONDS(*FString::Printf(TEXT("UDNACueManager::InitObjectLibraries    Actors. Paths: %s"), *FString::Join(Lib.Paths, TEXT(", "))), nullptr)
		Lib.ActorObjectLibrary->LoadBlueprintAssetDataFromPaths(Lib.Paths, Lib.bShouldSyncScan);
	}
	{
		//SCOPE_LOG_TIME_IN_SECONDS(*FString::Printf(TEXT("UDNACueManager::InitObjectLibraries    Objects")), nullptr)
		Lib.StaticObjectLibrary->LoadBlueprintAssetDataFromPaths(Lib.Paths, Lib.bShouldSyncScan);
	}

	// ---------------------------------------------------------
	// Sync load if told to do so	
	// ---------------------------------------------------------
	if (Lib.bShouldSyncLoad)
	{
#if STATS
		FString PerfMessage = FString::Printf(TEXT("Fully Loaded DNACueNotify object library"));
		SCOPE_LOG_TIME_IN_SECONDS(*PerfMessage, nullptr)
#endif
		Lib.ActorObjectLibrary->LoadAssetsFromAssetData();
		Lib.StaticObjectLibrary->LoadAssetsFromAssetData();
	}

	// ---------------------------------------------------------
	// Look for DNACueNotifies that handle events
	// ---------------------------------------------------------
	
	TArray<FAssetData> ActorAssetDatas;
	Lib.ActorObjectLibrary->GetAssetDataList(ActorAssetDatas);

	TArray<FAssetData> StaticAssetDatas;
	Lib.StaticObjectLibrary->GetAssetDataList(StaticAssetDatas);

	TArray<FDNACueReferencePair> CuesToAdd;
	TArray<FStringAssetReference> AssetsToLoad;

	// ------------------------------------------------------------------------------------------------------------------
	// Build Cue lists for loading. Determines what from the obj library needs to be loaded
	// ------------------------------------------------------------------------------------------------------------------
	BuildCuesToAddToGlobalSet(ActorAssetDatas, GET_MEMBER_NAME_CHECKED(ADNACueNotify_Actor, DNACueName), CuesToAdd, AssetsToLoad, Lib.ShouldLoad);
	BuildCuesToAddToGlobalSet(StaticAssetDatas, GET_MEMBER_NAME_CHECKED(UDNACueNotify_Static, DNACueName), CuesToAdd, AssetsToLoad, Lib.ShouldLoad);

	const FName PropertyName = GET_MEMBER_NAME_CHECKED(ADNACueNotify_Actor, DNACueName);
	check(PropertyName == GET_MEMBER_NAME_CHECKED(UDNACueNotify_Static, DNACueName));
	SearchDynamicClassCues(PropertyName, Lib.Paths, CuesToAdd, AssetsToLoad);

	// ------------------------------------------------------------------------------------------------------------------------------------
	// Add these cues to the set. The UDNACueSet is the data structure used in routing the DNA cue events at runtime.
	// ------------------------------------------------------------------------------------------------------------------------------------
	UDNACueSet* SetToAddTo = Lib.CueSet;
	if (!SetToAddTo)
	{
		SetToAddTo = RuntimeDNACueObjectLibrary.CueSet;
	}
	check(SetToAddTo);
	SetToAddTo->AddCues(CuesToAdd);

	// --------------------------------------------
	// Start loading them if necessary
	// --------------------------------------------
	if (Lib.bShouldAsyncLoad)
	{
		auto ForwardLambda = [](TArray<FStringAssetReference> AssetList, FOnDNACueNotifySetLoaded OnLoadedDelegate)
		{
			OnLoadedDelegate.ExecuteIfBound(AssetList);
		};

		if (AssetsToLoad.Num() > 0)
		{
			StreamableManager.RequestAsyncLoad(AssetsToLoad, FStreamableDelegate::CreateStatic( ForwardLambda, AssetsToLoad, Lib.OnLoaded), Lib.AsyncPriority);
		}
		else
		{
			// Still fire the delegate even if nothing was found to load
			Lib.OnLoaded.ExecuteIfBound(AssetsToLoad);
		}
	}

	// Build Tag Translation table
	TranslationManager.BuildTagTranslationTable();
}

static FAutoConsoleVariable CVarGameplyCueAddToGlobalSetDebug(TEXT("DNACue.AddToGlobalSet.DebugTag"), TEXT(""), TEXT("Debug Tag adding to global set"), ECVF_Default	);

void UDNACueManager::BuildCuesToAddToGlobalSet(const TArray<FAssetData>& AssetDataList, FName TagPropertyName, TArray<FDNACueReferencePair>& OutCuesToAdd, TArray<FStringAssetReference>& OutAssetsToLoad, FShouldLoadGCNotifyDelegate ShouldLoad)
{
	UDNATagsManager& Manager = UDNATagsManager::Get();

	OutAssetsToLoad.Reserve(OutAssetsToLoad.Num() + AssetDataList.Num());

	for (const FAssetData& Data: AssetDataList)
	{
		const FName FoundDNATag = Data.GetTagValueRef<FName>(TagPropertyName);
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (CVarGameplyCueAddToGlobalSetDebug->GetString().IsEmpty() == false && FoundDNATag.ToString().Contains(CVarGameplyCueAddToGlobalSetDebug->GetString()))
		{
			ABILITY_LOG(Display, TEXT("Adding Tag %s to GlobalSet"), *FoundDNATag.ToString());
		}
#endif

		// If ShouldLoad delegate is bound and it returns false, don't load this one
		if (ShouldLoad.IsBound() && (ShouldLoad.Execute(Data, FoundDNATag) == false))
		{
			continue;
		}
		
		if (ShouldLoadDNACueAssetData(Data) == false)
		{
			continue;
		}
		
		if (!FoundDNATag.IsNone())
		{
			const FString GeneratedClassTag = Data.GetTagValueRef<FString>("GeneratedClass");
			if (GeneratedClassTag.IsEmpty())
			{
				ABILITY_LOG(Warning, TEXT("Unable to find GeneratedClass value for AssetData %s"), *Data.ObjectPath.ToString());
				continue;
			}

			ABILITY_LOG(Log, TEXT("DNACueManager Found: %s / %s"), *FoundDNATag.ToString(), **GeneratedClassTag);

			FDNATag  DNACueTag = Manager.RequestDNATag(FoundDNATag, false);
			if (DNACueTag.IsValid())
			{
				// Add a new NotifyData entry to our flat list for this one
				FStringAssetReference StringRef;
				StringRef.SetPath(FPackageName::ExportTextPathToObjectPath(*GeneratedClassTag));

				OutCuesToAdd.Add(FDNACueReferencePair(DNACueTag, StringRef));

				OutAssetsToLoad.Add(StringRef);
			}
			else
			{
				// Warn about this tag but only once to cut down on spam (we may build cue sets multiple times in the editor)
				static TSet<FName> WarnedTags;
				if (WarnedTags.Contains(FoundDNATag) == false)
				{
					ABILITY_LOG(Warning, TEXT("Found DNACue tag %s in asset %s but there is no corresponding tag in the DNATagManager."), *FoundDNATag.ToString(), *Data.PackageName.ToString());
					WarnedTags.Add(FoundDNATag);
				}
			}
		}
	}
}

int32 DNACueCheckForTooManyRPCs = 1;
static FAutoConsoleVariableRef CVarDNACueCheckForTooManyRPCs(TEXT("DNAAbilitySystem.DNACueCheckForTooManyRPCs"), DNACueCheckForTooManyRPCs, TEXT("Warns if DNA cues are being throttled by network code"), ECVF_Default );

void UDNACueManager::CheckForTooManyRPCs(FName FuncName, const FDNACuePendingExecute& PendingCue, const FString& CueID, const FDNAEffectContext* EffectContext)
{
	if (DNACueCheckForTooManyRPCs)
	{
		static IConsoleVariable* MaxRPCPerNetUpdateCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("net.MaxRPCPerNetUpdate"));
		if (MaxRPCPerNetUpdateCVar)
		{
			AActor* Owner = PendingCue.OwningComponent ? PendingCue.OwningComponent->GetOwner() : nullptr;
			UWorld* World = Owner ? Owner->GetWorld() : nullptr;
			UNetDriver* NetDriver = World ? World->GetNetDriver() : nullptr;
			if (NetDriver)
			{
				const int32 MaxRPCs = MaxRPCPerNetUpdateCVar->GetInt();
				for (UNetConnection* ClientConnection : NetDriver->ClientConnections)
				{
					if (ClientConnection)
					{
						UActorChannel** OwningActorChannelPtr = ClientConnection->ActorChannels.Find(Owner);
						TSharedRef<FObjectReplicator>* ComponentReplicatorPtr = (OwningActorChannelPtr && *OwningActorChannelPtr) ? (*OwningActorChannelPtr)->ReplicationMap.Find(PendingCue.OwningComponent) : nullptr;
						if (ComponentReplicatorPtr)
						{
							const TArray<FObjectReplicator::FRPCCallInfo>& RemoteFuncInfo = (*ComponentReplicatorPtr)->RemoteFuncInfo;
							for (const FObjectReplicator::FRPCCallInfo& CallInfo : RemoteFuncInfo)
							{
								if (CallInfo.FuncName == FuncName)
								{
									if (CallInfo.Calls > MaxRPCs)
									{
										const FString Instigator = EffectContext ? EffectContext->ToString() : TEXT("None");
										ABILITY_LOG(Warning, TEXT("Attempted to fire %s when no more RPCs are allowed this net update. Max:%d Cue:%s Instigator:%s Component:%s"), *FuncName.ToString(), MaxRPCs, *CueID, *Instigator, *GetPathNameSafe(PendingCue.OwningComponent));
									
										// Returning here to only log once per offending RPC.
										return;
									}

									break;
								}
							}
						}
					}
				}
			}
		}
	}
}

void UDNACueManager::OnDNACueNotifyAsyncLoadComplete(TArray<FStringAssetReference> AssetList)
{
	for (FStringAssetReference StringRef : AssetList)
	{
		UClass* GCClass = FindObject<UClass>(nullptr, *StringRef.ToString());
		if (ensure(GCClass))
		{
			LoadedDNACueNotifyClasses.Add(GCClass);
			CheckForPreallocation(GCClass);
		}
	}
}

int32 UDNACueManager::FinishLoadingDNACueNotifies()
{
	int32 NumLoadeded = 0;
	return NumLoadeded;
}

UDNACueSet* UDNACueManager::GetRuntimeCueSet()
{
	return RuntimeDNACueObjectLibrary.CueSet;
}

TArray<UDNACueSet*> UDNACueManager::GetGlobalCueSets()
{
	TArray<UDNACueSet*> Set;
	if (RuntimeDNACueObjectLibrary.CueSet)
	{
		Set.Add(RuntimeDNACueObjectLibrary.CueSet);
	}
	if (EditorDNACueObjectLibrary.CueSet)
	{
		Set.Add(EditorDNACueObjectLibrary.CueSet);
	}
	return Set;
}

#if WITH_EDITOR

UDNACueSet* UDNACueManager::GetEditorCueSet()
{
	return EditorDNACueObjectLibrary.CueSet;
}

void UDNACueManager::HandleAssetAdded(UObject *Object)
{
	UBlueprint* Blueprint = Cast<UBlueprint>(Object);
	if (Blueprint && Blueprint->GeneratedClass)
	{
		UDNACueNotify_Static* StaticCDO = Cast<UDNACueNotify_Static>(Blueprint->GeneratedClass->ClassDefaultObject);
		ADNACueNotify_Actor* ActorCDO = Cast<ADNACueNotify_Actor>(Blueprint->GeneratedClass->ClassDefaultObject);
		
		if (StaticCDO || ActorCDO)
		{
			if (VerifyNotifyAssetIsInValidPath(Blueprint->GetOuter()->GetPathName()))
			{
				FStringAssetReference StringRef;
				StringRef.SetPath(Blueprint->GeneratedClass->GetPathName());

				TArray<FDNACueReferencePair> CuesToAdd;
				if (StaticCDO)
				{
					CuesToAdd.Add(FDNACueReferencePair(StaticCDO->DNACueTag, StringRef));
				}
				else if (ActorCDO)
				{
					CuesToAdd.Add(FDNACueReferencePair(ActorCDO->DNACueTag, StringRef));
				}

				for (UDNACueSet* Set : GetGlobalCueSets())
				{
					Set->AddCues(CuesToAdd);
				}

				OnDNACueNotifyAddOrRemove.Broadcast();
			}
		}
	}
}

/** Handles cleaning up an object library if it matches the passed in object */
void UDNACueManager::HandleAssetDeleted(UObject *Object)
{
	FStringAssetReference StringRefToRemove;
	UBlueprint* Blueprint = Cast<UBlueprint>(Object);
	if (Blueprint && Blueprint->GeneratedClass)
	{
		UDNACueNotify_Static* StaticCDO = Cast<UDNACueNotify_Static>(Blueprint->GeneratedClass->ClassDefaultObject);
		ADNACueNotify_Actor* ActorCDO = Cast<ADNACueNotify_Actor>(Blueprint->GeneratedClass->ClassDefaultObject);
		
		if (StaticCDO || ActorCDO)
		{
			StringRefToRemove.SetPath(Blueprint->GeneratedClass->GetPathName());
		}
	}

	if (StringRefToRemove.IsValid())
	{
		TArray<FStringAssetReference> StringRefs;
		StringRefs.Add(StringRefToRemove);
		
		
		for (UDNACueSet* Set : GetGlobalCueSets())
		{
			Set->RemoveCuesByStringRefs(StringRefs);
		}

		OnDNACueNotifyAddOrRemove.Broadcast();
	}
}

/** Handles cleaning up an object library if it matches the passed in object */
void UDNACueManager::HandleAssetRenamed(const FAssetData& Data, const FString& String)
{
	const FString ParentClassName = Data.GetTagValueRef<FString>("ParentClass");
	if (!ParentClassName.IsEmpty())
	{
		UClass* DataClass = FindObject<UClass>(nullptr, *ParentClassName);
		if (DataClass)
		{
			UDNACueNotify_Static* StaticCDO = Cast<UDNACueNotify_Static>(DataClass->ClassDefaultObject);
			ADNACueNotify_Actor* ActorCDO = Cast<ADNACueNotify_Actor>(DataClass->ClassDefaultObject);
			if (StaticCDO || ActorCDO)
			{
				VerifyNotifyAssetIsInValidPath(Data.PackagePath.ToString());

				for (UDNACueSet* Set : GetGlobalCueSets())
				{
					Set->UpdateCueByStringRefs(String + TEXT("_C"), Data.ObjectPath.ToString() + TEXT("_C"));
				}
				OnDNACueNotifyAddOrRemove.Broadcast();
			}
		}
	}
}

bool UDNACueManager::VerifyNotifyAssetIsInValidPath(FString Path)
{
	bool ValidPath = false;
	for (FString& str: GetValidDNACuePaths())
	{
		if (Path.Contains(str))
		{
			ValidPath = true;
		}
	}

	if (!ValidPath)
	{
		FString MessageTry = FString::Printf(TEXT("Warning: Invalid DNACue Path %s"));
		MessageTry += TEXT("\n\nDNACue Notifies should only be saved in the following folders:");

		ABILITY_LOG(Warning, TEXT("Warning: Invalid DNACuePath: %s"), *Path);
		ABILITY_LOG(Warning, TEXT("Valid Paths: "));
		for (FString& str: GetValidDNACuePaths())
		{
			ABILITY_LOG(Warning, TEXT("  %s"), *str);
			MessageTry += FString::Printf(TEXT("\n  %s"), *str);
		}

		MessageTry += FString::Printf(TEXT("\n\nThis asset must be moved to a valid location to work in game."));

		const FText MessageText = FText::FromString(MessageTry);
		const FText TitleText = NSLOCTEXT("DNACuePathWarning", "DNACuePathWarningTitle", "Invalid DNACue Path");
		FMessageDialog::Open(EAppMsgType::Ok, MessageText, &TitleText);
	}

	return ValidPath;
}

#endif


UWorld* UDNACueManager::GetWorld() const
{
#if WITH_EDITOR
	if (PreviewWorld)
		return PreviewWorld;
#endif

	return CurrentWorld;
}

void UDNACueManager::PrintDNACueNotifyMap()
{
	if (ensure(RuntimeDNACueObjectLibrary.CueSet))
	{
		RuntimeDNACueObjectLibrary.CueSet->PrintCues();
	}
}

void UDNACueManager::PrintLoadedDNACueNotifyClasses()
{
	for (UClass* NotifyClass : LoadedDNACueNotifyClasses)
	{
		ABILITY_LOG(Display, TEXT("%s"), *GetNameSafe(NotifyClass));
	}
	ABILITY_LOG(Display, TEXT("%d total classes"), LoadedDNACueNotifyClasses.Num());
}

static void	PrintDNACueNotifyMapConsoleCommandFunc(UWorld* InWorld)
{
	UDNAAbilitySystemGlobals::Get().GetDNACueManager()->PrintDNACueNotifyMap();
}

FAutoConsoleCommandWithWorld PrintDNACueNotifyMapConsoleCommand(
	TEXT("DNACue.PrintDNACueNotifyMap"),
	TEXT("Displays DNACue notify map"),
	FConsoleCommandWithWorldDelegate::CreateStatic(PrintDNACueNotifyMapConsoleCommandFunc)
	);

static void	PrintLoadedDNACueNotifyClasses(UWorld* InWorld)
{
	UDNAAbilitySystemGlobals::Get().GetDNACueManager()->PrintLoadedDNACueNotifyClasses();
}

FAutoConsoleCommandWithWorld PrintLoadedDNACueNotifyClassesCommand(
	TEXT("DNACue.PrintLoadedDNACueNotifyClasses"),
	TEXT("Displays DNACue Notify classes that are loaded"),
	FConsoleCommandWithWorldDelegate::CreateStatic(PrintLoadedDNACueNotifyClasses)
	);

FScopedDNACueSendContext::FScopedDNACueSendContext()
{
	UDNAAbilitySystemGlobals::Get().GetDNACueManager()->StartDNACueSendContext();
}
FScopedDNACueSendContext::~FScopedDNACueSendContext()
{
	UDNAAbilitySystemGlobals::Get().GetDNACueManager()->EndDNACueSendContext();
}

template<class AllocatorType>
void PullDNACueTagsFromSpec(const FDNAEffectSpec& Spec, TArray<FDNATag, AllocatorType>& OutArray)
{
	// Add all DNACue Tags from the GE into the DNACueTags PendingCue.list
	for (const FDNAEffectCue& EffectCue : Spec.Def->DNACues)
	{
		for (const FDNATag& Tag: EffectCue.DNACueTags)
		{
			if (Tag.IsValid())
			{
				OutArray.Add(Tag);
			}
		}
	}
}

/**
 *	Enabling DNAAbilitySystemAlwaysConvertGESpecToGCParams will mean that all calls to DNA cues with DNAEffectSpecs will be converted into DNACue Parameters server side and then replicated.
 *	This potentially saved bandwidth but also has less information, depending on how the GESpec is converted to GC Parameters and what your GC's need to know.
 */

int32 DNAAbilitySystemAlwaysConvertGESpecToGCParams = 0;
static FAutoConsoleVariableRef CVarDNAAbilitySystemAlwaysConvertGESpecToGCParams(TEXT("DNAAbilitySystem.AlwaysConvertGESpecToGCParams"), DNAAbilitySystemAlwaysConvertGESpecToGCParams, TEXT("Always convert a DNACue from GE Spec to GC from GC Parameters on the server"), ECVF_Default );

void UDNACueManager::InvokeDNACueAddedAndWhileActive_FromSpec(UDNAAbilitySystemComponent* OwningComponent, const FDNAEffectSpec& Spec, FPredictionKey PredictionKey)
{
	if (Spec.Def->DNACues.Num() == 0)
	{
		return;
	}

	if (DNAAbilitySystemAlwaysConvertGESpecToGCParams)
	{
		// Transform the GE Spec into DNACue parmameters here (on the server)

		FDNACueParameters Parameters;
		UDNAAbilitySystemGlobals::Get().InitDNACueParameters_GESpec(Parameters, Spec);

		static TArray<FDNATag, TInlineAllocator<4> > Tags;
		Tags.Reset();

		PullDNACueTagsFromSpec(Spec, Tags);

		if (Tags.Num() == 1)
		{
			OwningComponent->NetMulticast_InvokeDNACueAddedAndWhileActive_WithParams(Tags[0], PredictionKey, Parameters);
			
		}
		else if (Tags.Num() > 1)
		{
			OwningComponent->NetMulticast_InvokeDNACuesAddedAndWhileActive_WithParams(FDNATagContainer::CreateFromArray(Tags), PredictionKey, Parameters);
		}
		else
		{
			ABILITY_LOG(Warning, TEXT("No actual DNA cue tags found in DNAEffect %s (despite it having entries in its DNA cue list!"), *Spec.Def->GetName());

		}
	}
	else
	{
		OwningComponent->NetMulticast_InvokeDNACueAddedAndWhileActive_FromSpec(Spec, PredictionKey);

	}
}

void UDNACueManager::InvokeDNACueExecuted_FromSpec(UDNAAbilitySystemComponent* OwningComponent, const FDNAEffectSpec& Spec, FPredictionKey PredictionKey)
{	
	if (Spec.Def->DNACues.Num() == 0)
	{
		// This spec doesn't have any GCs, so early out
		ABILITY_LOG(Verbose, TEXT("No GCs in this Spec, so early out: %s"), *Spec.Def->GetName());
		return;
	}

	FDNACuePendingExecute PendingCue;

	if (DNAAbilitySystemAlwaysConvertGESpecToGCParams)
	{
		// Transform the GE Spec into DNACue parmameters here (on the server)
		PendingCue.PayloadType = EDNACuePayloadType::CueParameters;
		PendingCue.OwningComponent = OwningComponent;
		PendingCue.PredictionKey = PredictionKey;

		PullDNACueTagsFromSpec(Spec, PendingCue.DNACueTags);
		if (PendingCue.DNACueTags.Num() == 0)
		{
			ABILITY_LOG(Warning, TEXT("GE %s has DNACues but not valid DNACue tag."), *Spec.Def->GetName());			
			return;
		}
		
		UDNAAbilitySystemGlobals::Get().InitDNACueParameters_GESpec(PendingCue.CueParameters, Spec);
	}
	else
	{
		// Transform the GE Spec into a FDNAEffectSpecForRPC (holds less information than the GE Spec itself, but more information that the FGamepalyCueParameter)
		PendingCue.PayloadType = EDNACuePayloadType::FromSpec;
		PendingCue.OwningComponent = OwningComponent;
		PendingCue.FromSpec = FDNAEffectSpecForRPC(Spec);
		PendingCue.PredictionKey = PredictionKey;
	}

	if (ProcessPendingCueExecute(PendingCue))
	{
		PendingExecuteCues.Add(PendingCue);
	}

	if (DNACueSendContextCount == 0)
	{
		// Not in a context, flush now
		FlushPendingCues();
	}
}

void UDNACueManager::InvokeDNACueExecuted(UDNAAbilitySystemComponent* OwningComponent, const FDNATag DNACueTag, FPredictionKey PredictionKey, FDNAEffectContextHandle EffectContext)
{
	FDNACuePendingExecute PendingCue;
	PendingCue.PayloadType = EDNACuePayloadType::CueParameters;
	PendingCue.DNACueTags.Add(DNACueTag);
	PendingCue.OwningComponent = OwningComponent;
	UDNAAbilitySystemGlobals::Get().InitDNACueParameters(PendingCue.CueParameters, EffectContext);
	PendingCue.PredictionKey = PredictionKey;

	if (ProcessPendingCueExecute(PendingCue))
	{
		PendingExecuteCues.Add(PendingCue);
	}

	if (DNACueSendContextCount == 0)
	{
		// Not in a context, flush now
		FlushPendingCues();
	}
}

void UDNACueManager::InvokeDNACueExecuted_WithParams(UDNAAbilitySystemComponent* OwningComponent, const FDNATag DNACueTag, FPredictionKey PredictionKey, FDNACueParameters DNACueParameters)
{
	FDNACuePendingExecute PendingCue;
	PendingCue.PayloadType = EDNACuePayloadType::CueParameters;
	PendingCue.DNACueTags.Add(DNACueTag);
	PendingCue.OwningComponent = OwningComponent;
	PendingCue.CueParameters = DNACueParameters;
	PendingCue.PredictionKey = PredictionKey;

	if (ProcessPendingCueExecute(PendingCue))
	{
		PendingExecuteCues.Add(PendingCue);
	}

	if (DNACueSendContextCount == 0)
	{
		// Not in a context, flush now
		FlushPendingCues();
	}
}

void UDNACueManager::StartDNACueSendContext()
{
	DNACueSendContextCount++;
}

void UDNACueManager::EndDNACueSendContext()
{
	DNACueSendContextCount--;

	if (DNACueSendContextCount == 0)
	{
		FlushPendingCues();
	}
	else if (DNACueSendContextCount < 0)
	{
		ABILITY_LOG(Warning, TEXT("UDNACueManager::EndDNACueSendContext called too many times! Negative context count"));
	}
}

void UDNACueManager::FlushPendingCues()
{
	TArray<FDNACuePendingExecute> LocalPendingExecuteCues = PendingExecuteCues;
	PendingExecuteCues.Empty();
	for (int32 i = 0; i < LocalPendingExecuteCues.Num(); i++)
	{
		FDNACuePendingExecute& PendingCue = LocalPendingExecuteCues[i];

		// Our component may have gone away
		if (PendingCue.OwningComponent)
		{
			bool bHasAuthority = PendingCue.OwningComponent->IsOwnerActorAuthoritative();
			bool bLocalPredictionKey = PendingCue.PredictionKey.IsLocalClientKey();

			// TODO: Could implement non-rpc method for replicating if desired
			switch (PendingCue.PayloadType)
			{
			case EDNACuePayloadType::CueParameters:
				if (ensure(PendingCue.DNACueTags.Num() >= 1))
				{
					if (bHasAuthority)
					{
						PendingCue.OwningComponent->ForceReplication();
						if (PendingCue.DNACueTags.Num() > 1)
						{
							PendingCue.OwningComponent->NetMulticast_InvokeDNACuesExecuted_WithParams(FDNATagContainer::CreateFromArray(PendingCue.DNACueTags), PendingCue.PredictionKey, PendingCue.CueParameters);
						}
						else
						{
							PendingCue.OwningComponent->NetMulticast_InvokeDNACueExecuted_WithParams(PendingCue.DNACueTags[0], PendingCue.PredictionKey, PendingCue.CueParameters);
							static FName NetMulticast_InvokeDNACueExecuted_WithParamsName = TEXT("NetMulticast_InvokeDNACueExecuted_WithParams");
							CheckForTooManyRPCs(NetMulticast_InvokeDNACueExecuted_WithParamsName, PendingCue, PendingCue.DNACueTags[0].ToString(), nullptr);
						}
					}
					else if (bLocalPredictionKey)
					{
						for (const FDNATag& Tag : PendingCue.DNACueTags)
						{
							PendingCue.OwningComponent->InvokeDNACueEvent(Tag, EDNACueEvent::Executed, PendingCue.CueParameters);
						}
					}
				}
				break;
			case EDNACuePayloadType::EffectContext:
				if (ensure(PendingCue.DNACueTags.Num() >= 1))
				{
					if (bHasAuthority)
					{
						PendingCue.OwningComponent->ForceReplication();
						if (PendingCue.DNACueTags.Num() > 1)
						{
							PendingCue.OwningComponent->NetMulticast_InvokeDNACuesExecuted(FDNATagContainer::CreateFromArray(PendingCue.DNACueTags), PendingCue.PredictionKey, PendingCue.CueParameters.EffectContext);
						}
						else
						{
							PendingCue.OwningComponent->NetMulticast_InvokeDNACueExecuted(PendingCue.DNACueTags[0], PendingCue.PredictionKey, PendingCue.CueParameters.EffectContext);
							static FName NetMulticast_InvokeDNACueExecutedName = TEXT("NetMulticast_InvokeDNACueExecuted");
							CheckForTooManyRPCs(NetMulticast_InvokeDNACueExecutedName, PendingCue, PendingCue.DNACueTags[0].ToString(), PendingCue.CueParameters.EffectContext.Get());
						}
					}
					else if (bLocalPredictionKey)
					{
						for (const FDNATag& Tag : PendingCue.DNACueTags)
						{
							PendingCue.OwningComponent->InvokeDNACueEvent(Tag, EDNACueEvent::Executed, PendingCue.CueParameters.EffectContext);
						}
					}
				}
				break;
			case EDNACuePayloadType::FromSpec:
				if (bHasAuthority)
				{
					PendingCue.OwningComponent->ForceReplication();
					PendingCue.OwningComponent->NetMulticast_InvokeDNACueExecuted_FromSpec(PendingCue.FromSpec, PendingCue.PredictionKey);
					static FName NetMulticast_InvokeDNACueExecuted_FromSpecName = TEXT("NetMulticast_InvokeDNACueExecuted_FromSpec");
					CheckForTooManyRPCs(NetMulticast_InvokeDNACueExecuted_FromSpecName, PendingCue, PendingCue.FromSpec.Def ? PendingCue.FromSpec.ToSimpleString() : TEXT("FromSpecWithNoDef"), PendingCue.FromSpec.EffectContext.Get());
				}
				else if (bLocalPredictionKey)
				{
					PendingCue.OwningComponent->InvokeDNACueEvent(PendingCue.FromSpec, EDNACueEvent::Executed);
				}
				break;
			}
		}
	}
}

bool UDNACueManager::ProcessPendingCueExecute(FDNACuePendingExecute& PendingCue)
{
	// Subclasses can do something here
	return true;
}

bool UDNACueManager::DoesPendingCueExecuteMatch(FDNACuePendingExecute& PendingCue, FDNACuePendingExecute& ExistingCue)
{
	const FHitResult* PendingHitResult = NULL;
	const FHitResult* ExistingHitResult = NULL;

	if (PendingCue.PayloadType != ExistingCue.PayloadType)
	{
		return false;
	}

	if (PendingCue.OwningComponent != ExistingCue.OwningComponent)
	{
		return false;
	}

	if (PendingCue.PredictionKey.PredictiveConnection != ExistingCue.PredictionKey.PredictiveConnection)
	{
		// They can both by null, but if they were predicted by different people exclude it
		return false;
	}

	if (PendingCue.PayloadType == EDNACuePayloadType::FromSpec)
	{
		if (PendingCue.FromSpec.Def != ExistingCue.FromSpec.Def)
		{
			return false;
		}

		if (PendingCue.FromSpec.Level != ExistingCue.FromSpec.Level)
		{
			return false;
		}
	}
	else
	{
		if (PendingCue.DNACueTags != ExistingCue.DNACueTags)
		{
			return false;
		}
	}

	return true;
}

void UDNACueManager::CheckForPreallocation(UClass* GCClass)
{
	if (ADNACueNotify_Actor* InstancedCue = Cast<ADNACueNotify_Actor>(GCClass->ClassDefaultObject))
	{
		if (InstancedCue->NumPreallocatedInstances > 0 && DNACueClassesForPreallocation.Contains(InstancedCue) == false)
		{
			// Add this to the global list
			DNACueClassesForPreallocation.Add(InstancedCue);

			// Add it to any world specific lists
			for (FPreallocationInfo& Info : PreallocationInfoList_Internal)
			{
				ensure(Info.ClassesNeedingPreallocation.Contains(InstancedCue)==false);
				Info.ClassesNeedingPreallocation.Push(InstancedCue);
			}
		}
	}
}

// -------------------------------------------------------------

void UDNACueManager::ResetPreallocation(UWorld* World)
{
	FPreallocationInfo& Info = GetPreallocationInfo(World);

	Info.PreallocatedInstances.Reset();
	Info.ClassesNeedingPreallocation = DNACueClassesForPreallocation;
}

void UDNACueManager::UpdatePreallocation(UWorld* World)
{
#if WITH_EDITOR	
	// Don't preallocate
	if (World->IsPreviewWorld())
	{
		return;
	}
#endif

	FPreallocationInfo& Info = GetPreallocationInfo(World);

	if (Info.ClassesNeedingPreallocation.Num() > 0)
	{
		ADNACueNotify_Actor* CDO = Info.ClassesNeedingPreallocation.Last();
		TArray<ADNACueNotify_Actor*>& PreallocatedList = Info.PreallocatedInstances.FindOrAdd(CDO->GetClass());

		ADNACueNotify_Actor* PrespawnedInstance = Cast<ADNACueNotify_Actor>(World->SpawnActor(CDO->GetClass()));
		if (ensureMsgf(PrespawnedInstance, TEXT("Failed to prespawn GC notify for: %s"), *GetNameSafe(CDO)))
		{
			ensureMsgf(PrespawnedInstance->IsPendingKill() == false, TEXT("Newly spawned GC is PendingKILL: %s"), *GetNameSafe(CDO));

			if (LogDNACueActorSpawning)
			{
				ABILITY_LOG(Warning, TEXT("Prespawning GC %s"), *GetNameSafe(CDO));
			}

			PrespawnedInstance->bInRecycleQueue = true;
			PreallocatedList.Push(PrespawnedInstance);
			PrespawnedInstance->SetActorHiddenInGame(true);

			if (PreallocatedList.Num() >= CDO->NumPreallocatedInstances)
			{
				Info.ClassesNeedingPreallocation.Pop(false);
			}
		}
	}
}

FPreallocationInfo& UDNACueManager::GetPreallocationInfo(UWorld* World)
{
	FObjectKey ObjKey(World);

	for (FPreallocationInfo& Info : PreallocationInfoList_Internal)
	{
		if (ObjKey == Info.OwningWorldKey)
		{
			return Info;
		}
	}

	FPreallocationInfo NewInfo;
	NewInfo.OwningWorldKey = ObjKey;

	PreallocationInfoList_Internal.Add(NewInfo);
	return PreallocationInfoList_Internal.Last();
}

void UDNACueManager::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	DumpPreallocationStats(World);
#endif

	for (int32 idx=0; idx < PreallocationInfoList_Internal.Num(); ++idx)
	{
		if (PreallocationInfoList_Internal[idx].OwningWorldKey == FObjectKey(World))
		{
			ABILITY_LOG(Display, TEXT("UDNACueManager::OnWorldCleanup Removing PreallocationInfoList_Internal element %d"), idx);
			PreallocationInfoList_Internal.RemoveAtSwap(idx, 1, false);
			idx--;
		}
	}

	IDNACueInterface::ClearTagToFunctionMap();
}

void UDNACueManager::DumpPreallocationStats(UWorld* World)
{
	if (World == nullptr)
	{
		return;
	}

	FPreallocationInfo& Info = GetPreallocationInfo(World);
	for (auto &It : Info.PreallocatedInstances)
	{
		if (UClass* ThisClass = It.Key)
		{
			if (ADNACueNotify_Actor* CDO = ThisClass->GetDefaultObject<ADNACueNotify_Actor>())
			{
				TArray<ADNACueNotify_Actor*>& List = It.Value;
				if (List.Num() > CDO->NumPreallocatedInstances)
				{
					ABILITY_LOG(Display, TEXT("Notify class: %s was used simultaneously %d times. The CDO default is %d preallocated instanced."), *ThisClass->GetName(), List.Num(),  CDO->NumPreallocatedInstances); 
				}
			}
		}
	}
}

void UDNACueManager::OnPreReplayScrub(UWorld* World)
{
	// See if the World's demo net driver is the duplicated collection's driver,
	// and if so, don't reset preallocated instances. Since the preallocations are global
	// among all level collections, this would clear all current preallocated instances from the list,
	// but there's no need to, and the actor instances would still be around, causing a leak.
	const FLevelCollection* const DuplicateLevelCollection = World ? World->FindCollectionByType(ELevelCollectionType::DynamicDuplicatedLevels) : nullptr;
	if (DuplicateLevelCollection && DuplicateLevelCollection->GetDemoNetDriver() == World->DemoNetDriver)
	{
		return;
	}

	FPreallocationInfo& Info = GetPreallocationInfo(World);
	Info.PreallocatedInstances.Reset();
}

#if DNACUE_DEBUG
FDNACueDebugInfo* UDNACueManager::GetDebugInfo(int32 Handle, bool Reset)
{
	static const int32 MaxDebugEntries = 256;
	int32 Index = Handle % MaxDebugEntries;

	static TArray<FDNACueDebugInfo> DebugArray;
	if (DebugArray.Num() == 0)
	{
		DebugArray.AddDefaulted(MaxDebugEntries);
	}
	if (Reset)
	{
		DebugArray[Index] = FDNACueDebugInfo();
	}

	return &DebugArray[Index];
}
#endif

// ----------------------------------------------------------------

static void	RunDNACueTranslator(UWorld* InWorld)
{
	UDNAAbilitySystemGlobals::Get().GetDNACueManager()->TranslationManager.BuildTagTranslationTable();
}

FAutoConsoleCommandWithWorld RunDNACueTranslatorCmd(
	TEXT("DNACue.BuildDNACueTranslator"),
	TEXT("Displays DNACue notify map"),
	FConsoleCommandWithWorldDelegate::CreateStatic(RunDNACueTranslator)
	);

// -----------------------------------------------------

static void	PrintDNACueTranslator(UWorld* InWorld)
{
	UDNAAbilitySystemGlobals::Get().GetDNACueManager()->TranslationManager.PrintTranslationTable();
}

FAutoConsoleCommandWithWorld PrintDNACueTranslatorCmd(
	TEXT("DNACue.PrintDNACueTranslator"),
	TEXT("Displays DNACue notify map"),
	FConsoleCommandWithWorldDelegate::CreateStatic(PrintDNACueTranslator)
	);


#if WITH_EDITOR
#undef LOCTEXT_NAMESPACE
#endif
