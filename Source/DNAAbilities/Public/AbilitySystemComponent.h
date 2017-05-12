// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Engine/NetSerialization.h"
#include "Engine/EngineTypes.h"
#include "DNATagContainer.h"
#include "AttributeSet.h"
#include "EngineDefines.h"
#include "DNAEffectTypes.h"
#include "DNAPrediction.h"
#include "DNACueInterface.h"
#include "DNATagAssetInterface.h"
#include "DNAAbilitySpec.h"
#include "DNAEffect.h"
#include "Abilities/DNAAbilityTypes.h"
#include "DNATasksComponent.h"
#include "Abilities/DNAAbilityTargetTypes.h"
#include "Abilities/DNAAbility.h"
#include "AbilitySystemComponent.generated.h"

class ADNAAbilityTargetActor;
class AHUD;
class FDebugDisplayInfo;
class UAnimMontage;
class UCanvas;
class UInputComponent;

/** 
 *	UDNAAbilitySystemComponent	
 *
 *	A component to easily interface with the 3 aspects of the DNAAbilitySystem:
 *		-DNAAbilities
 *		-DNAEffects
 *		-DNAAttributes
 *		
 *	This component will make life easier for interfacing with these subsystems, but is not completely required. The main functions are:
 *	
 *	DNAAbilities:
 *		-Provides a way to give/assign abilities that can be used (by a player or AI for example)
 *		-Provides management of instanced abilities (something must hold onto them)
 *		-Provides replication functionality
 *			-Ability state must always be replicated on the UDNAAbility itself, but UDNAAbilitySystemComponent can provide RPC replication
 *			for non-instanced DNA abilities. (Explained more in DNAAbility.h).
 *			
 *	DNAEffects:
 *		-Provides an FActiveDNAEffectsContainer for holding active DNAEffects
 *		-Provides methods for apply DNAEffect to a target or to self
 *		-Provides wrappers for querying information in FActiveDNAEffectsContainers (duration, magnitude, etc)
 *		-Provides methods for clearing/remove DNAEffects
 *		
 *	DNAAttributes
 *		-Provides methods for allocating and initializing attribute sets
 *		-Provides methods for getting AttributeSets
 *  
 * 
 */

/** Called when a targeting actor rejects target confirmation */
DECLARE_MULTICAST_DELEGATE_OneParam(FTargetingRejectedConfirmation, int32);

/** Called when ability fails to activate, passes along the failed ability and a tag explaining why */
DECLARE_MULTICAST_DELEGATE_TwoParams(FAbilityFailedDelegate, const UDNAAbility*, const FDNATagContainer&);

/** Called when ability ends */
DECLARE_MULTICAST_DELEGATE_OneParam(FAbilityEnded, UDNAAbility*);

/** Notify interested parties that ability spec has been modified */
DECLARE_MULTICAST_DELEGATE_OneParam(FAbilitySpecDirtied, const FDNAAbilitySpec&);

/** Notifies when DNAEffectSpec is blocked by an ActiveDNAEffect due to immunity  */
DECLARE_MULTICAST_DELEGATE_TwoParams(FImmunityBlockGE, const FDNAEffectSpec& /*BlockedSpec*/, const FActiveDNAEffect* /*ImmunityDNAEffect*/);

UENUM()
enum class EReplicationMode : uint8
{
	/** Only replicate minimal DNA effect info*/
	Minimal,
	/** Only replicate minimal DNA effect info to simulated proxies but full info to owners and autonomous proxies */
	Mixed,
	/** Replicate full DNA info to all */
	Full,
};

/**
 *	The core ActorComponent for interfacing with the DNAAbilities System
 */
UCLASS(ClassGroup=DNAAbilitySystem, hidecategories=(Object,LOD,Lighting,Transform,Sockets,TextureStreaming), editinlinenew, meta=(BlueprintSpawnableComponent))
class DNAABILITIES_API UDNAAbilitySystemComponent : public UDNATasksComponent, public IDNATagAssetInterface
{
	GENERATED_UCLASS_BODY()

