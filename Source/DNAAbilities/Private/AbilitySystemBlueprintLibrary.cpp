// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "DNAEffectAggregator.h"
#include "DNAEffect.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"

UDNADNAAbilitySystemBlueprintLibrary::UDNADNAAbilitySystemBlueprintLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

UDNADNAAbilitySystemComponent* UDNADNAAbilitySystemBlueprintLibrary::GetDNADNAAbilitySystemComponent(AActor *Actor)
{
	return UDNADNAAbilitySystemGlobals::GetDNADNAAbilitySystemComponentFromActor(Actor);
}

void UDNADNAAbilitySystemBlueprintLibrary::SendDNAEventToActor(AActor* Actor, FDNATag EventTag, FDNAEventData Payload)
{
	if (Actor && !Actor->IsPendingKill())
	{
		IDNADNAAbilitySystemInterface* DNADNAAbilitySystemInterface = Cast<IDNADNAAbilitySystemInterface>(Actor);
		if (DNADNAAbilitySystemInterface != NULL)
		{
			UDNADNAAbilitySystemComponent* DNADNAAbilitySystemComponent = DNADNAAbilitySystemInterface->GetDNADNAAbilitySystemComponent();
			if (DNADNAAbilitySystemComponent != NULL)
			{
				FScopedPredictionWindow NewScopedWindow(DNADNAAbilitySystemComponent, true);
				DNADNAAbilitySystemComponent->HandleDNAEvent(EventTag, &Payload);
			}
		}
	}
}

bool UDNADNAAbilitySystemBlueprintLibrary::IsValid(FDNAAttribute Attribute)
{
	return Attribute.IsValid();
}

float UDNADNAAbilitySystemBlueprintLibrary::GetFloatAttribute(const class AActor* Actor, FDNAAttribute Attribute, bool& bSuccessfullyFoundAttribute)
{
	const UDNADNAAbilitySystemComponent* const DNADNAAbilitySystem = UDNADNAAbilitySystemGlobals::GetDNADNAAbilitySystemComponentFromActor(Actor);

	return GetFloatAttributeFromDNADNAAbilitySystemComponent(DNADNAAbilitySystem, Attribute, bSuccessfullyFoundAttribute);
}

float UDNADNAAbilitySystemBlueprintLibrary::GetFloatAttributeFromDNADNAAbilitySystemComponent(const class UDNADNAAbilitySystemComponent* DNADNAAbilitySystem, FDNAAttribute Attribute, bool& bSuccessfullyFoundAttribute)
{
	bSuccessfullyFoundAttribute = true;

	if (!DNADNAAbilitySystem || !DNADNAAbilitySystem->HasAttributeSetForAttribute(Attribute))
	{
		bSuccessfullyFoundAttribute = false;
		return 0.f;
	}

	const float Result = DNADNAAbilitySystem->GetNumericAttribute(Attribute);
	return Result;
}

float UDNADNAAbilitySystemBlueprintLibrary::GetFloatAttributeBase(const AActor* Actor, FDNAAttribute Attribute, bool& bSuccessfullyFoundAttribute)
{
	const UDNADNAAbilitySystemComponent* const DNADNAAbilitySystem = UDNADNAAbilitySystemGlobals::GetDNADNAAbilitySystemComponentFromActor(Actor);
	return GetFloatAttributeBaseFromDNADNAAbilitySystemComponent(DNADNAAbilitySystem, Attribute, bSuccessfullyFoundAttribute);
}

float UDNADNAAbilitySystemBlueprintLibrary::GetFloatAttributeBaseFromDNADNAAbilitySystemComponent(const UDNADNAAbilitySystemComponent* DNADNAAbilitySystemComponent, FDNAAttribute Attribute, bool& bSuccessfullyFoundAttribute)
{
	float Result = 0.f;
	bSuccessfullyFoundAttribute = false;

	if (DNADNAAbilitySystemComponent && DNADNAAbilitySystemComponent->HasAttributeSetForAttribute(Attribute))
	{
		bSuccessfullyFoundAttribute = true;
		Result = DNADNAAbilitySystemComponent->GetNumericAttributeBase(Attribute);
	}

	return Result;
}

