// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Tasks/DNATask_ClaimResource.h"

UDNATask_ClaimResource::UDNATask_ClaimResource(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

UDNATask_ClaimResource* UDNATask_ClaimResource::ClaimResource(TScriptInterface<IDNATaskOwnerInterface> InTaskOwner, TSubclassOf<UDNATaskResource> ResourceClass, const uint8 Priority, const FName TaskInstanceName)
{
	return InTaskOwner.GetInterface() ? ClaimResource(*InTaskOwner, ResourceClass, Priority, TaskInstanceName) : nullptr;
}

UDNATask_ClaimResource* UDNATask_ClaimResource::ClaimResources(TScriptInterface<IDNATaskOwnerInterface> InTaskOwner, TArray<TSubclassOf<UDNATaskResource> > ResourceClasses, const uint8 Priority, const FName TaskInstanceName)
{
	return InTaskOwner.GetInterface() ? ClaimResources(*InTaskOwner, ResourceClasses, Priority, TaskInstanceName) : nullptr;
}

UDNATask_ClaimResource* UDNATask_ClaimResource::ClaimResource(IDNATaskOwnerInterface& InTaskOwner, const TSubclassOf<UDNATaskResource> ResourceClass, const uint8 Priority, const FName TaskInstanceName)
{
	if (!ResourceClass)
	{
		return nullptr;
	}

	UDNATask_ClaimResource* MyTask = NewTaskUninitialized<UDNATask_ClaimResource>();
	if (MyTask)
	{
		MyTask->InitTask(InTaskOwner, Priority);
		MyTask->InstanceName = TaskInstanceName;
		MyTask->AddClaimedResource(ResourceClass);
	}

	return MyTask;
}

UDNATask_ClaimResource* UDNATask_ClaimResource::ClaimResources(IDNATaskOwnerInterface& InTaskOwner, const TArray<TSubclassOf<UDNATaskResource> >& ResourceClasses, const uint8 Priority, const FName TaskInstanceName)
{
	if (ResourceClasses.Num() == 0)
	{
		return nullptr;
	}

	UDNATask_ClaimResource* MyTask = NewTaskUninitialized<UDNATask_ClaimResource>();
	if (MyTask)
	{
		MyTask->InitTask(InTaskOwner, Priority);
		MyTask->InstanceName = TaskInstanceName;
		for (const TSubclassOf<UDNATaskResource>& ResourceClass : ResourceClasses)
		{
			MyTask->AddClaimedResource(ResourceClass);
		}
	}

	return MyTask;
}
