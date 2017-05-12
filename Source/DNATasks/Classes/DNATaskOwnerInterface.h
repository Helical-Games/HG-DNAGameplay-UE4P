// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"
#include "DNATaskTypes.h"
#include "DNATaskOwnerInterface.generated.h"

class AActor;
class UDNATask;
class UDNATasksComponent;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UDNATaskOwnerInterface : public UInterface
{
	GENERATED_BODY()
};

class DNATASKS_API IDNATaskOwnerInterface
{
	GENERATED_BODY()
public:

	/** Finds tasks component for given DNATask, Task.GetDNATasksComponent() may not be initialized at this point! */
	virtual UDNATasksComponent* GetDNATasksComponent(const UDNATask& Task) const PURE_VIRTUAL(IDNATaskOwnerInterface::GetDNATasksComponent, return nullptr;);

	/** Get owner of a task or default one when task is null */
	virtual AActor* GetDNATaskOwner(const UDNATask* Task) const PURE_VIRTUAL(IDNATaskOwnerInterface::GetDNATaskOwner, return nullptr;);

	/** Get "body" of task's owner / default, having location in world (e.g. Owner = AIController, Avatar = Pawn) */
	virtual AActor* GetDNATaskAvatar(const UDNATask* Task) const { return GetDNATaskOwner(Task); }

	/** Get default priority for running a task */
	virtual uint8 GetDNATaskDefaultPriority() const { return FDNATasks::DefaultPriority; }

	/** Notify called after DNATask finishes initialization (not active yet) */
	virtual void OnDNATaskInitialized(UDNATask& Task) {}
	
	/** Notify called after DNATask changes state to Active (initial activation or resuming) */
	virtual void OnDNATaskActivated(UDNATask& Task) {}

	/** Notify called after DNATask changes state from Active (finishing or pausing) */
	virtual void OnDNATaskDeactivated(UDNATask& Task) {}
};
