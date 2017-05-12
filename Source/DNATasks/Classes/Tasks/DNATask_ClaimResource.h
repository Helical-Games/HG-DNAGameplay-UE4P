// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "Templates/SubclassOf.h"
#include "UObject/ScriptInterface.h"
#include "DNATaskOwnerInterface.h"
#include "DNATask.h"
#include "DNATaskResource.h"
#include "DNATask_ClaimResource.generated.h"

UCLASS(BlueprintType)
class DNATASKS_API UDNATask_ClaimResource : public UDNATask
{
	GENERATED_BODY()
public:
	UDNATask_ClaimResource(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "AI|Tasks", meta = (AdvancedDisplay = "Priority, TaskInstanceName"))
	static UDNATask_ClaimResource* ClaimResource(TScriptInterface<IDNATaskOwnerInterface> InTaskOwner, TSubclassOf<UDNATaskResource> ResourceClass, const uint8 Priority = 192, const FName TaskInstanceName = NAME_None);

	UFUNCTION(BlueprintCallable, Category = "AI|Tasks", meta = (AdvancedDisplay = "Priority, TaskInstanceName"))
	static UDNATask_ClaimResource* ClaimResources(TScriptInterface<IDNATaskOwnerInterface> InTaskOwner, TArray<TSubclassOf<UDNATaskResource> > ResourceClasses, const uint8 Priority = 192, const FName TaskInstanceName = NAME_None);

	static UDNATask_ClaimResource* ClaimResource(IDNATaskOwnerInterface& InTaskOwner, const TSubclassOf<UDNATaskResource> ResourceClass, const uint8 Priority = FDNATasks::DefaultPriority, const FName TaskInstanceName = NAME_None);
	static UDNATask_ClaimResource* ClaimResources(IDNATaskOwnerInterface& InTaskOwner, const TArray<TSubclassOf<UDNATaskResource> >& ResourceClasses, const uint8 Priority = FDNATasks::DefaultPriority, const FName TaskInstanceName = NAME_None);
};
