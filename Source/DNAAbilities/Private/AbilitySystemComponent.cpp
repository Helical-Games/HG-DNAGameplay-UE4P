// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "AbilitySystemComponent.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Engine/Canvas.h"
#include "DisplayDebugHelpers.h"
#include "Engine/Engine.h"
#include "GameFramework/HUD.h"
#include "AbilitySystemStats.h"
#include "AbilitySystemGlobals.h"
#include "DNACueManager.h"
#include "Net/UnrealNetwork.h"
#include "Engine/ActorChannel.h"
#include "DNAEffectCustomApplicationRequirement.h"

DEFINE_LOG_CATEGORY(LogDNADNAAbilitySystemComponent);

#define LOCTEXT_NAMESPACE "DNADNAAbilitySystemComponent"

/** Enable to log out all render state create, destroy and updatetransform events */
#define LOG_RENDER_STATE 0

UDNADNAAbilitySystemComponent::UDNADNAAbilitySystemComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, DNATagCountContainer()
{
	bWantsInitializeComponent = true;

	PrimaryComponentTick.bStartWithTickEnabled = true; // FIXME! Just temp until timer manager figured out
	bAutoActivate = true;	// Forcing AutoActivate since above we manually force tick enabled.
							// if we don't have this, UpdateShouldTick() fails to have any effect
							// because we'll be receiving ticks but bIsActive starts as false
	
	CachedIsNetSimulated = false;
	UserAbilityActivationInhibited = false;

	GenericConfirmInputID = INDEX_NONE;
	GenericCancelInputID = INDEX_NONE;

	bSuppressGrantAbility = false;
	bSuppressDNACues = false;
	bPendingMontagerep = false;

	AbilityLastActivatedTime = 0.f;

	ReplicationMode = EReplicationMode::Full;
}

const UAttributeSet* UDNADNAAbilitySystemComponent::InitStats(TSubclassOf<class UAttributeSet> Attributes, const UDataTable* DataTable)
{
	const UAttributeSet* AttributeObj = NULL;
	if (Attributes)
	{
		AttributeObj = GetOrCreateAttributeSubobject(Attributes);
		if (AttributeObj && DataTable)
		{
			// This const_cast is OK - this is one of the few places we want to directly modify our AttributeSet properties rather
			// than go through a DNA effect
			const_cast<UAttributeSet*>(AttributeObj)->InitFromMetaDataTable(DataTable);
		}
	}
	return AttributeObj;
}

void UDNADNAAbilitySystemComponent::K2_InitStats(TSubclassOf<class UAttributeSet> Attributes, const UDataTable* DataTable)
{
	InitStats(Attributes, DataTable);
}

const UAttributeSet* UDNADNAAbilitySystemComponent::GetOrCreateAttributeSubobject(TSubclassOf<UAttributeSet> AttributeClass)
{
	AActor *OwningActor = GetOwner();
	const UAttributeSet *MyAttributes  = NULL;
	if (OwningActor && AttributeClass)
	{
		MyAttributes = GetAttributeSubobject(AttributeClass);
		if (!MyAttributes)
		{
			UAttributeSet *Attributes = NewObject<UAttributeSet>(OwningActor, AttributeClass);
			SpawnedAttributes.AddUnique(Attributes);
			MyAttributes = Attributes;
		}
	}

	return MyAttributes;
}

const UAttributeSet* UDNADNAAbilitySystemComponent::GetAttributeSubobjectChecked(const TSubclassOf<UAttributeSet> AttributeClass) const
{
	const UAttributeSet *Set = GetAttributeSubobject(AttributeClass);
	check(Set);
	return Set;
}

const UAttributeSet* UDNADNAAbilitySystemComponent::GetAttributeSubobject(const TSubclassOf<UAttributeSet> AttributeClass) const
{
	for (const UAttributeSet* Set : SpawnedAttributes)
	{
		if (Set && Set->IsA(AttributeClass))
		{
			return Set;
		}
	}
	return NULL;
}

bool UDNADNAAbilitySystemComponent::HasAttributeSetForAttribute(FDNAAttribute Attribute) const
{
	return (Attribute.IsValid() && (Attribute.IsSystemAttribute() || GetAttributeSubobject(Attribute.GetAttributeSetClass()) != nullptr));
}

void UDNADNAAbilitySystemComponent::GetAllAttributes(OUT TArray<FDNAAttribute>& Attributes)
{
	for (UAttributeSet* Set : SpawnedAttributes)
	{
		for ( TFieldIterator<UProperty> It(Set->GetClass()); It; ++It)
		{
			if (UFloatProperty* FloatProperty = Cast<UFloatProperty>(*It))
			{
				Attributes.Push( FDNAAttribute(FloatProperty) );
			}
		}
	}
}

void UDNADNAAbilitySystemComponent::OnRegister()
{
	Super::OnRegister();

	// Cached off netrole to avoid constant checking on owning actor
	CachedIsNetSimulated = IsNetSimulating();

	// Init starting data
	for (int32 i=0; i < DefaultStartingData.Num(); ++i)
	{
		if (DefaultStartingData[i].Attributes && DefaultStartingData[i].DefaultStartingTable)
		{
			UAttributeSet* Attributes = const_cast<UAttributeSet*>(GetOrCreateAttributeSubobject(DefaultStartingData[i].Attributes));
			Attributes->InitFromMetaDataTable(DefaultStartingData[i].DefaultStartingTable);
		}
	}

	ActiveDNAEffects.RegisterWithOwner(this);
	ActivatableAbilities.RegisterWithOwner(this);
	ActiveDNACues.Owner = this;
	ActiveDNACues.bMinimalReplication = false;
	MinimalReplicationDNACues.Owner = this;
	MinimalReplicationDNACues.bMinimalReplication = true;
	MinimalReplicationTags.Owner = this;

	/** Allocate an AbilityActorInfo. Note: this goes through a global function and is a SharedPtr so projects can make their own AbilityActorInfo */
	AbilityActorInfo = TSharedPtr<FDNAAbilityActorInfo>(UDNADNAAbilitySystemGlobals::Get().AllocAbilityActorInfo());
}

void UDNADNAAbilitySystemComponent::OnUnregister()
{
	Super::OnUnregister();

	DestroyActiveState();
}

void UDNADNAAbilitySystemComponent::BeginPlay()
{
	Super::BeginPlay();

	// Cache net role here as well since for map-placed actors on clients, the Role may not be set correctly yet in OnRegister.
	CachedIsNetSimulated = IsNetSimulating();
	ActiveDNAEffects.OwnerIsNetAuthority = !CachedIsNetSimulated;
}

// ---------------------------------------------------------

const FActiveDNAEffect* UDNADNAAbilitySystemComponent::GetActiveDNAEffect(const FActiveDNAEffectHandle Handle) const
{
	return ActiveDNAEffects.GetActiveDNAEffect(Handle);
}

bool UDNADNAAbilitySystemComponent::HasNetworkAuthorityToApplyDNAEffect(FPredictionKey PredictionKey) const
{
	return (IsOwnerActorAuthoritative() || PredictionKey.IsValidForMorePrediction());
}

void UDNADNAAbilitySystemComponent::SetNumericAttributeBase(const FDNAAttribute &Attribute, float NewFloatValue)
{
	// Go through our active DNA effects container so that aggregation/mods are handled properly.
	ActiveDNAEffects.SetAttributeBaseValue(Attribute, NewFloatValue);
}

float UDNADNAAbilitySystemComponent::GetNumericAttributeBase(const FDNAAttribute &Attribute) const
{
	if (Attribute.IsSystemAttribute())
	{
		return 0.f;
	}

	return ActiveDNAEffects.GetAttributeBaseValue(Attribute);
}

void UDNADNAAbilitySystemComponent::SetNumericAttribute_Internal(const FDNAAttribute &Attribute, float& NewFloatValue)
{
	// Set the attribute directly: update the UProperty on the attribute set.
	const UAttributeSet* AttributeSet = GetAttributeSubobjectChecked(Attribute.GetAttributeSetClass());
	Attribute.SetNumericValueChecked(NewFloatValue, const_cast<UAttributeSet*>(AttributeSet));
}

float UDNADNAAbilitySystemComponent::GetNumericAttribute(const FDNAAttribute &Attribute) const
{
	if (Attribute.IsSystemAttribute())
	{
		return 0.f;
	}

	const UAttributeSet* const AttributeSetOrNull = GetAttributeSubobject(Attribute.GetAttributeSetClass());
	if (AttributeSetOrNull == nullptr)
	{
		return 0.f;
	}

	return Attribute.GetNumericValue(AttributeSetOrNull);
}

float UDNADNAAbilitySystemComponent::GetNumericAttributeChecked(const FDNAAttribute &Attribute) const
{
	if(Attribute.IsSystemAttribute())
	{
		return 0.f;
	}

	const UAttributeSet* AttributeSet = GetAttributeSubobjectChecked(Attribute.GetAttributeSetClass());
	return Attribute.GetNumericValueChecked(AttributeSet);
}

void UDNADNAAbilitySystemComponent::ApplyModToAttribute(const FDNAAttribute &Attribute, TEnumAsByte<EDNAModOp::Type> ModifierOp, float ModifierMagnitude)
{
	// We can only apply loose mods on the authority. If we ever need to predict these, they would need to be turned into GEs and be given a prediction key so that
	// they can be rolled back.
	if (IsOwnerActorAuthoritative())
	{
		ActiveDNAEffects.ApplyModToAttribute(Attribute, ModifierOp, ModifierMagnitude);
	}
}

void UDNADNAAbilitySystemComponent::ApplyModToAttributeUnsafe(const FDNAAttribute &Attribute, TEnumAsByte<EDNAModOp::Type> ModifierOp, float ModifierMagnitude)
{
	ActiveDNAEffects.ApplyModToAttribute(Attribute, ModifierOp, ModifierMagnitude);
}

FDNAEffectSpecHandle UDNADNAAbilitySystemComponent::MakeOutgoingSpec(TSubclassOf<UDNAEffect> DNAEffectClass, float Level, FDNAEffectContextHandle Context) const
{
	SCOPE_CYCLE_COUNTER(STAT_GetOutgoingSpec);
	if (Context.IsValid() == false)
	{
		Context = MakeEffectContext();
	}

	if (DNAEffectClass)
	{
		UDNAEffect* DNAEffect = DNAEffectClass->GetDefaultObject<UDNAEffect>();

		FDNAEffectSpec* NewSpec = new FDNAEffectSpec(DNAEffect, Context, Level);
		return FDNAEffectSpecHandle(NewSpec);
	}

	return FDNAEffectSpecHandle(nullptr);
}

FDNAEffectContextHandle UDNADNAAbilitySystemComponent::MakeEffectContext() const
{
	FDNAEffectContextHandle Context = FDNAEffectContextHandle(UDNADNAAbilitySystemGlobals::Get().AllocDNAEffectContext());
	// By default use the owner and avatar as the instigator and causer
	check(AbilityActorInfo.IsValid());
	
	Context.AddInstigator(AbilityActorInfo->OwnerActor.Get(), AbilityActorInfo->AvatarActor.Get());
	return Context;
}

