// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNAEffectTypes.h"
#include "GameFramework/Pawn.h"
#include "DNATagAssetInterface.h"
#include "DNAEffect.h"
#include "AttributeSet.h"
#include "GameFramework/Controller.h"
#include "Misc/ConfigCacheIni.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"

#if WITH_EDITORONLY_DATA
const FName FDNAModEvaluationChannelSettings::ForceHideMetadataKey(TEXT("ForceHideEvaluationChannel"));
const FString FDNAModEvaluationChannelSettings::ForceHideMetadataEnabledValue(TEXT("True"));
#endif // #if WITH_EDITORONLY_DATA

FDNAModEvaluationChannelSettings::FDNAModEvaluationChannelSettings()
{
	static const UEnum* EvalChannelEnum = nullptr;
	static EDNAModEvaluationChannel DefaultChannel = EDNAModEvaluationChannel::Channel0;

	// The default value for this struct is actually dictated by a config value, so a degree of trickery is involved.
	// The first time through, try to find the enum and the default value, if any, and then use that to set the
	// static default channel used to initialize this struct
	if (!EvalChannelEnum)
	{
		EvalChannelEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EDNAModEvaluationChannel"));
		if (ensure(EvalChannelEnum) && ensure(GConfig))
		{
			const FString INISection(TEXT("/Script/DNAAbilities.DNAAbilitySystemGlobals"));
			const FString INIKey(TEXT("DefaultDNAModEvaluationChannel"));
			
			FString DefaultEnumString;
			if (GConfig->GetString(*INISection, *INIKey, DefaultEnumString, GGameIni))
			{
				if (!DefaultEnumString.IsEmpty())
				{
					const int32 EnumVal = EvalChannelEnum->GetValueByName(FName(*DefaultEnumString));
					if (EnumVal != INDEX_NONE)
					{
						DefaultChannel = static_cast<EDNAModEvaluationChannel>(EnumVal);
					}
				}
			}
		}
	}

	Channel = DefaultChannel;
}

EDNAModEvaluationChannel FDNAModEvaluationChannelSettings::GetEvaluationChannel() const
{
	if (ensure(UDNAAbilitySystemGlobals::Get().IsDNAModEvaluationChannelValid(Channel)))
	{
		return Channel;
	}

	return EDNAModEvaluationChannel::Channel0;
}

float DNAEffectUtilities::GetModifierBiasByModifierOp(EDNAModOp::Type ModOp)
{
	static const float ModifierOpBiases[EDNAModOp::Max] = {0.f, 1.f, 1.f, 0.f};
	check(ModOp >= 0 && ModOp < EDNAModOp::Max);

	return ModifierOpBiases[ModOp];
}

float DNAEffectUtilities::ComputeStackedModifierMagnitude(float BaseComputedMagnitude, int32 StackCount, EDNAModOp::Type ModOp)
{
	const float OperationBias = DNAEffectUtilities::GetModifierBiasByModifierOp(ModOp);

	StackCount = FMath::Clamp<int32>(StackCount, 0, StackCount);

	float StackMag = BaseComputedMagnitude;
	
	// Override modifiers don't care about stack count at all. All other modifier ops need to subtract out their bias value in order to handle
	// stacking correctly
	if (ModOp != EDNAModOp::Override)
	{
		StackMag -= OperationBias;
		StackMag *= StackCount;
		StackMag += OperationBias;
	}

	return StackMag;
}

bool FDNAEffectAttributeCaptureDefinition::operator==(const FDNAEffectAttributeCaptureDefinition& Other) const
{
	return ((AttributeToCapture == Other.AttributeToCapture) && (AttributeSource == Other.AttributeSource) && (bSnapshot == Other.bSnapshot));
}

bool FDNAEffectAttributeCaptureDefinition::operator!=(const FDNAEffectAttributeCaptureDefinition& Other) const
{
	return ((AttributeToCapture != Other.AttributeToCapture) || (AttributeSource != Other.AttributeSource) || (bSnapshot != Other.bSnapshot));
}