float UDNADNAAbilitySystemBlueprintLibrary::EvaluateAttributeValueWithTags(UDNADNAAbilitySystemComponent* DNADNAAbilitySystem, FDNAAttribute Attribute, const FDNATagContainer& SourceTags, const FDNATagContainer& TargetTags, bool& bSuccess)
{
	float RetVal = 0.f;
	if (!DNADNAAbilitySystem || !DNADNAAbilitySystem->HasAttributeSetForAttribute(Attribute))
	{
		bSuccess = false;
		return RetVal;
	}

	FDNAEffectAttributeCaptureDefinition Capture(Attribute, EDNAEffectAttributeCaptureSource::Source, true);

	FDNAEffectAttributeCaptureSpec CaptureSpec(Capture);
	DNADNAAbilitySystem->CaptureAttributeForDNAEffect(CaptureSpec);

	FAggregatorEvaluateParameters EvalParams;

	EvalParams.SourceTags = &SourceTags;
	EvalParams.TargetTags = &TargetTags;

	bSuccess = CaptureSpec.AttemptCalculateAttributeMagnitude(EvalParams, RetVal);

	return RetVal;
}

float UDNADNAAbilitySystemBlueprintLibrary::EvaluateAttributeValueWithTagsAndBase(UDNADNAAbilitySystemComponent* DNADNAAbilitySystem, FDNAAttribute Attribute, const FDNATagContainer& SourceTags, const FDNATagContainer& TargetTags, float BaseValue, bool& bSuccess)
{
	float RetVal = 0.f;
	if (!DNAAbilitySystem || !DNAAbilitySystem->HasAttributeSetForAttribute(Attribute))
	{
		bSuccess = false;
		return RetVal;
	}

	FDNAEffectAttributeCaptureDefinition Capture(Attribute, EDNAEffectAttributeCaptureSource::Source, true);

	FDNAEffectAttributeCaptureSpec CaptureSpec(Capture);
	DNAAbilitySystem->CaptureAttributeForDNAEffect(CaptureSpec);

	FAggregatorEvaluateParameters EvalParams;

	EvalParams.SourceTags = &SourceTags;
	EvalParams.TargetTags = &TargetTags;

	bSuccess = CaptureSpec.AttemptCalculateAttributeMagnitudeWithBase(EvalParams, BaseValue, RetVal);

	return RetVal;
}

bool UDNAAbilitySystemBlueprintLibrary::EqualEqual_DNAAttributeDNAAttribute(FDNAAttribute AttributeA, FDNAAttribute AttributeB)
{
	return (AttributeA == AttributeB);
}

bool UDNAAbilitySystemBlueprintLibrary::NotEqual_DNAAttributeDNAAttribute(FDNAAttribute AttributeA, FDNAAttribute AttributeB)
{
	return (AttributeA != AttributeB);
}

FDNAAbilityTargetDataHandle UDNAAbilitySystemBlueprintLibrary::AppendTargetDataHandle(FDNAAbilityTargetDataHandle TargetHandle, const FDNAAbilityTargetDataHandle& HandleToAdd)
{
	TargetHandle.Append(HandleToAdd);
	return TargetHandle;
}

FDNAAbilityTargetDataHandle UDNAAbilitySystemBlueprintLibrary::AbilityTargetDataFromLocations(const FDNAAbilityTargetingLocationInfo& SourceLocation, const FDNAAbilityTargetingLocationInfo& TargetLocation)
{
	// Construct TargetData
	FDNAAbilityTargetData_LocationInfo*	NewData = new FDNAAbilityTargetData_LocationInfo();
	NewData->SourceLocation = SourceLocation;
	NewData->TargetLocation = TargetLocation;

	// Give it a handle and return
	FDNAAbilityTargetDataHandle	Handle;
	Handle.Data.Add(TSharedPtr<FDNAAbilityTargetData_LocationInfo>(NewData));
	return Handle;
}

