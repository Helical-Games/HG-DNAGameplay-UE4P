// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNATasksComponent.h"
#include "UObject/Package.h"
#include "Net/UnrealNetwork.h"
#include "Engine/ActorChannel.h"
#include "GameFramework/Actor.h"
#include "VisualLogger/VisualLoggerTypes.h"
#include "VisualLogger/VisualLogger.h"
#include "DNATasksPrivate.h"
#include "Logging/MessageLog.h"

#define LOCTEXT_NAMESPACE "DNATasksComponent"

namespace
{
	FORCEINLINE const TCHAR* GetDNATaskEventName(EDNATaskEvent Event)
	{
		/*static const UEnum* DNATaskEventEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EDNATaskEvent"));
		return DNATaskEventEnum->GetEnumText(static_cast<int32>(Event)).ToString();*/

		return Event == EDNATaskEvent::Add ? TEXT("Add") : TEXT("Remove");
	}
}

UDNATasksComponent::FEventLock::FEventLock(UDNATasksComponent* InOwner) : Owner(InOwner)
{
	if (Owner)
	{
		Owner->EventLockCounter++;
	}
}

UDNATasksComponent::FEventLock::~FEventLock()
{
	if (Owner)
	{
		Owner->EventLockCounter--;

		if (Owner->TaskEvents.Num() && Owner->CanProcessEvents())
		{
			Owner->ProcessTaskEvents();
		}
	}
}

UDNATasksComponent::UDNATasksComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.TickGroup = TG_DuringPhysics;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick = true;

	bReplicates = true;
	bInEventProcessingInProgress = false;
	TopActivePriority = 0;
}
	
void UDNATasksComponent::OnDNATaskActivated(UDNATask& Task)
{
	// process events after finishing all operations
	FEventLock ScopeEventLock(this);

	if (Task.IsTickingTask())
	{
		check(TickingTasks.Contains(&Task) == false);
		TickingTasks.Add(&Task);

		// If this is our first ticking task, set this component as active so it begins ticking
		if (TickingTasks.Num() == 1)
		{
			UpdateShouldTick();
		}
	}
	if (Task.IsSimulatedTask())
	{
		check(SimulatedTasks.Contains(&Task) == false);
		SimulatedTasks.Add(&Task);
	}

	IDNATaskOwnerInterface* TaskOwner = Task.GetTaskOwner();
	if (!Task.IsOwnedByTasksComponent() && TaskOwner)
	{
		TaskOwner->OnDNATaskActivated(Task);
	}
}

void UDNATasksComponent::OnDNATaskDeactivated(UDNATask& Task)
{
	// process events after finishing all operations
	FEventLock ScopeEventLock(this);
	const bool bIsFinished = (Task.GetState() == EDNATaskState::Finished);

	if (Task.GetChildTask() && bIsFinished)
	{
		if (Task.HasOwnerFinished())
		{
			Task.GetChildTask()->TaskOwnerEnded();
		}
		else
		{
			Task.GetChildTask()->EndTask();
		}
	}

	if (Task.IsTickingTask())
	{
		// If we are removing our last ticking task, set this component as inactive so it stops ticking
		TickingTasks.RemoveSingleSwap(&Task);
	}

	if (Task.IsSimulatedTask())
	{
		SimulatedTasks.RemoveSingleSwap(&Task);
	}

	// Resource-using task
	if (Task.RequiresPriorityOrResourceManagement() && bIsFinished)
	{
		OnTaskEnded(Task);
	}

	IDNATaskOwnerInterface* TaskOwner = Task.GetTaskOwner();
	if (!Task.IsOwnedByTasksComponent() && !Task.HasOwnerFinished() && TaskOwner)
	{
		TaskOwner->OnDNATaskDeactivated(Task);
	}

	UpdateShouldTick();
}

void UDNATasksComponent::OnTaskEnded(UDNATask& Task)
{
	ensure(Task.RequiresPriorityOrResourceManagement() == true);
	RemoveResourceConsumingTask(Task);
}

void UDNATasksComponent::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	// Intentionally not calling super: We do not want to replicate bActive which controls ticking. We sometimes need to tick on client predictively.
	DOREPLIFETIME_CONDITION(UDNATasksComponent, SimulatedTasks, COND_SkipOwner);
}