int32 UDNADNAAbilitySystemComponent::GetDNAEffectCount(TSubclassOf<UDNAEffect> SourceDNAEffect, UDNADNAAbilitySystemComponent* OptionalInstigatorFilterComponent, bool bEnforceOnGoingCheck)
{
	int32 Count = 0;

	if (SourceDNAEffect)
	{
		FDNAEffectQuery Query;
		Query.CustomMatchDelegate.BindLambda([&](const FActiveDNAEffect& CurEffect)
		{
			bool bMatches = false;

			// First check at matching: backing GE class must be the exact same
			if (CurEffect.Spec.Def && SourceDNAEffect == CurEffect.Spec.Def->GetClass())
			{
				// If an instigator is specified, matching is dependent upon it
				if (OptionalInstigatorFilterComponent)
				{
					bMatches = (OptionalInstigatorFilterComponent == CurEffect.Spec.GetEffectContext().GetInstigatorDNADNAAbilitySystemComponent());
				}
				else
				{
					bMatches = true;
				}
			}

			return bMatches;
		});

		Count = ActiveDNAEffects.GetActiveEffectCount(Query, bEnforceOnGoingCheck);
	}

	return Count;
}

int32 UDNADNAAbilitySystemComponent::GetAggregatedStackCount(const FDNAEffectQuery& Query)
{
	return ActiveDNAEffects.GetActiveEffectCount(Query);
}

FActiveDNAEffectHandle UDNADNAAbilitySystemComponent::BP_ApplyDNAEffectToTarget(TSubclassOf<UDNAEffect> DNAEffectClass, UDNADNAAbilitySystemComponent* Target, float Level, FDNAEffectContextHandle Context)
{
	if (Target == nullptr)
	{
		ABILITY_LOG(Log, TEXT("UDNADNAAbilitySystemComponent::BP_ApplyDNAEffectToTarget called with null Target. %s. Context: %s"), *GetFullName(), *Context.ToString());
		return FActiveDNAEffectHandle();
	}

	if (DNAEffectClass == nullptr)
	{
		ABILITY_LOG(Error, TEXT("UDNADNAAbilitySystemComponent::BP_ApplyDNAEffectToTarget called with null DNAEffectClass. %s. Context: %s"), *GetFullName(), *Context.ToString());
		return FActiveDNAEffectHandle();
	}

	UDNAEffect* DNAEffect = DNAEffectClass->GetDefaultObject<UDNAEffect>();
	return ApplyDNAEffectToTarget(DNAEffect, Target, Level, Context);	
}

/** This is a helper function used in automated testing, I'm not sure how useful it will be to gamecode or blueprints */
FActiveDNAEffectHandle UDNADNAAbilitySystemComponent::ApplyDNAEffectToTarget(UDNAEffect *DNAEffect, UDNADNAAbilitySystemComponent *Target, float Level, FDNAEffectContextHandle Context, FPredictionKey PredictionKey)
{
	check(DNAEffect);
	if (HasNetworkAuthorityToApplyDNAEffect(PredictionKey))
	{
		if (!Context.IsValid())
		{
			Context = MakeEffectContext();
		}

		FDNAEffectSpec	Spec(DNAEffect, Context, Level);
		return ApplyDNAEffectSpecToTarget(Spec, Target, PredictionKey);
	}

	return FActiveDNAEffectHandle();
}

/** Helper function since we can't have default/optional values for FModifierQualifier in K2 function */
FActiveDNAEffectHandle UDNADNAAbilitySystemComponent::BP_ApplyDNAEffectToSelf(TSubclassOf<UDNAEffect> DNAEffectClass, float Level, FDNAEffectContextHandle EffectContext)
{
	if ( DNAEffectClass )
	{
		UDNAEffect* DNAEffect = DNAEffectClass->GetDefaultObject<UDNAEffect>();
		return ApplyDNAEffectToSelf(DNAEffect, Level, EffectContext);
	}

	return FActiveDNAEffectHandle();
}

/** This is a helper function - it seems like this will be useful as a blueprint interface at the least, but Level parameter may need to be expanded */
FActiveDNAEffectHandle UDNADNAAbilitySystemComponent::ApplyDNAEffectToSelf(const UDNAEffect *DNAEffect, float Level, const FDNAEffectContextHandle& EffectContext, FPredictionKey PredictionKey)
{
	if (DNAEffect == nullptr)
	{
		ABILITY_LOG(Error, TEXT("UDNADNAAbilitySystemComponent::ApplyDNAEffectToSelf called by Instigator %s with a null DNAEffect."), *EffectContext.ToString());
		return FActiveDNAEffectHandle();
	}

	if (HasNetworkAuthorityToApplyDNAEffect(PredictionKey))
	{
		FDNAEffectSpec	Spec(DNAEffect, EffectContext, Level);
		return ApplyDNAEffectSpecToSelf(Spec, PredictionKey);
	}

	return FActiveDNAEffectHandle();
}

FOnActiveDNAEffectRemoved* UDNADNAAbilitySystemComponent::OnDNAEffectRemovedDelegate(FActiveDNAEffectHandle Handle)
{
	FActiveDNAEffect* ActiveEffect = ActiveDNAEffects.GetActiveDNAEffect(Handle);
	if (ActiveEffect)
	{
		return &ActiveEffect->OnRemovedDelegate;
	}

	return nullptr;
}

FOnGivenActiveDNAEffectRemoved& UDNADNAAbilitySystemComponent::OnAnyDNAEffectRemovedDelegate()
{
	return ActiveDNAEffects.OnActiveDNAEffectRemovedDelegate;
}

FOnActiveDNAEffectStackChange* UDNADNAAbilitySystemComponent::OnDNAEffectStackChangeDelegate(FActiveDNAEffectHandle Handle)
{
	FActiveDNAEffect* ActiveEffect = ActiveDNAEffects.GetActiveDNAEffect(Handle);
	if (ActiveEffect)
	{
		return &ActiveEffect->OnStackChangeDelegate;
	}

	return nullptr;
}

FOnActiveDNAEffectTimeChange* UDNADNAAbilitySystemComponent::OnDNAEffectTimeChangeDelegate(FActiveDNAEffectHandle Handle)
{
	FActiveDNAEffect* ActiveEffect = ActiveDNAEffects.GetActiveDNAEffect(Handle);
	if (ActiveEffect)
	{
		return &ActiveEffect->OnTimeChangeDelegate;
	}

	return nullptr;
}

FOnDNAEffectTagCountChanged& UDNADNAAbilitySystemComponent::RegisterDNATagEvent(FDNATag Tag, EDNATagEventType::Type EventType)
{
	return DNATagCountContainer.RegisterDNATagEvent(Tag, EventType);
}

void UDNADNAAbilitySystemComponent::RegisterAndCallDNATagEvent(FDNATag Tag, FOnDNAEffectTagCountChanged::FDelegate Delegate, EDNATagEventType::Type EventType)
{
	DNATagCountContainer.RegisterDNATagEvent(Tag, EventType).Add(Delegate);

	const int32 TagCount = GetTagCount(Tag);
	if (TagCount > 0)
	{
		Delegate.Execute(Tag, TagCount);
	}
}

FOnDNAEffectTagCountChanged& UDNADNAAbilitySystemComponent::RegisterGenericDNATagEvent()
{
	return DNATagCountContainer.RegisterGenericDNAEvent();
}

FOnDNAAttributeChange& UDNADNAAbilitySystemComponent::RegisterDNAAttributeEvent(FDNAAttribute Attribute)
{
	return ActiveDNAEffects.RegisterDNAAttributeEvent(Attribute);
}

UProperty* UDNADNAAbilitySystemComponent::GetOutgoingDurationProperty()
{
	static UProperty* DurationProperty = FindFieldChecked<UProperty>(UDNADNAAbilitySystemComponent::StaticClass(), GET_MEMBER_NAME_CHECKED(UDNADNAAbilitySystemComponent, OutgoingDuration));
	return DurationProperty;
}

UProperty* UDNADNAAbilitySystemComponent::GetIncomingDurationProperty()
{
	static UProperty* DurationProperty = FindFieldChecked<UProperty>(UDNADNAAbilitySystemComponent::StaticClass(), GET_MEMBER_NAME_CHECKED(UDNADNAAbilitySystemComponent, IncomingDuration));
	return DurationProperty;
}

const FDNAEffectAttributeCaptureDefinition& UDNADNAAbilitySystemComponent::GetOutgoingDurationCapture()
{
	// We will just always take snapshots of the source's duration mods
	static FDNAEffectAttributeCaptureDefinition OutgoingDurationCapture(GetOutgoingDurationProperty(), EDNAEffectAttributeCaptureSource::Source, true);
	return OutgoingDurationCapture;

}
const FDNAEffectAttributeCaptureDefinition& UDNADNAAbilitySystemComponent::GetIncomingDurationCapture()
{
	// Never take snapshots of the target's duration mods: we are going to evaluate this on apply only.
	static FDNAEffectAttributeCaptureDefinition IncomingDurationCapture(GetIncomingDurationProperty(), EDNAEffectAttributeCaptureSource::Target, false);
	return IncomingDurationCapture;
}

// ------------------------------------------------------------------------

void UDNADNAAbilitySystemComponent::ResetTagMap()
{
	DNATagCountContainer.Reset();
}

void UDNADNAAbilitySystemComponent::NotifyTagMap_StackCountChange(const FDNATagContainer& Container)
{
	for (auto TagIt = Container.CreateConstIterator(); TagIt; ++TagIt)
	{
		const FDNATag& Tag = *TagIt;
		DNATagCountContainer.Notify_StackCountChange(Tag);
	}
}

// ------------------------------------------------------------------------

FActiveDNAEffectHandle UDNADNAAbilitySystemComponent::ApplyDNAEffectSpecToTarget(OUT FDNAEffectSpec &Spec, UDNADNAAbilitySystemComponent *Target, FPredictionKey PredictionKey)
{
	if (!UDNADNAAbilitySystemGlobals::Get().ShouldPredictTargetDNAEffects())
	{
		// If we don't want to predict target effects, clear prediction key
		PredictionKey = FPredictionKey();
	}

	FActiveDNAEffectHandle ReturnHandle;

	if (!UDNADNAAbilitySystemGlobals::Get().ShouldPredictTargetDNAEffects())
	{
		// If we don't want to predict target effects, clear prediction key
		PredictionKey = FPredictionKey();
	}

	if (Target)
	{
		ReturnHandle = Target->ApplyDNAEffectSpecToSelf(Spec, PredictionKey);
	}

	return ReturnHandle;
}