FDNAAbilityTargetDataHandle UDNAAbilitySystemBlueprintLibrary::AbilityTargetDataFromActor(AActor* Actor)
{
	// Construct TargetData
	FDNAAbilityTargetData_ActorArray*	NewData = new FDNAAbilityTargetData_ActorArray();
	NewData->TargetActorArray.Add(Actor);
	FDNAAbilityTargetDataHandle		Handle(NewData);
	return Handle;
}
FDNAAbilityTargetDataHandle UDNAAbilitySystemBlueprintLibrary::AbilityTargetDataFromActorArray(const TArray<AActor*>& ActorArray, bool OneTargetPerHandle)
{
	// Construct TargetData
	if (OneTargetPerHandle)
	{
		FDNAAbilityTargetDataHandle Handle;
		for (int32 i = 0; i < ActorArray.Num(); ++i)
		{
			if (::IsValid(ActorArray[i]))
			{
				FDNAAbilityTargetDataHandle TempHandle = AbilityTargetDataFromActor(ActorArray[i]);
				Handle.Append(TempHandle);
			}
		}
		return Handle;
	}
	else
	{
		FDNAAbilityTargetData_ActorArray*	NewData = new FDNAAbilityTargetData_ActorArray();
		NewData->TargetActorArray.Reset();
		for (auto Actor : ActorArray)
		{
			NewData->TargetActorArray.Add(Actor);
		}
		FDNAAbilityTargetDataHandle		Handle(NewData);
		return Handle;
	}
}

FDNAAbilityTargetDataHandle UDNAAbilitySystemBlueprintLibrary::FilterTargetData(const FDNAAbilityTargetDataHandle& TargetDataHandle, FDNATargetDataFilterHandle FilterHandle)
{
	FDNAAbilityTargetDataHandle ReturnDataHandle;
	
	for (int32 i = 0; TargetDataHandle.IsValid(i); ++i)
	{
		const FDNAAbilityTargetData* UnfilteredData = TargetDataHandle.Get(i);
		check(UnfilteredData);
		if (UnfilteredData->GetActors().Num() > 0)
		{
			TArray<TWeakObjectPtr<AActor>> FilteredActors = UnfilteredData->GetActors().FilterByPredicate(FilterHandle);
			if (FilteredActors.Num() > 0)
			{
				//Copy the data first, since we don't understand the internals of it
				const UScriptStruct* ScriptStruct = UnfilteredData->GetScriptStruct();
				FDNAAbilityTargetData* NewData = (FDNAAbilityTargetData*)FMemory::Malloc(ScriptStruct->GetCppStructOps()->GetSize());
				ScriptStruct->InitializeStruct(NewData);
				ScriptStruct->CopyScriptStruct(NewData, UnfilteredData);
				ReturnDataHandle.Data.Add(TSharedPtr<FDNAAbilityTargetData>(NewData));
				if (FilteredActors.Num() < UnfilteredData->GetActors().Num())
				{
					//We have lost some, but not all, of our actors, so replace the array. This should only be possible with targeting types that permit actor-array setting.
					if (!NewData->SetActors(FilteredActors))
					{
						//This is an error, though we could ignore it. We somehow filtered out part of a list, but the class doesn't support changing the list, so now it's all or nothing.
						check(false);
					}
				}
			}
		}
	}

	return ReturnDataHandle;
}

FDNATargetDataFilterHandle UDNAAbilitySystemBlueprintLibrary::MakeFilterHandle(FDNATargetDataFilter Filter, AActor* FilterActor)
{
	FDNATargetDataFilterHandle FilterHandle;
	FDNATargetDataFilter* NewFilter = new FDNATargetDataFilter(Filter);
	NewFilter->InitializeFilterContext(FilterActor);
	FilterHandle.Filter = TSharedPtr<FDNATargetDataFilter>(NewFilter);
	return FilterHandle;
}

FDNAEffectSpecHandle UDNAAbilitySystemBlueprintLibrary::MakeSpecHandle(UDNAEffect* InDNAEffect, AActor* InInstigator, AActor* InEffectCauser, float InLevel)
{
	FDNAEffectContext* EffectContext = new FDNAEffectContext(InInstigator, InEffectCauser);
	return FDNAEffectSpecHandle(new FDNAEffectSpec(InDNAEffect, FDNAEffectContextHandle(EffectContext), InLevel));
}