FString FDNAEffectAttributeCaptureDefinition::ToSimpleString() const
{
	return FString::Printf(TEXT("Attribute: %s, Capture: %s, Snapshot: %d"), *AttributeToCapture.GetName(), AttributeSource == EDNAEffectAttributeCaptureSource::Source ? TEXT("Source") : TEXT("Target"), bSnapshot);
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	FDNAEffectContext
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

void FDNAEffectContext::AddInstigator(class AActor *InInstigator, class AActor *InEffectCauser)
{
	Instigator = InInstigator;
	EffectCauser = InEffectCauser;
	InstigatorDNAAbilitySystemComponent = NULL;

	// Cache off his DNAAbilitySystemComponent.
	IDNAAbilitySystemInterface* DNAAbilitySystemInterface = Cast<IDNAAbilitySystemInterface>(Instigator.Get());
	if (DNAAbilitySystemInterface)
	{
		InstigatorDNAAbilitySystemComponent = DNAAbilitySystemInterface->GetDNAAbilitySystemComponent();
	}
}

void FDNAEffectContext::SetAbility(const UDNAAbility* InDNAAbility)
{
	if (InDNAAbility)
	{
		Ability = InDNAAbility->GetClass();
		AbilityLevel = InDNAAbility->GetAbilityLevel();
	}
}

const UDNAAbility* FDNAEffectContext::GetAbility() const
{
	return Ability.GetDefaultObject();
}

void FDNAEffectContext::AddActors(const TArray<TWeakObjectPtr<AActor>>& InActors, bool bReset)
{
	if (bReset && Actors.Num())
	{
		Actors.Reset();
	}

	Actors.Append(InActors);
}

void FDNAEffectContext::AddHitResult(const FHitResult& InHitResult, bool bReset)
{
	if (bReset && HitResult.IsValid())
	{
		HitResult.Reset();
		bHasWorldOrigin = false;
	}

	check(!HitResult.IsValid());
	HitResult = TSharedPtr<FHitResult>(new FHitResult(InHitResult));
	if (bHasWorldOrigin == false)
	{
		AddOrigin(InHitResult.TraceStart);
	}
}

bool FDNAEffectContext::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	uint8 RepBits = 0;
	if (Ar.IsSaving())
	{
		if (Instigator.IsValid() )
		{
			RepBits |= 1 << 0;
		}
		if (EffectCauser.IsValid() )
		{
			RepBits |= 1 << 1;
		}
		if (*Ability)
		{
			RepBits |= 1 << 2;
		}
		if (SourceObject.IsValid())
		{
			RepBits |= 1 << 3;
		}
		if (Actors.Num() > 0)
		{
			RepBits |= 1 << 4;
		}
		if (HitResult.IsValid())
		{
			RepBits |= 1 << 5;
		}
		if (bHasWorldOrigin)
		{
			RepBits |= 1 << 6;
		}
	}

	Ar.SerializeBits(&RepBits, 7);

	if (RepBits & (1 << 0))
	{
		Ar << Instigator;
	}
	if (RepBits & (1 << 1))
	{
		Ar << EffectCauser;
	}
	if (RepBits & (1 << 2))
	{
		Ar << Ability;
	}
	if (RepBits & (1 << 3))
	{
		Ar << SourceObject;
	}
	if (RepBits & (1 << 4))
	{
		SafeNetSerializeTArray_Default<31>(Ar, Actors);
	}
	if (RepBits & (1 << 5))
	{
		if (Ar.IsLoading())
		{
			if (!HitResult.IsValid())
			{
				HitResult = TSharedPtr<FHitResult>(new FHitResult());
			}
		}
		HitResult->NetSerialize(Ar, Map, bOutSuccess);
	}
	if (RepBits & (1 << 6))
	{
		Ar << WorldOrigin;
		bHasWorldOrigin = true;
	}
	else
	{
		bHasWorldOrigin = false;
	}

	if (Ar.IsLoading())
	{
		AddInstigator(Instigator.Get(), EffectCauser.Get()); // Just to initialize InstigatorDNAAbilitySystemComponent
	}	
	
	bOutSuccess = true;
	return true;
}

bool FDNAEffectContext::IsLocallyControlled() const
{
	APawn* Pawn = Cast<APawn>(Instigator.Get());
	if (!Pawn)
	{
		Pawn = Cast<APawn>(EffectCauser.Get());
	}
	if (Pawn)
	{
		return Pawn->IsLocallyControlled();
	}
	return false;
}

