// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Templates/SubclassOf.h"
#include "DNATaskResource.generated.h"

struct FPropertyChangedEvent;

UCLASS(Abstract, config = "Game", hidedropdown)
class DNATASKS_API UDNATaskResource : public UObject
{
	GENERATED_BODY()

protected:
	/** Overrides AutoResourceID. -1 means auto ID will be applied */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Task Resource", meta = (ClampMin = "-1", ClampMax = "15", UIMin = "-1", UIMax = "15", EditCondition = "bManuallySetID"), config)
	int32 ManualResourceID;

private:
	UPROPERTY()
	int8 AutoResourceID;

public:
	UPROPERTY(EditDefaultsOnly, Category = "Task Resource", meta=(InlineEditConditionToggle))
	uint32 bManuallySetID : 1;

public:

	UDNATaskResource(const FObjectInitializer& ObjectInitializer);

	uint8 GetResourceID() const
	{
		return bManuallySetID || (ManualResourceID != INDEX_NONE) ? ManualResourceID : AutoResourceID;
	}

	template <class T>
	static uint8 GetResourceID()
	{
		return GetDefault<T>()->GetResourceID();
	}

	static uint8 GetResourceID(const TSubclassOf<UDNATaskResource>& RequiredResource)
	{
		return RequiredResource->GetDefaultObject<UDNATaskResource>()->GetResourceID();
	}

	virtual void PostInitProperties() override;

protected:
#if WITH_EDITOR
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
#endif // WITH_EDITOR

	void UpdateAutoResourceID();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
protected:
	static TArray<FString> ResourceDescriptions;
	virtual FString GenerateDebugDescription() const;

public:
	static FString GetDebugDescription(uint8 ResourceId)
	{
		return ResourceDescriptions.IsValidIndex(ResourceId) ? ResourceDescriptions[ResourceId] : FString();
	}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
};