FDNAAbilityTargetDataHandle UDNAAbilitySystemBlueprintLibrary::AbilityTargetDataFromHitResult(const FHitResult& HitResult)
{
	// Construct TargetData
	FDNAAbilityTargetData_SingleTargetHit* TargetData = new FDNAAbilityTargetData_SingleTargetHit(HitResult);

	// Give it a handle and return
	FDNAAbilityTargetDataHandle	Handle;
	Handle.Data.Add(TSharedPtr<FDNAAbilityTargetData>(TargetData));

	return Handle;
}

int32 UDNAAbilitySystemBlueprintLibrary::GetDataCountFromTargetData(const FDNAAbilityTargetDataHandle& TargetData)
{
	return TargetData.Data.Num();
}

TArray<AActor*> UDNAAbilitySystemBlueprintLibrary::GetActorsFromTargetData(const FDNAAbilityTargetDataHandle& TargetData, int32 Index)
{
	if (TargetData.Data.IsValidIndex(Index))
	{
		FDNAAbilityTargetData* Data = TargetData.Data[Index].Get();
		TArray<AActor*>	ResolvedArray;
		if (Data)
		{
			TArray<TWeakObjectPtr<AActor> > WeakArray = Data->GetActors();
			for (TWeakObjectPtr<AActor> WeakPtr : WeakArray)
			{
				ResolvedArray.Add(WeakPtr.Get());
			}
		}
		return ResolvedArray;
	}
	return TArray<AActor*>();
}

bool UDNAAbilitySystemBlueprintLibrary::DoesTargetDataContainActor(const FDNAAbilityTargetDataHandle& TargetData, int32 Index, AActor* Actor)
{
	if (TargetData.Data.IsValidIndex(Index))
	{
		FDNAAbilityTargetData* Data = TargetData.Data[Index].Get();
		if (Data)
		{
			for (auto DataActorIter : Data->GetActors())
			{
				if (DataActorIter == Actor)
				{
					return true;
				}
			}
		}
	}
	return false;
}

bool UDNAAbilitySystemBlueprintLibrary::TargetDataHasActor(const FDNAAbilityTargetDataHandle& TargetData, int32 Index)
{
	if (TargetData.Data.IsValidIndex(Index))
	{
		FDNAAbilityTargetData* Data = TargetData.Data[Index].Get();
		if (Data)
		{
			return (Data->GetActors().Num() > 0);
		}
	}
	return false;
}

bool UDNAAbilitySystemBlueprintLibrary::TargetDataHasHitResult(const FDNAAbilityTargetDataHandle& TargetData, int32 Index)
{
	if (TargetData.Data.IsValidIndex(Index))
	{
		FDNAAbilityTargetData* Data = TargetData.Data[Index].Get();
		if (Data)
		{
			return Data->HasHitResult();
		}
	}
	return false;
}

FHitResult UDNAAbilitySystemBlueprintLibrary::GetHitResultFromTargetData(const FDNAAbilityTargetDataHandle& TargetData, int32 Index)
{
	if (TargetData.Data.IsValidIndex(Index))
	{
		FDNAAbilityTargetData* Data = TargetData.Data[Index].Get();
		if (Data)
		{
			const FHitResult* HitResultPtr = Data->GetHitResult();
			if (HitResultPtr)
			{
				return *HitResultPtr;
			}
		}
	}

	return FHitResult();
}

bool UDNAAbilitySystemBlueprintLibrary::TargetDataHasOrigin(const FDNAAbilityTargetDataHandle& TargetData, int32 Index)
{
	if (TargetData.Data.IsValidIndex(Index) == false)
	{
		return false;
	}

	FDNAAbilityTargetData* Data = TargetData.Data[Index].Get();
	if (Data)
	{
		return (Data->HasHitResult() || Data->HasOrigin());
	}
	return false;
}

FTransform UDNAAbilitySystemBlueprintLibrary::GetTargetDataOrigin(const FDNAAbilityTargetDataHandle& TargetData, int32 Index)
{
	if (TargetData.Data.IsValidIndex(Index) == false)
	{
		return FTransform::Identity;
	}

	FDNAAbilityTargetData* Data = TargetData.Data[Index].Get();
	if (Data)
	{
		if (Data->HasOrigin())
		{
			return Data->GetOrigin();
		}
		if (Data->HasHitResult())
		{
			const FHitResult* HitResultPtr = Data->GetHitResult();
			FTransform ReturnTransform;
			ReturnTransform.SetLocation(HitResultPtr->TraceStart);
			ReturnTransform.SetRotation((HitResultPtr->Location - HitResultPtr->TraceStart).GetSafeNormal().Rotation().Quaternion());
			return ReturnTransform;
		}
	}

	return FTransform::Identity;
}