bool FDNAEffectContext::IsLocallyControlledPlayer() const
{
	APawn* Pawn = Cast<APawn>(Instigator.Get());
	if (!Pawn)
	{
		Pawn = Cast<APawn>(EffectCauser.Get());
	}
	if (Pawn && Pawn->Controller)
	{
		return Pawn->Controller->IsLocalPlayerController();
	}
	return false;
}

void FDNAEffectContext::AddOrigin(FVector InOrigin)
{
	bHasWorldOrigin = true;
	WorldOrigin = InOrigin;
}

void FDNAEffectContext::GetOwnedDNATags(OUT FDNATagContainer& ActorTagContainer, OUT FDNATagContainer& SpecTagContainer) const
{
	IDNATagAssetInterface* TagInterface = Cast<IDNATagAssetInterface>(Instigator.Get());
	if (TagInterface)
	{
		TagInterface->GetOwnedDNATags(ActorTagContainer);
	}
	else if (InstigatorDNAAbilitySystemComponent.IsValid())
	{
		InstigatorDNAAbilitySystemComponent->GetOwnedDNATags(ActorTagContainer);
	}
}

bool FDNAEffectContextHandle::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	bool ValidData = Data.IsValid();
	Ar.SerializeBits(&ValidData,1);

	if (ValidData)
	{
		if (Ar.IsLoading())
		{
			// For now, just always reset/reallocate the data when loading.
			// Longer term if we want to generalize this and use it for property replication, we should support
			// only reallocating when necessary
			
			if (Data.IsValid() == false)
			{
				Data = TSharedPtr<FDNAEffectContext>(UDNAAbilitySystemGlobals::Get().AllocDNAEffectContext());
			}
		}

		void* ContainerPtr = Data.Get();
		UScriptStruct* ScriptStruct = Data->GetScriptStruct();

		if (ScriptStruct->StructFlags & STRUCT_NetSerializeNative)
		{
			ScriptStruct->GetCppStructOps()->NetSerialize(Ar, Map, bOutSuccess, Data.Get());
		}
		else
		{
			// This won't work since UStructProperty::NetSerializeItem is deprecrated.
			//	1) we have to manually crawl through the topmost struct's fields since we don't have a UStructProperty for it (just the UScriptProperty)
			//	2) if there are any UStructProperties in the topmost struct's fields, we will assert in UStructProperty::NetSerializeItem.

			ABILITY_LOG(Fatal, TEXT("FDNAEffectContextHandle::NetSerialize called on data struct %s without a native NetSerialize"), *ScriptStruct->GetName());
		}
	}

	bOutSuccess = true;
	return true;
}


// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	Misc
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

FString EDNAModOpToString(int32 Type)
{
	static UEnum *e = FindObject<UEnum>(ANY_PACKAGE, TEXT("EDNAModOp"));
	return e->GetEnumName(Type);
}

FString EDNAModEffectToString(int32 Type)
{
	static UEnum *e = FindObject<UEnum>(ANY_PACKAGE, TEXT("EDNAModEffect"));
	return e->GetEnumName(Type);
}

FString EDNACueEventToString(int32 Type)
{
	static UEnum *e = FindObject<UEnum>(ANY_PACKAGE, TEXT("EDNACueEvent"));
	return e->GetEnumName(Type);
}

void FDNATagCountContainer::Notify_StackCountChange(const FDNATag& Tag)
{	
	// The purpose of this function is to let anyone listening on the EDNATagEventType::AnyCountChange event know that the 
	// stack count of a GE that was backing this GE has changed. We do not update our internal map/count with this info, since that
	// map only counts the number of GE/sources that are giving that tag.
	FDNATagContainer TagAndParentsContainer = Tag.GetDNATagParents();
	for (auto CompleteTagIt = TagAndParentsContainer.CreateConstIterator(); CompleteTagIt; ++CompleteTagIt)
	{
		const FDNATag& CurTag = *CompleteTagIt;
		FDelegateInfo* DelegateInfo = DNATagEventMap.Find(CurTag);
		if (DelegateInfo)
		{
			int32 TagCount = DNATagCountMap.FindOrAdd(CurTag);
			DelegateInfo->OnAnyChange.Broadcast(CurTag, TagCount);
		}
	}
}