FActiveDNAEffectHandle UDNADNAAbilitySystemComponent::ApplyDNAEffectSpecToSelf(OUT FDNAEffectSpec &Spec, FPredictionKey PredictionKey)
{
	// Scope lock the container after the addition has taken place to prevent the new effect from potentially getting mangled during the remainder
	// of the add operation
	FScopedActiveDNAEffectLock ScopeLock(ActiveDNAEffects);

	const bool bIsNetAuthority = IsOwnerActorAuthoritative();

	// Check Network Authority
	if (!HasNetworkAuthorityToApplyDNAEffect(PredictionKey))
	{
		return FActiveDNAEffectHandle();
	}

	// Don't allow prediction of periodic effects
	if(PredictionKey.IsValidKey() && Spec.GetPeriod() > 0.f)
	{
		if(IsOwnerActorAuthoritative())
		{
			// Server continue with invalid prediction key
			PredictionKey = FPredictionKey();
		}
		else
		{
			// Client just return now
			return FActiveDNAEffectHandle();
		}
	}

	// Are we currently immune to this? (ApplicationImmunity)
	const FActiveDNAEffect* ImmunityGE=nullptr;
	if (ActiveDNAEffects.HasApplicationImmunityToSpec(Spec, ImmunityGE))
	{
		OnImmunityBlockDNAEffect(Spec, ImmunityGE);
		return FActiveDNAEffectHandle();
	}

	// Check AttributeSet requirements: make sure all attributes are valid
	// We may want to cache this off in some way to make the runtime check quicker.
	// We also need to handle things in the execution list
	for (const FDNAModifierInfo& Mod : Spec.Def->Modifiers)
	{
		if (!Mod.Attribute.IsValid())
		{
			ABILITY_LOG(Warning, TEXT("%s has a null modifier attribute."), *Spec.Def->GetPathName());
			return FActiveDNAEffectHandle();
		}
	}

	// check if the effect being applied actually succeeds
	float ChanceToApply = Spec.GetChanceToApplyToTarget();
	if ((ChanceToApply < 1.f - SMALL_NUMBER) && (FMath::FRand() > ChanceToApply))
	{
		return FActiveDNAEffectHandle();
	}

	// Get MyTags.
	//	We may want to cache off a DNATagContainer instead of rebuilding it every time.
	//	But this will also be where we need to merge in context tags? (Headshot, executing ability, etc?)
	//	Or do we push these tags into (our copy of the spec)?

	{
		// Note: static is ok here since the scope is so limited, but wider usage of MyTags is not safe since this function can be recursively called
		static FDNATagContainer MyTags;
		MyTags.Reset();

		GetOwnedDNATags(MyTags);

		if (Spec.Def->ApplicationTagRequirements.RequirementsMet(MyTags) == false)
		{
			return FActiveDNAEffectHandle();
		}
	}

	// Custom application requirement check
	for (const TSubclassOf<UDNAEffectCustomApplicationRequirement>& AppReq : Spec.Def->ApplicationRequirements)
	{
		if (*AppReq && AppReq->GetDefaultObject<UDNAEffectCustomApplicationRequirement>()->CanApplyDNAEffect(Spec.Def, Spec, this) == false)
		{
			return FActiveDNAEffectHandle();
		}
	}

	// Clients should treat predicted instant effects as if they have infinite duration. The effects will be cleaned up later.
	bool bTreatAsInfiniteDuration = GetOwnerRole() != ROLE_Authority && PredictionKey.IsLocalClientKey() && Spec.Def->DurationPolicy == EDNAEffectDurationType::Instant;

	// Make sure we create our copy of the spec in the right place
	// We initialize the FActiveDNAEffectHandle here with INDEX_NONE to handle the case of instant GE
	// Initializing it like this will set the bPassedFiltersAndWasExecuted on the FActiveDNAEffectHandle to true so we can know that we applied a GE
	FActiveDNAEffectHandle	MyHandle(INDEX_NONE);
	bool bInvokeDNACueApplied = Spec.Def->DurationPolicy != EDNAEffectDurationType::Instant; // Cache this now before possibly modifying predictive instant effect to infinite duration effect.
	bool bFoundExistingStackableGE = false;

	FActiveDNAEffect* AppliedEffect = nullptr;

	FDNAEffectSpec* OurCopyOfSpec = nullptr;
	TSharedPtr<FDNAEffectSpec> StackSpec;
	{
		if (Spec.Def->DurationPolicy != EDNAEffectDurationType::Instant || bTreatAsInfiniteDuration)
		{
			AppliedEffect = ActiveDNAEffects.ApplyDNAEffectSpec(Spec, PredictionKey, bFoundExistingStackableGE);
			if (!AppliedEffect)
			{
				return FActiveDNAEffectHandle();
			}

			MyHandle = AppliedEffect->Handle;
			OurCopyOfSpec = &(AppliedEffect->Spec);

			// Log results of applied GE spec
			if (UE_LOG_ACTIVE(VLogDNADNAAbilitySystem, Log))
			{
				ABILITY_VLOG(OwnerActor, Log, TEXT("Applied %s"), *OurCopyOfSpec->Def->GetFName().ToString());

				for (FDNAModifierInfo Modifier : Spec.Def->Modifiers)
				{
					float Magnitude = 0.f;
					Modifier.ModifierMagnitude.AttemptCalculateMagnitude(Spec, Magnitude);
					ABILITY_VLOG(OwnerActor, Log, TEXT("         %s: %s %f"), *Modifier.Attribute.GetName(), *EDNAModOpToString(Modifier.ModifierOp), Magnitude);
				}
			}
		}

		if (!OurCopyOfSpec)
		{
			StackSpec = TSharedPtr<FDNAEffectSpec>(new FDNAEffectSpec(Spec));
			OurCopyOfSpec = StackSpec.Get();
			UDNADNAAbilitySystemGlobals::Get().GlobalPreDNAEffectSpecApply(*OurCopyOfSpec, this);
			OurCopyOfSpec->CaptureAttributeDataFromTarget(this);
		}

		// if necessary add a modifier to OurCopyOfSpec to force it to have an infinite duration
		if (bTreatAsInfiniteDuration)
		{
			// This should just be a straight set of the duration float now
			OurCopyOfSpec->SetDuration(UDNAEffect::INFINITE_DURATION, true);
		}
	}
	

	// We still probably want to apply tags and stuff even if instant?
	// If bSuppressStackingCues is set for this DNAEffect, only add the DNACue if this is the first instance of the DNAEffect
	if (!bSuppressDNACues && bInvokeDNACueApplied && AppliedEffect && !AppliedEffect->bIsInhibited && 
		(!bFoundExistingStackableGE || !Spec.Def->bSuppressStackingCues))
	{
		// We both added and activated the DNACue here.
		// On the client, who will invoke the DNA cue from an OnRep, he will need to look at the StartTime to determine
		// if the Cue was actually added+activated or just added (due to relevancy)

		// Fixme: what if we wanted to scale Cue magnitude based on damage? E.g, scale an cue effect when the GE is buffed?

		if (OurCopyOfSpec->StackCount > Spec.StackCount)
		{
			// Because PostReplicatedChange will get called from modifying the stack count
			// (and not PostReplicatedAdd) we won't know which GE was modified.
			// So instead we need to explicitly RPC the client so it knows the GC needs updating
			UDNADNAAbilitySystemGlobals::Get().GetDNACueManager()->InvokeDNACueAddedAndWhileActive_FromSpec(this, *OurCopyOfSpec, PredictionKey);
		}
		else
		{
			// Otherwise these will get replicated to the client when the GE gets added to the replicated array
			InvokeDNACueEvent(*OurCopyOfSpec, EDNACueEvent::OnActive);
			InvokeDNACueEvent(*OurCopyOfSpec, EDNACueEvent::WhileActive);
		}
	}
	
	// Execute the GE at least once (if instant, this will execute once and be done. If persistent, it was added to ActiveDNAEffects above)
	
	// Execute if this is an instant application effect
	if (bTreatAsInfiniteDuration)
	{
		// This is an instant application but we are treating it as an infinite duration for prediction. We should still predict the execute DNACUE.
		// (in non predictive case, this will happen inside ::ExecuteDNAEffect)

		if (!bSuppressDNACues)
		{
			UDNADNAAbilitySystemGlobals::Get().GetDNACueManager()->InvokeDNACueExecuted_FromSpec(this, *OurCopyOfSpec, PredictionKey);
		}
	}
	else if (Spec.Def->DurationPolicy == EDNAEffectDurationType::Instant)
	{
		if (OurCopyOfSpec->Def->OngoingTagRequirements.IsEmpty())
		{
			ExecuteDNAEffect(*OurCopyOfSpec, PredictionKey);
		}
		else
		{
			ABILITY_LOG(Warning, TEXT("%s is instant but has tag requirements. Tag requirements can only be used with DNA effects that have a duration. This DNA effect will be ignored."), *Spec.Def->GetPathName());
		}
	}

	if (Spec.GetPeriod() != UDNAEffect::NO_PERIOD && Spec.TargetEffectSpecs.Num() > 0)
	{
		ABILITY_LOG(Warning, TEXT("%s is periodic but also applies DNAEffects to its target. DNAEffects will only be applied once, not every period."), *Spec.Def->GetPathName());
	}

	// ------------------------------------------------------
	//	Remove DNA effects with tags
	//		Remove any active DNA effects that match the RemoveDNAEffectsWithTags in the definition for this spec
	//		Only call this if we are the Authoritative owner and we have some RemoveDNAEffectsWithTags.CombinedTag to remove
	// ------------------------------------------------------
	if (bIsNetAuthority && Spec.Def->RemoveDNAEffectsWithTags.CombinedTags.Num() > 0)
	{
		// Clear tags is always removing all stacks.
		FDNAEffectQuery ClearQuery = FDNAEffectQuery::MakeQuery_MatchAnyOwningTags(Spec.Def->RemoveDNAEffectsWithTags.CombinedTags);
		if (MyHandle.IsValid())
		{
			ClearQuery.IgnoreHandles.Add(MyHandle);
		}
		ActiveDNAEffects.RemoveActiveEffects(ClearQuery, -1);
	}
	
	// ------------------------------------------------------
	// Apply Linked effects
	// todo: this is ignoring the returned handles, should we put them into a TArray and return all of the handles?
	// ------------------------------------------------------
	for (const FDNAEffectSpecHandle TargetSpec: Spec.TargetEffectSpecs)
	{
		if (TargetSpec.IsValid())
		{
			ApplyDNAEffectSpecToSelf(*TargetSpec.Data.Get(), PredictionKey);
		}
	}

	UDNADNAAbilitySystemComponent* InstigatorASC = Spec.GetContext().GetInstigatorDNADNAAbilitySystemComponent();

	// Send ourselves a callback	
	OnDNAEffectAppliedToSelf(InstigatorASC, *OurCopyOfSpec, MyHandle);

	// Send the instigator a callback
	if (InstigatorASC)
	{
		InstigatorASC->OnDNAEffectAppliedToTarget(this, *OurCopyOfSpec, MyHandle);
	}

	return MyHandle;
}

FActiveDNAEffectHandle UDNADNAAbilitySystemComponent::BP_ApplyDNAEffectSpecToTarget(FDNAEffectSpecHandle& SpecHandle, UDNADNAAbilitySystemComponent* Target)
{
	FActiveDNAEffectHandle ReturnHandle;
	if (SpecHandle.IsValid() && Target)
	{
		ReturnHandle = ApplyDNAEffectSpecToTarget(*SpecHandle.Data.Get(), Target);
	}

	return ReturnHandle;
}

FActiveDNAEffectHandle UDNADNAAbilitySystemComponent::BP_ApplyDNAEffectSpecToSelf(FDNAEffectSpecHandle& SpecHandle)
{
	FActiveDNAEffectHandle ReturnHandle;
	if (SpecHandle.IsValid())
	{
		ReturnHandle = ApplyDNAEffectSpecToSelf(*SpecHandle.Data.Get());
	}

	return ReturnHandle;
}

void UDNADNAAbilitySystemComponent::ExecutePeriodicEffect(FActiveDNAEffectHandle	Handle)
{
	ActiveDNAEffects.ExecutePeriodicDNAEffect(Handle);
}