bool UDNAAbilitySystemBlueprintLibrary::TargetDataHasEndPoint(const FDNAAbilityTargetDataHandle& TargetData, int32 Index)
{
	if (TargetData.Data.IsValidIndex(Index))
	{
		FDNAAbilityTargetData* Data = TargetData.Data[Index].Get();
		if (Data)
		{
			return (Data->HasHitResult() || Data->HasEndPoint());
		}
	}
	return false;
}

FVector UDNAAbilitySystemBlueprintLibrary::GetTargetDataEndPoint(const FDNAAbilityTargetDataHandle& TargetData, int32 Index)
{
	if (TargetData.Data.IsValidIndex(Index))
	{
		FDNAAbilityTargetData* Data = TargetData.Data[Index].Get();
		if (Data)
		{
			const FHitResult* HitResultPtr = Data->GetHitResult();
			if (HitResultPtr)
			{
				return HitResultPtr->Location;
			}
			else if (Data->HasEndPoint())
			{
				return Data->GetEndPoint();
			}
		}
	}

	return FVector::ZeroVector;
}

FTransform UDNAAbilitySystemBlueprintLibrary::GetTargetDataEndPointTransform(const FDNAAbilityTargetDataHandle& TargetData, int32 Index)
{
	if (TargetData.Data.IsValidIndex(Index))
	{
		FDNAAbilityTargetData* Data = TargetData.Data[Index].Get();
		if (Data)
		{
			return Data->GetEndPointTransform();
		}
	}

	return FTransform::Identity;
}


// -------------------------------------------------------------------------------------

bool UDNAAbilitySystemBlueprintLibrary::EffectContextIsValid(FDNAEffectContextHandle EffectContext)
{
	return EffectContext.IsValid();
}

bool UDNAAbilitySystemBlueprintLibrary::EffectContextIsInstigatorLocallyControlled(FDNAEffectContextHandle EffectContext)
{
	return EffectContext.IsLocallyControlled();
}

FHitResult UDNAAbilitySystemBlueprintLibrary::EffectContextGetHitResult(FDNAEffectContextHandle EffectContext)
{
	if (EffectContext.GetHitResult())
	{
		return *EffectContext.GetHitResult();
	}

	return FHitResult();
}

bool UDNAAbilitySystemBlueprintLibrary::EffectContextHasHitResult(FDNAEffectContextHandle EffectContext)
{
	return EffectContext.GetHitResult() != NULL;
}

void UDNAAbilitySystemBlueprintLibrary::EffectContextAddHitResult(FDNAEffectContextHandle EffectContext, FHitResult HitResult, bool bReset)
{
	EffectContext.AddHitResult(HitResult, bReset);
}

AActor*	UDNAAbilitySystemBlueprintLibrary::EffectContextGetInstigatorActor(FDNAEffectContextHandle EffectContext)
{
	return EffectContext.GetInstigator();
}

AActor*	UDNAAbilitySystemBlueprintLibrary::EffectContextGetOriginalInstigatorActor(FDNAEffectContextHandle EffectContext)
{
	return EffectContext.GetOriginalInstigator();
}

AActor*	UDNAAbilitySystemBlueprintLibrary::EffectContextGetEffectCauser(FDNAEffectContextHandle EffectContext)
{
	return EffectContext.GetEffectCauser();
}

UObject* UDNAAbilitySystemBlueprintLibrary::EffectContextGetSourceObject(FDNAEffectContextHandle EffectContext)
{
	return const_cast<UObject*>( EffectContext.GetSourceObject() );
}

FVector UDNAAbilitySystemBlueprintLibrary::EffectContextGetOrigin(FDNAEffectContextHandle EffectContext)
{
	if (EffectContext.HasOrigin())
	{
		return EffectContext.GetOrigin();
	}

	return FVector::ZeroVector;
}