FOnDNAEffectTagCountChanged& FDNATagCountContainer::RegisterDNATagEvent(const FDNATag& Tag, EDNATagEventType::Type EventType)
{
	FDelegateInfo& Info = DNATagEventMap.FindOrAdd(Tag);

	if (EventType == EDNATagEventType::NewOrRemoved)
	{
		return Info.OnNewOrRemove;
	}

	return Info.OnAnyChange;
}

void FDNATagCountContainer::Reset()
{
	DNATagEventMap.Reset();
	DNATagCountMap.Reset();
	ExplicitTagCountMap.Reset();
	ExplicitTags.Reset();
	OnAnyTagChangeDelegate.Clear();
}

bool FDNATagCountContainer::UpdateTagMap_Internal(const FDNATag& Tag, int32 CountDelta)
{
	const bool bTagAlreadyExplicitlyExists = ExplicitTags.HasTagExact(Tag);

	// Need special case handling to maintain the explicit tag list correctly, adding the tag to the list if it didn't previously exist and a
	// positive delta comes in, and removing it from the list if it did exist and a negative delta comes in.
	if (!bTagAlreadyExplicitlyExists)
	{
		// Brand new tag with a positive delta needs to be explicitly added
		if (CountDelta > 0)
		{
			ExplicitTags.AddTag(Tag);
		}
		// Block attempted reduction of non-explicit tags, as they were never truly added to the container directly
		else
		{
			// only warn about tags that are in the container but will not be removed because they aren't explicitly in the container
			if (ExplicitTags.HasTag(Tag))
			{
				ABILITY_LOG(Warning, TEXT("Attempted to remove tag: %s from tag count container, but it is not explicitly in the container!"), *Tag.ToString());
			}
			return false;
		}
	}

	// Update the explicit tag count map. This has to be separate than the map below because otherwise the count of nested tags ends up wrong
	int32& ExistingCount = ExplicitTagCountMap.FindOrAdd(Tag);

	ExistingCount = FMath::Max(ExistingCount + CountDelta, 0);

	// If our new count is 0, remove us from the explicit tag list
	if (ExistingCount <= 0)
	{
		// Remove from the explicit list
		ExplicitTags.RemoveTag(Tag);
	}

	// Check if change delegates are required to fire for the tag or any of its parents based on the count change
	FDNATagContainer TagAndParentsContainer = Tag.GetDNATagParents();
	bool CreatedSignificantChange = false;
	for (auto CompleteTagIt = TagAndParentsContainer.CreateConstIterator(); CompleteTagIt; ++CompleteTagIt)
	{
		const FDNATag& CurTag = *CompleteTagIt;

		// Get the current count of the specified tag. NOTE: Stored as a reference, so subsequent changes propogate to the map.
		int32& TagCountRef = DNATagCountMap.FindOrAdd(CurTag);

		const int32 OldCount = TagCountRef;

		// Apply the delta to the count in the map
		int32 NewTagCount = FMath::Max(OldCount + CountDelta, 0);
		TagCountRef = NewTagCount;

		// If a significant change (new addition or total removal) occurred, trigger related delegates
		bool SignificantChange = (OldCount == 0 || NewTagCount == 0);
		CreatedSignificantChange |= SignificantChange;
		if (SignificantChange)
		{
			OnAnyTagChangeDelegate.Broadcast(CurTag, NewTagCount);
		}

		FDelegateInfo* DelegateInfo = DNATagEventMap.Find(CurTag);
		if (DelegateInfo)
		{
			// Prior to calling OnAnyChange delegate, copy our OnNewOrRemove delegate, since things listening to OnAnyChange could add or remove 
			// to this map causing our pointer to become invalid.
			FOnDNAEffectTagCountChanged OnNewOrRemoveLocalCopy = DelegateInfo->OnNewOrRemove;

			DelegateInfo->OnAnyChange.Broadcast(CurTag, NewTagCount);
			if (SignificantChange)
			{
				OnNewOrRemoveLocalCopy.Broadcast(CurTag, NewTagCount);
			}
		}
	}

	return CreatedSignificantChange;
}