bool UDNATasksComponent::ReplicateSubobjects(UActorChannel* Channel, class FOutBunch *Bunch, FReplicationFlags *RepFlags)
{
	bool WroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);
	
	if (!RepFlags->bNetOwner)
	{
		for (UDNATask* SimulatedTask : SimulatedTasks)
		{
			if (SimulatedTask && !SimulatedTask->IsPendingKill())
			{
				WroteSomething |= Channel->ReplicateSubobject(SimulatedTask, *Bunch, *RepFlags);
			}
		}
	}

	return WroteSomething;
}

void UDNATasksComponent::OnRep_SimulatedTasks()
{
	for (UDNATask* SimulatedTask : SimulatedTasks)
	{
		// Temp check 
		if (SimulatedTask && SimulatedTask->IsTickingTask() && TickingTasks.Contains(SimulatedTask) == false)
		{
			SimulatedTask->InitSimulatedTask(*this);
			if (TickingTasks.Num() == 0)
			{
				UpdateShouldTick();
			}

			TickingTasks.Add(SimulatedTask);
		}
	}
}

void UDNATasksComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	SCOPE_CYCLE_COUNTER(STAT_TickDNATasks);

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Because we have no control over what a task may do when it ticks, we must be careful.
	// Ticking a task may kill the task right here. It could also potentially kill another task
	// which was waiting on the original task to do something. Since when a tasks is killed, it removes
	// itself from the TickingTask list, we will make a copy of the tasks we want to service before ticking any

	int32 NumTickingTasks = TickingTasks.Num();
	int32 NumActuallyTicked = 0;
	switch (NumTickingTasks)
	{
	case 0:
		break;
	case 1:
		if (TickingTasks[0])
		{
			TickingTasks[0]->TickTask(DeltaTime);
			NumActuallyTicked++;
		}
		break;
	default:
	{

		static TArray<UDNATask*> LocalTickingTasks;
		LocalTickingTasks.Reset();
		LocalTickingTasks.Append(TickingTasks);
		for (UDNATask* TickingTask : LocalTickingTasks)
		{
			if (TickingTask)
			{
				TickingTask->TickTask(DeltaTime);
				NumActuallyTicked++;
			}
		}
	}
		break;
	};

	// Stop ticking if no more active tasks
	if (NumActuallyTicked == 0)
	{
		TickingTasks.SetNum(0, false);
		UpdateShouldTick();
	}
}

bool UDNATasksComponent::GetShouldTick() const
{
	return TickingTasks.Num() > 0;
}

void UDNATasksComponent::RequestTicking()
{
	if (bIsActive == false)
	{
		SetActive(true);
	}
}

void UDNATasksComponent::UpdateShouldTick()
{
	const bool bShouldTick = GetShouldTick();	
	if (bIsActive != bShouldTick)
	{
		SetActive(bShouldTick);
	}
}

//----------------------------------------------------------------------//
// Priority and resources handling
//----------------------------------------------------------------------//
void UDNATasksComponent::AddTaskReadyForActivation(UDNATask& NewTask)
{
	UE_VLOG(this, LogDNATasks, Log, TEXT("AddTaskReadyForActivation %s"), *NewTask.GetName());

	ensure(NewTask.RequiresPriorityOrResourceManagement() == true);
	
	TaskEvents.Add(FDNATaskEventData(EDNATaskEvent::Add, NewTask));
	// trigger the actual processing only if it was the first event added to the list
	if (TaskEvents.Num() == 1 && CanProcessEvents())
	{
		ProcessTaskEvents();
	}
}

void UDNATasksComponent::RemoveResourceConsumingTask(UDNATask& Task)
{
	UE_VLOG(this, LogDNATasks, Log, TEXT("RemoveResourceConsumingTask %s"), *Task.GetName());

	TaskEvents.Add(FDNATaskEventData(EDNATaskEvent::Remove, Task));
	// trigger the actual processing only if it was the first event added to the list
	if (TaskEvents.Num() == 1 && CanProcessEvents())
	{
		ProcessTaskEvents();
	}
}

