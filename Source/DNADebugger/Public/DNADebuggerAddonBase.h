// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "DNADebuggerTypes.h"

class AActor;
class ADNADebuggerCategoryReplicator;

class DNADEBUGGER_API FDNADebuggerAddonBase : public TSharedFromThis<FDNADebuggerAddonBase>
{
public:

	virtual ~FDNADebuggerAddonBase() {}

	int32 GetNumInputHandlers() const { return InputHandlers.Num(); }
	FDNADebuggerInputHandler& GetInputHandler(int32 HandlerId) { return InputHandlers[HandlerId]; }
	FString GetInputHandlerDescription(int32 HandlerId) const;

	/** [ALL] called when DNA debugger is activated */
	virtual void OnDNADebuggerActivated();

	/** [ALL] called when DNA debugger is deactivated */
	virtual void OnDNADebuggerDeactivated();

	/** check if simulate in editor mode is active */
	static bool IsSimulateInEditor();

protected:

	/** tries to find selected actor in local world */
	AActor* FindLocalDebugActor() const;

	/** returns replicator actor */
	ADNADebuggerCategoryReplicator* GetReplicator() const;

	/** creates new key binding handler: single key press */
	template<class UserClass>
	bool BindKeyPress(FName KeyName, UserClass* KeyHandlerObject, typename FDNADebuggerInputHandler::FHandler::TRawMethodDelegate< UserClass >::FMethodPtr KeyHandlerFunc, EDNADebuggerInputMode InputMode = EDNADebuggerInputMode::Local)
	{
		FDNADebuggerInputHandler NewHandler;
		NewHandler.KeyName = KeyName;
		NewHandler.Delegate.BindRaw(KeyHandlerObject, KeyHandlerFunc);
		NewHandler.Mode = InputMode;

		if (NewHandler.IsValid())
		{
			InputHandlers.Add(NewHandler);
			return true;
		}

		return false;
	}

	/** creates new key binding handler: key press with modifiers */
	template<class UserClass>
	bool BindKeyPress(FName KeyName, FDNADebuggerInputModifier KeyModifer, UserClass* KeyHandlerObject, typename FDNADebuggerInputHandler::FHandler::TRawMethodDelegate< UserClass >::FMethodPtr KeyHandlerFunc, EDNADebuggerInputMode InputMode = EDNADebuggerInputMode::Local)
	{
		FDNADebuggerInputHandler NewHandler;
		NewHandler.KeyName = KeyName;
		NewHandler.Modifier = KeyModifer;
		NewHandler.Delegate.BindRaw(KeyHandlerObject, KeyHandlerFunc);
		NewHandler.Mode = InputMode;

		if (NewHandler.IsValid())
		{
			InputHandlers.Add(NewHandler);
			return true;
		}

		return false;
	}

	/** creates new key binding handler: customizable key press, stored in config files */
	template<class UserClass>
	bool BindKeyPress(const FDNADebuggerInputHandlerConfig& InputConfig, UserClass* KeyHandlerObject, typename FDNADebuggerInputHandler::FHandler::TRawMethodDelegate< UserClass >::FMethodPtr KeyHandlerFunc, EDNADebuggerInputMode InputMode = EDNADebuggerInputMode::Local)
	{
		FDNADebuggerInputHandler NewHandler;
		NewHandler.KeyName = InputConfig.KeyName;
		NewHandler.Modifier = InputConfig.Modifier;
		NewHandler.Delegate.BindRaw(KeyHandlerObject, KeyHandlerFunc);
		NewHandler.Mode = InputMode;

		if (NewHandler.IsValid())
		{
			InputHandlers.Add(NewHandler);
			return true;
		}

		return false;
	}

private:

	friend class FDNADebuggerAddonManager;

	/** list of input handlers */
	TArray<FDNADebuggerInputHandler> InputHandlers;

	/** replicator actor */
	TWeakObjectPtr<ADNADebuggerCategoryReplicator> RepOwner;
};