bool FDNATagRequirements::RequirementsMet(const FDNATagContainer& Container) const
{
	bool HasRequired = Container.HasAll(RequireTags);
	bool HasIgnored = Container.HasAny(IgnoreTags);

	return HasRequired && !HasIgnored;
}

bool FDNATagRequirements::IsEmpty() const
{
	return (RequireTags.Num() == 0 && IgnoreTags.Num() == 0);
}

FString FDNATagRequirements::ToString() const
{
	FString Str;

	if (RequireTags.Num() > 0)
	{
		Str += FString::Printf(TEXT("require: %s "), *RequireTags.ToStringSimple());
	}
	if (IgnoreTags.Num() >0)
	{
		Str += FString::Printf(TEXT("ignore: %s "), *IgnoreTags.ToStringSimple());
	}

	return Str;
}

void FActiveDNAEffectsContainer::PrintAllDNAEffects() const
{
	for (const FActiveDNAEffect& Effect : this)
	{
		Effect.PrintAll();
	}
}

void FActiveDNAEffect::PrintAll() const
{
	ABILITY_LOG(Log, TEXT("Handle: %s"), *Handle.ToString());
	ABILITY_LOG(Log, TEXT("StartWorldTime: %.2f"), StartWorldTime);
	Spec.PrintAll();
}

void FDNAEffectSpec::PrintAll() const
{
	ABILITY_LOG(Log, TEXT("Def: %s"), *Def->GetName());
	ABILITY_LOG(Log, TEXT("Duration: %.2f"), GetDuration());
	ABILITY_LOG(Log, TEXT("Period: %.2f"), GetPeriod());
	ABILITY_LOG(Log, TEXT("Modifiers:"));
}

FString FDNAEffectSpec::ToSimpleString() const
{
	return FString::Printf(TEXT("%s"), *GetNameSafe(Def));
}

const FDNATagContainer* FTagContainerAggregator::GetAggregatedTags() const
{
	if (CacheIsValid == false)
	{
		CacheIsValid = true;
		CachedAggregator.Reset(CapturedActorTags.Num() + CapturedSpecTags.Num() + ScopedTags.Num());
		CachedAggregator.AppendTags(CapturedActorTags);
		CachedAggregator.AppendTags(CapturedSpecTags);
		CachedAggregator.AppendTags(ScopedTags);
	}

	return &CachedAggregator;
}

FDNATagContainer& FTagContainerAggregator::GetActorTags()
{
	CacheIsValid = false;
	return CapturedActorTags;
}

const FDNATagContainer& FTagContainerAggregator::GetActorTags() const
{
	return CapturedActorTags;
}

FDNATagContainer& FTagContainerAggregator::GetSpecTags()
{
	CacheIsValid = false;
	return CapturedSpecTags;
}

const FDNATagContainer& FTagContainerAggregator::GetSpecTags() const
{
	CacheIsValid = false;
	return CapturedSpecTags;
}


FDNAEffectSpecHandle::FDNAEffectSpecHandle()
{

}

FDNAEffectSpecHandle::FDNAEffectSpecHandle(FDNAEffectSpec* DataPtr)
	: Data(DataPtr)
{

}

FDNACueParameters::FDNACueParameters(const FDNAEffectSpecForRPC& Spec)
: NormalizedMagnitude(0.0f)
, RawMagnitude(0.0f)
, Location(ForceInitToZero)
, Normal(ForceInitToZero)
, DNAEffectLevel(1)
, AbilityLevel(1)
{
	UDNAAbilitySystemGlobals::Get().InitDNACueParameters(*this, Spec);
}

FDNACueParameters::FDNACueParameters(const struct FDNAEffectContextHandle& InEffectContext)
: NormalizedMagnitude(0.0f)
, RawMagnitude(0.0f)
, Location(ForceInitToZero)
, Normal(ForceInitToZero)
, DNAEffectLevel(1)
, AbilityLevel(1)
{
	UDNAAbilitySystemGlobals::Get().InitDNACueParameters(*this, InEffectContext);
}