void UDNATasksComponent::EndAllResourceConsumingTasksOwnedBy(const IDNATaskOwnerInterface& TaskOwner)
{
	FEventLock ScopeEventLock(this);

	for (int32 Idx = 0; Idx < TaskPriorityQueue.Num(); Idx++)
	{
		if (TaskPriorityQueue[Idx] && TaskPriorityQueue[Idx]->GetTaskOwner() == &TaskOwner)
		{
			// finish task, remove event will be processed after all locks are cleared
			TaskPriorityQueue[Idx]->TaskOwnerEnded();
		}
	}
}

bool UDNATasksComponent::FindAllResourceConsumingTasksOwnedBy(const IDNATaskOwnerInterface& TaskOwner, TArray<UDNATask*>& FoundTasks) const
{
	int32 NumFound = 0;
	for (int32 TaskIndex = 0; TaskIndex < TaskPriorityQueue.Num(); TaskIndex++)
	{
		if (TaskPriorityQueue[TaskIndex] && TaskPriorityQueue[TaskIndex]->GetTaskOwner() == &TaskOwner)
		{
			FoundTasks.Add(TaskPriorityQueue[TaskIndex]);
			NumFound++;
		}
	}

	return (NumFound > 0);
}

UDNATask* UDNATasksComponent::FindResourceConsumingTaskByName(const FName TaskInstanceName) const
{
	for (int32 TaskIndex = 0; TaskIndex < TaskPriorityQueue.Num(); TaskIndex++)
	{
		if (TaskPriorityQueue[TaskIndex] && TaskPriorityQueue[TaskIndex]->GetInstanceName() == TaskInstanceName)
		{
			return TaskPriorityQueue[TaskIndex];
		}
	}

	return nullptr;
}

bool UDNATasksComponent::HasActiveTasks(UClass* TaskClass) const
{
	for (int32 Idx = 0; Idx < TaskPriorityQueue.Num(); Idx++)
	{
		if (TaskPriorityQueue[Idx] && TaskPriorityQueue[Idx]->IsA(TaskClass))
		{
			return true;
		}
	}

	for (int32 Idx = 0; Idx < TickingTasks.Num(); Idx++)
	{
		if (TickingTasks[Idx] && TickingTasks[Idx]->IsA(TaskClass))
		{
			return true;
		}
	}

	return false;
}

void UDNATasksComponent::ProcessTaskEvents()
{
	static const int32 MaxIterations = 16;
	bInEventProcessingInProgress = true;

	int32 IterCounter = 0;
	while (TaskEvents.Num() > 0)
	{
		IterCounter++;
		if (IterCounter > MaxIterations)
		{
			UE_VLOG(this, LogDNATasks, Error, TEXT("UDNATasksComponent::ProcessTaskEvents has exceeded allowes number of iterations. Check your DNATasks for logic loops!"));
			TaskEvents.Reset();
			break;
		}

		for (int32 EventIndex = 0; EventIndex < TaskEvents.Num(); ++EventIndex)
		{
			UE_VLOG(this, LogDNATasks, Verbose, TEXT("UDNATasksComponent::ProcessTaskEvents: %s event %s")
				, *TaskEvents[EventIndex].RelatedTask.GetName(), GetDNATaskEventName(TaskEvents[EventIndex].Event));

			if (TaskEvents[EventIndex].RelatedTask.IsPendingKill())
			{
				UE_VLOG(this, LogDNATasks, Verbose, TEXT("%s is PendingKill"), *TaskEvents[EventIndex].RelatedTask.GetName());
				// we should ignore it, but just in case run the removal code.
				RemoveTaskFromPriorityQueue(TaskEvents[EventIndex].RelatedTask);
				continue;
			}

			switch (TaskEvents[EventIndex].Event)
			{
			case EDNATaskEvent::Add:
				if (TaskEvents[EventIndex].RelatedTask.TaskState != EDNATaskState::Finished)
				{
					AddTaskToPriorityQueue(TaskEvents[EventIndex].RelatedTask);
				}
				else
				{
					UE_VLOG(this, LogDNATasks, Error, TEXT("UDNATasksComponent::ProcessTaskEvents trying to add a finished task to priority queue!"));
				}
				break;
			case EDNATaskEvent::Remove:
				RemoveTaskFromPriorityQueue(TaskEvents[EventIndex].RelatedTask);
				break;
			default:
				checkNoEntry();
				break;
			}
		}

		TaskEvents.Reset();
		UpdateTaskActivations();

		// task activation changes may create new events, loop over to check it
	}

	bInEventProcessingInProgress = false;
}

