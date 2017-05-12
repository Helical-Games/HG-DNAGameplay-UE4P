// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNATask.h"
#include "UObject/Package.h"
#include "GameFramework/Actor.h"
#include "VisualLogger/VisualLogger.h"
#include "DNATaskResource.h"
#include "DNATasksComponent.h"

UDNATask::UDNATask(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = false;
	bSimulatedTask = false;
	bIsSimulating = false;
	bOwnedByTasksComponent = false;
	bClaimRequiredResources = true;
	bOwnerFinished = false;
	TaskState = EDNATaskState::Uninitialized;
	ResourceOverlapPolicy = ETaskResourceOverlapPolicy::StartOnTop;
	Priority = FDNATasks::DefaultPriority;

	SetFlags(RF_StrongRefOnFrame);
}

IDNATaskOwnerInterface* UDNATask::ConvertToTaskOwner(UObject& OwnerObject)
{
	IDNATaskOwnerInterface* OwnerInterface = Cast<IDNATaskOwnerInterface>(&OwnerObject);

	if (OwnerInterface == nullptr)
	{
		AActor* AsActor = Cast<AActor>(&OwnerObject);
		if (AsActor)
		{
			OwnerInterface = AsActor->FindComponentByClass<UDNATasksComponent>();
		}
	}
	return OwnerInterface;
}

IDNATaskOwnerInterface* UDNATask::ConvertToTaskOwner(AActor& OwnerActor)
{
	IDNATaskOwnerInterface* OwnerInterface = Cast<IDNATaskOwnerInterface>(&OwnerActor);

	if (OwnerInterface == nullptr)
	{
		OwnerInterface = OwnerActor.FindComponentByClass<UDNATasksComponent>();
	}
	return OwnerInterface;
}

void UDNATask::ReadyForActivation()
{
	if (TasksComponent.IsValid())
	{
		if (RequiresPriorityOrResourceManagement() == false)
		{
			PerformActivation();
		}
		else
		{
			TasksComponent->AddTaskReadyForActivation(*this);
		}
	}
	else
	{
		EndTask();
	}
}

void UDNATask::InitTask(IDNATaskOwnerInterface& InTaskOwner, uint8 InPriority)
{
	Priority = InPriority;
	TaskOwner = InTaskOwner;
	TaskState = EDNATaskState::AwaitingActivation;

	if (bClaimRequiredResources)
	{
		ClaimedResources.AddSet(RequiredResources);
	}

	// call owner.OnDNATaskInitialized before accessing owner.GetDNATasksComponent, this is required for child tasks
	InTaskOwner.OnDNATaskInitialized(*this);

	UDNATasksComponent* GTComponent = InTaskOwner.GetDNATasksComponent(*this);
	TasksComponent = GTComponent;
	bOwnedByTasksComponent = (TaskOwner == GTComponent);

	// make sure that task component knows about new task
	if (GTComponent && !bOwnedByTasksComponent)
	{
		GTComponent->OnDNATaskInitialized(*this);
	}
}

void UDNATask::InitSimulatedTask(UDNATasksComponent& InDNATasksComponent)
{
	TasksComponent = &InDNATasksComponent;
	bIsSimulating = true;
}

UWorld* UDNATask::GetWorld() const
{
	if (TasksComponent.IsValid())
	{
		return TasksComponent.Get()->GetWorld();
	}

	return nullptr;
}

AActor* UDNATask::GetOwnerActor() const
{
	if (TaskOwner.IsValid())
	{
		return TaskOwner->GetDNATaskOwner(this);		
	}
	else if (TasksComponent.IsValid())
	{
		return TasksComponent->GetDNATaskOwner(this);
	}

	return nullptr;
}

AActor* UDNATask::GetAvatarActor() const
{
	if (TaskOwner.IsValid())
	{
		return TaskOwner->GetDNATaskAvatar(this);
	}
	else if (TasksComponent.IsValid())
	{
		return TasksComponent->GetDNATaskAvatar(this);
	}

	return nullptr;
}

void UDNATask::TaskOwnerEnded()
{
	UE_VLOG(GetDNATasksComponent(), LogDNATasks, Verbose
		, TEXT("%s TaskOwnerEnded called, current State: %s")
		, *GetName(), *GetTaskStateName());

	if (TaskState != EDNATaskState::Finished && !IsPendingKill())
	{
		bOwnerFinished = true;
		OnDestroy(true);
	}
}