	/** Used to register callbacks to ability-key input */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAbilityAbilityKey, /*UDNAAbility*, Ability, */int32, InputID);

	/** Used to register callbacks to confirm/cancel input */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAbilityConfirmOrCancel);

	friend struct FActiveDNAEffectAction_Add;
	friend FDNAEffectSpec;
	friend class ADNAAbilitySystemDebugHUD;

	virtual void InitializeComponent() override;
	virtual void UninitializeComponent() override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	/** retrieves information whether this component should be ticking taken current
	 *	activity into consideration*/
	virtual bool GetShouldTick() const override;

	/** Finds existing AttributeSet */
	template <class T >
	const T*	GetSet() const
	{
		return (T*)GetAttributeSubobject(T::StaticClass());
	}

	/** Finds existing AttributeSet. Asserts if it isn't there. */
	template <class T >
	const T*	GetSetChecked() const
	{
		return (T*)GetAttributeSubobjectChecked(T::StaticClass());
	}

	/** Adds a new AttributeSet (initialized to default values) */
	template <class T >
	const T*  AddSet()
	{
		return (T*)GetOrCreateAttributeSubobject(T::StaticClass());
	}

	/** Adds a new AttributeSet that is a DSO (created by called in their CStor) */
	template <class T>
	const T*	AddDefaultSubobjectSet(T* Subobject)
	{
		SpawnedAttributes.AddUnique(Subobject);
		return Subobject;
	}

	/**
	* Does this ability system component have this attribute?
	*
	* @param Attribute	Handle of the DNA effect to retrieve target tags from
	*
	* @return true if Attribute is valid and this ability system component contains an attribute set that contains Attribute. Returns false otherwise.
	*/
	bool HasAttributeSetForAttribute(FDNAAttribute Attribute) const;

	const UDNAAttributeSet* InitStats(TSubclassOf<class UDNAAttributeSet> Attributes, const UDataTable* DataTable);

	UFUNCTION(BlueprintCallable, Category="Skills", meta=(DisplayName="InitStats"))
	void K2_InitStats(TSubclassOf<class UDNAAttributeSet> Attributes, const UDataTable* DataTable);
		
	/** Returns a list of all attributes for this abiltiy system component */
	void GetAllAttributes(OUT TArray<FDNAAttribute>& Attributes);

	UPROPERTY(EditAnywhere, Category="AttributeTest")
	TArray<FAttributeDefaults>	DefaultStartingData;

	UPROPERTY(Replicated)
	TArray<UDNAAttributeSet*>	SpawnedAttributes;

	/** Sets the base value of an attribute. Existing active modifiers are NOT cleared and will act upon the new base value. */
	void SetNumericAttributeBase(const FDNAAttribute &Attribute, float NewBaseValue);

	/** Gets the base value of an attribute. That is, the value of the attribute with no stateful modifiers */
	float GetNumericAttributeBase(const FDNAAttribute &Attribute) const;

	/**
	 *	Applies an inplace mod to the given attribute. This correctly update the attribute's aggregator, updates the attribute set property,
	 *	and invokes the OnDirty callbacks.
	 *	
	 *	This does not invoke Pre/PostDNAEffectExecute calls on the attribute set. This does no tag checking, application requirements, immunity, etc.
	 *	No DNAEffectSpec is created or is applied!
	 *
	 *	This should only be used in cases where applying a real DNAEffectSpec is too slow or not possible.
	 */
	void ApplyModToAttribute(const FDNAAttribute &Attribute, TEnumAsByte<EDNAModOp::Type> ModifierOp, float ModifierMagnitude);

	/**
	 *  Applies an inplace mod to the given attribute. Unlike ApplyModToAttribute this function will run on the client or server.
	 *  This may result in problems related to prediction and will not roll back properly.
	 */
	void ApplyModToAttributeUnsafe(const FDNAAttribute &Attribute, TEnumAsByte<EDNAModOp::Type> ModifierOp, float ModifierMagnitude);

	/** Returns current (final) value of an attribute */
	float GetNumericAttribute(const FDNAAttribute &Attribute) const;
	float GetNumericAttributeChecked(const FDNAAttribute &Attribute) const;

	// -- Replication -------------------------------------------------------------------------------------------------

	virtual bool ReplicateSubobjects(class UActorChannel *Channel, class FOutBunch *Bunch, FReplicationFlags *RepFlags) override;
	
	/** Force owning actor to update it's replication, to make sure that DNA cues get sent down quickly. Override to change how aggressive this is */
	virtual void ForceReplication();

	/** Forces avatar actor to update it's replication. Useful for things like needing to replication for movement / locations reasons. */
	virtual void ForceAvatarReplication();

	virtual void GetSubobjectsWithStableNamesForNetworking(TArray<UObject*>& Objs) override;

	virtual void PreNetReceive() override;
	
	virtual void PostNetReceive() override;

	/** When true, we will not replicate active DNA effects for this abiltiy system component, so attributes and tags */
	void SetReplicationMode(EReplicationMode NewReplicationMode);

	EReplicationMode ReplicationMode;

	/** PredictionKeys, see more info in DNAPrediction.h */
	UPROPERTY(ReplicatedUsing=OnRep_PredictionKey)
	FPredictionKey	ReplicatedPredictionKey;

	FPredictionKey	ScopedPredictionKey;

	FPredictionKey GetPredictionKeyForNewAction() const
	{
		return ScopedPredictionKey.IsValidForMorePrediction() ? ScopedPredictionKey : FPredictionKey();
	}

	/** Do we have a valid prediction key to do more predictive actions with */
	bool CanPredict() const
	{
		return ScopedPredictionKey.IsValidForMorePrediction();
	}

	bool HasAuthorityOrPredictionKey(const FDNAAbilityActivationInfo* ActivationInfo) const;	

	UFUNCTION()
	void OnRep_PredictionKey();

	// A pending activation that cannot be activated yet, will be rechecked at a later point
	struct FPendingAbilityInfo
	{
		bool operator==(const FPendingAbilityInfo& Other) const
		{
			// Don't compare event data, not valid to have multiple activations in flight with same key and handle but different event data
			return PredictionKey == Other.PredictionKey	&& Handle == Other.Handle;
		}

		/** Properties of the ability that needs to be activated */
		FDNAAbilitySpecHandle Handle;
		FPredictionKey	PredictionKey;
		FDNAEventData TriggerEventData;

		/** True if this ability was activated remotely and needs to follow up, false if the ability hasn't been activated at all yet */
		bool bPartiallyActivated;

		FPendingAbilityInfo()
			: bPartiallyActivated(false)
		{}
	};

	// This is a list of DNAAbilities that are predicted by the client and were triggered by abilities that were also predicted by the client
	// When the server version of the predicted ability executes it should trigger copies of these and the copies will be associated with the correct prediction keys
	TArray<FPendingAbilityInfo> PendingClientActivatedAbilities;

	// This is a list of DNAAbilities that were activated on the server and can't yet execute on the client. It will try to execute these at a later point
	TArray<FPendingAbilityInfo> PendingServerActivatedAbilities;

	enum class EAbilityExecutionState : uint8
	{
		Executing,
		Succeeded,
		Failed,
	};

	struct FExecutingAbilityInfo
	{
		FExecutingAbilityInfo() : State(EAbilityExecutionState::Executing) {};

		bool operator==(const FExecutingAbilityInfo& Other) const
		{
			return PredictionKey == Other.PredictionKey	&& State == Other.State;
		}

		FPredictionKey PredictionKey;
		EAbilityExecutionState State;
		FDNAAbilitySpecHandle Handle;
	};

	TArray<FExecutingAbilityInfo> ExecutingServerAbilities;

	// ----------------------------------------------------------------------------------------------------------------
	//
	//	DNAEffects	
	//	
	// ----------------------------------------------------------------------------------------------------------------

	// --------------------------------------------
	// Primary outward facing API for other systems:
	// --------------------------------------------
	FActiveDNAEffectHandle ApplyDNAEffectSpecToTarget(OUT FDNAEffectSpec& DNAEffect, UDNAAbilitySystemComponent *Target, FPredictionKey PredictionKey=FPredictionKey());
	FActiveDNAEffectHandle ApplyDNAEffectSpecToSelf(OUT FDNAEffectSpec& DNAEffect, FPredictionKey PredictionKey = FPredictionKey());

	UFUNCTION(BlueprintCallable, Category = DNAEffects, meta=(DisplayName = "ApplyDNAEffectSpecToTarget"))
	FActiveDNAEffectHandle BP_ApplyDNAEffectSpecToTarget(UPARAM(ref) FDNAEffectSpecHandle& SpecHandle, UDNAAbilitySystemComponent* Target);

	UFUNCTION(BlueprintCallable, Category = DNAEffects, meta=(DisplayName = "ApplyDNAEffectSpecToSelf"))
	FActiveDNAEffectHandle BP_ApplyDNAEffectSpecToSelf(UPARAM(ref) FDNAEffectSpecHandle& SpecHandle);
	
	/** Gets the FActiveDNAEffect based on the passed in Handle */
	const UDNAEffect* GetDNAEffectDefForHandle(FActiveDNAEffectHandle Handle);

	/** Removes DNAEffect by Handle. StacksToRemove=-1 will remove all stacks. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = DNAEffects)
	bool RemoveActiveDNAEffect(FActiveDNAEffectHandle Handle, int32 StacksToRemove=-1);

	/** 
	 * Remove active DNA effects whose backing definition are the specified DNA effect class
	 *
	 * @param DNAEffect					Class of DNA effect to remove; Does nothing if left null
	 * @param InstigatorDNAAbilitySystemComponent	If specified, will only remove DNA effects applied from this instigator ability system component
	 * @param StacksToRemove					Number of stacks to remove, -1 means remove all
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = DNAEffects)
	void RemoveActiveDNAEffectBySourceEffect(TSubclassOf<UDNAEffect> DNAEffect, UDNAAbilitySystemComponent* InstigatorDNAAbilitySystemComponent, int32 StacksToRemove = -1);

	/** Get an outgoing DNAEffectSpec that is ready to be applied to other things. */
	UFUNCTION(BlueprintCallable, Category = DNAEffects)
	FDNAEffectSpecHandle MakeOutgoingSpec(TSubclassOf<UDNAEffect> DNAEffectClass, float Level, FDNAEffectContextHandle Context) const;

	/** Create an EffectContext for the owner of this DNAAbilitySystemComponent */
	UFUNCTION(BlueprintCallable, Category = DNAEffects)
	FDNAEffectContextHandle MakeEffectContext() const;

	/**
	 * Get the count of the specified source effect on the ability system component. For non-stacking effects, this is the sum of all active instances.
	 * For stacking effects, this is the sum of all valid stack counts. If an instigator is specified, only effects from that instigator are counted.
	 * 
	 * @param SourceDNAEffect					Effect to get the count of
	 * @param OptionalInstigatorFilterComponent		If specified, only count effects applied by this ability system component
	 * 
	 * @return Count of the specified source effect
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category=DNAEffects)
	int32 GetDNAEffectCount(TSubclassOf<UDNAEffect> SourceDNAEffect, UDNAAbilitySystemComponent* OptionalInstigatorFilterComponent, bool bEnforceOnGoingCheck = true);

	/** Returns the sum of StackCount of all DNA effects that pass query */
	int32 GetAggregatedStackCount(const FDNAEffectQuery& Query);

	/** This only exists so it can be hooked up to a multicast delegate */
	void RemoveActiveDNAEffect_NoReturn(FActiveDNAEffectHandle Handle, int32 StacksToRemove=-1)
	{
		RemoveActiveDNAEffect(Handle, StacksToRemove);
	}

	/** Needed for delegate callback for tag prediction */
	void RemoveOneTagCount_NoReturn(FDNATag Tag)
	{
		UpdateTagMap(Tag, -1);
	}

	/** Called for predictively added DNA cue. Needs to remove tag count and possible invoke OnRemove event if misprediction */
	void OnPredictiveDNACueCatchup(FDNATag Tag);

	float GetDNAEffectDuration(FActiveDNAEffectHandle Handle) const;

	void GetDNAEffectStartTimeAndDuration(FActiveDNAEffectHandle Handle, float& StartEffectTime, float& Duration) const;

	/** Updates the level of an already applied DNA effect. The intention is that this is 'seemless' and doesnt behave like removing/reapplying */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = DNAEffects)
	void SetActiveDNAEffectLevel(FActiveDNAEffectHandle ActiveHandle, int32 NewLevel);

	/** Updates the level of an already applied DNA effect. The intention is that this is 'seemless' and doesnt behave like removing/reapplying */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = DNAEffects)
	void SetActiveDNAEffectLevelUsingQuery(FDNAEffectQuery Query, int32 NewLevel);

	// Not happy with this interface but don't see a better way yet. How should outside code (UI, etc) ask things like 'how much is this DNA effect modifying my damage by'
	// (most likely we want to catch this on the backend - when damage is applied we can get a full dump/history of how the number got to where it is. But still we may need polling methods like below (how much would my damage be)
	UFUNCTION(BlueprintCallable, Category = DNAEffects)
	float GetDNAEffectMagnitude(FActiveDNAEffectHandle Handle, FDNAAttribute Attribute) const;

	/** Returns current stack count of an already applied GE */
	int32 GetCurrentStackCount(FActiveDNAEffectHandle Handle) const;

	/** Returns current stack count of an already applied GE, but given the ability spec handle that was granted by the GE */
	int32 GetCurrentStackCount(FDNAAbilitySpecHandle Handle) const;

	/** Returns debug string describing active DNA effect */
	FString GetActiveGEDebugString(FActiveDNAEffectHandle Handle) const;

	/** Gets the GE Handle of the GE that granted the passed in Ability */
	FActiveDNAEffectHandle FindActiveDNAEffectHandle(FDNAAbilitySpecHandle Handle) const;

	/**
	 * Get the source tags from the DNA spec represented by the specified handle, if possible
	 * 
	 * @param Handle	Handle of the DNA effect to retrieve source tags from
	 * 
	 * @return Source tags from the DNA spec represented by the handle, if possible
	 */
	const FDNATagContainer* GetDNAEffectSourceTagsFromHandle(FActiveDNAEffectHandle Handle) const
	{
		return ActiveDNAEffects.GetDNAEffectSourceTagsFromHandle(Handle);
	}

	/**
	 * Get the target tags from the DNA spec represented by the specified handle, if possible
	 * 
	 * @param Handle	Handle of the DNA effect to retrieve target tags from
	 * 
	 * @return Target tags from the DNA spec represented by the handle, if possible
	 */
	const FDNATagContainer* GetDNAEffectTargetTagsFromHandle(FActiveDNAEffectHandle Handle) const
	{
		return ActiveDNAEffects.GetDNAEffectTargetTagsFromHandle(Handle);
	}

	/**
	 * Populate the specified capture spec with the data necessary to capture an attribute from the component
	 * 
	 * @param OutCaptureSpec	[OUT] Capture spec to populate with captured data
	 */
	void CaptureAttributeForDNAEffect(OUT FDNAEffectAttributeCaptureSpec& OutCaptureSpec)
	{
		// Verify the capture is happening on an attribute the component actually has a set for; if not, can't capture the value
		const FDNAAttribute& AttributeToCapture = OutCaptureSpec.BackingDefinition.AttributeToCapture;
		if (AttributeToCapture.IsValid() && (AttributeToCapture.IsSystemAttribute() || GetAttributeSubobject(AttributeToCapture.GetAttributeSetClass())))
		{
			ActiveDNAEffects.CaptureAttributeForDNAEffect(OutCaptureSpec);
		}
	}
	
	// --------------------------------------------
	// Callbacks / Notifies
	// (these need to be at the UObject level so we can safetly bind, rather than binding to raw at the ActiveDNAEffect/Container level which is unsafe if the DNAAbilitySystemComponent were killed).
	// --------------------------------------------

	void OnAttributeAggregatorDirty(FAggregator* Aggregator, FDNAAttribute Attribute);

	void OnMagnitudeDependencyChange(FActiveDNAEffectHandle Handle, const FAggregator* ChangedAggregator);

	/** This ASC has successfully applied a GE to something (potentially itself) */
	void OnDNAEffectAppliedToTarget(UDNAAbilitySystemComponent* Target, const FDNAEffectSpec& SpecApplied, FActiveDNAEffectHandle ActiveHandle);

	void OnDNAEffectAppliedToSelf(UDNAAbilitySystemComponent* Source, const FDNAEffectSpec& SpecApplied, FActiveDNAEffectHandle ActiveHandle);

	void OnPeriodicDNAEffectExecuteOnTarget(UDNAAbilitySystemComponent* Target, const FDNAEffectSpec& SpecExecuted, FActiveDNAEffectHandle ActiveHandle);

	void OnPeriodicDNAEffectExecuteOnSelf(UDNAAbilitySystemComponent* Source, const FDNAEffectSpec& SpecExecuted, FActiveDNAEffectHandle ActiveHandle);

	virtual void OnDNAEffectDurationChange(struct FActiveDNAEffect& ActiveEffect);

	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnDNAEffectAppliedDelegate, UDNAAbilitySystemComponent*, const FDNAEffectSpec&, FActiveDNAEffectHandle);

	/** Called on server whenever a GE is applied to self. This includes instant and duration based GEs. */
	FOnDNAEffectAppliedDelegate OnDNAEffectAppliedDelegateToSelf;

	/** Called on server whenever a GE is applied to someone else. This includes instant and duration based GEs. */
	FOnDNAEffectAppliedDelegate OnDNAEffectAppliedDelegateToTarget;

	/** Called on both client and server whenever a duraton based GE is added (E.g., instant GEs do not trigger this). */
	FOnDNAEffectAppliedDelegate OnActiveDNAEffectAddedDelegateToSelf;

	/** Called on server whenever a periodic GE executes on self */
	FOnDNAEffectAppliedDelegate OnPeriodicDNAEffectExecuteDelegateOnSelf;

	/** Called on server whenever a periodic GE executes on target */
	FOnDNAEffectAppliedDelegate OnPeriodicDNAEffectExecuteDelegateOnTarget;

	// --------------------------------------------
	// Tags
	// --------------------------------------------
	FORCEINLINE bool HasMatchingDNATag(FDNATag TagToCheck) const override
	{
		return DNATagCountContainer.HasMatchingDNATag(TagToCheck);
	}

	FORCEINLINE bool HasAllMatchingDNATags(const FDNATagContainer& TagContainer) const override
	{
		return DNATagCountContainer.HasAllMatchingDNATags(TagContainer);
	}

	FORCEINLINE bool HasAnyMatchingDNATags(const FDNATagContainer& TagContainer) const override
	{
		return DNATagCountContainer.HasAnyMatchingDNATags(TagContainer);
	}

	FORCEINLINE void GetOwnedDNATags(FDNATagContainer& TagContainer) const override
	{
		TagContainer.AppendTags(DNATagCountContainer.GetExplicitDNATags());
	}

	FORCEINLINE int32 GetTagCount(FDNATag TagToCheck) const
	{
		return DNATagCountContainer.GetTagCount(TagToCheck);
	}

	/** 	 
	 *  Allows GameCode to add loose DNAtags which are not backed by a DNAEffect. 
	 *
	 *	Tags added this way are not replicated! 
	 *	
	 *	It is up to the calling GameCode to make sure these tags are added on clients/server where necessary
	 */

	FORCEINLINE void AddLooseDNATag(const FDNATag& DNATag, int32 Count=1)
	{
		UpdateTagMap(DNATag, Count);
	}

	FORCEINLINE void AddLooseDNATags(const FDNATagContainer& DNATags, int32 Count = 1)
	{
		UpdateTagMap(DNATags, Count);
	}

	FORCEINLINE void RemoveLooseDNATag(const FDNATag& DNATag, int32 Count = 1)
	{
		UpdateTagMap(DNATag, -Count);
	}

	FORCEINLINE void RemoveLooseDNATags(const FDNATagContainer& DNATags, int32 Count = 1)
	{
		UpdateTagMap(DNATags, -Count);
	}

	FORCEINLINE void SetLooseDNATagCount(const FDNATag& DNATag, int32 NewCount)
	{
		SetTagMapCount(DNATag, NewCount);
	}

	/** 	 
	 * Minimally replicated tags are replicated tags that come from GEs when in bMinimalReplication mode. 
	 * (The GEs do not replicate, but the tags they grant do replicate via these functions)
	 */

	FORCEINLINE void AddMinimalReplicationDNATag(const FDNATag& DNATag)
	{
		MinimalReplicationTags.AddTag(DNATag);
	}

	FORCEINLINE void AddMinimalReplicationDNATags(const FDNATagContainer& DNATags)
	{
		MinimalReplicationTags.AddTags(DNATags);
	}

	FORCEINLINE void RemoveMinimalReplicationDNATag(const FDNATag& DNATag)
	{
		MinimalReplicationTags.RemoveTag(DNATag);
	}

	FORCEINLINE void RemoveMinimalReplicationDNATags(const FDNATagContainer& DNATags)
	{
		MinimalReplicationTags.RemoveTags(DNATags);
	}
	
	/** Allow events to be registered for specific DNA tags being added or removed */
	FOnDNAEffectTagCountChanged& RegisterDNATagEvent(FDNATag Tag, EDNATagEventType::Type EventType=EDNATagEventType::NewOrRemoved);

	void RegisterAndCallDNATagEvent(FDNATag Tag, FOnDNAEffectTagCountChanged::FDelegate Delegate, EDNATagEventType::Type EventType=EDNATagEventType::NewOrRemoved);

	/** Returns multicast delegate that is invoked whenever a tag is added or removed (but not if just count is increased. Only for 'new' and 'removed' events) */
	FOnDNAEffectTagCountChanged& RegisterGenericDNATagEvent();

	FOnDNAAttributeChange& RegisterDNAAttributeEvent(FDNAAttribute Attribute);

	// --------------------------------------------
	// System Attributes
	// --------------------------------------------
	
	UPROPERTY(meta=(SystemDNAAttribute="true"))
	float OutgoingDuration;

	UPROPERTY(meta = (SystemDNAAttribute = "true"))
	float IncomingDuration;

	static UProperty* GetOutgoingDurationProperty();
	static UProperty* GetIncomingDurationProperty();

	static const FDNAEffectAttributeCaptureDefinition& GetOutgoingDurationCapture();
	static const FDNAEffectAttributeCaptureDefinition& GetIncomingDurationCapture();


	// --------------------------------------------
	// Additional Helper Functions
	// --------------------------------------------
	
	FOnActiveDNAEffectRemoved* OnDNAEffectRemovedDelegate(FActiveDNAEffectHandle Handle);
	FOnGivenActiveDNAEffectRemoved& OnAnyDNAEffectRemovedDelegate();

	FOnActiveDNAEffectStackChange* OnDNAEffectStackChangeDelegate(FActiveDNAEffectHandle Handle);

	FOnActiveDNAEffectTimeChange* OnDNAEffectTimeChangeDelegate(FActiveDNAEffectHandle Handle);

	UFUNCTION(BlueprintCallable, Category = DNAEffects, meta=(DisplayName = "ApplyDNAEffectToTarget"))
	FActiveDNAEffectHandle BP_ApplyDNAEffectToTarget(TSubclassOf<UDNAEffect> DNAEffectClass, UDNAAbilitySystemComponent *Target, float Level, FDNAEffectContextHandle Context);

	FActiveDNAEffectHandle ApplyDNAEffectToTarget(UDNAEffect *DNAEffect, UDNAAbilitySystemComponent *Target, float Level = UDNAEffect::INVALID_LEVEL, FDNAEffectContextHandle Context = FDNAEffectContextHandle(), FPredictionKey PredictionKey = FPredictionKey());

	UFUNCTION(BlueprintCallable, Category = DNAEffects, meta=(DisplayName = "ApplyDNAEffectToSelf"))
	FActiveDNAEffectHandle BP_ApplyDNAEffectToSelf(TSubclassOf<UDNAEffect> DNAEffectClass, float Level, FDNAEffectContextHandle EffectContext);
	
	FActiveDNAEffectHandle ApplyDNAEffectToSelf(const UDNAEffect *DNAEffect, float Level, const FDNAEffectContextHandle& EffectContext, FPredictionKey PredictionKey = FPredictionKey());

	// Returns the number of DNA effects that are currently active on this ability system component
	int32 GetNumActiveDNAEffects() const
	{
		return ActiveDNAEffects.GetNumDNAEffects();
	}

	// Makes a copy of all the active effects on this ability component
	void GetAllActiveDNAEffectSpecs(TArray<FDNAEffectSpec>& OutSpecCopies) const
	{
		ActiveDNAEffects.GetAllActiveDNAEffectSpecs(OutSpecCopies);
	}

	void SetBaseAttributeValueFromReplication(float NewValue, FDNAAttribute Attribute)
	{
		ActiveDNAEffects.SetBaseAttributeValueFromReplication(Attribute, NewValue);
	}

	void SetBaseAttributeValueFromReplication(FDNAAttributeData NewValue, FDNAAttribute Attribute)
	{
		ActiveDNAEffects.SetBaseAttributeValueFromReplication(Attribute, NewValue.GetBaseValue());
	}

	/** Tests if all modifiers in this DNAEffect will leave the attribute > 0.f */
	bool CanApplyAttributeModifiers(const UDNAEffect *DNAEffect, float Level, const FDNAEffectContextHandle& EffectContext)
	{
		return ActiveDNAEffects.CanApplyAttributeModifiers(DNAEffect, Level, EffectContext);
	}

	// Generic 'Get expected magnitude (list) if I was to apply this outgoing or incoming'

	// Get duration or magnitude (list) of active effects
	//		-Get duration of CD
	//		-Get magnitude + duration of a movespeed buff

	TArray<float> GetActiveEffectsTimeRemaining(const FDNAEffectQuery& Query) const;

	TArray<float> GetActiveEffectsDuration(const FDNAEffectQuery& Query) const;

	TArray<TPair<float,float>> GetActiveEffectsTimeRemainingAndDuration(const FDNAEffectQuery& Query) const;

	TArray<FActiveDNAEffectHandle> GetActiveEffects(const FDNAEffectQuery& Query) const;

	/** This will give the world time that all effects matching this query will be finished. If multiple effects match, it returns the one that returns last.*/
	float GetActiveEffectsEndTime(const FDNAEffectQuery& Query) const
	{
		return ActiveDNAEffects.GetActiveEffectsEndTime(Query);
	}

	bool GetActiveEffectsEndTimeAndDuration(const FDNAEffectQuery& Query, float& EndTime, float& Duration) const
	{
		return ActiveDNAEffects.GetActiveEffectsEndTimeAndDuration(Query, EndTime, Duration);
	}

	void ModifyActiveEffectStartTime(FActiveDNAEffectHandle Handle, float StartTimeDiff)
	{
		ActiveDNAEffects.ModifyActiveEffectStartTime(Handle, StartTimeDiff);
	}

	/** Removes all active effects that contain any of the tags in Tags */
	UFUNCTION(BlueprintCallable, Category = DNAEffects)
	int32 RemoveActiveEffectsWithTags(FDNATagContainer Tags);

	/** Removes all active effects with captured source tags that contain any of the tags in Tags */
	UFUNCTION(BlueprintCallable, Category = DNAEffects)
	int32 RemoveActiveEffectsWithSourceTags(FDNATagContainer Tags);

	/** Removes all active effects that apply any of the tags in Tags */
	UFUNCTION(BlueprintCallable, Category = DNAEffects)
	int32 RemoveActiveEffectsWithAppliedTags(FDNATagContainer Tags);

	/** Removes all active effects that grant any of the tags in Tags */
	UFUNCTION(BlueprintCallable, Category = DNAEffects)
	int32 RemoveActiveEffectsWithGrantedTags(FDNATagContainer Tags);

	/** Removes all active effects that match given query. StacksToRemove=-1 will remove all stacks. */
	int32 RemoveActiveEffects(const FDNAEffectQuery& Query, int32 StacksToRemove = -1);

	void OnRestackDNAEffects();	
	
	void PrintAllDNAEffects() const;

	bool CachedIsNetSimulated;

	/** Returns true of this component has authority */
	bool IsOwnerActorAuthoritative() const
	{
		return !CachedIsNetSimulated;
	}

	// ----------------------------------------------------------------------------------------------------------------
	//
	//	DNACues
	// 
	// ----------------------------------------------------------------------------------------------------------------
	 
	// Do not call these functions directly, call the wrappers on DNACueManager instead
	UFUNCTION(NetMulticast, unreliable)
	void NetMulticast_InvokeDNACueExecuted_FromSpec(const FDNAEffectSpecForRPC Spec, FPredictionKey PredictionKey);

	UFUNCTION(NetMulticast, unreliable)
	void NetMulticast_InvokeDNACueExecuted(const FDNATag DNACueTag, FPredictionKey PredictionKey, FDNAEffectContextHandle EffectContext);

	UFUNCTION(NetMulticast, unreliable)
	void NetMulticast_InvokeDNACuesExecuted(const FDNATagContainer DNACueTags, FPredictionKey PredictionKey, FDNAEffectContextHandle EffectContext);

	UFUNCTION(NetMulticast, unreliable)
	void NetMulticast_InvokeDNACueExecuted_WithParams(const FDNATag DNACueTag, FPredictionKey PredictionKey, FDNACueParameters DNACueParameters);

	UFUNCTION(NetMulticast, unreliable)
	void NetMulticast_InvokeDNACuesExecuted_WithParams(const FDNATagContainer DNACueTags, FPredictionKey PredictionKey, FDNACueParameters DNACueParameters);

	UFUNCTION(NetMulticast, unreliable)
	void NetMulticast_InvokeDNACueAdded(const FDNATag DNACueTag, FPredictionKey PredictionKey, FDNAEffectContextHandle EffectContext);

	UFUNCTION(NetMulticast, unreliable)
	void NetMulticast_InvokeDNACueAdded_WithParams(const FDNATag DNACueTag, FPredictionKey PredictionKey, FDNACueParameters Parameters);
	
	UFUNCTION(NetMulticast, unreliable)
	void NetMulticast_InvokeDNACueAddedAndWhileActive_FromSpec(const FDNAEffectSpecForRPC& Spec, FPredictionKey PredictionKey);

	UFUNCTION(NetMulticast, unreliable)
	void NetMulticast_InvokeDNACueAddedAndWhileActive_WithParams(const FDNATag DNACueTag, FPredictionKey PredictionKey, FDNACueParameters DNACueParameters);

	UFUNCTION(NetMulticast, unreliable)
	void NetMulticast_InvokeDNACuesAddedAndWhileActive_WithParams(const FDNATagContainer DNACueTags, FPredictionKey PredictionKey, FDNACueParameters DNACueParameters);

	// DNACues can also come on their own. These take an optional effect context to pass through hit result, etc
	void ExecuteDNACue(const FDNATag DNACueTag, FDNAEffectContextHandle EffectContext = FDNAEffectContextHandle());

	// This version allows the caller to set an explicit FDNACueParmeters.
	void ExecuteDNACue(const FDNATag DNACueTag, const FDNACueParameters& DNACueParameters);

	// -------------------------

	void AddDNACue(const FDNATag DNACueTag, FDNAEffectContextHandle EffectContext = FDNAEffectContextHandle())
	{
		AddDNACue_Internal(DNACueTag, EffectContext, ActiveDNACues);
	}

	/** Add DNAcue for minimal replication mode. Should only be called in paths that would replicate DNAcues in other ways (through GE for example) if not in minimal replication mode */
	void AddDNACue_MinimalReplication(const FDNATag DNACueTag, FDNAEffectContextHandle EffectContext = FDNAEffectContextHandle())
	{
		AddDNACue_Internal(DNACueTag, EffectContext, MinimalReplicationDNACues);
	}

	void AddDNACue_Internal(const FDNATag DNACueTag, const FDNAEffectContextHandle& EffectContext, FActiveDNACueContainer& DNACueContainer );

	// -------------------------
	
	void RemoveDNACue(const FDNATag DNACueTag)
	{
		RemoveDNACue_Internal(DNACueTag, ActiveDNACues);
	}

	/** Remove DNAcue for minimal replication mode. Should only be called in paths that would replicate DNAcues in other ways (through GE for example) if not in minimal replication mode */
	void RemoveDNACue_MinimalReplication(const FDNATag DNACueTag)
	{
		RemoveDNACue_Internal(DNACueTag, MinimalReplicationDNACues);
	}

	void RemoveDNACue_Internal(const FDNATag DNACueTag, FActiveDNACueContainer& DNACueContainer);

	// -------------------------

	/** Removes any DNACue added on its own, i.e. not as part of a DNAEffect. */
	void RemoveAllDNACues();
	
	void InvokeDNACueEvent(const FDNAEffectSpecForRPC& Spec, EDNACueEvent::Type EventType);

	void InvokeDNACueEvent(const FDNATag DNACueTag, EDNACueEvent::Type EventType, FDNAEffectContextHandle EffectContext = FDNAEffectContextHandle());

	void InvokeDNACueEvent(const FDNATag DNACueTag, EDNACueEvent::Type EventType, const FDNACueParameters& DNACueParameters);

	/** Allows polling to see if a DNACue is active. We expect most DNACue handling to be event based, but some cases we may need to check if a GamepalyCue is active (Animation Blueprint for example) */
	UFUNCTION(BlueprintCallable, Category="DNACue", meta=(DNATagFilter="DNACue"))
	bool IsDNACueActive(const FDNATag DNACueTag) const
	{
		return HasMatchingDNATag(DNACueTag);
	}

	/** Will initialize DNA cue parameters with this ASC's Owner (Instigator) and AvatarActor (EffectCauser) */
	virtual void InitDefaultDNACueParameters(FDNACueParameters& Parameters);

	// ----------------------------------------------------------------------------------------------------------------

	/**
	 *	DNAAbilities
	 *	
	 *	The role of the DNAAbilitySystemComponent with respect to Abilities is to provide:
	 *		-Management of ability instances (whether per actor or per execution instance).
	 *			-Someone *has* to keep track of these instances.
	 *			-Non instanced abilities *could* be executed without any ability stuff in DNAAbilitySystemComponent.
	 *				They should be able to operate on an DNAAbilityActorInfo + DNAAbility.
	 *		
	 *	As convenience it may provide some other features:
	 *		-Some basic input binding (whether instanced or non instanced abilities).
	 *		-Concepts like "this component has these abilities
	 *	
	 */

	/** Grants Ability. Returns handle that can be used in TryActivateAbility, etc. */
	FDNAAbilitySpecHandle GiveAbility(const FDNAAbilitySpec& AbilitySpec);

	/** Grants an ability and attempts to activate it exactly one time, which will cause it to be removed. Only valid on the server! */
	FDNAAbilitySpecHandle GiveAbilityAndActivateOnce(const FDNAAbilitySpec& AbilitySpec);

	/** Wipes all 'given' abilities. */
	void ClearAllAbilities();

	/** Removes the specified ability */
	void ClearAbility(const FDNAAbilitySpecHandle& Handle);
	
	/** Sets an ability spec to remove when its finished. If the spec is not currently active, it terminates it immediately. Also clears InputID of the Spec. */
	void SetRemoveAbilityOnEnd(FDNAAbilitySpecHandle AbilitySpecHandle);

	/** 
	 * Gets all Activatable DNA Abilities that match all tags in DNATagContainer AND for which
	 * DoesAbilitySatisfyTagRequirements() is true.  The latter requirement allows this function to find the correct
	 * ability without requiring advanced knowledge.  For example, if there are two "Melee" abilities, one of which
	 * requires a weapon and one of which requires being unarmed, then those abilities can use Blocking and Required
	 * tags to determine when they can fire.  Using the Satisfying Tags requirements simplifies a lot of usage cases.
	 * For example, Behavior Trees can use various decorators to test an ability fetched using this mechanism as well
	 * as the Task to execute the ability without needing to know that there even is more than one such ability.
	 */
	void GetActivatableDNAAbilitySpecsByAllMatchingTags(const FDNATagContainer& DNATagContainer, TArray < struct FDNAAbilitySpec* >& MatchingDNAAbilities, bool bOnlyAbilitiesThatSatisfyTagRequirements = true) const;

	/** 
	 * Attempts to activate every DNA ability that matches the given tag and DoesAbilitySatisfyTagRequirements().
	 * Returns true if anything attempts to activate. Can activate more than one ability and the ability may fail later.
	 * If bAllowRemoteActivation is true, it will remotely activate local/server abilities, if false it will only try to locally activate abilities.
	 */
	UFUNCTION(BlueprintCallable, Category = "Abilities")
	bool TryActivateAbilitiesByTag(const FDNATagContainer& DNATagContainer, bool bAllowRemoteActivation = true);

	/**
	 * Attempts to activate the ability that is passed in. This will check costs and requirements before doing so.
	 * Returns true if it thinks it activated, but it may return false positives due to failure later in activation.
	 * If bAllowRemoteActivation is true, it will remotely activate local/server abilities, if false it will only try to locally activate the ability
	 */
	UFUNCTION(BlueprintCallable, Category = "Abilities")
	bool TryActivateAbilityByClass(TSubclassOf<UDNAAbility> InAbilityToActivate, bool bAllowRemoteActivation = true);

	/** 
	 * Attempts to activate the given ability, will check costs and requirements before doing so.
	 * Returns true if it thinks it activated, but it may return false positives due to failure later in activation.
	 * If bAllowRemoteActivation is true, it will remotely activate local/server abilities, if false it will only try to locally activate the ability
	 */
	bool TryActivateAbility(FDNAAbilitySpecHandle AbilityToActivate, bool bAllowRemoteActivation = true);

	/** Triggers an ability from a DNA event, will only trigger on local/server depending on execution flags */
	bool TriggerAbilityFromDNAEvent(FDNAAbilitySpecHandle AbilityToTrigger, FDNAAbilityActorInfo* ActorInfo, FDNATag Tag, const FDNAEventData* Payload, UDNAAbilitySystemComponent& Component);

	// --------------------------------------------
	// Ability Cancelling/Interrupts
	// --------------------------------------------

	/** Cancels the specified ability CDO. */
	void CancelAbility(UDNAAbility* Ability);	

	/** Cancels the ability indicated by passed in spec handle. If handle is not found among reactivated abilities nothing happens. */
	void CancelAbilityHandle(const FDNAAbilitySpecHandle& AbilityHandle);

	/** Cancel all abilities with the specified tags. Will not cancel the Ignore instance */
	void CancelAbilities(const FDNATagContainer* WithTags=nullptr, const FDNATagContainer* WithoutTags=nullptr, UDNAAbility* Ignore=nullptr);

	/** Cancels all abilities regardless of tags. Will not cancel the ignore instance */
	void CancelAllAbilities(UDNAAbility* Ignore=nullptr);

	/** Cancels all abilities and kills any remaining instanced abilities */
	virtual void DestroyActiveState();

	// ----------------------------------------------------------------------------------------------------------------
	/** 
	 * Called from ability activation or native code, will apply the correct ability blocking tags and cancel existing abilities. Subclasses can override the behavior 
	 * @param AbilityTags The tags of the ability that has block and cancel flags
	 * @param RequestingAbility The DNA ability requesting the change, can be NULL for native events
	 * @param bEnableBlockTags If true will enable the block tags, if false will disable the block tags
	 * @param BlockTags What tags to block
	 * @param bExecuteCancelTags If true will cancel abilities matching tags
	 * @param CancelTags what tags to cancel
	 */
	virtual void ApplyAbilityBlockAndCancelTags(const FDNATagContainer& AbilityTags, UDNAAbility* RequestingAbility, bool bEnableBlockTags, const FDNATagContainer& BlockTags, bool bExecuteCancelTags, const FDNATagContainer& CancelTags);

	/** Called when an ability is cancellable or not. Doesn't do anything by default, can be overridden to tie into DNA events */
	virtual void HandleChangeAbilityCanBeCanceled(const FDNATagContainer& AbilityTags, UDNAAbility* RequestingAbility, bool bCanBeCanceled) {}

	/** Returns true if any passed in tags are blocked */
	virtual bool AreAbilityTagsBlocked(const FDNATagContainer& Tags) const;

	void BlockAbilitiesWithTags(const FDNATagContainer& Tags);
	void UnBlockAbilitiesWithTags(const FDNATagContainer& Tags);

	/** Checks if the ability system is currently blocking InputID. Returns true if InputID is blocked, false otherwise.  */
	bool IsAbilityInputBlocked(int32 InputID) const;

	void BlockAbilityByInputID(int32 InputID);
	void UnBlockAbilityByInputID(int32 InputID);

	// Functions meant to be called from DNAAbility and subclasses, but not meant for general use

	/** Returns the list of all activatable abilities */
	const TArray<FDNAAbilitySpec>& GetActivatableAbilities() const
	{
		return ActivatableAbilities.Items;
	}

	TArray<FDNAAbilitySpec>& GetActivatableAbilities()
	{
		return ActivatableAbilities.Items;
	}

	/** Returns local world time that an ability was activated. Valid on authority (server) and autonomous proxy (controlling client).  */
	float GetAbilityLastActivatedTime() const { return AbilityLastActivatedTime; }

	/** Returns an ability spec from a handle. If modifying call MarkAbilitySpecDirty */
	FDNAAbilitySpec* FindAbilitySpecFromHandle(FDNAAbilitySpecHandle Handle);
	
	/** Returns an ability spec from a GE handle. If modifying call MarkAbilitySpecDirty */
	FDNAAbilitySpec* FindAbilitySpecFromGEHandle(FActiveDNAEffectHandle Handle);

	/** Returns an ability spec corresponding to given ability class. If modifying call MarkAbilitySpecDirty */
	FDNAAbilitySpec* FindAbilitySpecFromClass(TSubclassOf<UDNAAbility> InAbilityClass);

	/** Returns an ability spec from a handle. If modifying call MarkAbilitySpecDirty */
	FDNAAbilitySpec* FindAbilitySpecFromInputID(int32 InputID);

	/** Retrieves the EffectContext of the DNAEffect of the active DNAEffect. */
	FDNAEffectContextHandle GetEffectContextFromActiveGEHandle(FActiveDNAEffectHandle Handle);

	/** Call to mark that an ability spec has been modified */
	void MarkAbilitySpecDirty(FDNAAbilitySpec& Spec);

	/** Attempts to activate the given ability, will only work if called from the correct client/server context */
	bool InternalTryActivateAbility(FDNAAbilitySpecHandle AbilityToActivate, FPredictionKey InPredictionKey = FPredictionKey(), UDNAAbility ** OutInstancedAbility = nullptr, FOnDNAAbilityEnded::FDelegate* OnDNAAbilityEndedDelegate = nullptr, const FDNAEventData* TriggerEventData = nullptr);

	/** Called from the ability to let the component know it is ended */
	virtual void NotifyAbilityEnded(FDNAAbilitySpecHandle Handle, UDNAAbility* Ability, bool bWasCancelled);

	/** Replicate that an ability has ended, to the client or server as appropriate */
	void ReplicateEndAbility(FDNAAbilitySpecHandle Handle, FDNAAbilityActivationInfo ActivationInfo, UDNAAbility* Ability);

	void IncrementAbilityListLock();
	void DecrementAbilityListLock();

	// --------------------------------------------
	// Debugging
	// --------------------------------------------

	struct FDNAAbilitySystemComponentDebugInfo
	{
		FDNAAbilitySystemComponentDebugInfo()
		{
			FMemory::Memzero(*this);
		}

		class UCanvas* Canvas;

		bool bPrintToLog;

		bool bShowAttributes;
		bool bShowDNAEffects;;
		bool bShowAbilities;

		float XPos;
		float YPos;
		float OriginalX;
		float OriginalY;
		float MaxY;
		float NewColumnYPadding;
		float YL;

		bool Accumulate;
		TArray<FString>	Strings;

		int32 GameFlags; // arbitrary flags for games to set/read in Debug_Internal
	};

	static void OnShowDebugInfo(AHUD* HUD, UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos);

	virtual void DisplayDebug(class UCanvas* Canvas, const class FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos);
	virtual void PrintDebug();

	void AccumulateScreenPos(FDNAAbilitySystemComponentDebugInfo& Info);
	virtual void Debug_Internal(struct FDNAAbilitySystemComponentDebugInfo& Info);
	void DebugLine(struct FDNAAbilitySystemComponentDebugInfo& Info, FString Str, float XOffset, float YOffset);
	FString CleanupName(FString Str);

	UFUNCTION(Server, reliable, WithValidation)
	void ServerPrintDebug_Request();

	UFUNCTION(Client, reliable)
	void ClientPrintDebug_Response(const TArray<FString>& Strings, int32 GameFlags);
	virtual void OnClientPrintDebug_Response(const TArray<FString>& Strings, int32 GameFlags);

	/** Called when the ability is forced cancelled due to replication */
	void ForceCancelAbilityDueToReplication(UDNAAbility* Instance);