void UDNAAbilitySystemBlueprintLibrary::EffectContextSetOrigin(FDNAEffectContextHandle EffectContext, FVector Origin)
{
	EffectContext.AddOrigin(Origin);
}

bool UDNAAbilitySystemBlueprintLibrary::IsInstigatorLocallyControlled(FDNACueParameters Parameters)
{
	return Parameters.IsInstigatorLocallyControlled();
}

bool UDNAAbilitySystemBlueprintLibrary::IsInstigatorLocallyControlledPlayer(FDNACueParameters Parameters)
{
	return Parameters.IsInstigatorLocallyControlledPlayer();
}

int32 UDNAAbilitySystemBlueprintLibrary::GetActorCount(FDNACueParameters Parameters)
{
	return Parameters.EffectContext.GetActors().Num();
}

AActor* UDNAAbilitySystemBlueprintLibrary::GetActorByIndex(FDNACueParameters Parameters, int32 Index)
{
	if (Parameters.EffectContext.GetActors().IsValidIndex(Index))
	{
		return Parameters.EffectContext.GetActors()[Index].Get();
	}
	return NULL;
}

FHitResult UDNAAbilitySystemBlueprintLibrary::GetHitResult(FDNACueParameters Parameters)
{
	if (Parameters.EffectContext.GetHitResult())
	{
		return *Parameters.EffectContext.GetHitResult();
	}
	
	return FHitResult();
}

bool UDNAAbilitySystemBlueprintLibrary::HasHitResult(FDNACueParameters Parameters)
{
	return Parameters.EffectContext.GetHitResult() != NULL;
}

void UDNAAbilitySystemBlueprintLibrary::ForwardDNACueToTarget(TScriptInterface<IDNACueInterface> TargetCueInterface, EDNACueEvent::Type EventType, FDNACueParameters Parameters)
{
	AActor* ActorTarget = Cast<AActor>(TargetCueInterface.GetObject());
	if (TargetCueInterface && ActorTarget)
	{
		TargetCueInterface->HandleDNACue(ActorTarget, Parameters.OriginalTag, EventType, Parameters);
	}
}

AActor*	UDNAAbilitySystemBlueprintLibrary::GetInstigatorActor(FDNACueParameters Parameters)
{
	return Parameters.GetInstigator();
}

FTransform UDNAAbilitySystemBlueprintLibrary::GetInstigatorTransform(FDNACueParameters Parameters)
{
	AActor* InstigatorActor = GetInstigatorActor(Parameters);
	if (InstigatorActor)
	{
		return InstigatorActor->GetTransform();
	}

	ABILITY_LOG(Warning, TEXT("UDNAAbilitySystemBlueprintLibrary::GetInstigatorTransform called on DNACue with no valid instigator"));
	return FTransform::Identity;
}

FVector UDNAAbilitySystemBlueprintLibrary::GetOrigin(FDNACueParameters Parameters)
{
	if (Parameters.EffectContext.HasOrigin())
	{
		return Parameters.EffectContext.GetOrigin();
	}

	return Parameters.Location;
}

bool UDNAAbilitySystemBlueprintLibrary::GetDNACueEndLocationAndNormal(AActor* TargetActor, FDNACueParameters Parameters, FVector& Location, FVector& Normal)
{
	FDNAEffectContext* Data = Parameters.EffectContext.Get();
	if (Parameters.Location.IsNearlyZero() == false)
	{
		Location = Parameters.Location;
		Normal = Parameters.Normal;
	}
	else if (Data && Data->GetHitResult())
	{
		Location = Data->GetHitResult()->Location;
		Normal = Data->GetHitResult()->Normal;
		return true;
	}
	else if(TargetActor)
	{
		Location = TargetActor->GetActorLocation();
		Normal = TargetActor->GetActorForwardVector();
		return true;
	}

	return false;
}