void UDNADNAAbilitySystemComponent::ExecuteDNAEffect(FDNAEffectSpec &Spec, FPredictionKey PredictionKey)
{
	// Should only ever execute effects that are instant application or periodic application
	// Effects with no period and that aren't instant application should never be executed
	check( (Spec.GetDuration() == UDNAEffect::INSTANT_APPLICATION || Spec.GetPeriod() != UDNAEffect::NO_PERIOD) );

	if (UE_LOG_ACTIVE(VLogDNADNAAbilitySystem, Log))
	{
		ABILITY_VLOG(OwnerActor, Log, TEXT("Executed %s"), *Spec.Def->GetFName().ToString());
		
		for (FDNAModifierInfo Modifier : Spec.Def->Modifiers)
		{
			float Magnitude = 0.f;
			Modifier.ModifierMagnitude.AttemptCalculateMagnitude(Spec, Magnitude);
			ABILITY_VLOG(OwnerActor, Log, TEXT("         %s: %s %f"), *Modifier.Attribute.GetName(), *EDNAModOpToString(Modifier.ModifierOp), Magnitude);
		}
	}

	ActiveDNAEffects.ExecuteActiveEffectsFrom(Spec, PredictionKey);
}

void UDNADNAAbilitySystemComponent::CheckDurationExpired(FActiveDNAEffectHandle Handle)
{
	ActiveDNAEffects.CheckDuration(Handle);
}

const UDNAEffect* UDNADNAAbilitySystemComponent::GetDNAEffectDefForHandle(FActiveDNAEffectHandle Handle)
{
	FActiveDNAEffect* ActiveGE = ActiveDNAEffects.GetActiveDNAEffect(Handle);
	if (ActiveGE)
	{
		return ActiveGE->Spec.Def;
	}

	return nullptr;
}

bool UDNADNAAbilitySystemComponent::RemoveActiveDNAEffect(FActiveDNAEffectHandle Handle, int32 StacksToRemove)
{
	return ActiveDNAEffects.RemoveActiveDNAEffect(Handle, StacksToRemove);
}

void UDNADNAAbilitySystemComponent::RemoveActiveDNAEffectBySourceEffect(TSubclassOf<UDNAEffect> DNAEffect, UDNADNAAbilitySystemComponent* InstigatorDNADNAAbilitySystemComponent, int32 StacksToRemove /*= -1*/)
{
	if (DNAEffect)
	{
		FDNAEffectQuery Query;
		Query.CustomMatchDelegate.BindLambda([&](const FActiveDNAEffect& CurEffect)
		{
			bool bMatches = false;

			// First check at matching: backing GE class must be the exact same
			if (CurEffect.Spec.Def && DNAEffect == CurEffect.Spec.Def->GetClass())
			{
				// If an instigator is specified, matching is dependent upon it
				if (InstigatorDNADNAAbilitySystemComponent)
				{
					bMatches = (InstigatorDNADNAAbilitySystemComponent == CurEffect.Spec.GetEffectContext().GetInstigatorDNADNAAbilitySystemComponent());
				}
				else
				{
					bMatches = true;
				}
			}

			return bMatches;
		});

		ActiveDNAEffects.RemoveActiveEffects(Query, StacksToRemove);
	}
}

float UDNADNAAbilitySystemComponent::GetDNAEffectDuration(FActiveDNAEffectHandle Handle) const
{
	float StartEffectTime = 0.0f;
	float Duration = 0.0f;
	ActiveDNAEffects.GetDNAEffectStartTimeAndDuration(Handle, StartEffectTime, Duration);

	return Duration;
}

void UDNADNAAbilitySystemComponent::GetDNAEffectStartTimeAndDuration(FActiveDNAEffectHandle Handle, float& StartEffectTime, float& Duration) const
{
	return ActiveDNAEffects.GetDNAEffectStartTimeAndDuration(Handle, StartEffectTime, Duration);
}

float UDNADNAAbilitySystemComponent::GetDNAEffectMagnitude(FActiveDNAEffectHandle Handle, FDNAAttribute Attribute) const
{
	return ActiveDNAEffects.GetDNAEffectMagnitude(Handle, Attribute);
}

void UDNADNAAbilitySystemComponent::SetActiveDNAEffectLevel(FActiveDNAEffectHandle ActiveHandle, int32 NewLevel)
{
	ActiveDNAEffects.SetActiveDNAEffectLevel(ActiveHandle, NewLevel);
}

void UDNADNAAbilitySystemComponent::SetActiveDNAEffectLevelUsingQuery(FDNAEffectQuery Query, int32 NewLevel)
{
	TArray<FActiveDNAEffectHandle> ActiveDNAEffectHandles = ActiveDNAEffects.GetActiveEffects(Query);
	for (FActiveDNAEffectHandle ActiveHandle : ActiveDNAEffectHandles)
	{
		SetActiveDNAEffectLevel(ActiveHandle, NewLevel);
	}
}

int32 UDNADNAAbilitySystemComponent::GetCurrentStackCount(FActiveDNAEffectHandle Handle) const
{
	if (const FActiveDNAEffect* ActiveGE = ActiveDNAEffects.GetActiveDNAEffect(Handle))
	{
		return ActiveGE->Spec.StackCount;
	}
	return 0;
}

int32 UDNADNAAbilitySystemComponent::GetCurrentStackCount(FDNAAbilitySpecHandle Handle) const
{
	FActiveDNAEffectHandle GEHandle = FindActiveDNAEffectHandle(Handle);
	if (GEHandle.IsValid())
	{
		return GetCurrentStackCount(GEHandle);
	}
	return 0;
}

FString UDNADNAAbilitySystemComponent::GetActiveGEDebugString(FActiveDNAEffectHandle Handle) const
{
	FString Str;

	if (const FActiveDNAEffect* ActiveGE = ActiveDNAEffects.GetActiveDNAEffect(Handle))
	{
		Str = FString::Printf(TEXT("%s - (Level: %.2f. Stacks: %d)"), *ActiveGE->Spec.Def->GetName(), ActiveGE->Spec.GetLevel(), ActiveGE->Spec.StackCount);
	}

	return Str;
}

FActiveDNAEffectHandle UDNADNAAbilitySystemComponent::FindActiveDNAEffectHandle(FDNAAbilitySpecHandle Handle) const
{
	for (const FActiveDNAEffect& ActiveGE : &ActiveDNAEffects)
	{
		for (const FDNAAbilitySpecDef& AbilitySpecDef : ActiveGE.Spec.GrantedAbilitySpecs)
		{
			if (AbilitySpecDef.AssignedHandle == Handle)
			{
				return ActiveGE.Handle;
			}
		}
	}
	return FActiveDNAEffectHandle();
}

void UDNADNAAbilitySystemComponent::OnImmunityBlockDNAEffect(const FDNAEffectSpec& Spec, const FActiveDNAEffect* ImmunityGE)
{
	OnImmunityBlockDNAEffectDelegate.Broadcast(Spec, ImmunityGE);
}

void UDNADNAAbilitySystemComponent::InitDefaultDNACueParameters(FDNACueParameters& Parameters)
{
	Parameters.Instigator = OwnerActor;
	Parameters.EffectCauser = AvatarActor;
}

void UDNADNAAbilitySystemComponent::InvokeDNACueEvent(const FDNAEffectSpecForRPC &Spec, EDNACueEvent::Type EventType)
{
	AActor* ActorAvatar = AbilityActorInfo->AvatarActor.Get();
	if (ActorAvatar == nullptr && !bSuppressDNACues)
	{
		// No avatar actor to call this DNAcue on.
		return;
	}

	if (!Spec.Def)
	{
		ABILITY_LOG(Warning, TEXT("InvokeDNACueEvent Actor %s that has no DNA effect!"), ActorAvatar ? *ActorAvatar->GetName() : TEXT("NULL"));
		return;
	}
	
	float ExecuteLevel = Spec.GetLevel();

	FDNACueParameters CueParameters(Spec);

	for (FDNAEffectCue CueInfo : Spec.Def->DNACues)
	{
		if (CueInfo.MagnitudeAttribute.IsValid())
		{
			if (const FDNAEffectModifiedAttribute* ModifiedAttribute = Spec.GetModifiedAttribute(CueInfo.MagnitudeAttribute))
			{
				CueParameters.RawMagnitude = ModifiedAttribute->TotalMagnitude;
			}
			else
			{
				CueParameters.RawMagnitude = 0.0f;
			}
		}
		else
		{
			CueParameters.RawMagnitude = 0.0f;
		}

		CueParameters.NormalizedMagnitude = CueInfo.NormalizeLevel(ExecuteLevel);

		UDNADNAAbilitySystemGlobals::Get().GetDNACueManager()->HandleDNACues(ActorAvatar, CueInfo.DNACueTags, EventType, CueParameters);
	}
}

void UDNADNAAbilitySystemComponent::InvokeDNACueEvent(const FDNATag DNACueTag, EDNACueEvent::Type EventType, FDNAEffectContextHandle EffectContext)
{
	FDNACueParameters CueParameters(EffectContext);

	CueParameters.NormalizedMagnitude = 1.f;
	CueParameters.RawMagnitude = 0.f;

	InvokeDNACueEvent(DNACueTag, EventType, CueParameters);
}

void UDNADNAAbilitySystemComponent::InvokeDNACueEvent(const FDNATag DNACueTag, EDNACueEvent::Type EventType, const FDNACueParameters& DNACueParameters)
{
	AActor* ActorAvatar = AbilityActorInfo->AvatarActor.Get();
	
	if (ActorAvatar != nullptr && !bSuppressDNACues)
	{
		UDNADNAAbilitySystemGlobals::Get().GetDNACueManager()->HandleDNACue(ActorAvatar, DNACueTag, EventType, DNACueParameters);
	}
}

void UDNADNAAbilitySystemComponent::ExecuteDNACue(const FDNATag DNACueTag, FDNAEffectContextHandle EffectContext)
{
	// Send to the wrapper on the cue manager
	UDNADNAAbilitySystemGlobals::Get().GetDNACueManager()->InvokeDNACueExecuted(this, DNACueTag, ScopedPredictionKey, EffectContext);
}

void UDNADNAAbilitySystemComponent::ExecuteDNACue(const FDNATag DNACueTag, const FDNACueParameters& DNACueParameters)
{
	// Send to the wrapper on the cue manager
	UDNADNAAbilitySystemGlobals::Get().GetDNACueManager()->InvokeDNACueExecuted_WithParams(this, DNACueTag, ScopedPredictionKey, DNACueParameters);
}

void UDNADNAAbilitySystemComponent::AddDNACue_Internal(const FDNATag DNACueTag, const FDNAEffectContextHandle& EffectContext, FActiveDNACueContainer& DNACueContainer)
{
	FDNACueParameters Parameters(EffectContext);

	if (IsOwnerActorAuthoritative())
	{
		bool bWasInList = HasMatchingDNATag(DNACueTag);

		ForceReplication();
		DNACueContainer.AddCue(DNACueTag, ScopedPredictionKey, Parameters);
		
		// For mixed minimal replication mode, we do NOT want the owning client to play the OnActive event through this RPC, since he will get the full replicated 
		// GE in his AGE array. Generate a prediction key for him, which he will look for on the _Implementation function and ignore.
		{
			FPredictionKey PredictionKeyForRPC = ScopedPredictionKey;
			if (DNACueContainer.bMinimalReplication && (ReplicationMode == EReplicationMode::Mixed) && ScopedPredictionKey.IsValidKey() == false)
			{
				PredictionKeyForRPC = FPredictionKey::CreateNewServerInitiatedKey(this);
			}
			NetMulticast_InvokeDNACueAdded_WithParams(DNACueTag, PredictionKeyForRPC, Parameters);
		}

		if (!bWasInList)
		{
			// Call on server here, clients get it from repnotify
			InvokeDNACueEvent(DNACueTag, EDNACueEvent::WhileActive, Parameters);
		}
	}
	else if (ScopedPredictionKey.IsLocalClientKey())
	{
		DNACueContainer.PredictiveAdd(DNACueTag, ScopedPredictionKey);

		// Allow for predictive DNAcue events? Needs more thought
		InvokeDNACueEvent(DNACueTag, EDNACueEvent::OnActive, Parameters);
		InvokeDNACueEvent(DNACueTag, EDNACueEvent::WhileActive, Parameters);
	}
}