protected:

	/**
	 *	The abilities we can activate. 
	 *		-This will include CDOs for non instanced abilities and per-execution instanced abilities. 
	 *		-Actor-instanced abilities will be the actual instance (not CDO)
	 *		
	 *	This array is not vital for things to work. It is a convenience thing for 'giving abilities to the actor'. But abilities could also work on things
	 *	without an DNAAbilitySystemComponent. For example an ability could be written to execute on a StaticMeshActor. As long as the ability doesn't require 
	 *	instancing or anything else that the DNAAbilitySystemComponent would provide, then it doesn't need the component to function.
	 */

	UPROPERTY(ReplicatedUsing=OnRep_ActivateAbilities, BlueprintReadOnly, Category = "Abilities")
	FDNAAbilitySpecContainer	ActivatableAbilities;

	/** Maps from an ability spec to the target data. Used to track replicated data and callbacks */
	TMap<FDNAAbilitySpecHandleAndPredictionKey, FAbilityReplicatedDataCache> AbilityTargetDataMap;

	/** Full list of all instance-per-execution DNA abilities associated with this component */
	UPROPERTY()
	TArray<UDNAAbility*>	AllReplicatedInstancedAbilities;

	/** Will be called from GiveAbility or from OnRep. Initializes events (triggers and inputs) with the given ability */
	virtual void OnGiveAbility(FDNAAbilitySpec& AbilitySpec);

	/** Will be called from RemoveAbility or from OnRep. Unbinds inputs with the given ability */
	virtual void OnRemoveAbility(FDNAAbilitySpec& AbilitySpec);

	/** Called from ClearAbility, ClearAllAbilities or OnRep. Clears any triggers that should no longer exist. */
	void CheckForClearedAbilities();

	/** Cancel a specific ability spec */
	void CancelAbilitySpec(FDNAAbilitySpec& Spec, UDNAAbility* Ignore);

	/** Creates a new instance of an ability, storing it in the spec */
	UDNAAbility* CreateNewInstanceOfAbility(FDNAAbilitySpec& Spec, const UDNAAbility* Ability);

	int32 AbilityScopeLockCount;
	TArray<FDNAAbilitySpecHandle, TInlineAllocator<2> > AbilityPendingRemoves;
	TArray<FDNAAbilitySpec, TInlineAllocator<2> > AbilityPendingAdds;

	/** Local World time of the last ability activation. This is used for AFK/idle detection */
	float AbilityLastActivatedTime;

	UFUNCTION()
	void	OnRep_ActivateAbilities();

	UFUNCTION(Server, reliable, WithValidation)
	void	ServerTryActivateAbility(FDNAAbilitySpecHandle AbilityToActivate, bool InputPressed, FPredictionKey PredictionKey);

	UFUNCTION(Server, reliable, WithValidation)
	void	ServerTryActivateAbilityWithEventData(FDNAAbilitySpecHandle AbilityToActivate, bool InputPressed, FPredictionKey PredictionKey, FDNAEventData TriggerEventData);

	UFUNCTION(Client, reliable)
	void	ClientTryActivateAbility(FDNAAbilitySpecHandle AbilityToActivate);

	/** Called by ServerEndAbility and ClientEndAbility; avoids code duplication. */
	void	RemoteEndOrCancelAbility(FDNAAbilitySpecHandle AbilityToEnd, FDNAAbilityActivationInfo ActivationInfo, bool bWasCanceled);

	UFUNCTION(Server, reliable, WithValidation)
	void	ServerEndAbility(FDNAAbilitySpecHandle AbilityToEnd, FDNAAbilityActivationInfo ActivationInfo, FPredictionKey PredictionKey);

	UFUNCTION(Client, reliable)
	void	ClientEndAbility(FDNAAbilitySpecHandle AbilityToEnd, FDNAAbilityActivationInfo ActivationInfo);

	UFUNCTION(Server, reliable, WithValidation)
	void    ServerCancelAbility(FDNAAbilitySpecHandle AbilityToCancel, FDNAAbilityActivationInfo ActivationInfo);

	UFUNCTION(Client, reliable)
	void    ClientCancelAbility(FDNAAbilitySpecHandle AbilityToCancel, FDNAAbilityActivationInfo ActivationInfo);

	UFUNCTION(Client, Reliable)
	void	ClientActivateAbilityFailed(FDNAAbilitySpecHandle AbilityToActivate, int16 PredictionKey);

	
	void	OnClientActivateAbilityCaughtUp(FDNAAbilitySpecHandle AbilityToActivate, FPredictionKey::KeyType PredictionKey);

	UFUNCTION(Client, Reliable)
	void	ClientActivateAbilitySucceed(FDNAAbilitySpecHandle AbilityToActivate, FPredictionKey PredictionKey);

	UFUNCTION(Client, Reliable)
	void	ClientActivateAbilitySucceedWithEventData(FDNAAbilitySpecHandle AbilityToActivate, FPredictionKey PredictionKey, FDNAEventData TriggerEventData);