void UDNATasksComponent::AddTaskToPriorityQueue(UDNATask& NewTask)
{
	const bool bStartOnTopOfSamePriority = (NewTask.GetResourceOverlapPolicy() == ETaskResourceOverlapPolicy::StartOnTop);
	int32 InsertionPoint = INDEX_NONE;
	
	for (int32 Idx = 0; Idx < TaskPriorityQueue.Num(); ++Idx)
	{
		if (TaskPriorityQueue[Idx] == nullptr)
		{
			continue;
		}

		if ((bStartOnTopOfSamePriority && TaskPriorityQueue[Idx]->GetPriority() <= NewTask.GetPriority())
			|| (!bStartOnTopOfSamePriority && TaskPriorityQueue[Idx]->GetPriority() < NewTask.GetPriority()))
		{
			TaskPriorityQueue.Insert(&NewTask, Idx);
			InsertionPoint = Idx;
			break;
		}
	}
	
	if (InsertionPoint == INDEX_NONE)
	{
		TaskPriorityQueue.Add(&NewTask);
	}
}

void UDNATasksComponent::RemoveTaskFromPriorityQueue(UDNATask& Task)
{	
	const int32 RemovedTaskIndex = TaskPriorityQueue.Find(&Task);
	if (RemovedTaskIndex != INDEX_NONE)
	{
		TaskPriorityQueue.RemoveAt(RemovedTaskIndex, 1, /*bAllowShrinking=*/false);
	}
	else
	{
		// take a note and ignore
		UE_VLOG(this, LogDNATasks, Verbose, TEXT("RemoveTaskFromPriorityQueue for %s called, but it's not in the queue. Might have been already removed"), *Task.GetName());
	}
}

void UDNATasksComponent::UpdateTaskActivations()
{
	FDNAResourceSet ResourcesClaimed;
	bool bHasNulls = false;

	if (TaskPriorityQueue.Num() > 0)
	{
		TArray<UDNATask*> ActivationList;
		ActivationList.Reserve(TaskPriorityQueue.Num());

		FDNAResourceSet ResourcesBlocked;
		for (int32 TaskIndex = 0; TaskIndex < TaskPriorityQueue.Num(); ++TaskIndex)
		{
			if (TaskPriorityQueue[TaskIndex])
			{
				const FDNAResourceSet RequiredResources = TaskPriorityQueue[TaskIndex]->GetRequiredResources();
				const FDNAResourceSet ClaimedResources = TaskPriorityQueue[TaskIndex]->GetClaimedResources();
				if (RequiredResources.GetOverlap(ResourcesBlocked).IsEmpty())
				{
					// postpone activations, it's some tasks (like MoveTo) require pausing old ones first
					ActivationList.Add(TaskPriorityQueue[TaskIndex]);
					ResourcesClaimed.AddSet(ClaimedResources);
				}
				else
				{
					TaskPriorityQueue[TaskIndex]->PauseInTaskQueue();
				}

				ResourcesBlocked.AddSet(ClaimedResources);
			}
			else
			{
				bHasNulls = true;

				UE_VLOG(this, LogDNATasks, Warning, TEXT("UpdateTaskActivations found null entry in task queue at index:%d!"), TaskIndex);
			}
		}

		for (int32 Idx = 0; Idx < ActivationList.Num(); Idx++)
		{
			// check if task wasn't already finished as a result of activating previous elements of this list
			if (ActivationList[Idx] && !ActivationList[Idx]->IsFinished())
			{
				ActivationList[Idx]->ActivateInTaskQueue();
			}
		}
	}
	
	SetCurrentlyClaimedResources(ResourcesClaimed);

	// remove all null entries after processing activation changes
	if (bHasNulls)
	{
		TaskPriorityQueue.RemoveAll([](UDNATask* Task) { return Task == nullptr; });
	}
}