bool UDNAAbilitySystemBlueprintLibrary::GetDNACueDirection(AActor* TargetActor, FDNACueParameters Parameters, FVector& Direction)
{
	if (Parameters.Normal.IsNearlyZero() == false)
	{
		Direction = -Parameters.Normal;
		return true;
	}

	if (FDNAEffectContext* Ctx = Parameters.EffectContext.Get())
	{
		if (Ctx->GetHitResult())
		{
			// Most projectiles and melee attacks will use this
			Direction = (-1.f * Ctx->GetHitResult()->Normal);
			return true;
		}
		else if (TargetActor && Ctx->HasOrigin())
		{
			// Fallback to trying to use the target location and the origin of the effect
			FVector NewVec = (TargetActor->GetActorLocation() - Ctx->GetOrigin());
			NewVec.Normalize();
			Direction = NewVec;
			return true;
		}
		else if (TargetActor && Ctx->GetEffectCauser())
		{
			// Finally, try to use the direction between the causer of the effect and the target of the effect
			FVector NewVec = (TargetActor->GetActorLocation() - Ctx->GetEffectCauser()->GetActorLocation());
			NewVec.Normalize();
			Direction = NewVec;
			return true;
		}
	}

	Direction = FVector::ZeroVector;
	return false;
}

bool UDNAAbilitySystemBlueprintLibrary::DoesDNACueMeetTagRequirements(FDNACueParameters Parameters, FDNATagRequirements& SourceTagReqs, FDNATagRequirements& TargetTagReqs)
{
	return SourceTagReqs.RequirementsMet(Parameters.AggregatedSourceTags)
		&& TargetTagReqs.RequirementsMet(Parameters.AggregatedSourceTags);
}

// ---------------------------------------------------------------------------------------

FDNAEffectSpecHandle UDNAAbilitySystemBlueprintLibrary::AssignSetByCallerMagnitude(FDNAEffectSpecHandle SpecHandle, FName DataName, float Magnitude)
{
	FDNAEffectSpec* Spec = SpecHandle.Data.Get();
	if (Spec)
	{
		Spec->SetSetByCallerMagnitude(DataName, Magnitude);
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UDNAAbilitySystemBlueprintLibrary::AssignSetByCallerMagnitude called with invalid SpecHandle"));
	}

	return SpecHandle;
}

FDNAEffectSpecHandle UDNAAbilitySystemBlueprintLibrary::SetDuration(FDNAEffectSpecHandle SpecHandle, float Duration)
{
	FDNAEffectSpec* Spec = SpecHandle.Data.Get();
	if (Spec)
	{
		Spec->SetDuration(Duration, true);
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UDNAAbilitySystemBlueprintLibrary::SetDuration called with invalid SpecHandle"));
	}

	return SpecHandle;
}

FDNAEffectSpecHandle UDNAAbilitySystemBlueprintLibrary::AddGrantedTag(FDNAEffectSpecHandle SpecHandle, FDNATag NewDNATag)
{
	FDNAEffectSpec* Spec = SpecHandle.Data.Get();
	if (Spec)
	{
		Spec->DynamicGrantedTags.AddTag(NewDNATag);
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UDNAAbilitySystemBlueprintLibrary::AddGrantedTag called with invalid SpecHandle"));
	}

	return SpecHandle;
}

FDNAEffectSpecHandle UDNAAbilitySystemBlueprintLibrary::AddGrantedTags(FDNAEffectSpecHandle SpecHandle, FDNATagContainer NewDNATags)
{
	FDNAEffectSpec* Spec = SpecHandle.Data.Get();
	if (Spec)
	{
		Spec->DynamicGrantedTags.AppendTags(NewDNATags);
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UDNAAbilitySystemBlueprintLibrary::AddGrantedTags called with invalid SpecHandle"));
	}

	return SpecHandle;
}

FDNAEffectSpecHandle UDNAAbilitySystemBlueprintLibrary::AddAssetTag(FDNAEffectSpecHandle SpecHandle, FDNATag NewDNATag)
{
	FDNAEffectSpec* Spec = SpecHandle.Data.Get();
	if (Spec)
	{
		Spec->DynamicAssetTags.AddTag(NewDNATag);
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UDNAAbilitySystemBlueprintLibrary::AddEffectTag called with invalid SpecHandle"));
	}

	return SpecHandle;
}

FDNAEffectSpecHandle UDNAAbilitySystemBlueprintLibrary::AddAssetTags(FDNAEffectSpecHandle SpecHandle, FDNATagContainer NewDNATags)
{
	FDNAEffectSpec* Spec = SpecHandle.Data.Get();
	if (Spec)
	{
		Spec->DynamicAssetTags.AppendTags(NewDNATags);
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UDNAAbilitySystemBlueprintLibrary::AddEffectTags called with invalid SpecHandle"));
	}

	return SpecHandle;
}
	