public:

	// ----------------------------------------------------------------------------------------------------------------

	// This is meant to be used to inhibit activating an ability from an input perspective. (E.g., the menu is pulled up, another game mechanism is consuming all input, etc)
	// This should only be called on locally owned players.
	// This should not be used to game mechanics like silences or disables. Those should be done through DNA effects.

	UFUNCTION(BlueprintCallable, Category="Abilities")
	bool	GetUserAbilityActivationInhibited() const;
	
	/** Disable or Enable a local user from being able to activate abilities. This should only be used for input/UI etc related inhibition. Do not use for game mechanics. */
	UFUNCTION(BlueprintCallable, Category="Abilities")
	void	SetUserAbilityActivationInhibited(bool NewInhibit);

	bool	UserAbilityActivationInhibited;

	// ----------------------------------------------------------------------------------------------------------------

	virtual void BindToInputComponent(UInputComponent* InputComponent);

	void SetBlockAbilityBindingsArray(FDNAAbiliyInputBinds BindInfo);
	virtual void BindAbilityActivationToInputComponent(UInputComponent* InputComponent, FDNAAbiliyInputBinds BindInfo);

	virtual void AbilityLocalInputPressed(int32 InputID);
	virtual void AbilityLocalInputReleased(int32 InputID);
	
	virtual void LocalInputConfirm();
	virtual void LocalInputCancel();
	
	/** Replicate that an ability has ended/canceled, to the client or server as appropriate */
	void	ReplicateEndOrCancelAbility(FDNAAbilitySpecHandle Handle, FDNAAbilityActivationInfo ActivationInfo, UDNAAbility* Ability, bool bWasCanceled);

	/** InputID for binding GenericConfirm/Cancel events */
	int32 GenericConfirmInputID;
	int32 GenericCancelInputID;

	bool IsGenericConfirmInputBound(int32 InputID) const	{ return ((InputID == GenericConfirmInputID) && GenericLocalConfirmCallbacks.IsBound()); }
	bool IsGenericCancelInputBound(int32 InputID) const		{ return ((InputID == GenericCancelInputID) && GenericLocalCancelCallbacks.IsBound()); }

	/** Generic local callback for generic ConfirmEvent that any ability can listen to */
	FAbilityConfirmOrCancel	GenericLocalConfirmCallbacks;

	FAbilityEnded AbilityEndedCallbacks;

	FAbilitySpecDirtied AbilitySpecDirtiedCallbacks;

	/** Generic local callback for generic CancelEvent that any ability can listen to */
	FAbilityConfirmOrCancel	GenericLocalCancelCallbacks;

	/** A generic callback anytime an ability is activated (started) */	
	FGenericAbilityDelegate AbilityActivatedCallbacks;

	/** A generic callback anytime an ability is commited (cost/cooldown applied) */
	FGenericAbilityDelegate AbilityCommitedCallbacks;
	FAbilityFailedDelegate AbilityFailedCallbacks;

	/** Executes a DNA event. Returns the number of successful ability activations triggered by the event */
	virtual int32 HandleDNAEvent(FDNATag EventTag, const FDNAEventData* Payload);

	/** Generic callbacks for DNA events. See UDNAAbilityTask_WaitDNAEvent */
	TMap<FDNATag, FDNAEventMulticastDelegate> GenericDNAEventCallbacks;

	virtual void NotifyAbilityCommit(UDNAAbility* Ability);
	virtual void NotifyAbilityActivated(const FDNAAbilitySpecHandle Handle, UDNAAbility* Ability);
	virtual void NotifyAbilityFailed(const FDNAAbilitySpecHandle Handle, UDNAAbility* Ability, const FDNATagContainer& FailureReason);

	UPROPERTY()
	TArray<ADNAAbilityTargetActor*>	SpawnedTargetActors;

	/** Any active targeting actors will be told to stop and return current targeting data */
	UFUNCTION(BlueprintCallable, Category = "Abilities")
	void TargetConfirm();

	/** Any active targeting actors will be stopped and canceled, not returning any targeting data */
	UFUNCTION(BlueprintCallable, Category = "Abilities")
	void TargetCancel();

	// ----------------------------------------------------------------------------------------------------------------
	//	AnimMontage Support
	//	
	//	TODO:
	//	-Continously update RepAnimMontageInfo on server for join in progress clients.
	//	-Some missing functionality may still be needed (GetCurrentSectionTime, etc)	
	// ----------------------------------------------------------------------------------------------------------------	

	/** Plays a montage and handles replication and prediction based on passed in ability/activation info */
	float PlayMontage(UDNAAbility* AnimatingAbility, FDNAAbilityActivationInfo ActivationInfo, UAnimMontage* Montage, float InPlayRate, FName StartSectionName = NAME_None);

	/** Plays a montage without updating replication/prediction structures. Used by simulated proxies when replication tells them to play a montage. */
	float PlayMontageSimulated(UAnimMontage* Montage, float InPlayRate, FName StartSectionName = NAME_None);

	/** Stops whatever montage is currently playing. Expectation is caller should only be stopping it if they are the current animating ability (or have good reason not to check) */
	void CurrentMontageStop(float OverrideBlendOutTime = -1.0f);

	/** Clear the animating ability that is passed in, if it's still currently animating */
	void ClearAnimatingAbility(UDNAAbility* Ability);

	/** Jumps current montage to given section. Expectation is caller should only be stopping it if they are the current animating ability (or have good reason not to check) */
	void CurrentMontageJumpToSection(FName SectionName);

	/** Sets current montages next section name. Expectation is caller should only be stopping it if they are the current animating ability (or have good reason not to check) */
	void CurrentMontageSetNextSectionName(FName FromSectionName, FName ToSectionName);

	/** Sets current montage's play rate */
	void CurrentMontageSetPlayRate(float InPlayRate);

	/** Returns true if the passed in ability is the current animating ability */
	bool IsAnimatingAbility(UDNAAbility* Ability) const;

	/** Returns the current animating ability */
	UDNAAbility* GetAnimatingAbility();

	/** Returns montage that is currently playing */
	UAnimMontage* GetCurrentMontage() const;

	/** Get SectionID of currently playing AnimMontage */
	int32 GetCurrentMontageSectionID() const;

	/** Get SectionName of currently playing AnimMontage */
	FName GetCurrentMontageSectionName() const;

	/** Get length in time of current section */
	float GetCurrentMontageSectionLength() const;

	/** Returns amount of time left in current section */
	float GetCurrentMontageSectionTimeLeft() const;