void UDNATasksComponent::SetCurrentlyClaimedResources(FDNAResourceSet NewClaimedSet)
{
	if (CurrentlyClaimedResources != NewClaimedSet)
	{
		FDNAResourceSet ReleasedResources = FDNAResourceSet(CurrentlyClaimedResources).RemoveSet(NewClaimedSet);
		FDNAResourceSet ClaimedResources = FDNAResourceSet(NewClaimedSet).RemoveSet(CurrentlyClaimedResources);
		CurrentlyClaimedResources = NewClaimedSet;
		OnClaimedResourcesChange.Broadcast(ClaimedResources, ReleasedResources);
	}
}

//----------------------------------------------------------------------//
// debugging
//----------------------------------------------------------------------//
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
FString UDNATasksComponent::GetTickingTasksDescription() const
{
	FString TasksDescription;
	for (auto& Task : TickingTasks)
	{
		if (Task)
		{
			TasksDescription += FString::Printf(TEXT("\n%s %s"), *GetTaskStateName(Task->GetState()), *Task->GetDebugDescription());
		}
		else
		{
			TasksDescription += TEXT("\nNULL");
		}
	}
	return TasksDescription;
}

FString UDNATasksComponent::GetTasksPriorityQueueDescription() const
{
	FString TasksDescription;
	for (auto Task : TaskPriorityQueue)
	{
		if (Task != nullptr)
		{
			TasksDescription += FString::Printf(TEXT("\n%s %s"), *GetTaskStateName(Task->GetState()), *Task->GetDebugDescription());
		}
		else
		{
			TasksDescription += TEXT("\nNULL");
		}
	}
	return TasksDescription;
}

FString UDNATasksComponent::GetTaskStateName(EDNATaskState Value)
{
	static const UEnum* Enum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EDNATaskState"));
	check(Enum);
	return Enum->GetEnumName(int32(Value));
}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

FConstDNATaskIterator UDNATasksComponent::GetTickingTaskIterator() const
{
	return TickingTasks.CreateConstIterator();
}

FConstDNATaskIterator UDNATasksComponent::GetPriorityQueueIterator() const
{
	return TaskPriorityQueue.CreateConstIterator();
}

#if ENABLE_VISUAL_LOG
void UDNATasksComponent::DescribeSelfToVisLog(FVisualLogEntry* Snapshot) const
{
	static const FString CategoryName = TEXT("DNATasks");
	static const FString TickingTasksName = TEXT("Ticking tasks");
	static const FString PriorityQueueName = TEXT("Priority Queue");

	if (IsPendingKill())
	{
		return;
	}

	FVisualLogStatusCategory StatusCategory(CategoryName);

	StatusCategory.Add(TickingTasksName, GetTickingTasksDescription());
	StatusCategory.Add(PriorityQueueName, GetTasksPriorityQueueDescription());

	Snapshot->Status.Add(StatusCategory);
}
#endif // ENABLE_VISUAL_LOG

EDNATaskRunResult UDNATasksComponent::RunDNATask(IDNATaskOwnerInterface& TaskOwner, UDNATask& Task, uint8 Priority, FDNAResourceSet AdditionalRequiredResources, FDNAResourceSet AdditionalClaimedResources)
{
	const FText NoneText = FText::FromString(TEXT("None"));

	if (Task.GetState() == EDNATaskState::Paused || Task.GetState() == EDNATaskState::Active)
	{
		// return as success if already running for the same owner, failure otherwise 
		return Task.GetTaskOwner() == &TaskOwner
			? (Task.GetState() == EDNATaskState::Paused ? EDNATaskRunResult::Success_Paused : EDNATaskRunResult::Success_Active)
			: EDNATaskRunResult::Error;
	}

	// this is a valid situation if the task has been created via "Construct Object" mechanics
	if (Task.GetState() == EDNATaskState::Uninitialized)
	{
		Task.InitTask(TaskOwner, Priority);
	}

	Task.AddRequiredResourceSet(AdditionalRequiredResources);
	Task.AddClaimedResourceSet(AdditionalClaimedResources);
	Task.ReadyForActivation();

	switch (Task.GetState())
	{
	case EDNATaskState::AwaitingActivation:
	case EDNATaskState::Paused:
		return EDNATaskRunResult::Success_Paused;
		break;
	case EDNATaskState::Active:
		return EDNATaskRunResult::Success_Active;
		break;
	case EDNATaskState::Finished:
		return EDNATaskRunResult::Success_Active;
		break;
	}

	return EDNATaskRunResult::Error;
}