FDNAEffectSpecHandle UDNAAbilitySystemBlueprintLibrary::AddLinkedDNAEffectSpec(FDNAEffectSpecHandle SpecHandle, FDNAEffectSpecHandle LinkedDNAEffectSpec)
{
	FDNAEffectSpec* Spec = SpecHandle.Data.Get();
	if (Spec)
	{
		Spec->TargetEffectSpecs.Add(LinkedDNAEffectSpec);
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UDNAAbilitySystemBlueprintLibrary::AddLinkedDNAEffectSpec called with invalid SpecHandle"));
	}

	return SpecHandle;
}


FDNAEffectSpecHandle UDNAAbilitySystemBlueprintLibrary::SetStackCount(FDNAEffectSpecHandle SpecHandle, int32 StackCount)
{
	FDNAEffectSpec* Spec = SpecHandle.Data.Get();
	if (Spec)
	{
		Spec->StackCount = StackCount;
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UDNAAbilitySystemBlueprintLibrary::AddLinkedDNAEffectSpec called with invalid SpecHandle"));
	}
	return SpecHandle;
}
	
FDNAEffectSpecHandle UDNAAbilitySystemBlueprintLibrary::SetStackCountToMax(FDNAEffectSpecHandle SpecHandle)
{
	FDNAEffectSpec* Spec = SpecHandle.Data.Get();
	if (Spec && Spec->Def)
	{
		Spec->StackCount = Spec->Def->StackLimitCount;
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UDNAAbilitySystemBlueprintLibrary::AddLinkedDNAEffectSpec called with invalid SpecHandle"));
	}
	return SpecHandle;
}

FDNAEffectContextHandle UDNAAbilitySystemBlueprintLibrary::GetEffectContext(FDNAEffectSpecHandle SpecHandle)
{
	FDNAEffectSpec* Spec = SpecHandle.Data.Get();
	if (Spec)
	{
		return Spec->GetEffectContext();
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UDNAAbilitySystemBlueprintLibrary::GetEffectContext called with invalid SpecHandle"));
	}

	return FDNAEffectContextHandle();
}

int32 UDNAAbilitySystemBlueprintLibrary::GetActiveDNAEffectStackCount(FActiveDNAEffectHandle ActiveHandle)
{
	UDNAAbilitySystemComponent* ASC = ActiveHandle.GetOwningDNAAbilitySystemComponent();
	if (ASC)
	{
		return ASC->GetCurrentStackCount(ActiveHandle);
	}
	return 0;
}

int32 UDNAAbilitySystemBlueprintLibrary::GetActiveDNAEffectStackLimitCount(FActiveDNAEffectHandle ActiveHandle)
{
	UDNAAbilitySystemComponent* ASC = ActiveHandle.GetOwningDNAAbilitySystemComponent();
	if (ASC)
	{
		const UDNAEffect* ActiveGE = ASC->GetDNAEffectDefForHandle(ActiveHandle);
		if (ActiveGE)
		{
			return ActiveGE->StackLimitCount;
		}
	}
	return 0;
}

float UDNAAbilitySystemBlueprintLibrary::GetModifiedAttributeMagnitude(FDNAEffectSpecHandle SpecHandle, FDNAAttribute Attribute)
{
	FDNAEffectSpec* Spec = SpecHandle.Data.Get();
	float Delta = 0.f;
	if (Spec)
	{
		for (FDNAEffectModifiedAttribute &Mod : Spec->ModifiedAttributes)
		{
			if (Mod.Attribute == Attribute)
			{
				Delta += Mod.TotalMagnitude;
			}
		}
	}
	return Delta;
}

FString UDNAAbilitySystemBlueprintLibrary::GetActiveDNAEffectDebugString(FActiveDNAEffectHandle ActiveHandle)
{
	FString Str;
	UDNAAbilitySystemComponent* ASC = ActiveHandle.GetOwningDNAAbilitySystemComponent();
	if (ASC)
	{
		Str = ASC->GetActiveGEDebugString(ActiveHandle);
	}
	return Str;
}