protected:

	/** Implementation of ServerTryActivateAbility */
	virtual void InternalServerTryActiveAbility(FDNAAbilitySpecHandle AbilityToActivate, bool InputPressed, const FPredictionKey& PredictionKey, const FDNAEventData* TriggerEventData);

	/** Called when a prediction key that played a montage is rejected */
	void OnPredictiveMontageRejected(UAnimMontage* PredictiveMontage);

	/** Copy LocalAnimMontageInfo into RepAnimMontageInfo */
	void AnimMontage_UpdateReplicatedData();

	/** Data structure for replicating montage info to simulated clients */
	UPROPERTY(ReplicatedUsing=OnRep_ReplicatedAnimMontage)
	FDNAAbilityRepAnimMontage RepAnimMontageInfo;

	/** Set if montage rep happens while we don't have the animinstance associated with us yet */
	bool bPendingMontagerep;

	/** Data structure for montages that were instigated locally (everything if server, predictive if client. replicated if simulated proxy) */
	UPROPERTY()
	FDNAAbilityLocalAnimMontage LocalAnimMontageInfo;

	UFUNCTION()
	void OnRep_ReplicatedAnimMontage();

	/** Returns true if we are ready to handle replicated montage information */
	virtual bool IsReadyForReplicatedMontage();

	/** RPC function called from CurrentMontageSetNextSectopnName, replicates to other clients */
	UFUNCTION(reliable, server, WithValidation)
	void ServerCurrentMontageSetNextSectionName(UAnimMontage* ClientAnimMontage, float ClientPosition, FName SectionName, FName NextSectionName);

	/** RPC function called from CurrentMontageJumpToSection, replicates to other clients */
	UFUNCTION(reliable, server, WithValidation)
	void ServerCurrentMontageJumpToSectionName(UAnimMontage* ClientAnimMontage, FName SectionName);

	/** RPC function called from CurrentMontageSetPlayRate, replicates to other clients */
	UFUNCTION(reliable, server, WithValidation)
	void ServerCurrentMontageSetPlayRate(UAnimMontage* ClientAnimMontage, float InPlayRate);

	/** Abilities that are triggered from a DNA event */
	TMap<FDNATag, TArray<FDNAAbilitySpecHandle > > DNAEventTriggeredAbilities;

	/** Abilities that are triggered from a tag being added to the owner */
	TMap<FDNATag, TArray<FDNAAbilitySpecHandle > > OwnedTagTriggeredAbilities;

	/** Callback that is called when an owned tag bound to an ability changes */
	virtual void MonitoredTagChanged(const FDNATag Tag, int32 NewCount);

	/** Returns true if the specified ability should be activated from an event in this network mode */
	bool HasNetworkAuthorityToActivateTriggeredAbility(const FDNAAbilitySpec &Spec) const;

	virtual void OnImmunityBlockDNAEffect(const FDNAEffectSpec& Spec, const FActiveDNAEffect* ImmunityGE);

	// -----------------------------------------------------------------------------
