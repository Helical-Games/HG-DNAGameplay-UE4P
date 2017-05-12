// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#include "LevelEditor.h"
#endif // WITH_EDITOR
#include "DNADebuggerAddonManager.h"
#include "DNADebuggerPlayerManager.h"

class ADNADebuggingReplicator;

class FDNADebuggerCompat : public FSelfRegisteringExec, public DNADebugger
{
public:
	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End IModuleInterface

	void WorldAdded(UWorld* InWorld);
	void WorldDestroyed(UWorld* InWorld);
#if WITH_EDITOR
	void OnLevelActorAdded(AActor* InActor);
	void OnLevelActorDeleted(AActor* InActor);
	TSharedRef<FExtender> OnExtendLevelEditorViewMenu(const TSharedRef<FUICommandList> CommandList);
	void CreateSnappingOptionsMenu(FMenuBuilder& Builder);
	void CreateSettingSubMenu(FMenuBuilder& Builder);
	void HandleSettingChanged(FName PropertyName);
#endif

	TArray<TWeakObjectPtr<ADNADebuggingReplicator> >& GetAllReplicators(UWorld* InWorld);
	void AddReplicator(UWorld* InWorld, ADNADebuggingReplicator* InReplicator);
	void RemoveReplicator(UWorld* InWorld, ADNADebuggingReplicator* InReplicator);

	// Begin FExec Interface
	virtual bool Exec(UWorld* Inworld, const TCHAR* Cmd, FOutputDevice& Ar) override;
	// End FExec Interface

private:
	virtual bool CreateDNADebuggerForPlayerController(APlayerController* PlayerController) override;
	virtual bool IsDNADebuggerActiveForPlayerController(APlayerController* PlayerController) override;

	bool DoesDNADebuggingReplicatorExistForPlayerController(APlayerController* PlayerController);

	TMap<TWeakObjectPtr<UWorld>, TArray<TWeakObjectPtr<ADNADebuggingReplicator> > > AllReplicatorsPerWorlds;

public:
	virtual void RegisterCategory(FName CategoryName, FOnGetCategory MakeInstanceDelegate, EDNADebuggerCategoryState CategoryState = EDNADebuggerCategoryState::Disabled, int32 SlotIdx = INDEX_NONE);
	virtual void UnregisterCategory(FName CategoryName);
	virtual void NotifyCategoriesChanged();
	virtual void RegisterExtension(FName ExtensionName, IDNADebugger::FOnGetExtension MakeInstanceDelegate);
	virtual void UnregisterExtension(FName ExtensionName);
	virtual void NotifyExtensionsChanged();
	virtual void UseNewDNADebugger();

	void StartupNewDebugger();
	void ShutdownNewDebugger();
	ADNADebuggerPlayerManager& GetPlayerManager(UWorld* World);
	void OnWorldInitialized(UWorld* World, const UWorld::InitializationValues IVS);

	bool bNewDebuggerEnabled;
	FDNADebuggerAddonManager AddonManager;
	TMap<TWeakObjectPtr<UWorld>, TWeakObjectPtr<ADNADebuggerPlayerManager>> PlayerManagers;

private:

#if WITH_EDITOR
	FLevelEditorModule::FLevelEditorMenuExtender ViewMenuExtender;
#endif
};