bool FDNACueParameters::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	static const uint8 NUM_LEVEL_BITS = 4;
	static const uint8 MAX_LEVEL = (1 << NUM_LEVEL_BITS) - 1;

	enum RepFlag
	{
		REP_NormalizedMagnitude = 0,
		REP_RawMagnitude,
		REP_EffectContext,
		REP_Location,
		REP_Normal,
		REP_Instigator,
		REP_EffectCauser,
		REP_SourceObject,
		REP_TargetAttachComponent,
		REP_PhysMaterial,
		REP_GELevel,
		REP_AbilityLevel,

		REP_MAX
	};

	uint16 RepBits = 0;
	if (Ar.IsSaving())
	{
		if (NormalizedMagnitude != 0.f)
		{
			RepBits |= (1 << REP_NormalizedMagnitude);
		}
		if (RawMagnitude != 0.f)
		{
			RepBits |= (1 << REP_RawMagnitude);
		}
		if (EffectContext.IsValid())
		{
			RepBits |= (1 << REP_EffectContext);
		}
		if (Location.IsNearlyZero() == false)
		{
			RepBits |= (1 << REP_Location);
		}
		if (Normal.IsNearlyZero() == false)
		{
			RepBits |= (1 << REP_Normal);
		}
		if (Instigator.IsValid())
		{
			RepBits |= (1 << REP_Instigator);
		}
		if (EffectCauser.IsValid())
		{
			RepBits |= (1 << REP_EffectCauser);
		}
		if (SourceObject.IsValid())
		{
			RepBits |= (1 << REP_SourceObject);
		}
		if (TargetAttachComponent.IsValid())
		{
			RepBits |= (1 << REP_TargetAttachComponent);
		}
		if (PhysicalMaterial.IsValid())
		{
			RepBits |= (1 << REP_PhysMaterial);
		}
		if (DNAEffectLevel != 1)
		{
			RepBits |= (1 << REP_GELevel);
		}
		if (AbilityLevel != 1)
		{
			RepBits |= (1 << REP_AbilityLevel);
		}
	}

	Ar.SerializeBits(&RepBits, REP_MAX);

	// Tag containers serialize empty containers with 1 bit, so no need to serialize this in the RepBits field.
	AggregatedSourceTags.NetSerialize(Ar, Map, bOutSuccess);
	AggregatedTargetTags.NetSerialize(Ar, Map, bOutSuccess);

	if (RepBits & (1 << REP_NormalizedMagnitude))
	{
		Ar << NormalizedMagnitude;
	}
	if (RepBits & (1 << REP_RawMagnitude))
	{
		Ar << RawMagnitude;
	}
	if (RepBits & (1 << REP_EffectContext))
	{
		EffectContext.NetSerialize(Ar, Map, bOutSuccess);
	}
	if (RepBits & (1 << REP_Location))
	{
		Location.NetSerialize(Ar, Map, bOutSuccess);
	}
	if (RepBits & (1 << REP_Normal))
	{
		Normal.NetSerialize(Ar, Map, bOutSuccess);
	}
	if (RepBits & (1 << REP_Instigator))
	{
		Ar << Instigator;
	}
	if (RepBits & (1 << REP_EffectCauser))
	{
		Ar << EffectCauser;
	}
	if (RepBits & (1 << REP_SourceObject))
	{
		Ar << SourceObject;
	}
	if (RepBits & (1 << REP_TargetAttachComponent))
	{
		Ar << TargetAttachComponent;
	}
	if (RepBits & (1 << REP_PhysMaterial))
	{
		Ar << PhysicalMaterial;
	}
	if (RepBits & (1 << REP_GELevel))
	{
		ensureMsgf(DNAEffectLevel <= MAX_LEVEL, TEXT("FDNACueParameters::NetSerialize trying to serialize GC parameters with a DNAEffectLevel of %d"), DNAEffectLevel);
		if (Ar.IsLoading())
		{
			DNAEffectLevel = 0;
		}

		Ar.SerializeBits(&DNAEffectLevel, NUM_LEVEL_BITS);
	}
	if (RepBits & (1 << REP_AbilityLevel))
	{
		ensureMsgf(AbilityLevel <= MAX_LEVEL, TEXT("FDNACueParameters::NetSerialize trying to serialize GC parameters with an AbilityLevel of %d"), AbilityLevel);
		if (Ar.IsLoading())
		{
			AbilityLevel = 0;
		}

		Ar.SerializeBits(&AbilityLevel, NUM_LEVEL_BITS);
	}

	bOutSuccess = true;
	return true;
}