public:

	/** Immunity notification support */
	FImmunityBlockGE OnImmunityBlockDNAEffectDelegate;

	/** The actor that owns this component logically */
	UPROPERTY(ReplicatedUsing = OnRep_OwningActor)
	AActor* OwnerActor;

	/** The actor that is the physical representation used for abilities. Can be NULL */
	UPROPERTY(ReplicatedUsing = OnRep_OwningActor)
	AActor* AvatarActor;
	
	UFUNCTION()
	void OnRep_OwningActor();

	/** Cached off data about the owning actor that abilities will need to frequently access (movement component, mesh component, anim instance, etc) */
	TSharedPtr<FDNAAbilityActorInfo>	AbilityActorInfo;

	/**
	 *	Initialized the Abilities' ActorInfo - the structure that holds information about who we are acting on and who controls us.
	 *      OwnerActor is the actor that logically owns this component.
	 *		AvatarActor is what physical actor in the world we are acting on. Usually a Pawn but it could be a Tower, Building, Turret, etc, may be the same as Owner
	 */
	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor);

	/** Changes the avatar actor, leaves the owner actor the same */
	void SetAvatarActor(AActor* InAvatarActor);

	/** called when the ASC's AbilityActorInfo has a PlayerController set. */
	virtual void OnPlayerControllerSet() { }

	/**
	* This is called when the actor that is initialized to this system dies, this will clear that actor from this system and FDNAAbilityActorInfo
	*/
	void ClearActorInfo();

	/**
	 *	This will refresh the Ability's ActorInfo structure based on the current ActorInfo. That is, AvatarActor will be the same but we will look for new
	 *	AnimInstance, MovementComponent, PlayerController, etc.
	 */	
	void RefreshAbilityActorInfo();

	// -----------------------------------------------------------------------------

	/**
	 *	While these appear to be state, these are actually synchronization events w/ some payload data
	 */
	
	/** Replicates the Generic Replicated Event to the server. */
	UFUNCTION(Server, reliable, WithValidation)
	void ServerSetReplicatedEvent(EAbilityGenericReplicatedEvent::Type EventType, FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey, FPredictionKey CurrentPredictionKey);

	/** Replicates the Generic Replicated Event to the server with payload. */
	UFUNCTION(Server, reliable, WithValidation)
	void ServerSetReplicatedEventWithPayload(EAbilityGenericReplicatedEvent::Type EventType, FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey, FPredictionKey CurrentPredictionKey, FVector_NetQuantize100 VectorPayload);

	/** Replicates the Generic Replicated Event to the client. */
	UFUNCTION(Client, reliable)
	void ClientSetReplicatedEvent(EAbilityGenericReplicatedEvent::Type EventType, FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey);

	/** Calls local callbacks that are registered with the given Generic Replicated Event */
	bool InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::Type EventType, FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey, FPredictionKey CurrentPredictionKey = FPredictionKey());

	/** Calls local callbacks that are registered with the given Generic Replicated Event */
	bool InvokeReplicatedEventWithPayload(EAbilityGenericReplicatedEvent::Type EventType, FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey, FPredictionKey CurrentPredictionKey, FVector_NetQuantize100 VectorPayload);
	
	/**  */
	UFUNCTION(Server, reliable, WithValidation)
	void ServerSetReplicatedTargetData(FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey, const FDNAAbilityTargetDataHandle& ReplicatedTargetDataHandle, FDNATag ApplicationTag, FPredictionKey CurrentPredictionKey);

	/** Replicates to the server that targeting has been cancelled */
	UFUNCTION(Server, reliable, WithValidation)
	void ServerSetReplicatedTargetDataCancelled(FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey, FPredictionKey CurrentPredictionKey);

	/** Sets the current target data and calls applicable callbacks */
	void ConfirmAbilityTargetData(FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey, const FDNAAbilityTargetDataHandle& TargetData, const FDNATag& ApplicationTag);

	/** Cancels the ability target data and calls callbacks */
	void CancelAbilityTargetData(FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey);

	/** Deletes all cached ability client data (Was: ConsumeAbilityTargetData)*/
	void ConsumeAllReplicatedData(FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey);
	/** Consumes cached TargetData from client (only TargetData) */
	void ConsumeClientReplicatedTargetData(FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey);

	/** Consumes the given Generic Replicated Event (unsets it). */
	void ConsumeGenericReplicatedEvent(EAbilityGenericReplicatedEvent::Type EventType, FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey);

	/** Gets replicated data of the given Generic Replicated Event. */
	FAbilityReplicatedData GetReplicatedDataOfGenericReplicatedEvent(EAbilityGenericReplicatedEvent::Type EventType, FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey);
	
	/** Calls any Replicated delegates that have been sent (TargetData or Generic Replicated Events). Note this can be dangerous if multiple places in an ability register events and then call this function. */
	void CallAllReplicatedDelegatesIfSet(FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey);

	/** Calls the TargetData Confirm/Cancel events if they have been sent. */
	bool CallReplicatedTargetDataDelegatesIfSet(FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey);

	/** Calls a given Generic Replicated Event delegate if the event has already been sent */
	bool CallReplicatedEventDelegateIfSet(EAbilityGenericReplicatedEvent::Type EventType, FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey);

	/** Calls passed in delegate if the Client Event has already been sent. If not, it adds the delegate to our multicast callback that will fire when it does. */
	bool CallOrAddReplicatedDelegate(EAbilityGenericReplicatedEvent::Type EventType, FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey, FSimpleMulticastDelegate::FDelegate Delegate);

	/** Returns TargetDataSet delegate for a given Ability/PredictionKey pair */
	FAbilityTargetDataSetDelegate& AbilityTargetDataSetDelegate(FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey);

	/** Returns TargetData Cancelled delegate for a given Ability/PredictionKey pair */
	FSimpleMulticastDelegate& AbilityTargetDataCancelledDelegate(FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey);

	/** Returns Generic Replicated Event for a given Ability/PredictionKey pair */
	FSimpleMulticastDelegate& AbilityReplicatedEventDelegate(EAbilityGenericReplicatedEvent::Type EventType, FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey);

	// Direct Input state replication. These will be called if bReplicateInputDirectly is true on the ability and is generally not a good thing to use. (Instead, prefer to use Generic Replicated Events).
	UFUNCTION(Server, reliable, WithValidation)
	void ServerSetInputPressed(FDNAAbilitySpecHandle AbilityHandle);

	UFUNCTION(Server, reliable, WithValidation)
	void ServerSetInputReleased(FDNAAbilitySpecHandle AbilityHandle);

	/** Called on local player always. Called on server only if bReplicateInputDirectly is set on the DNAAbility. */
	virtual void AbilitySpecInputPressed(FDNAAbilitySpec& Spec);

	/** Called on local player always. Called on server only if bReplicateInputDirectly is set on the DNAAbility. */
	virtual void AbilitySpecInputReleased(FDNAAbilitySpec& Spec);

	// ---------------------------------------------------------------------

	FORCEINLINE void SetTagMapCount(const FDNATag& Tag, int32 NewCount)
	{
		DNATagCountContainer.SetTagCount(Tag, NewCount);
	}
	
	FORCEINLINE void UpdateTagMap(const FDNATag& BaseTag, int32 CountDelta)
	{
		if (DNATagCountContainer.UpdateTagCount(BaseTag, CountDelta))
		{
			OnTagUpdated(BaseTag, CountDelta > 0);
		}
	}
	
	FORCEINLINE void UpdateTagMap(const FDNATagContainer& Container, int32 CountDelta)
	{
		for (auto TagIt = Container.CreateConstIterator(); TagIt; ++TagIt)
		{
			const FDNATag& Tag = *TagIt;
			UpdateTagMap(Tag, CountDelta);
		}
	}	