void UDNATask::EndTask()
{
	UE_VLOG(GetDNATasksComponent(), LogDNATasks, Verbose
		, TEXT("%s EndTask called, current State: %s")
		, *GetName(), *GetTaskStateName());

	if (TaskState != EDNATaskState::Finished && !IsPendingKill())
	{
		OnDestroy(false);
	}
}

void UDNATask::ExternalConfirm(bool bEndTask)
{
	UE_VLOG(GetDNATasksComponent(), LogDNATasks, Verbose
		, TEXT("%s ExternalConfirm called, bEndTask = %s, State : %s")
		, *GetName(), bEndTask ? TEXT("TRUE") : TEXT("FALSE"), *GetTaskStateName());

	if (bEndTask)
	{
		EndTask();
	}
}

void UDNATask::ExternalCancel()
{
	UE_VLOG(GetDNATasksComponent(), LogDNATasks, Verbose
		, TEXT("%s ExternalCancel called, current State: %s")
		, *GetName(), *GetTaskStateName());

	EndTask();
}

void UDNATask::OnDestroy(bool bInOwnerFinished)
{
	ensure(TaskState != EDNATaskState::Finished && !IsPendingKill());
	TaskState = EDNATaskState::Finished;

	if (TasksComponent.IsValid())
	{
		TasksComponent->OnDNATaskDeactivated(*this);
	}

	MarkPendingKill();
}

FString UDNATask::GetDebugString() const
{
	return FString::Printf(TEXT("%s (%s)"), *GetName(), *InstanceName.ToString());
}

void UDNATask::AddRequiredResource(TSubclassOf<UDNATaskResource> RequiredResource)
{
	check(RequiredResource);
	const uint8 ResourceID = UDNATaskResource::GetResourceID(RequiredResource);
	RequiredResources.AddID(ResourceID);	
}

void UDNATask::AddRequiredResourceSet(const TArray<TSubclassOf<UDNATaskResource> >& RequiredResourceSet)
{
	for (auto Resource : RequiredResourceSet)
	{
		if (Resource)
		{
			const uint8 ResourceID = UDNATaskResource::GetResourceID(Resource);
			RequiredResources.AddID(ResourceID);
		}
	}
}

void UDNATask::AddRequiredResourceSet(FDNAResourceSet RequiredResourceSet)
{
	RequiredResources.AddSet(RequiredResourceSet);
}

void UDNATask::AddClaimedResource(TSubclassOf<UDNATaskResource> ClaimedResource)
{
	check(ClaimedResource);
	const uint8 ResourceID = UDNATaskResource::GetResourceID(ClaimedResource);
	ClaimedResources.AddID(ResourceID);
}

void UDNATask::AddClaimedResourceSet(const TArray<TSubclassOf<UDNATaskResource> >& AdditionalResourcesToClaim)
{
	for (auto ResourceClass : AdditionalResourcesToClaim)
	{
		if (ResourceClass)
		{
			ClaimedResources.AddID(UDNATaskResource::GetResourceID(ResourceClass));
		}
	}
}

void UDNATask::AddClaimedResourceSet(FDNAResourceSet AdditionalResourcesToClaim)
{
	ClaimedResources.AddSet(AdditionalResourcesToClaim);
}

void UDNATask::PerformActivation()
{
	if (TaskState == EDNATaskState::Active)
	{
		UE_VLOG(GetDNATasksComponent(), LogDNATasks, Warning
			, TEXT("%s PerformActivation called while TaskState is already Active. Bailing out.")
			, *GetName());
		return;
	}

	TaskState = EDNATaskState::Active;

	Activate();

	TasksComponent->OnDNATaskActivated(*this);
}

void UDNATask::Activate()
{
	UE_VLOG(GetDNATasksComponent(), LogDNATasks, Verbose
		, TEXT("%s Activate called, current State: %s")
		, *GetName(), *GetTaskStateName());
}

void UDNATask::Pause()
{
	UE_VLOG(GetDNATasksComponent(), LogDNATasks, Verbose
		, TEXT("%s Pause called, current State: %s")
		, *GetName(), *GetTaskStateName());

	TaskState = EDNATaskState::Paused;

	TasksComponent->OnDNATaskDeactivated(*this);
}

void UDNATask::Resume()
{
	UE_VLOG(GetDNATasksComponent(), LogDNATasks, Verbose
		, TEXT("%s Resume called, current State: %s")
		, *GetName(), *GetTaskStateName());

	TaskState = EDNATaskState::Active;

	TasksComponent->OnDNATaskActivated(*this);
}

