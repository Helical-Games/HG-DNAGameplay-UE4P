// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNATaskResource.h"
#include "DNATask.h"
#include "Modules/ModuleManager.h"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
TArray<FString> UDNATaskResource::ResourceDescriptions;
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

UDNATaskResource::UDNATaskResource(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bManuallySetID = false;
	ManualResourceID = INDEX_NONE;
	AutoResourceID = INDEX_NONE;
}

void UDNATaskResource::PostInitProperties()
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject) && (GetClass()->HasAnyClassFlags(CLASS_Abstract) == false)
#if WITH_HOT_RELOAD
		&& !GIsHotReload
#endif // WITH_HOT_RELOAD
		)
	{
		if (bManuallySetID == false || ManualResourceID == INDEX_NONE)
		{
			UpdateAutoResourceID();
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		const uint8 DebugId = GetResourceID();
		ResourceDescriptions.SetNum(FMath::Max(DebugId + 1, ResourceDescriptions.Num()));
		ResourceDescriptions[DebugId] = GenerateDebugDescription();
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	}
}

void UDNATaskResource::UpdateAutoResourceID()
{
	static uint16 NextAutoResID = 0;

	if (AutoResourceID == INDEX_NONE)
	{
		AutoResourceID = NextAutoResID++;
		if (AutoResourceID >= FDNAResourceSet::MaxResources)
		{
			UE_LOG(LogDNATasks, Error, TEXT("AutoResourceID out of bounds (probably too much DNATaskResource classes, consider manually assigning values if you can split all classes into non-overlapping sets"));
		}
	}
}

#if WITH_EDITOR
void UDNATaskResource::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	static const FName NAME_ManuallySetID = GET_MEMBER_NAME_CHECKED(UDNATaskResource, bManuallySetID);

	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property != NULL)
	{
		FName PropName = PropertyChangedEvent.Property->GetFName();

		if (PropName == NAME_ManuallySetID)
		{
			if (!bManuallySetID)
			{
				ManualResourceID = INDEX_NONE;
				// if we don't have ManualResourceID make sure AutoResourceID is valid
				UpdateAutoResourceID();
			}
		}
	}
}
#endif // WITH_EDITOR

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
FString UDNATaskResource::GenerateDebugDescription() const
{
	const FString ClassName = GetClass()->GetName();
	int32 SeparatorIdx = INDEX_NONE;
	if (ClassName.FindChar(TEXT('_'), SeparatorIdx))
	{
		return ClassName.Mid(SeparatorIdx + 1);
	}

	return ClassName;

}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