#if ENABLE_VISUAL_LOG
	void ClearDebugInstantEffects();
#endif // ENABLE_VISUAL_LOG

	const FActiveDNAEffect* GetActiveDNAEffect(const FActiveDNAEffectHandle Handle) const;

	virtual AActor* GetDNATaskAvatar(const UDNATask* Task) const override;
	AActor* GetAvatarActor() const;

	/** Suppress all ability granting through GEs on this component */
	bool bSuppressGrantAbility;

	/** Suppress all DNACues on this component */
	bool bSuppressDNACues;

protected:

	/** Actually pushes the final attribute value to the attribute set's property. Should not be called by outside code since this does not go through the attribute aggregator system. */
	void SetNumericAttribute_Internal(const FDNAAttribute &Attribute, float& NewFloatValue);

	bool HasNetworkAuthorityToApplyDNAEffect(FPredictionKey PredictionKey) const;

	void ExecutePeriodicEffect(FActiveDNAEffectHandle	Handle);

	void ExecuteDNAEffect(FDNAEffectSpec &Spec, FPredictionKey PredictionKey);

	void CheckDurationExpired(FActiveDNAEffectHandle Handle);

	void OnAttributeDNAEffectSpecExected(const FDNAAttribute &Attribute, const struct FDNAEffectSpec &Spec, struct FDNAModifierEvaluatedData &Data);
		
	TArray<UDNATask*>&	GetAbilityActiveTasks(UDNAAbility* Ability);
	// --------------------------------------------
	
	// Contains all of the DNA effects that are currently active on this component
	UPROPERTY(Replicated)
	FActiveDNAEffectsContainer	ActiveDNAEffects;

	UPROPERTY(Replicated)
	FActiveDNACueContainer	ActiveDNACues;

	/** Replicated DNAcues when in minimal replication mode. These are cues that would come normally come from ActiveDNAEffects */
	UPROPERTY(Replicated)
	FActiveDNACueContainer	MinimalReplicationDNACues;

	/** Abilities with these tags are not able to be activated */
	FDNATagCountContainer BlockedAbilityTags;

	/** Tracks abilities that are blocked based on input binding. An ability is blocked if BlockedAbilityBindings[InputID] > 0 */
	UPROPERTY(Transient, Replicated)
	TArray<uint8> BlockedAbilityBindings;

	void DebugCyclicAggregatorBroadcasts(struct FAggregator* Aggregator);

	// ---------------------------------------------
	
	// Acceleration map for all DNA tags (OwnedDNATags from GEs and explicit DNACueTags)
	FDNATagCountContainer DNATagCountContainer;

	UPROPERTY(Replicated)
	FMinimalReplicationTagCountMap MinimalReplicationTags;

	void ResetTagMap();

	void NotifyTagMap_StackCountChange(const FDNATagContainer& Container);

	virtual void OnTagUpdated(const FDNATag& Tag, bool TagExists) {};
	
	// ---------------------------------------------

	virtual void OnRegister() override;
	virtual void OnUnregister() override;

	virtual void BeginPlay() override;

	const UDNAAttributeSet*	GetAttributeSubobject(const TSubclassOf<UDNAAttributeSet> AttributeClass) const;
	const UDNAAttributeSet*	GetAttributeSubobjectChecked(const TSubclassOf<UDNAAttributeSet> AttributeClass) const;
	const UDNAAttributeSet*	GetOrCreateAttributeSubobject(TSubclassOf<UDNAAttributeSet> AttributeClass);

	friend struct FActiveDNAEffect;
	friend struct FActiveDNAEffectAction;
	friend struct FActiveDNAEffectsContainer;
	friend struct FActiveDNACue;
	friend struct FActiveDNACueContainer;
	friend struct FDNAAbilitySpec;
	friend struct FDNAAbilitySpecContainer;
	friend struct FAggregator;

private:
	FDelegateHandle MonitoredTagChangedDelegateHandle;
	FTimerHandle    OnRep_ActivateAbilitiesTimerHandle;
};