//----------------------------------------------------------------------//
// BP API
//----------------------------------------------------------------------//
EDNATaskRunResult UDNATasksComponent::K2_RunDNATask(TScriptInterface<IDNATaskOwnerInterface> TaskOwner, UDNATask* Task, uint8 Priority, TArray<TSubclassOf<UDNATaskResource> > AdditionalRequiredResources, TArray<TSubclassOf<UDNATaskResource> > AdditionalClaimedResources)
{
	const FText NoneText = FText::FromString(TEXT("None"));

	if (TaskOwner.GetInterface() == nullptr)
	{
		FMessageLog("PIE").Error(FText::Format(
			LOCTEXT("RunDNATaskNullOwner", "Tried running a DNA task {0} while owner is None!"),
			Task ? FText::FromName(Task->GetFName()) : NoneText));
		return EDNATaskRunResult::Error;
	}

	IDNATaskOwnerInterface& OwnerInstance = *TaskOwner;

	if (Task == nullptr)
	{
		FMessageLog("PIE").Error(FText::Format(
			LOCTEXT("RunNullDNATask", "Tried running a None task for {0}"),
			FText::FromString(Cast<UObject>(&OwnerInstance)->GetName())
			));
		return EDNATaskRunResult::Error;
	}

	if (Task->GetState() == EDNATaskState::Paused || Task->GetState() == EDNATaskState::Active)
	{
		FMessageLog("PIE").Warning(FText::Format(
			LOCTEXT("RunNullDNATask", "Tried running a None task for {0}"),
			FText::FromString(Cast<UObject>(&OwnerInstance)->GetName())
			));
		// return as success if already running for the same owner, failure otherwise 
		return Task->GetTaskOwner() == &OwnerInstance 
			? (Task->GetState() == EDNATaskState::Paused ? EDNATaskRunResult::Success_Paused : EDNATaskRunResult::Success_Active)
			: EDNATaskRunResult::Error;
	}

	// this is a valid situation if the task has been created via "Construct Object" mechanics
	if (Task->GetState() == EDNATaskState::Uninitialized)
	{
		Task->InitTask(OwnerInstance, Priority);
	}

	Task->AddRequiredResourceSet(AdditionalRequiredResources);
	Task->AddClaimedResourceSet(AdditionalClaimedResources);
	Task->ReadyForActivation();

	switch (Task->GetState())
	{
	case EDNATaskState::AwaitingActivation:
	case EDNATaskState::Paused:
		return EDNATaskRunResult::Success_Paused;
		break;
	case EDNATaskState::Active:
		return EDNATaskRunResult::Success_Active;
		break;
	case EDNATaskState::Finished:
		return EDNATaskRunResult::Success_Active;
		break;
	}

	return EDNATaskRunResult::Error;
}

//----------------------------------------------------------------------//
// FDNAResourceSet
//----------------------------------------------------------------------//
FString FDNAResourceSet::GetDebugDescription() const
{
	static const int32 FlagsCount = sizeof(FFlagContainer) * 8;
	FFlagContainer FlagsCopy = Flags;
	int32 FlagIndex = 0;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	FString Description;
	for (; FlagIndex < FlagsCount && FlagsCopy != 0; ++FlagIndex)
	{
		if (FlagsCopy & (1 << FlagIndex))
		{
			Description += UDNATaskResource::GetDebugDescription(FlagIndex);
			Description += TEXT(' ');
		}

		FlagsCopy &= ~(1 << FlagIndex);
	}
	return Description;
#else
	TCHAR Description[FlagsCount + 1];
	for (; FlagIndex < FlagsCount && FlagsCopy != 0; ++FlagIndex)
	{
		Description[FlagIndex] = (FlagsCopy & (1 << FlagIndex)) ? TCHAR('1') : TCHAR('0');
		FlagsCopy &= ~(1 << FlagIndex);
	}
	Description[FlagIndex] = TCHAR('\0');
	return FString(Description);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

#undef LOCTEXT_NAMESPACE