void UDNADNAAbilitySystemComponent::RemoveDNACue_Internal(const FDNATag DNACueTag, FActiveDNACueContainer& DNACueContainer)
{
	if (IsOwnerActorAuthoritative())
	{
		bool bWasInList = HasMatchingDNATag(DNACueTag);

		DNACueContainer.RemoveCue(DNACueTag);

		if (bWasInList)
		{
			FDNACueParameters Parameters;
			InitDefaultDNACueParameters(Parameters);

			// Call on server here, clients get it from repnotify
			InvokeDNACueEvent(DNACueTag, EDNACueEvent::Removed, Parameters);
		}
		// Don't need to multicast broadcast this, ActiveDNACues replication handles it
	}
	else if (ScopedPredictionKey.IsLocalClientKey())
	{
		DNACueContainer.PredictiveRemove(DNACueTag);
	}
}

void UDNADNAAbilitySystemComponent::RemoveAllDNACues()
{
	for (int32 i = (ActiveDNACues.DNACues.Num() - 1); i >= 0; --i)
	{
		RemoveDNACue(ActiveDNACues.DNACues[i].DNACueTag);
	}
}

void UDNADNAAbilitySystemComponent::NetMulticast_InvokeDNACueExecuted_FromSpec_Implementation(const FDNAEffectSpecForRPC Spec, FPredictionKey PredictionKey)
{
	if (IsOwnerActorAuthoritative() || PredictionKey.IsLocalClientKey() == false)
	{
		InvokeDNACueEvent(Spec, EDNACueEvent::Executed);
	}
}

// -----------

void UDNADNAAbilitySystemComponent::NetMulticast_InvokeDNACueExecuted_Implementation(const FDNATag DNACueTag, FPredictionKey PredictionKey, FDNAEffectContextHandle EffectContext)
{
	if (IsOwnerActorAuthoritative() || PredictionKey.IsLocalClientKey() == false)
	{
		InvokeDNACueEvent(DNACueTag, EDNACueEvent::Executed, EffectContext);
	}
}

void UDNADNAAbilitySystemComponent::NetMulticast_InvokeDNACuesExecuted_Implementation(const FDNATagContainer DNACueTags, FPredictionKey PredictionKey, FDNAEffectContextHandle EffectContext)
{
	if (IsOwnerActorAuthoritative() || PredictionKey.IsLocalClientKey() == false)
	{
		for (const FDNATag& DNACueTag : DNACueTags)
		{
			InvokeDNACueEvent(DNACueTag, EDNACueEvent::Executed, EffectContext);
		}
	}
}

// -----------

void UDNADNAAbilitySystemComponent::NetMulticast_InvokeDNACueExecuted_WithParams_Implementation(const FDNATag DNACueTag, FPredictionKey PredictionKey, FDNACueParameters DNACueParameters)
{
	if (IsOwnerActorAuthoritative() || PredictionKey.IsLocalClientKey() == false)
	{
		InvokeDNACueEvent(DNACueTag, EDNACueEvent::Executed, DNACueParameters);
	}
}

void UDNADNAAbilitySystemComponent::NetMulticast_InvokeDNACuesExecuted_WithParams_Implementation(const FDNATagContainer DNACueTags, FPredictionKey PredictionKey, FDNACueParameters DNACueParameters)
{
	if (IsOwnerActorAuthoritative() || PredictionKey.IsLocalClientKey() == false)
	{
		for (const FDNATag& DNACueTag : DNACueTags)
		{
			InvokeDNACueEvent(DNACueTag, EDNACueEvent::Executed, DNACueParameters);
		}
	}
}

// -----------

void UDNADNAAbilitySystemComponent::NetMulticast_InvokeDNACueAdded_Implementation(const FDNATag DNACueTag, FPredictionKey PredictionKey, FDNAEffectContextHandle EffectContext)
{
	if (IsOwnerActorAuthoritative() || PredictionKey.IsLocalClientKey() == false)
	{
		InvokeDNACueEvent(DNACueTag, EDNACueEvent::OnActive, EffectContext);
	}
}

void UDNADNAAbilitySystemComponent::NetMulticast_InvokeDNACueAdded_WithParams_Implementation(const FDNATag DNACueTag, FPredictionKey PredictionKey, FDNACueParameters Parameters)
{
	// If server generated prediction key and auto proxy, skip this message. 
	// This is an RPC from mixed replication mode code, we will get the "real" message from our OnRep on the autonomous proxy
	// See UDNADNAAbilitySystemComponent::AddDNACue_Internal for more info.
	bool bIsMixedReplicationFromServer = (ReplicationMode == EReplicationMode::Mixed && PredictionKey.IsServerInitiatedKey() && AbilityActorInfo->IsLocallyControlledPlayer());

	if (IsOwnerActorAuthoritative() || (PredictionKey.IsLocalClientKey() == false && !bIsMixedReplicationFromServer))
	{
		InvokeDNACueEvent(DNACueTag, EDNACueEvent::OnActive, Parameters);
	}
}

// -----------

void UDNADNAAbilitySystemComponent::NetMulticast_InvokeDNACueAddedAndWhileActive_FromSpec_Implementation(const FDNAEffectSpecForRPC& Spec, FPredictionKey PredictionKey)
{
	if (IsOwnerActorAuthoritative() || PredictionKey.IsLocalClientKey() == false)
	{
		InvokeDNACueEvent(Spec, EDNACueEvent::OnActive);
		InvokeDNACueEvent(Spec, EDNACueEvent::WhileActive);
	}
}

void UDNADNAAbilitySystemComponent::NetMulticast_InvokeDNACueAddedAndWhileActive_WithParams_Implementation(const FDNATag DNACueTag, FPredictionKey PredictionKey, FDNACueParameters DNACueParameters)
{
	if (IsOwnerActorAuthoritative() || PredictionKey.IsLocalClientKey() == false)
	{
		InvokeDNACueEvent(DNACueTag, EDNACueEvent::OnActive, DNACueParameters);
		InvokeDNACueEvent(DNACueTag, EDNACueEvent::WhileActive, DNACueParameters);
	}
}

	
void UDNADNAAbilitySystemComponent::NetMulticast_InvokeDNACuesAddedAndWhileActive_WithParams_Implementation(const FDNATagContainer DNACueTags, FPredictionKey PredictionKey, FDNACueParameters DNACueParameters)
{
	if (IsOwnerActorAuthoritative() || PredictionKey.IsLocalClientKey() == false)
	{
		for (const FDNATag& DNACueTag : DNACueTags)
		{
			InvokeDNACueEvent(DNACueTag, EDNACueEvent::OnActive, DNACueParameters);
			InvokeDNACueEvent(DNACueTag, EDNACueEvent::WhileActive, DNACueParameters);
		}
	}
}

// ----------------------------------------------------------------------------------------

TArray<float> UDNADNAAbilitySystemComponent::GetActiveEffectsTimeRemaining(const FDNAEffectQuery& Query) const
{
	return ActiveDNAEffects.GetActiveEffectsTimeRemaining(Query);
}

TArray<TPair<float,float>> UDNADNAAbilitySystemComponent::GetActiveEffectsTimeRemainingAndDuration(const FDNAEffectQuery& Query) const
{
	return ActiveDNAEffects.GetActiveEffectsTimeRemainingAndDuration(Query);
}

TArray<float> UDNADNAAbilitySystemComponent::GetActiveEffectsDuration(const FDNAEffectQuery& Query) const
{
	return ActiveDNAEffects.GetActiveEffectsDuration(Query);
}

TArray<FActiveDNAEffectHandle> UDNADNAAbilitySystemComponent::GetActiveEffects(const FDNAEffectQuery& Query) const
{
	return ActiveDNAEffects.GetActiveEffects(Query);
}

int32 UDNADNAAbilitySystemComponent::RemoveActiveEffectsWithTags(const FDNATagContainer Tags)
{
	if (IsOwnerActorAuthoritative())
	{
		return RemoveActiveEffects(FDNAEffectQuery::MakeQuery_MatchAnyEffectTags(Tags));
	}
	return 0;
}

int32 UDNADNAAbilitySystemComponent::RemoveActiveEffectsWithSourceTags(FDNATagContainer Tags)
{
	if (IsOwnerActorAuthoritative())
	{
		return RemoveActiveEffects(FDNAEffectQuery::MakeQuery_MatchAnySourceTags(Tags));
	}
	return 0;
}

int32 UDNADNAAbilitySystemComponent::RemoveActiveEffectsWithAppliedTags(FDNATagContainer Tags)
{
	if (IsOwnerActorAuthoritative())
	{
		return RemoveActiveEffects(FDNAEffectQuery::MakeQuery_MatchAnyOwningTags(Tags));
	}
	return 0;
}

int32 UDNADNAAbilitySystemComponent::RemoveActiveEffectsWithGrantedTags(const FDNATagContainer Tags)
{
	if (IsOwnerActorAuthoritative())
	{
		return RemoveActiveEffects(FDNAEffectQuery::MakeQuery_MatchAnyOwningTags(Tags));
	}
	return 0;
}

int32 UDNADNAAbilitySystemComponent::RemoveActiveEffects(const FDNAEffectQuery& Query, int32 StacksToRemove)
{
	if (IsOwnerActorAuthoritative())
	{
		return ActiveDNAEffects.RemoveActiveEffects(Query, StacksToRemove);
	}

	return 0;
}

// ---------------------------------------------------------------------------------------