//----------------------------------------------------------------------//
// DNATasksComponent-related functions
//----------------------------------------------------------------------//
void UDNATask::ActivateInTaskQueue()
{
	switch(TaskState)
	{
	case EDNATaskState::Uninitialized:
		UE_VLOG(GetDNATasksComponent(), LogDNATasks, Error
			, TEXT("UDNATask::ActivateInTaskQueue Task %s passed for activation withouth having InitTask called on it!")
			, *GetName());
		break;
	case EDNATaskState::AwaitingActivation:
		PerformActivation();
		break;
	case EDNATaskState::Paused:
		// resume
		Resume();
		break;
	case EDNATaskState::Active:
		// nothing to do here
		break;
	case EDNATaskState::Finished:
		// If a task has finished, and it's being revived let's just treat the same as AwaitingActivation
		PerformActivation();
		break;
	default:
		checkNoEntry(); // looks like unhandled value! Probably a new enum entry has been added
		break;
	}
}

void UDNATask::PauseInTaskQueue()
{
	switch (TaskState)
	{
	case EDNATaskState::Uninitialized:
		UE_VLOG(GetDNATasksComponent(), LogDNATasks, Error
			, TEXT("UDNATask::PauseInTaskQueue Task %s passed for pausing withouth having InitTask called on it!")
			, *GetName());
		break;
	case EDNATaskState::AwaitingActivation:
		// nothing to do here. Don't change the state to indicate this task has never been run before
		break;
	case EDNATaskState::Paused:
		// nothing to do here. Already paused
		break;
	case EDNATaskState::Active:
		// pause!
		Pause();
		break;
	case EDNATaskState::Finished:
		// nothing to do here. But sounds odd, so let's log this, just in case
		UE_VLOG(GetDNATasksComponent(), LogDNATasks, Log
			, TEXT("UDNATask::PauseInTaskQueue Task %s being pause while already marked as Finished")
			, *GetName());
		break;
	default:
		checkNoEntry(); // looks like unhandled value! Probably a new enum entry has been added
		break;
	}
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
//----------------------------------------------------------------------//
// debug
//----------------------------------------------------------------------//
FString UDNATask::GenerateDebugDescription() const
{
	if (RequiresPriorityOrResourceManagement())
	{
		UObject* OwnerOb = Cast<UObject>(GetTaskOwner());
		return FString::Printf(TEXT("%s:%s Pri:%d Owner:%s Res:%s"),
			*GetName(), InstanceName != NAME_None ? *InstanceName.ToString() : TEXT("-"),
			(int32)Priority,
			*GetNameSafe(OwnerOb),
			*RequiredResources.GetDebugDescription());
	}

	return GetName();
}

FString UDNATask::GetTaskStateName() const
{
	static const UEnum* Enum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EDNATaskState"));
	check(Enum);
	return Enum->GetEnumName(int32(TaskState));
}

#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

//////////////////////////////////////////////////////////////////////////
// Child tasks

UDNATasksComponent* UDNATask::GetDNATasksComponent(const UDNATask& Task) const
{
	return ((&Task == ChildTask) || (&Task == this)) ? GetDNATasksComponent() : nullptr;
}

AActor* UDNATask::GetDNATaskOwner(const UDNATask* Task) const
{
	return ((Task == ChildTask) || (Task == this)) ? UDNATask::GetOwnerActor() : nullptr;
}

AActor* UDNATask::GetDNATaskAvatar(const UDNATask* Task) const
{
	return ((Task == ChildTask) || (Task == this)) ? UDNATask::GetAvatarActor() : nullptr;
}

uint8 UDNATask::GetDNATaskDefaultPriority() const
{
	return GetPriority();
}

void UDNATask::OnDNATaskDeactivated(UDNATask& Task)
{
	// cleanup after deactivation
	if (&Task == ChildTask)
	{
		UE_VLOG(GetDNATasksComponent(), LogDNATasks, Verbose, TEXT("%s> Child task deactivated: %s (state: %s)"), *GetName(), *Task.GetName(), *Task.GetTaskStateName());
		if (Task.IsFinished())
		{
			ChildTask = nullptr;
		}
	}
}

void UDNATask::OnDNATaskInitialized(UDNATask& Task)
{
	UE_VLOG(GetDNATasksComponent(), LogDNATasks, Verbose, TEXT("%s> Child task initialized: %s"), *GetName(), *Task.GetName());

	// only one child task is allowed
	if (ChildTask)
	{
		UE_VLOG(GetDNATasksComponent(), LogDNATasks, Verbose, TEXT(">> terminating previous child task: %s"), *ChildTask->GetName());
		ChildTask->EndTask();
	}

	ChildTask = &Task;
}