bool FDNACueParameters::IsInstigatorLocallyControlled() const
{
	if (EffectContext.IsValid())
	{
		return EffectContext.IsLocallyControlled();
	}

	APawn* Pawn = Cast<APawn>(Instigator.Get());
	if (!Pawn)
	{
		Pawn = Cast<APawn>(EffectCauser.Get());
	}
	if (Pawn)
	{
		return Pawn->IsLocallyControlled();
	}
	return false;
}

bool FDNACueParameters::IsInstigatorLocallyControlledPlayer(AActor* FallbackActor) const
{
	// If there is an effect context, just ask it
	if (EffectContext.IsValid())
	{
		return EffectContext.IsLocallyControlledPlayer();
	}
	
	// Look for a pawn and use its controller
	{
		APawn* Pawn = Cast<APawn>(Instigator.Get());
		if (!Pawn)
		{
			// If no instigator, look at effect causer
			Pawn = Cast<APawn>(EffectCauser.Get());
			if (!Pawn && FallbackActor != nullptr)
			{
				// Fallback to passed in actor
				Pawn = Cast<APawn>(FallbackActor);
				if (!Pawn)
				{
					Pawn = FallbackActor->GetInstigator<APawn>();
				}
			}
		}

		if (Pawn && Pawn->Controller)
		{
			return Pawn->Controller->IsLocalPlayerController();
		}
	}

	return false;
}

AActor* FDNACueParameters::GetInstigator() const
{
	if (Instigator.IsValid())
	{
		return Instigator.Get();
	}

	// Fallback to effect context if the explicit data on DNAcue parameters is not there.
	return EffectContext.GetInstigator();
}

AActor* FDNACueParameters::GetEffectCauser() const
{
	if (EffectCauser.IsValid())
	{
		return EffectCauser.Get();
	}

	// Fallback to effect context if the explicit data on DNAcue parameters is not there.
	return EffectContext.GetEffectCauser();
}

const UObject* FDNACueParameters::GetSourceObject() const
{
	if (SourceObject.IsValid())
	{
		return SourceObject.Get();
	}

	// Fallback to effect context if the explicit data on DNAcue parameters is not there.
	return EffectContext.GetSourceObject();
}

bool FMinimalReplicationTagCountMap::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	const int32 CountBits = UDNAAbilitySystemGlobals::Get().MinimalReplicationTagCountBits;
	const int32 MaxCount = ((1 << CountBits)-1);

	if (Ar.IsSaving())
	{
		int32 Count = TagMap.Num();
		if (Count > MaxCount)
		{
			ABILITY_LOG(Error, TEXT("FMinimapReplicationTagCountMap has too many tags (%d). This will cause tags to not replicate. See FMinimapReplicationTagCountMap::NetSerialize"), TagMap.Num());
			Count = MaxCount;
		}

		Ar.SerializeBits(&Count, CountBits);
		for(auto& It : TagMap)
		{
			FDNATag& Tag = It.Key;
			Tag.NetSerialize(Ar, Map, bOutSuccess);
			if (--Count <= 0)
			{
				break;
			}
		}
	}
	else
	{
		int32 Count = TagMap.Num();
		Ar.SerializeBits(&Count, CountBits);

		// Reset our local map
		for(auto& It : TagMap)
		{
			It.Value = 0;
		}

		// See what we have
		while(Count-- > 0)
		{
			FDNATag Tag;
			Tag.NetSerialize(Ar, Map, bOutSuccess);
			TagMap.FindOrAdd(Tag) = 1;
		}

		if (Owner)
		{
			// Update our tags with owner tags
			for(auto& It : TagMap)
			{
				Owner->SetTagMapCount(It.Key, It.Value);
			}
		}
	}


	bOutSuccess = true;
	return true;
}