void UDNADNAAbilitySystemComponent::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{	
	DOREPLIFETIME(UDNADNAAbilitySystemComponent, SpawnedAttributes);
	DOREPLIFETIME(UDNADNAAbilitySystemComponent, ActiveDNAEffects);
	DOREPLIFETIME(UDNADNAAbilitySystemComponent, ActiveDNACues);
	
	DOREPLIFETIME_CONDITION(UDNADNAAbilitySystemComponent, ActivatableAbilities, COND_ReplayOrOwner);
	DOREPLIFETIME_CONDITION(UDNADNAAbilitySystemComponent, BlockedAbilityBindings, COND_OwnerOnly);

	DOREPLIFETIME(UDNADNAAbilitySystemComponent, OwnerActor);
	DOREPLIFETIME(UDNADNAAbilitySystemComponent, AvatarActor);

	DOREPLIFETIME(UDNADNAAbilitySystemComponent, ReplicatedPredictionKey);
	DOREPLIFETIME(UDNADNAAbilitySystemComponent, RepAnimMontageInfo);
	
	DOREPLIFETIME_CONDITION(UDNADNAAbilitySystemComponent, MinimalReplicationDNACues, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(UDNADNAAbilitySystemComponent, MinimalReplicationTags, COND_SkipOwner);
	
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

void UDNADNAAbilitySystemComponent::ForceReplication()
{
	AActor *OwningActor = GetOwner();
	if (OwningActor && OwningActor->Role == ROLE_Authority)
	{
		OwningActor->ForceNetUpdate();
	}
}

void UDNADNAAbilitySystemComponent::ForceAvatarReplication()
{
	if (AvatarActor && AvatarActor->Role == ROLE_Authority)
	{
		AvatarActor->ForceNetUpdate();
	}
}

bool UDNADNAAbilitySystemComponent::ReplicateSubobjects(class UActorChannel *Channel, class FOutBunch *Bunch, FReplicationFlags *RepFlags)
{
	bool WroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);

	for (const UAttributeSet* Set : SpawnedAttributes)
	{
		if (Set)
		{
			WroteSomething |= Channel->ReplicateSubobject(const_cast<UAttributeSet*>(Set), *Bunch, *RepFlags);
		}
	}

	for (UDNAAbility* Ability : AllReplicatedInstancedAbilities)
	{
		if (Ability && !Ability->IsPendingKill())
		{
			WroteSomething |= Channel->ReplicateSubobject(Ability, *Bunch, *RepFlags);
		}
	}

	return WroteSomething;
}

void UDNADNAAbilitySystemComponent::GetSubobjectsWithStableNamesForNetworking(TArray<UObject*>& Objs)
{
	for (const UAttributeSet* Set : SpawnedAttributes)
	{
		if (Set && Set->IsNameStableForNetworking())
		{
			Objs.Add(const_cast<UAttributeSet*>(Set));
		}
	}
}

void UDNADNAAbilitySystemComponent::PreNetReceive()
{
	ActiveDNAEffects.IncrementLock();
}
	
void UDNADNAAbilitySystemComponent::PostNetReceive()
{
	ActiveDNAEffects.DecrementLock();
}

void UDNADNAAbilitySystemComponent::OnRep_PredictionKey()
{
	// Every predictive action we've done up to and including the current value of ReplicatedPredictionKey needs to be wiped
	FPredictionKeyDelegates::CatchUpTo(ReplicatedPredictionKey.Current);
}

bool UDNADNAAbilitySystemComponent::HasAuthorityOrPredictionKey(const FDNAAbilityActivationInfo* ActivationInfo) const
{
	return ((ActivationInfo->ActivationMode == EDNAAbilityActivationMode::Authority) || CanPredict());
}

void UDNADNAAbilitySystemComponent::SetReplicationMode(EReplicationMode NewReplicationMode)
{
	ReplicationMode = NewReplicationMode;
}

void UDNADNAAbilitySystemComponent::OnPredictiveDNACueCatchup(FDNATag Tag)
{
	// Remove it
	RemoveOneTagCount_NoReturn(Tag);

	if (HasMatchingDNATag(Tag) == 0)
	{
		// Invoke Removed event if we no longer have this tag (probably a mispredict)
		InvokeDNACueEvent(Tag, EDNACueEvent::Removed);
	}
}

// ---------------------------------------------------------------------------------------

void UDNADNAAbilitySystemComponent::PrintAllDNAEffects() const
{
	ABILITY_LOG(Log, TEXT("Owner: %s. Avatar: %s"), *GetOwner()->GetName(), *AbilityActorInfo->AvatarActor->GetName());
	ActiveDNAEffects.PrintAllDNAEffects();
}

// ------------------------------------------------------------------------

void UDNADNAAbilitySystemComponent::OnAttributeAggregatorDirty(FAggregator* Aggregator, FDNAAttribute Attribute)
{
	ActiveDNAEffects.OnAttributeAggregatorDirty(Aggregator, Attribute);
}

void UDNADNAAbilitySystemComponent::OnMagnitudeDependencyChange(FActiveDNAEffectHandle Handle, const FAggregator* ChangedAggregator)
{
	ActiveDNAEffects.OnMagnitudeDependencyChange(Handle, ChangedAggregator);
}

void UDNADNAAbilitySystemComponent::OnDNAEffectDurationChange(struct FActiveDNAEffect& ActiveEffect)
{

}

void UDNADNAAbilitySystemComponent::OnDNAEffectAppliedToTarget(UDNADNAAbilitySystemComponent* Target, const FDNAEffectSpec& SpecApplied, FActiveDNAEffectHandle ActiveHandle)
{
	OnDNAEffectAppliedDelegateToTarget.Broadcast(Target, SpecApplied, ActiveHandle);
	ActiveDNAEffects.ApplyStackingLogicPostApplyAsSource(Target, SpecApplied, ActiveHandle);
}

void UDNADNAAbilitySystemComponent::OnDNAEffectAppliedToSelf(UDNADNAAbilitySystemComponent* Source, const FDNAEffectSpec& SpecApplied, FActiveDNAEffectHandle ActiveHandle)
{
	OnDNAEffectAppliedDelegateToSelf.Broadcast(Source, SpecApplied, ActiveHandle);
}

void UDNADNAAbilitySystemComponent::OnPeriodicDNAEffectExecuteOnTarget(UDNADNAAbilitySystemComponent* Target, const FDNAEffectSpec& SpecExecuted, FActiveDNAEffectHandle ActiveHandle)
{
	OnPeriodicDNAEffectExecuteDelegateOnTarget.Broadcast(Target, SpecExecuted, ActiveHandle);
}

void UDNADNAAbilitySystemComponent::OnPeriodicDNAEffectExecuteOnSelf(UDNADNAAbilitySystemComponent* Source, const FDNAEffectSpec& SpecExecuted, FActiveDNAEffectHandle ActiveHandle)
{
	OnPeriodicDNAEffectExecuteDelegateOnSelf.Broadcast(Source, SpecExecuted, ActiveHandle);
}

TArray<UDNATask*>&	UDNADNAAbilitySystemComponent::GetAbilityActiveTasks(UDNAAbility* Ability)
{
	return Ability->ActiveTasks;
}

AActor* UDNADNAAbilitySystemComponent::GetDNATaskAvatar(const UDNATask* Task) const
{
	check(AbilityActorInfo.IsValid());
	return AbilityActorInfo->AvatarActor.Get();
}

AActor* UDNADNAAbilitySystemComponent::GetAvatarActor() const
{
	check(AbilityActorInfo.IsValid());
	return AbilityActorInfo->AvatarActor.Get();
}

void UDNADNAAbilitySystemComponent::DebugCyclicAggregatorBroadcasts(FAggregator* Aggregator)
{
	ActiveDNAEffects.DebugCyclicAggregatorBroadcasts(Aggregator);
}

// ------------------------------------------------------------------------

bool UDNADNAAbilitySystemComponent::ServerPrintDebug_Request_Validate()
{
	return true;
}

void UDNADNAAbilitySystemComponent::ServerPrintDebug_Request_Implementation()
{
	FDNADNAAbilitySystemComponentDebugInfo DebugInfo;
	DebugInfo.bShowAbilities = true;
	DebugInfo.bShowAttributes = true;
	DebugInfo.bShowDNAEffects = true;
	DebugInfo.Accumulate = true;

	Debug_Internal(DebugInfo);

	ClientPrintDebug_Response(DebugInfo.Strings, DebugInfo.GameFlags);
}

void UDNADNAAbilitySystemComponent::ClientPrintDebug_Response_Implementation(const TArray<FString>& Strings, int32 GameFlags)
{
	OnClientPrintDebug_Response(Strings, GameFlags);
}
void UDNADNAAbilitySystemComponent::OnClientPrintDebug_Response(const TArray<FString>& Strings, int32 GameFlags)
{
	ABILITY_LOG(Warning, TEXT(" "));
	ABILITY_LOG(Warning, TEXT("Server State: "));

	for (FString Str : Strings)
	{
		ABILITY_LOG(Warning, TEXT("%s"), *Str);
	}


	// Now that we've heard back from server, append his strings and broadcast the delegate
	UDNADNAAbilitySystemGlobals::Get().DNADNAAbilitySystemDebugStrings.Append(Strings);
	UDNADNAAbilitySystemGlobals::Get().OnClientServerDebugAvailable.Broadcast();
	UDNADNAAbilitySystemGlobals::Get().DNADNAAbilitySystemDebugStrings.Reset(); // we are done with this now. Clear it to signal that this can be ran again
}

FString UDNADNAAbilitySystemComponent::CleanupName(FString Str)
{
	Str.RemoveFromStart(TEXT("Default__"));
	Str.RemoveFromEnd(TEXT("_c"));
	return Str;
}

void UDNADNAAbilitySystemComponent::AccumulateScreenPos(FDNADNAAbilitySystemComponentDebugInfo& Info)
{
	const float ColumnWidth = Info.Canvas ? Info.Canvas->ClipX * 0.4f : 0.f;

	float NewY = Info.YPos + Info.YL;
	if (NewY > Info.MaxY)
	{
		// Need new column, reset Y to original height
		NewY = Info.NewColumnYPadding;
		Info.XPos += ColumnWidth;
	}
	Info.YPos = NewY;
}

void UDNADNAAbilitySystemComponent::DebugLine(FDNADNAAbilitySystemComponentDebugInfo& Info, FString Str, float XOffset, float YOffset)
{
	if (Info.Canvas)
	{
		Info.YL = Info.Canvas->DrawText(GEngine->GetTinyFont(), Str, Info.XPos + XOffset, Info.YPos );
		AccumulateScreenPos(Info);
	}

	if (Info.bPrintToLog)
	{
		FString LogStr;
		for (int32 i=0; i < (int32)XOffset; ++i)
		{
			LogStr += TEXT(" ");
		}
		LogStr += Str;
		ABILITY_LOG(Warning, TEXT("%s"), *LogStr);
	}

	if (Info.Accumulate)
	{
		FString LogStr;
		for (int32 i=0; i < (int32)XOffset; ++i)
		{
			LogStr += TEXT(" ");
		}
		LogStr += Str;
		Info.Strings.Add(Str);
	}
}

struct FASCDebugTargetInfo
{
	FASCDebugTargetInfo()
	{
		DebugCategoryIndex = 0;
		DebugCategories.Add(TEXT("Attributes"));
		DebugCategories.Add(TEXT("DNAEffects"));
		DebugCategories.Add(TEXT("Ability"));
	}

	TArray<FName> DebugCategories;
	int32 DebugCategoryIndex;

	TWeakObjectPtr<UWorld>	TargetWorld;
	TWeakObjectPtr<UDNADNAAbilitySystemComponent>	LastDebugTarget;
};

TArray<FASCDebugTargetInfo>	DNADNAAbilitySystemDebugInfoList;

FASCDebugTargetInfo* GetDebugTargetInfo(UWorld* World)
{
	FASCDebugTargetInfo* TargetInfo = nullptr;
	for (FASCDebugTargetInfo& Info : DNADNAAbilitySystemDebugInfoList )
	{
		if (Info.TargetWorld.Get() == World)
		{
			TargetInfo = &Info;
			break;
		}
	}
	if (TargetInfo == nullptr)
	{
		TargetInfo = &DNADNAAbilitySystemDebugInfoList[DNADNAAbilitySystemDebugInfoList.AddDefaulted()];
		TargetInfo->TargetWorld = World;
	}
	return TargetInfo;
}

static void CycleDebugCategory(UWorld* InWorld)
{
	FASCDebugTargetInfo* TargetInfo = GetDebugTargetInfo(InWorld);
	TargetInfo->DebugCategoryIndex = (TargetInfo->DebugCategoryIndex+1) % TargetInfo->DebugCategories.Num();
}

UDNADNAAbilitySystemComponent* GetDebugTarget(FASCDebugTargetInfo* Info)
{
	// Return target if we already have one
	if (UDNAAbilitySystemComponent* ASC = Info->LastDebugTarget.Get())
	{
		return ASC;
	}

	// Find one
	for (TObjectIterator<UDNAAbilitySystemComponent> It; It; ++It)
	{
		if (UDNAAbilitySystemComponent* ASC = *It)
		{
			// Make use it belongs to our world and will be valid in a TWeakObjPtr (e.g.  not pending kill)
			if (ASC->GetWorld() == Info->TargetWorld.Get() && TWeakObjectPtr<UDNAAbilitySystemComponent>(ASC).Get())
			{
				Info->LastDebugTarget = ASC;
				if (ASC->AbilityActorInfo->IsLocallyControlledPlayer())
				{
					// Default to local player first
					break;
				}
			}
		}
	}

	return Info->LastDebugTarget.Get();
}

void CycleDebugTarget(FASCDebugTargetInfo* TargetInfo, bool Next)
{
	GetDebugTarget(TargetInfo);

	// Build a list	of ASCs
	TArray<UDNAAbilitySystemComponent*> List;
	for (TObjectIterator<UDNAAbilitySystemComponent> It; It; ++It)
	{
		if (UDNAAbilitySystemComponent* ASC = *It)
		{
			if (ASC->GetWorld() == TargetInfo->TargetWorld.Get())
			{
				List.Add(ASC);
			}
		}
	}

	// Search through list to find prev/next target
	UDNAAbilitySystemComponent* Previous = nullptr;
	for (int32 idx=0; idx < List.Num() + 1; ++idx)
	{
		UDNAAbilitySystemComponent* ASC = List[idx % List.Num()];

		if (Next && Previous == TargetInfo->LastDebugTarget.Get())
		{
			TargetInfo->LastDebugTarget = ASC;
			return;
		}
		if (!Next && ASC == TargetInfo->LastDebugTarget.Get())
		{
			TargetInfo->LastDebugTarget = Previous;
			return;
		}

		Previous = ASC;
	}
}

static void	DNAAbilitySystemCycleDebugTarget(UWorld* InWorld, bool Next)
{
	CycleDebugTarget( GetDebugTargetInfo(InWorld), Next );
}

FAutoConsoleCommandWithWorld DNAAbilitySystemNextDebugTargetCmd(
	TEXT("DNAAbilitySystem.Debug.NextTarget"),
	TEXT("Targets next DNAAbilitySystemComponent in ShowDebug DNAAbilitySystem"),
	FConsoleCommandWithWorldDelegate::CreateStatic(DNAAbilitySystemCycleDebugTarget, true)
	);

FAutoConsoleCommandWithWorld DNAAbilitySystemPrevDebugTargetCmd(
	TEXT("DNAAbilitySystem.Debug.PrevTarget"),
	TEXT("Targets previous DNAAbilitySystemComponent in ShowDebug DNAAbilitySystem"),
	FConsoleCommandWithWorldDelegate::CreateStatic(DNAAbilitySystemCycleDebugTarget, false)
	);

static void	DNAAbilitySystemDebugNextCategory(UWorld* InWorld, bool Next)
{
	CycleDebugTarget( GetDebugTargetInfo(InWorld), Next );
}

FAutoConsoleCommandWithWorld DNAAbilitySystemDebugNextCategoryCmd(
	TEXT("DNAAbilitySystem.Debug.NextCategory"),
	TEXT("Targets previous DNAAbilitySystemComponent in ShowDebug DNAAbilitySystem"),
	FConsoleCommandWithWorldDelegate::CreateStatic(CycleDebugCategory)
	);

void UDNAAbilitySystemComponent::OnShowDebugInfo(AHUD* HUD, UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos)
{
	if (DisplayInfo.IsDisplayOn(TEXT("DNAAbilitySystem")))
	{
		UWorld* World = HUD->GetWorld();
		FASCDebugTargetInfo* TargetInfo = GetDebugTargetInfo(World);
	
		if (UDNAAbilitySystemComponent* ASC = GetDebugTarget(TargetInfo))
		{
			TArray<FName> LocalDisplayNames;
			LocalDisplayNames.Add( TargetInfo->DebugCategories[ TargetInfo->DebugCategoryIndex ] );

			FDebugDisplayInfo LocalDisplayInfo( LocalDisplayNames, TArray<FName>() );

			ASC->DisplayDebug(Canvas, LocalDisplayInfo, YL, YPos);
		}
	}
}

void UDNAAbilitySystemComponent::DisplayDebug(class UCanvas* Canvas, const class FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
	FDNAAbilitySystemComponentDebugInfo DebugInfo;

	if (DebugDisplay.IsDisplayOn(FName(TEXT("Attributes"))))
	{
		DebugInfo.bShowAbilities = false;
		DebugInfo.bShowAttributes = true;
		DebugInfo.bShowDNAEffects = false;
	}
	if (DebugDisplay.IsDisplayOn(FName(TEXT("Ability"))))
	{
		DebugInfo.bShowAbilities = true;
		DebugInfo.bShowAttributes = false;
		DebugInfo.bShowDNAEffects = false;
	}
	else if (DebugDisplay.IsDisplayOn(FName(TEXT("DNAEffects"))))
	{
		DebugInfo.bShowAbilities = false;
		DebugInfo.bShowAttributes = false;
		DebugInfo.bShowDNAEffects = true;
	}

	DebugInfo.bPrintToLog = false;
	DebugInfo.Canvas = Canvas;
	DebugInfo.XPos = 0.f;
	DebugInfo.YPos = YPos;
	DebugInfo.OriginalX = 0.f;
	DebugInfo.OriginalY = YPos;
	DebugInfo.MaxY = Canvas->ClipY - 150.f; // Give some padding for any non-columnizing debug output following this output
	DebugInfo.NewColumnYPadding = 30.f;

	Debug_Internal(DebugInfo);

	YPos = DebugInfo.YPos;
	YL = DebugInfo.YL;
}

void UDNAAbilitySystemComponent::PrintDebug()
{
	FDNAAbilitySystemComponentDebugInfo DebugInfo;
	DebugInfo.bShowAbilities = true;
	DebugInfo.bShowAttributes = true;
	DebugInfo.bShowDNAEffects = true;
	DebugInfo.bPrintToLog = true;
	DebugInfo.Accumulate = true;

	Debug_Internal(DebugInfo);

	// Store our local strings in the global debug array. Wait for server to respond with his.
	if (UDNAAbilitySystemGlobals::Get().DNAAbilitySystemDebugStrings.Num() > 0)
	{
		ABILITY_LOG(Warning, TEXT("UDNAAbilitySystemComponent::PrintDebug called while DNAAbilitySystemDebugStrings was not empty. Still waiting for server response from a previous call?"));
	}
		
	UDNAAbilitySystemGlobals::Get().DNAAbilitySystemDebugStrings = DebugInfo.Strings;

	if (IsOwnerActorAuthoritative() == false)
	{
		// See what the server thinks
		ServerPrintDebug_Request();
	}
	else
	{
		UDNAAbilitySystemGlobals::Get().OnClientServerDebugAvailable.Broadcast();
		UDNAAbilitySystemGlobals::Get().DNAAbilitySystemDebugStrings.Reset();
	}
}

void UDNAAbilitySystemComponent::Debug_Internal(FDNAAbilitySystemComponentDebugInfo& Info)
{
	// Draw title at top of screen (default HUD debug text starts at 50 ypos, we can position this on top)*
	//   *until someone changes it unknowingly
	{
		FString DebugTitle("");
		// Category
		if (Info.bShowAbilities) DebugTitle += TEXT("ABILITIES ");
		if (Info.bShowAttributes) DebugTitle += TEXT("ATTRIBUTES ");
		if (Info.bShowDNAEffects) DebugTitle += TEXT("DNAEFFECTS ");
		// Avatar info
		if (AvatarActor)
		{
			DebugTitle += FString::Printf(TEXT("for avatar %s "), *AvatarActor->GetName());
			if (AvatarActor->Role == ROLE_AutonomousProxy) DebugTitle += TEXT("(local player) ");
			else if (AvatarActor->Role == ROLE_SimulatedProxy) DebugTitle += TEXT("(simulated) ");
			else if (AvatarActor->Role == ROLE_Authority) DebugTitle += TEXT("(authority) ");
		}
		// Owner info
		if (OwnerActor && OwnerActor != AvatarActor)
		{
			DebugTitle += FString::Printf(TEXT("for owner %s "), *OwnerActor->GetName());
			if (OwnerActor->Role == ROLE_AutonomousProxy) DebugTitle += TEXT("(autonomous) ");
			else if (OwnerActor->Role == ROLE_SimulatedProxy) DebugTitle += TEXT("(simulated) ");
			else if (OwnerActor->Role == ROLE_Authority) DebugTitle += TEXT("(authority) ");
		}

		if (Info.Canvas)
		{
			Info.Canvas->SetDrawColor(FColor::White);
			Info.Canvas->DrawText(GEngine->GetLargeFont(), DebugTitle, Info.XPos + 4.f, 10.f, 1.5f, 1.5f);
		}
		else
		{
			DebugLine(Info, DebugTitle, 0.f, 0.f);
		}
	}

	FDNATagContainer OwnerTags;
	GetOwnedDNATags(OwnerTags);

	if (Info.Canvas) Info.Canvas->SetDrawColor(FColor::White);

	DebugLine(Info, FString::Printf(TEXT("Owned Tags: %s"), *OwnerTags.ToStringSimple()), 4.f, 0.f);

	if (BlockedAbilityTags.GetExplicitDNATags().Num() > 0)
	{
		DebugLine(Info, FString::Printf(TEXT("BlockedAbilityTags: %s"), *BlockedAbilityTags.GetExplicitDNATags().ToStringSimple()), 4.f, 0.f);
		
	}

	TSet<FDNAAttribute> DrawAttributes;

	float MaxCharHeight = 10;
	if (GetOwner()->GetNetMode() != NM_DedicatedServer)
	{
		MaxCharHeight = GEngine->GetTinyFont()->GetMaxCharHeight();
	}

	// -------------------------------------------------------------

	if (Info.bShowAttributes)
	{
		// Draw the attribute aggregator map.
		for (auto It = ActiveDNAEffects.AttributeAggregatorMap.CreateConstIterator(); It; ++It)
		{
			FDNAAttribute Attribute = It.Key();
			const FAggregatorRef& AggregatorRef = It.Value();
			if (AggregatorRef.Get())
			{
				FAggregator& Aggregator = *AggregatorRef.Get();

				TMap<EDNAModEvaluationChannel, const TArray<FAggregatorMod>*> ModMap;
				Aggregator.DebugGetAllAggregatorMods(ModMap);

				if (ModMap.Num() == 0)
				{
					continue;
				}

				float FinalValue = GetNumericAttribute(Attribute);
				float BaseValue = Aggregator.GetBaseValue();

				FString AttributeString = FString::Printf(TEXT("%s %.2f "), *Attribute.GetName(), GetNumericAttribute(Attribute));
				if (FMath::Abs<float>(BaseValue - FinalValue) > SMALL_NUMBER)
				{
					AttributeString += FString::Printf(TEXT(" (Base: %.2f)"), BaseValue);
				}

				if (Info.Canvas)
				{
					Info.Canvas->SetDrawColor(FColor::White);
				}

				DebugLine(Info, AttributeString, 4.f, 0.f);

				DrawAttributes.Add(Attribute);

 				for (const auto& CurMapElement : ModMap)
 				{
					const EDNAModEvaluationChannel Channel = CurMapElement.Key;
					const TArray<FAggregatorMod>* ModArrays = CurMapElement.Value;

					const FString ChannelNameString = UDNAAbilitySystemGlobals::Get().GetDNAModEvaluationChannelAlias(Channel).ToString();
					for (int32 ModOpIdx = 0; ModOpIdx < EDNAModOp::Max; ++ModOpIdx)
					{
						const TArray<FAggregatorMod>& CurModArray = ModArrays[ModOpIdx];
						for (const FAggregatorMod& Mod : CurModArray)
						{
							FAggregatorEvaluateParameters EmptyParams;
							bool IsActivelyModifyingAttribute = Mod.Qualifies(EmptyParams);
							if (Info.Canvas)
							{
								Info.Canvas->SetDrawColor(IsActivelyModifyingAttribute ? FColor::Yellow : FColor(128, 128, 128));
							}

							FActiveDNAEffect* ActiveGE = ActiveDNAEffects.GetActiveDNAEffect(Mod.ActiveHandle);
							FString SrcName = ActiveGE ? ActiveGE->Spec.Def->GetName() : FString(TEXT(""));

							if (IsActivelyModifyingAttribute == false)
							{
								if (Mod.SourceTagReqs)
								{
									SrcName += FString::Printf(TEXT(" SourceTags: [%s] "), *Mod.SourceTagReqs->ToString());
								}
								if (Mod.TargetTagReqs)
								{
									SrcName += FString::Printf(TEXT("TargetTags: [%s]"), *Mod.TargetTagReqs->ToString());
								}
							}

							DebugLine(Info, FString::Printf(TEXT("   %s %s\t %.2f - %s"), *ChannelNameString, *EDNAModOpToString(ModOpIdx), Mod.EvaluatedMagnitude, *SrcName), 7.f, 0.f);
							Info.NewColumnYPadding = FMath::Max<float>(Info.NewColumnYPadding, Info.YPos + Info.YL);
						}
					}
 				}

				AccumulateScreenPos(Info);
			}
		}
	}

	// -------------------------------------------------------------

	if (Info.bShowDNAEffects)
	{
		for (FActiveDNAEffect& ActiveGE : &ActiveDNAEffects)
		{

			if (Info.Canvas) Info.Canvas->SetDrawColor(FColor::White);

			FString DurationStr = TEXT("Infinite Duration ");
			if (ActiveGE.GetDuration() > 0.f)
			{
				DurationStr = FString::Printf(TEXT("Duration: %.2f. Remaining: %.2f "), ActiveGE.GetDuration(), ActiveGE.GetTimeRemaining(GetWorld()->GetTimeSeconds()));
			}
			if (ActiveGE.GetPeriod() > 0.f)
			{
				DurationStr += FString::Printf(TEXT("Period: %.2f"), ActiveGE.GetPeriod());
			}

			FString StackString;
			if (ActiveGE.Spec.StackCount > 1)
			{

				if (ActiveGE.Spec.Def->StackingType == EDNAEffectStackingType::AggregateBySource)
				{
					StackString = FString::Printf(TEXT("(Stacks: %d. From: %s) "), ActiveGE.Spec.StackCount, *GetNameSafe(ActiveGE.Spec.GetContext().GetInstigatorDNAAbilitySystemComponent()->AvatarActor));
				}
				else
				{
					StackString = FString::Printf(TEXT("(Stacks: %d) "), ActiveGE.Spec.StackCount);
				}
			}

			FString LevelString;
			if (ActiveGE.Spec.GetLevel() > 1.f)
			{
				LevelString = FString::Printf(TEXT("Level: %.2f"), ActiveGE.Spec.GetLevel());
			}

			FString PredictionString;
			if (ActiveGE.PredictionKey.IsValidKey())
			{
				if (ActiveGE.PredictionKey.WasLocallyGenerated() )
				{
					PredictionString = FString::Printf(TEXT("(Predicted and Waiting)"));
				}
				else
				{
					PredictionString = FString::Printf(TEXT("(Predicted and Caught Up)"));
				}
			}

			if (Info.Canvas) Info.Canvas->SetDrawColor(ActiveGE.bIsInhibited ? FColor(128, 128, 128) : FColor::White);

			DebugLine(Info, FString::Printf(TEXT("%s %s %s %s %s"), *CleanupName(GetNameSafe(ActiveGE.Spec.Def)), *DurationStr, *StackString, *LevelString, *PredictionString), 4.f, 0.f);

			FDNATagContainer GrantedTags;
			ActiveGE.Spec.GetAllGrantedTags(GrantedTags);
			if (GrantedTags.Num() > 0)
			{
				DebugLine(Info, FString::Printf(TEXT("Granted Tags: %s"), *GrantedTags.ToStringSimple()), 7.f, 0.f);
			}

			for (int32 ModIdx = 0; ModIdx < ActiveGE.Spec.Modifiers.Num(); ++ModIdx)
			{
				const FModifierSpec& ModSpec = ActiveGE.Spec.Modifiers[ModIdx];
				const FDNAModifierInfo& ModInfo = ActiveGE.Spec.Def->Modifiers[ModIdx];

				// Do a quick Qualifies() check to see if this mod is active.
				FAggregatorMod TempMod;
				TempMod.SourceTagReqs = &ModInfo.SourceTags;
				TempMod.TargetTagReqs = &ModInfo.TargetTags;
				TempMod.IsPredicted = false;

				FAggregatorEvaluateParameters EmptyParams;
				bool IsActivelyModifyingAttribute = TempMod.Qualifies(EmptyParams);

				if (IsActivelyModifyingAttribute == false)
				{
					if (Info.Canvas) Info.Canvas->SetDrawColor(FColor(128, 128, 128));
				}

				DebugLine(Info, FString::Printf(TEXT("Mod: %s. %s. %.2f"), *ModInfo.Attribute.GetName(), *EDNAModOpToString(ModInfo.ModifierOp), ModSpec.GetEvaluatedMagnitude()), 7.f, 0.f);

				if (Info.Canvas) Info.Canvas->SetDrawColor(ActiveGE.bIsInhibited ? FColor(128, 128, 128) : FColor::White);
			}

			AccumulateScreenPos(Info);
		}
	}

	// -------------------------------------------------------------

	if (Info.bShowAttributes)
	{
		if (Info.Canvas) Info.Canvas->SetDrawColor(FColor::White);
		for (UAttributeSet* Set : SpawnedAttributes)
		{
			for (TFieldIterator<UProperty> It(Set->GetClass()); It; ++It)
			{
				FDNAAttribute	Attribute(*It);

				if (DrawAttributes.Contains(Attribute))
					continue;

				if (Attribute.IsValid())
				{
					float Value = GetNumericAttribute(Attribute);

					DebugLine(Info, FString::Printf(TEXT("%s %.2f"), *Attribute.GetName(), Value), 4.f, 0.f);
				}
			}
		}
		AccumulateScreenPos(Info);
	}

	// -------------------------------------------------------------

	bool bShowDNAAbilityTaskDebugMessages = true;

	if (Info.bShowAbilities)
	{
		for (const FDNAAbilitySpec& AbilitySpec : GetActivatableAbilities())
		{
			if (AbilitySpec.Ability == nullptr)
				continue;

			FString StatusText;
			FColor AbilityTextColor = FColor(128, 128, 128);
			if (AbilitySpec.IsActive())
			{
				StatusText = FString::Printf(TEXT(" (Active %d)"), AbilitySpec.ActiveCount);
				AbilityTextColor = FColor::Yellow;
			}
			else if (BlockedAbilityBindings.IsValidIndex(AbilitySpec.InputID) && BlockedAbilityBindings[AbilitySpec.InputID])
			{
				StatusText = TEXT(" (InputBlocked)");
				AbilityTextColor = FColor::Red;
			}
			else if (AbilitySpec.Ability->AbilityTags.HasAny(BlockedAbilityTags.GetExplicitDNATags()))
			{
				StatusText = TEXT(" (TagBlocked)");
				AbilityTextColor = FColor::Red;
			}
			else if (AbilitySpec.Ability->CanActivateAbility(AbilitySpec.Handle, AbilityActorInfo.Get()) == false)
			{
				StatusText = TEXT(" (CantActivate)");
				AbilityTextColor = FColor::Red;
			}

			FString InputPressedStr = AbilitySpec.InputPressed ? TEXT("(InputPressed)") : TEXT("");
			FString ActivationModeStr = AbilitySpec.IsActive() ? UEnum::GetValueAsString(TEXT("DNAAbilities.EDNAAbilityActivationMode"), AbilitySpec.ActivationInfo.ActivationMode) : TEXT("");

			if (Info.Canvas) Info.Canvas->SetDrawColor(AbilityTextColor);

			DebugLine(Info, FString::Printf(TEXT("%s %s %s %s"), *CleanupName(GetNameSafe(AbilitySpec.Ability)), *StatusText, *InputPressedStr, *ActivationModeStr), 4.f, 0.f);

			if (AbilitySpec.IsActive())
			{
				TArray<UDNAAbility*> Instances = AbilitySpec.GetAbilityInstances();
				for (int32 InstanceIdx = 0; InstanceIdx < Instances.Num(); ++InstanceIdx)
				{
					UDNAAbility* Instance = Instances[InstanceIdx];
					if (!Instance)
						continue;

					if (Info.Canvas) Info.Canvas->SetDrawColor(FColor::White);
					for (UDNATask* Task : Instance->ActiveTasks)
					{
						if (Task)
						{
							DebugLine(Info, FString::Printf(TEXT("%s"), *Task->GetDebugString()), 7.f, 0.f);

							if (bShowDNAAbilityTaskDebugMessages)
							{
								for (FDNAAbilityTaskDebugMessage& Msg : Instance->TaskDebugMessages)
								{
									if (Msg.FromTask == Task)
									{
										DebugLine(Info, FString::Printf(TEXT("%s"), *Msg.Message), 9.f, 0.f);
									}
								}
							}
						}
					}

					bool FirstTaskMsg=true;
					int32 MsgCount = 0;
					for (FDNAAbilityTaskDebugMessage& Msg : Instance->TaskDebugMessages)
					{
						// Cap finished task msgs to 5 per ability if we are printing to screen (else things will scroll off)
						if ( Info.Canvas && ++MsgCount > 5 )
						{
							break;
						}

						if (Instance->ActiveTasks.Contains(Msg.FromTask) == false)
						{
							if (FirstTaskMsg)
							{
								DebugLine(Info, TEXT("[FinishedTasks]"), 7.f, 0.f);
								FirstTaskMsg = false;
							}

							DebugLine(Info, FString::Printf(TEXT("%s"), *Msg.Message), 9.f, 0.f);
						}
					}

					if (InstanceIdx < Instances.Num() - 2)
					{
						if (Info.Canvas) Info.Canvas->SetDrawColor(FColor(128, 128, 128));
						DebugLine(Info, FString::Printf(TEXT("--------")), 7.f, 0.f);
					}
				}
			}
		}
		AccumulateScreenPos(Info);
	}

	if (Info.XPos > Info.OriginalX)
	{
		// We flooded to new columns, returned YPos should be max Y (and some padding)
		Info.YPos = Info.MaxY + MaxCharHeight*2.f;
	}
	Info.YL = MaxCharHeight;
}


#undef LOCTEXT_NAMESPACE
