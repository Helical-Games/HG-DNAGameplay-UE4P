// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "UObject/CoreNet.h"
#include "UObject/ScriptInterface.h"
#include "Components/ActorComponent.h"
#include "EngineDefines.h"
#include "DNATaskOwnerInterface.h"
#include "DNATask.h"
#include "DNATaskResource.h"
#include "DNATasksComponent.generated.h"

class AActor;
class Error;
class FOutBunch;
class UActorChannel;

enum class EDNATaskEvent : uint8
{
	Add,
	Remove,
};

UENUM()
enum class EDNATaskRunResult : uint8
{
	/** When tried running a null-task*/
	Error,
	Failed,
	/** Successfully registered for running, but currently paused due to higher priority tasks running */
	Success_Paused,
	/** Successfully activated */
	Success_Active,
	/** Successfully activated, but finished instantly */
	Success_Finished,
};

struct FDNATaskEventData
{
	EDNATaskEvent Event;
	UDNATask& RelatedTask;

	FDNATaskEventData(EDNATaskEvent InEvent, UDNATask& InRelatedTask)
		: Event(InEvent), RelatedTask(InRelatedTask)
	{

	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnClaimedResourcesChangeSignature, FDNAResourceSet, NewlyClaimed, FDNAResourceSet, FreshlyReleased);

typedef TArray<UDNATask*>::TConstIterator FConstDNATaskIterator;

/**
*	The core ActorComponent for interfacing with the DNAAbilities System
*/
UCLASS(ClassGroup = DNATasks, hidecategories = (Object, LOD, Lighting, Transform, Sockets, TextureStreaming), editinlinenew, meta = (BlueprintSpawnableComponent))
class DNATASKS_API UDNATasksComponent : public UActorComponent, public IDNATaskOwnerInterface
{
	GENERATED_BODY()

protected:
	/** Tasks that run on simulated proxies */
	UPROPERTY(ReplicatedUsing = OnRep_SimulatedTasks)
	TArray<UDNATask*> SimulatedTasks;

	UPROPERTY()
	TArray<UDNATask*> TaskPriorityQueue;
	
	/** Transient array of events whose main role is to avoid
	 *	long chain of recurrent calls if an activated/paused/removed task 
	 *	wants to push/pause/kill other tasks.
	 *	Note: this TaskEvents is assumed to be used in a single thread */
	TArray<FDNATaskEventData> TaskEvents;

	/** Array of currently active UDNATask that require ticking */
	UPROPERTY()
	TArray<UDNATask*> TickingTasks;

	/** Indicates what's the highest priority among currently running tasks */
	uint8 TopActivePriority;

	/** Resources used by currently active tasks */
	FDNAResourceSet CurrentlyClaimedResources;

public:
	UPROPERTY(BlueprintReadWrite, Category = "DNA Tasks")
	FOnClaimedResourcesChangeSignature OnClaimedResourcesChange;

	UDNATasksComponent(const FObjectInitializer& ObjectInitializer);
	
	UFUNCTION()
	void OnRep_SimulatedTasks();

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const override;
	virtual bool ReplicateSubobjects(UActorChannel *Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags) override;

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void UpdateShouldTick();

	/** retrieves information whether this component should be ticking taken current
	*	activity into consideration*/
	virtual bool GetShouldTick() const;
	
	/** processes the task and figures out if it should get triggered instantly or wait
	 *	based on task's RequiredResources, Priority and ResourceOverlapPolicy */
	void AddTaskReadyForActivation(UDNATask& NewTask);

	void RemoveResourceConsumingTask(UDNATask& Task);
	void EndAllResourceConsumingTasksOwnedBy(const IDNATaskOwnerInterface& TaskOwner);

	bool FindAllResourceConsumingTasksOwnedBy(const IDNATaskOwnerInterface& TaskOwner, TArray<UDNATask*>& FoundTasks) const;
	
	/** finds first resource-consuming task of given name */
	UDNATask* FindResourceConsumingTaskByName(const FName TaskInstanceName) const;

	bool HasActiveTasks(UClass* TaskClass) const;

	FORCEINLINE FDNAResourceSet GetCurrentlyUsedResources() const { return CurrentlyClaimedResources; }

	// BEGIN IDNATaskOwnerInterface
	virtual UDNATasksComponent* GetDNATasksComponent(const UDNATask& Task) const { return const_cast<UDNATasksComponent*>(this); }
	virtual AActor* GetDNATaskOwner(const UDNATask* Task) const override { return GetOwner(); }
	virtual AActor* GetDNATaskAvatar(const UDNATask* Task) const override { return GetOwner(); }
	virtual void OnDNATaskActivated(UDNATask& Task) override;
	virtual void OnDNATaskDeactivated(UDNATask& Task) override;
	// END IDNATaskOwnerInterface

	UFUNCTION(BlueprintCallable, DisplayName="Run DNA Task", Category = "DNA Tasks", meta = (AutoCreateRefTerm = "AdditionalRequiredResources, AdditionalClaimedResources", AdvancedDisplay = "AdditionalRequiredResources, AdditionalClaimedResources"))
	static EDNATaskRunResult K2_RunDNATask(TScriptInterface<IDNATaskOwnerInterface> TaskOwner, UDNATask* Task, uint8 Priority, TArray<TSubclassOf<UDNATaskResource> > AdditionalRequiredResources, TArray<TSubclassOf<UDNATaskResource> > AdditionalClaimedResources);

	static EDNATaskRunResult RunDNATask(IDNATaskOwnerInterface& TaskOwner, UDNATask& Task, uint8 Priority, FDNAResourceSet AdditionalRequiredResources, FDNAResourceSet AdditionalClaimedResources);
	
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	FString GetTickingTasksDescription() const;
	FString GetTasksPriorityQueueDescription() const;
	static FString GetTaskStateName(EDNATaskState Value);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	FConstDNATaskIterator GetTickingTaskIterator() const;
	FConstDNATaskIterator GetPriorityQueueIterator() const;

#if ENABLE_VISUAL_LOG
	void DescribeSelfToVisLog(struct FVisualLogEntry* Snapshot) const;
#endif // ENABLE_VISUAL_LOG

protected:
	struct FEventLock
	{
		FEventLock(UDNATasksComponent* InOwner);
		~FEventLock();

	protected:
		UDNATasksComponent* Owner;
	};

	void RequestTicking();
	void ProcessTaskEvents();
	void UpdateTaskActivations();

	void SetCurrentlyClaimedResources(FDNAResourceSet NewClaimedSet);

private:
	/** called when a task gets ended with an external call, meaning not coming from UDNATasksComponent mechanics */
	void OnTaskEnded(UDNATask& Task);

	void AddTaskToPriorityQueue(UDNATask& NewTask);
	void RemoveTaskFromPriorityQueue(UDNATask& Task);

	friend struct FEventLock;
	int32 EventLockCounter;
	uint32 bInEventProcessingInProgress : 1;

	FORCEINLINE bool CanProcessEvents() const { return !bInEventProcessingInProgress && (EventLockCounter == 0); }
};
