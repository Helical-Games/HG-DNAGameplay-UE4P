// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptInterface.h"
#include "DNATaskOwnerInterface.h"
#include "DNATask.h"
#include "DNATask_WaitDelay.generated.h"

UCLASS(MinimalAPI)
class UDNATask_WaitDelay : public UDNATask
{
	GENERATED_BODY()

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTaskDelayDelegate);

public:
	UDNATask_WaitDelay(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(BlueprintAssignable)
	FTaskDelayDelegate OnFinish;

	virtual void Activate() override;

	/** Return debug string describing task */
	virtual FString GetDebugString() const override;

	/** Wait specified time. This is functionally the same as a standard Delay node. */
	UFUNCTION(BlueprintCallable, Category = "DNATasks", meta = (AdvancedDisplay = "TaskOwner, Priority", DefaultToSelf = "TaskOwner", BlueprintInternalUseOnly = "TRUE"))
	static UDNATask_WaitDelay* TaskWaitDelay(TScriptInterface<IDNATaskOwnerInterface> TaskOwner, float Time, const uint8 Priority = 192);

	DNATASKS_API static UDNATask_WaitDelay* TaskWaitDelay(IDNATaskOwnerInterface& InTaskOwner, float Time, const uint8 Priority = FDNATasks::DefaultPriority);

private:

	void OnTimeFinish();
	
	float Time;
	float TimeStarted;
};
