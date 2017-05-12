// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Templates/SubclassOf.h"
#include "DNATagContainer.h"
#include "DNAEffectTypes.h"
#include "DNAAbilitySpec.h"
#include "DNAEffect.h"
#include "Abilities/DNAAbilityTypes.h"
#include "DNATaskOwnerInterface.h"
#include "Abilities/DNAAbilityTargetTypes.h"
#include "DNAAbility.generated.h"

class UDNAAbilitySystemComponent;
class UAnimMontage;
class UDNAAbility;
class UDNATask;
class UDNATasksComponent;
struct EventData;

/**
 * UDNAAbility
 *	
 *	Abilities define custom DNA logic that can be activated or triggered.
 *	
 *	The main features provided by the DNAAbilitySystem for DNAAbilities are: 
 *		-CanUse functionality:
 *			-Cooldowns
 *			-Resources (mana, stamina, etc)
 *			-etc
 *			
 *		-Replication support
 *			-Client/Server communication for ability activation
 *			-Client prediction for ability activation
 *			
 *		-Instancing support
 *			-Abilities can be non-instanced (default)
 *			-Instanced per owner
 *			-Instanced per execution
 *			
 *		-Basic, extendable support for:
 *			-Input binding
 *			-'Giving' abilities (that can be used) to actors
 *	
 *	
 *	 
 *	
 *	The intention is for programmers to create these non instanced abilities in C++. Designers can then
 *	extend them as data assets (E.g., they can change default properties, they cannot implement blueprint graphs).
 *	
 *	See DNAAbility_Montage for example.
 *		-Plays a montage and applies a DNAEffect to its target while the montage is playing.
 *		-When finished, removes DNAEffect.
 *		
 *	
 *	Note on replication support:
 *		-Non instanced abilities have limited replication support. 
 *			-Cannot have state (obviously) so no replicated properties
 *			-RPCs on the ability class are not possible either.
 *			
 *			-However: generic RPC functionality can be achieved through the UDNAAbilitySystemAttribute.
 *				-E.g.: ServerTryActivateAbility(class UDNAAbility* AbilityToActivate, int32 PredictionKey)
 *				
 *	A lot is possible with non instanced abilities but care must be taken.
 *	
 *	
 *	To support state or event replication, an ability must be instanced. This can be done with the InstancingPolicy property.
 *	
 *
 *	
 */


/** Notification delegate definition for when the DNA ability ends */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnDNAAbilityEnded, UDNAAbility*);

/** Notification delegate definition for when the DNA ability is cancelled */
DECLARE_MULTICAST_DELEGATE(FOnDNAAbilityCancelled);

/** Used to notify ability state tasks that a state is being ended */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnDNAAbilityStateEnded, FName);

/** Used to delay execution until we leave a critical section */
DECLARE_DELEGATE(FPostLockDelegate);

/** TriggerData */
USTRUCT()
struct FDNAAbilityTriggerData
{
	GENERATED_USTRUCT_BODY()

	FAbilityTriggerData() 
	: TriggerSource(EDNAAbilityTriggerSource::DNAEvent)
	{}

	/** The tag to respond to */
	UPROPERTY(EditAnywhere, Category=TriggerData)
	FDNATag TriggerTag;

	/** The type of trigger to respond to */
	UPROPERTY(EditAnywhere, Category=TriggerData)
	TEnumAsByte<EDNAAbilityTriggerSource::Type> TriggerSource;
};

/**
 *	Abilities define custom DNA logic that can be activated by players or external game logic.
 */
UCLASS(Blueprintable)
class DNAABILITIES_API UDNAAbility : public UObject, public IDNATaskOwnerInterface
{
	GENERATED_UCLASS_BODY()

	friend class UDNAAbilitySystemComponent;
	friend class UDNAAbilitySet;
	friend struct FScopedTargetListLock;

public:

	// ----------------------------------------------------------------------------------------------------------------
	//
	//	The important functions:
	//	
	//		CanActivateAbility()	- const function to see if ability is activatable. Callable by UI etc
	//
	//		TryActivateAbility()	- Attempts to activate the ability. Calls CanActivateAbility(). Input events can call this directly.
	//								- Also handles instancing-per-execution logic and replication/prediction calls.
	//		
	//		CallActivate()			- Protected, non virtual function. Does some boilerplate 'pre activate' stuff, then calls Activate()
	//
	//		Activate()				- What the abilities *does*. This is what child classes want to override.
	//	
	//		Commit()				- Commits reources/cooldowns etc. Activate() must call this!
	//		
	//		CancelAbility()			- Interrupts the ability (from an outside source).
	//									-We may want to add some info on what/who cancelled.
	//
	//		EndAbility()			- The ability has ended. This is intended to be called by the ability to end itself.
	//	
	// ----------------------------------------------------------------------------------------------------------------
		

	/** Returns true if this ability can be activated right now. Has no side effects */
	virtual bool CanActivateAbility(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNATagContainer* SourceTags = nullptr, const FDNATagContainer* TargetTags = nullptr, OUT FDNATagContainer* OptionalRelevantTags = nullptr) const;

	/** Returns true if this ability can be triggered right now. Has no side effects */
	virtual bool ShouldAbilityRespondToEvent(const FDNAAbilityActorInfo* ActorInfo, const FDNAEventData* Payload) const;
	
	/** Returns the time in seconds remaining on the currently active cooldown. */
	virtual float GetCooldownTimeRemaining(const FDNAAbilityActorInfo* ActorInfo) const;

	/** Returns the time in seconds remaining on the currently active cooldown and the original duration for this cooldown. */
	virtual void GetCooldownTimeRemainingAndDuration(FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, float& TimeRemaining, float& CooldownDuration) const;

	virtual const FDNATagContainer* GetCooldownTags() const;
	
	/** Returns true if none of the ability's tags are blocked and if it doesn't have a "Blocking" tag and has all "Required" tags. */
	bool DoesAbilitySatisfyTagRequirements(const UDNAAbilitySystemComponent& DNAAbilitySystemComponent, const FDNATagContainer* SourceTags = nullptr, const FDNATagContainer* TargetTags = nullptr, OUT FDNATagContainer* OptionalRelevantTags = nullptr) const;

	EDNAAbilityInstancingPolicy::Type GetInstancingPolicy() const
	{
		return InstancingPolicy;
	}

	EDNAAbilityReplicationPolicy::Type GetReplicationPolicy() const
	{
		return ReplicationPolicy;
	}

	EDNAAbilityNetExecutionPolicy::Type GetNetExecutionPolicy() const
	{
		return NetExecutionPolicy;
	}

	/** Gets the current actor info bound to this ability - can only be called on instanced abilities. */
	const FDNAAbilityActorInfo* GetCurrentActorInfo() const
	{
		check(IsInstantiated());
		return CurrentActorInfo;
	}

	/** Gets the current activation info bound to this ability - can only be called on instanced abilities. */
	FDNAAbilityActivationInfo GetCurrentActivationInfo() const
	{
		check(IsInstantiated());
		return CurrentActivationInfo;
	}

	FDNAAbilityActivationInfo& GetCurrentActivationInfoRef()
	{
		check(IsInstantiated());
		return CurrentActivationInfo;
	}

	/** Gets the current AbilitySpecHandle- can only be called on instanced abilities. */
	FDNAAbilitySpecHandle GetCurrentAbilitySpecHandle() const
	{
		check(IsInstantiated());
		return CurrentSpecHandle;
	}

	/** Retrieves the actual AbilitySpec for this ability. Can only be called on instanced abilities. */
	FDNAAbilitySpec* GetCurrentAbilitySpec() const;

	/** Retrieves the EffectContext of the DNAEffect that granted this ability. Can only be called on instanced abilities. */
	UFUNCTION(BlueprintCallable, Category = Ability)
	FDNAEffectContextHandle GetGrantedByEffectContext() const;

	/** Removes the DNAEffect that granted this ability. Can only be called on instanced abilities. */
	UFUNCTION(BlueprintCallable, Category = Ability)
	void RemoveGrantedByEffect();

	/** Returns an effect context, given a specified actor info */
	virtual FDNAEffectContextHandle MakeEffectContext(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo *ActorInfo) const;

	virtual UWorld* GetWorld() const override
	{
		if (!IsInstantiated())
		{
			// If we are a CDO, we must return nullptr instead of calling Outer->GetWorld() to fool UObject::ImplementsGetWorld.
			return nullptr;
		}
		return GetOuter()->GetWorld();
	}

	int32 GetFunctionCallspace(UFunction* Function, void* Parameters, FFrame* Stack) override;

	bool CallRemoteFunction(UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack) override;

	void PostNetInit();

	/** Returns true if the ability is currently active */
	bool IsActive() const;

	/** Notification that the ability has ended.  Set using TryActivateAbility. */
	FOnDNAAbilityEnded OnDNAAbilityEnded;

	/** Notification that the ability is being cancelled.  Called before OnDNAAbilityEnded. */
	FOnDNAAbilityCancelled OnDNAAbilityCancelled;

	/** Used by the ability state task to handle when a state is ended */
	FOnDNAAbilityStateEnded OnDNAAbilityStateEnded;

	/** This ability has these tags */
	UPROPERTY(EditDefaultsOnly, Category = Tags)
	FDNATagContainer AbilityTags;

	/** Callback for when this ability has been confirmed by the server */
	FGenericAbilityDelegate	OnConfirmDelegate;

	/** Is this ability triggered from TriggerData (or is it triggered explicitly through input/game code) */
	bool IsTriggered() const;

	bool IsPredictingClient() const;

	bool IsForRemoteClient() const;

	bool IsLocallyControlled() const;

	bool HasAuthority(const FDNAAbilityActivationInfo* ActivationInfo) const;

	bool HasAuthorityOrPredictionKey(const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo* ActivationInfo) const;

	/** Called when the ability is given to an DNAAbilitySystemComponent */
	virtual void OnGiveAbility(const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilitySpec& Spec);

	/** Called when the avatar actor is set/changes */
	virtual void OnAvatarSet(const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilitySpec& Spec);
	
	// --------------------------------------
	//	IDNATaskOwnerInterface
	// --------------------------------------	
	virtual UDNATasksComponent* GetDNATasksComponent(const UDNATask& Task) const override;
	virtual AActor* GetDNATaskOwner(const UDNATask* Task) const override;
	virtual AActor* GetDNATaskAvatar(const UDNATask* Task) const override;
	virtual void OnDNATaskInitialized(UDNATask& Task) override;
	virtual void OnDNATaskActivated(UDNATask& Task) override;
	virtual void OnDNATaskDeactivated(UDNATask& Task) override;

	// --------------------------------------
	//	Input
	// --------------------------------------

	/** Input binding stub. */
	virtual void InputPressed(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo) {};

	/** Input binding stub. */
	virtual void InputReleased(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo) {};

	/** If true, this ability will always replicate input press/release events to the server. */
	UPROPERTY(EditDefaultsOnly, Category = Input)
	bool bReplicateInputDirectly;

	// --------------------------------------
	//	CancelAbility
	// --------------------------------------

	/** Destroys instanced-per-execution abilities. Instance-per-actor abilities should 'reset'. Any active ability state tasks receive the 'OnAbilityStateInterrupted' event. Non instance abilities - what can we do? */
	virtual void CancelAbility(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility);

	/** Returns true if an an ability should be activated */
	virtual bool ShouldActivateAbility(ENetRole Role) const;

protected:

	// --------------------------------------
	//	ShouldAbilityRespondToEvent
	// --------------------------------------

	/** Returns true if this ability can be activated right now. Has no side effects */
	UFUNCTION(BlueprintImplementableEvent, Category = Ability, DisplayName = "ShouldAbilityRespondToEvent")
	bool K2_ShouldAbilityRespondToEvent(FDNAAbilityActorInfo ActorInfo, FDNAEventData Payload) const;

	bool bHasBlueprintShouldAbilityRespondToEvent;
		
	// --------------------------------------
	//	CanActivate
	// --------------------------------------
	
	/** Returns true if this ability can be activated right now. Has no side effects */
	UFUNCTION(BlueprintImplementableEvent, Category = Ability, DisplayName="CanActivateAbility")
	bool K2_CanActivateAbility(FDNAAbilityActorInfo ActorInfo, FDNATagContainer& RelevantTags) const;

	bool bHasBlueprintCanUse;

	// --------------------------------------
	//	ActivateAbility
	// --------------------------------------

	/**
	 * The main function that defines what an ability does.
	 *  -Child classes will want to override this
	 *  -This function graph should call CommitAbility
	 *  -This function graph should call EndAbility
	 *  
	 *  Latent/async actions are ok in this graph. Note that Commit and EndAbility calling requirements speak to the K2_ActivateAbility graph. 
	 *  In C++, the call to K2_ActivateAbility() may return without CommitAbility or EndAbility having been called. But it is expected that this
	 *  will only occur when latent/async actions are pending. When K2_ActivateAbility logically finishes, then we will expect Commit/End to have been called.
	 *  
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = Ability, DisplayName = "ActivateAbility")
	void K2_ActivateAbility();

	UFUNCTION(BlueprintImplementableEvent, Category = Ability, DisplayName = "ActivateAbilityFromEvent")
	void K2_ActivateAbilityFromEvent(const FDNAEventData& EventData);

	virtual void ActivateAbility(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo, const FDNAEventData* TriggerEventData);

	bool bHasBlueprintActivate;
	bool bHasBlueprintActivateFromEvent;

	void CallActivateAbility(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo, FOnDNAAbilityEnded::FDelegate* OnDNAAbilityEndedDelegate = nullptr, const FDNAEventData* TriggerEventData = nullptr);

	/** Called on a predictive ability when the server confirms its execution */
	virtual void ConfirmActivateSucceed();

	UFUNCTION(BlueprintCallable, Category = "Abilities")
	virtual void SendDNAEvent(FDNATag EventTag, FDNAEventData Payload);

public:

	// --------------------------------------
	//	CommitAbility
	// --------------------------------------

	/**
	 * Attempts to commit the ability (spend resources, etc). This our last chance to fail.
	 *	-Child classes that override ActivateAbility must call this themselves!
	 */
	UFUNCTION(BlueprintCallable, Category = Ability, DisplayName = "CommitAbility")
	virtual bool K2_CommitAbility();

	/** Attempts to commit the ability's cooldown only. If BroadcastCommitEvent is true, it will broadcast the commit event that tasks like WaitAbilityCommit are listening for. */
	UFUNCTION(BlueprintCallable, Category = Ability, DisplayName = "CommitAbilityCooldown")
	virtual bool K2_CommitAbilityCooldown(bool BroadcastCommitEvent=false, bool ForceCooldown=false);

	/** Attempts to commit the ability's cost only. If BroadcastCommitEvent is true, it will broadcast the commit event that tasks like WaitAbilityCommit are listening for. */
	UFUNCTION(BlueprintCallable, Category = Ability, DisplayName = "CommitAbilityCost")
	virtual bool K2_CommitAbilityCost(bool BroadcastCommitEvent=false);

	/** Checks the ability's cooldown, but does not apply it.*/
	UFUNCTION(BlueprintCallable, Category = Ability, DisplayName = "CheckAbilityCooldown")
	virtual bool K2_CheckAbilityCooldown();

	/** Checks the ability's cost, but does not apply it. */
	UFUNCTION(BlueprintCallable, Category = Ability, DisplayName = "CheckAbilityCost")
	virtual bool K2_CheckAbilityCost();

	virtual bool CommitAbility(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo);
	virtual bool CommitAbilityCooldown(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo, const bool ForceCooldown);
	virtual bool CommitAbilityCost(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo);

	/**
	 * The last chance to fail before commiting
	 *	-This will usually be the same as CanActivateAbility. Some abilities may need to do extra checks here if they are consuming extra stuff in CommitExecute
	 */
	virtual bool CommitCheck(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo);

	// --------------------------------------
	//	CommitExecute
	// --------------------------------------

	UFUNCTION(BlueprintImplementableEvent, Category = Ability, DisplayName="CommitExecute")
	void K2_CommitExecute();

	/** Does the commit atomically (consume resources, do cooldowns, etc) */
	virtual void CommitExecute(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo);

	/** Do boilerplate init stuff and then call ActivateAbility */
	virtual void PreActivate(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo, FOnDNAAbilityEnded::FDelegate* OnDNAAbilityEndedDelegate);

protected:
	/** Destroys instanced-per-execution abilities. Instance-per-actor abilities should 'reset'. Non instance abilities - what can we do? */
	UFUNCTION(BlueprintCallable, Category = Ability)
	void ConfirmTaskByInstanceName(FName InstanceName, bool bEndTask);

	/** Internal function, cancels all the tasks we asked to cancel last frame (by instance name). */
	void EndOrCancelTasksByInstanceName();
	TArray<FName> CancelTaskInstanceNames;

	/** Add any task with this instance name to a list to be ended (not canceled) next frame.  See also CancelTaskByInstanceName. */
	UFUNCTION(BlueprintCallable, Category = Ability)
	void EndTaskByInstanceName(FName InstanceName);
	TArray<FName> EndTaskInstanceNames;

	/** Add any task with this instance name to a list to be canceled (not ended) next frame.  See also EndTaskByInstanceName. */
	UFUNCTION(BlueprintCallable, Category = Ability)
	void CancelTaskByInstanceName(FName InstanceName);

	/** Ends any active ability state task with the given name. If name is 'None' all active states will be ended (in an arbitrary order). */
	UFUNCTION(BlueprintCallable, Category = Ability)
	void EndAbilityState(FName OptionalStateNameToEnd);

	// -------------------------------------
	//	EndAbility
	// -------------------------------------

	/** Call from kismet to end the ability naturally */
	UFUNCTION(BlueprintCallable, Category = Ability, DisplayName="EndAbility")
	virtual void K2_EndAbility();

	/** Kismet event, will be called if an ability ends normally or abnormally */
	UFUNCTION(BlueprintImplementableEvent, Category = Ability, DisplayName = "OnEndAbility")
	void K2_OnEndAbility();

	/** Check if the ability can be ended */
	bool IsEndAbilityValid(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo) const;

	/** Native function, called if an ability ends normally or abnormally. If bReplicate is set to true, try to replicate the ending to the client/server */
	virtual void EndAbility(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled);

	// -------------------------------------
	//	DNAEffects
	//	
	// -------------------------------------

	// ---------
	// Apply Self
	// ---------

	UFUNCTION(BlueprintCallable, Category = Ability, DisplayName="ApplyDNAEffectToOwner")
	FActiveDNAEffectHandle BP_ApplyDNAEffectToOwner(TSubclassOf<UDNAEffect> DNAEffectClass, int32 DNAEffectLevel = 1, int32 Stacks = 1);

	/** Non blueprintcallable, safe to call on CDO/NonInstance abilities */
	FActiveDNAEffectHandle ApplyDNAEffectToOwner(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo, const UDNAEffect* DNAEffect, float DNAEffectLevel, int32 Stacks = 1) const;

	UFUNCTION(BlueprintCallable, Category = Ability, DisplayName = "ApplyDNAEffectSpecToOwner")
	FActiveDNAEffectHandle K2_ApplyDNAEffectSpecToOwner(const FDNAEffectSpecHandle EffectSpecHandle);

	FActiveDNAEffectHandle ApplyDNAEffectSpecToOwner(const FDNAAbilitySpecHandle AbilityHandle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo, const FDNAEffectSpecHandle SpecHandle) const;

	// ---------
	// Apply Target
	// ---------

	UFUNCTION(BlueprintCallable, Category = Ability, DisplayName = "ApplyDNAEffectToTarget")
	TArray<FActiveDNAEffectHandle> BP_ApplyDNAEffectToTarget(FDNAAbilityTargetDataHandle TargetData, TSubclassOf<UDNAEffect> DNAEffectClass, int32 DNAEffectLevel = 1, int32 Stacks = 1);

	/** Non blueprintcallable, safe to call on CDO/NonInstance abilities */
	TArray<FActiveDNAEffectHandle> ApplyDNAEffectToTarget(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo, const FDNAAbilityTargetDataHandle& Target, TSubclassOf<UDNAEffect> DNAEffectClass, float DNAEffectLevel, int32 Stacks = 1) const;

	UFUNCTION(BlueprintCallable, Category = Ability, DisplayName = "ApplyDNAEffectSpecToTarget")
	TArray<FActiveDNAEffectHandle> K2_ApplyDNAEffectSpecToTarget(const FDNAEffectSpecHandle EffectSpecHandle, FDNAAbilityTargetDataHandle TargetData);

	TArray<FActiveDNAEffectHandle> ApplyDNAEffectSpecToTarget(const FDNAAbilitySpecHandle AbilityHandle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo, const FDNAEffectSpecHandle SpecHandle, const FDNAAbilityTargetDataHandle& TargetData) const;

	// ---------
	// Remove Self
	// ---------
	
	/** Removes DNAEffects from owner which match the given asset level tags. */
	UFUNCTION(BlueprintCallable, Category = Ability, DisplayName="RemoveDNAEffectFromOwnerWithAssetTags")
	void BP_RemoveDNAEffectFromOwnerWithAssetTags(FDNATagContainer WithAssetTags, int32 StacksToRemove = -1);

	UFUNCTION(BlueprintCallable, Category = Ability, DisplayName="RemoveDNAEffectFromOwnerWithGrantedTags")
	void BP_RemoveDNAEffectFromOwnerWithGrantedTags(FDNATagContainer WithGrantedTags, int32 StacksToRemove = -1);

	// -------------------------------------
	//	DNACue
	//	Abilities can invoke DNACues without having to create DNAEffects
	// -------------------------------------
	
	UFUNCTION(BlueprintCallable, Category = Ability, meta=(DNATagFilter="DNACue"), DisplayName="ExecuteDNACue")
	virtual void K2_ExecuteDNACue(FDNATag DNACueTag, FDNAEffectContextHandle Context);

	UFUNCTION(BlueprintCallable, Category = Ability, meta = (DNATagFilter = "DNACue"), DisplayName = "ExecuteDNACueWithParams")
	virtual void K2_ExecuteDNACueWithParams(FDNATag DNACueTag, const FDNACueParameters& DNACueParameters);

	UFUNCTION(BlueprintCallable, Category = Ability, meta=(DNATagFilter="DNACue"), DisplayName="AddDNACue")
	virtual void K2_AddDNACue(FDNATag DNACueTag, FDNAEffectContextHandle Context, bool bRemoveOnAbilityEnd = true);

	UFUNCTION(BlueprintCallable, Category = Ability, meta=(DNATagFilter="DNACue"), DisplayName="RemoveDNACue")
	virtual void K2_RemoveDNACue(FDNATag DNACueTag);

	/** Generates a DNAEffectContextHandle from our owner and an optional TargetData.*/
	UFUNCTION(BlueprintCallable, Category = Ability, meta = (DNATagFilter = "DNACue"))
	virtual FDNAEffectContextHandle GetContextFromOwner(FDNAAbilityTargetDataHandle OptionalTargetData) const;


	// -------------------------------------


public:
	bool IsInstantiated() const
	{
		return !HasAllFlags(RF_ClassDefaultObject);
	}

protected:
	virtual void SetCurrentActorInfo(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo) const
	{
		if (IsInstantiated())
		{
			CurrentActorInfo = ActorInfo;
			CurrentSpecHandle = Handle;
		}
	}

	virtual void SetCurrentActivationInfo(const FDNAAbilityActivationInfo ActivationInfo)
	{
		if (IsInstantiated())
		{
			CurrentActivationInfo = ActivationInfo;
		}
	}

	void SetCurrentInfo(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo)
	{
		SetCurrentActorInfo(Handle, ActorInfo);
		SetCurrentActivationInfo(ActivationInfo);
	}

public:

	/** Returns the actor info associated with this ability, has cached pointers to useful objects */
	UFUNCTION(BlueprintCallable, Category=Ability)
	FDNAAbilityActorInfo GetActorInfo() const;

	/** Returns the actor that owns this ability, which may not have a physical location */
	UFUNCTION(BlueprintCallable, Category = Ability)
	AActor* GetOwningActorFromActorInfo() const;

	/** Returns the physical actor that is executing this ability. May be null */
	UFUNCTION(BlueprintCallable, Category = Ability)
	AActor* GetAvatarActorFromActorInfo() const;

	/** Convenience method for abilities to get skeletal mesh component - useful for aiming abilities */
	UFUNCTION(BlueprintCallable, DisplayName = "GetSkeletalMeshComponentFromActorInfo", Category = Ability)
	USkeletalMeshComponent* GetOwningComponentFromActorInfo() const;

	/** Convenience method for abilities to get outgoing DNA effect specs (for example, to pass on to projectiles to apply to whoever they hit) */
	UFUNCTION(BlueprintCallable, Category=Ability)
	FDNAEffectSpecHandle MakeOutgoingDNAEffectSpec(TSubclassOf<UDNAEffect> DNAEffectClass, float Level=1.f) const;

	FDNAEffectSpecHandle MakeOutgoingDNAEffectSpec(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo, TSubclassOf<UDNAEffect> DNAEffectClass, float Level = 1.f) const;

	/** Add the Ability's tags to the given DNAEffectSpec. This is likely to be overridden per project. */
	virtual void ApplyAbilityTagsToDNAEffectSpec(FDNAEffectSpec& Spec, FDNAAbilitySpec* AbilitySpec) const;

	/** Returns the currently playing montage for this ability, if any */
	UFUNCTION(BlueprintCallable, Category = Animation)
	UAnimMontage* GetCurrentMontage() const;

	/** Call to set/get the current montage from a montage task. Set to allow hooking up montage events to ability events */
	virtual void SetCurrentMontage(class UAnimMontage* InCurrentMontage);

	/** Returns true if this ability can be canceled */
	virtual bool CanBeCanceled() const;

	/** Sets whether the ability should ignore cancel requests. Only valid on instanced abilities */
	UFUNCTION(BlueprintCallable, Category=Ability)
	virtual void SetCanBeCanceled(bool bCanBeCanceled);

	/** Returns true if this ability is blocking other abilities */
	virtual bool IsBlockingOtherAbilities() const;

	/** Sets rather ability block flags are enabled or disabled. Only valid on instanced abilities */
	UFUNCTION(BlueprintCallable, Category = Ability)
	virtual void SetShouldBlockOtherAbilities(bool bShouldBlockAbilities);

	bool IsSupportedForNetworking() const override;

	/** Returns the DNA effect used to determine cooldown */
	virtual class UDNAEffect* GetCooldownDNAEffect() const;

	/** Returns the DNA effect used to apply cost */
	class UDNAEffect* GetCostDNAEffect() const;

	/** Checks cooldown. returns true if we can be used again. False if not */
	virtual bool CheckCooldown(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, OUT FDNATagContainer* OptionalRelevantTags = nullptr) const;

	/** Applies CooldownDNAEffect to the target */
	virtual void ApplyCooldown(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo) const;

	/** Checks cost. returns true if we can pay for the ability. False if not */
	virtual bool CheckCost(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, OUT FDNATagContainer* OptionalRelevantTags = nullptr) const;

	/** Applies the ability's cost to the target */
	virtual void ApplyCost(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo) const;

	/** Movement Sync */
	virtual void SetMovementSyncPoint(FName SyncName);

	/** Set when the remote instance of this ability has ended (but the local instance may still be running or finishing up */
	bool RemoteInstanceEnded;

	/** Called by ability system component to inform this ability instance the remote instance was ended */
	void SetRemoteInstanceHasEnded();

	/** Called to inform the ability that the AvatarActor has been replaced. If the ability is dependant on avatar state, it may want to end itself. */
	void NotifyAvatarDestroyed();

	void NotifyDNAAbilityTaskWaitingOnPlayerData(class UDNAAbilityTask* UDNAAbilityTask);

	void NotifyDNAAbilityTaskWaitingOnAvatar(class UDNAAbilityTask* UDNAAbilityTask);

	UFUNCTION(BlueprintCallable, Category = Ability)
	float GetCooldownTimeRemaining() const;

	/** 
	 * Invalidates the current prediction key. This should be used in cases where there is a valid prediction window, but the server is doing logic that only he can do, and afterwards performs an action that the client could predict (had the client been able to run the server-only code prior).
	 * This returns instantly and has no other side effects other than clearing the current prediction key.
	 */ 
	UFUNCTION(BlueprintCallable, Category = Ability)
	void InvalidateClientPredictionKey() const;

protected:

	// -----------------------------------------------	

	UPROPERTY(EditDefaultsOnly, Category = Advanced)
	TEnumAsByte<EDNAAbilityReplicationPolicy::Type> ReplicationPolicy;

	UPROPERTY(EditDefaultsOnly, Category = Advanced)
	TEnumAsByte<EDNAAbilityInstancingPolicy::Type>	InstancingPolicy;					

	/** If this is set, the server-side version of the ability can be canceled by the client-side version. The client-side version can always be canceled by the server. */
	UPROPERTY(EditDefaultsOnly, Category = Advanced)
	bool bServerRespectsRemoteAbilityCancellation;

	/** if true, and trying to activate an already active instanced ability, end it and re-trigger it. */
	UPROPERTY(EditDefaultsOnly, Category = Advanced)
	bool bRetriggerInstancedAbility;

	/** This is information specific to this instance of the ability. E.g, whether it is predicting, authoring, confirmed, etc. */
	UPROPERTY(BlueprintReadOnly, Category = Ability)
	FDNAAbilityActivationInfo	CurrentActivationInfo;

	UPROPERTY(BlueprintReadOnly, Category = Ability)
	FDNAEventData CurrentEventData;

	UPROPERTY(EditDefaultsOnly, Category=Advanced)
	TEnumAsByte<EDNAAbilityNetExecutionPolicy::Type> NetExecutionPolicy;

	/** This DNAEffect represents the cost (mana, stamina, etc) of the ability. It will be applied when the ability is committed. */
	UPROPERTY(EditDefaultsOnly, Category = Costs)
	TSubclassOf<class UDNAEffect> CostDNAEffectClass;

	/** Triggers to determine if this ability should execute in response to an event */
	UPROPERTY(EditDefaultsOnly, Category = Triggers)
	TArray<FAbilityTriggerData> AbilityTriggers;

	// ----------------------------------------------------------------------------------------------------------------
	//
	//	Cooldowns
	//
	// ----------------------------------------------------------------------------------------------------------------
			
	/** Deprecated? This DNAEffect represents the cooldown. It will be applied when the ability is committed and the ability cannot be used again until it is expired. */
	UPROPERTY(EditDefaultsOnly, Category = Cooldowns)
	TSubclassOf<class UDNAEffect> CooldownDNAEffectClass;
	
	// ----------------------------------------------------------------------------------------------------------------
	//
	//	Ability exclusion / canceling
	//
	// ----------------------------------------------------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, Category = TagQueries)
	FDNATagQuery CancelAbilitiesMatchingTagQuery;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = TagQueries)
	FDNATagQuery ConstTagQuery;

	/** Abilities with these tags are cancelled when this ability is executed */
	UPROPERTY(EditDefaultsOnly, Category = Tags)
	FDNATagContainer CancelAbilitiesWithTag;

	/** Abilities with these tags are blocked while this ability is active */
	UPROPERTY(EditDefaultsOnly, Category = Tags)
	FDNATagContainer BlockAbilitiesWithTag;

	/** Tags to apply to activating owner while this ability is active */
	UPROPERTY(EditDefaultsOnly, Category = Tags)
	FDNATagContainer ActivationOwnedTags;

	/** This ability can only be activated if the activating actor/component has all of these tags */
	UPROPERTY(EditDefaultsOnly, Category = Tags)
	FDNATagContainer ActivationRequiredTags;

	/** This ability is blocked if the activating actor/component has any of these tags */
	UPROPERTY(EditDefaultsOnly, Category = Tags)
	FDNATagContainer ActivationBlockedTags;

	/** This ability can only be activated if the source actor/component has all of these tags */
	UPROPERTY(EditDefaultsOnly, Category = Tags)
	FDNATagContainer SourceRequiredTags;

	/** This ability is blocked if the source actor/component has any of these tags */
	UPROPERTY(EditDefaultsOnly, Category = Tags)
	FDNATagContainer SourceBlockedTags;

	/** This ability can only be activated if the target actor/component has all of these tags */
	UPROPERTY(EditDefaultsOnly, Category = Tags)
	FDNATagContainer TargetRequiredTags;

	/** This ability is blocked if the target actor/component has any of these tags */
	UPROPERTY(EditDefaultsOnly, Category = Tags)
	FDNATagContainer TargetBlockedTags;


	// ----------------------------------------------------------------------------------------------------------------
	//
	//	Ability Tasks
	//
	// ----------------------------------------------------------------------------------------------------------------
	
	UPROPERTY()
	TArray<UDNATask*>	ActiveTasks;

	/** Tasks can emit debug messages throughout their life for debugging purposes. Saved on the ability so that they persist after task is finished */
	TArray<FDNAAbilityTaskDebugMessage> TaskDebugMessages;

public:
	void AddDNAAbilityTaskDebugMessage(UDNATask* UDNAAbilityTask, FString DebugMessage);

protected:

	// ----------------------------------------------------------------------------------------------------------------
	//
	//	Animation
	//
	// ----------------------------------------------------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category="Ability|Animation")
	void MontageJumpToSection(FName SectionName);

	UFUNCTION(BlueprintCallable, Category = "Ability|Animation")
	void MontageSetNextSectionName(FName FromSectionName, FName ToSectionName);

	/**
	 * Stops the current animation montage.
	 *
	 * @param OverrideBlendTime If < 0, will override the BlendOutTime parameter on the AnimMontage instance
	 */
	UFUNCTION(BlueprintCallable, Category="Ability|Animation", Meta = (AdvancedDisplay = "OverrideBlendOutTime"))
	void MontageStop(float OverrideBlendOutTime = -1.0f);

	// ----------------------------------------------------------------------------------------------------------------
	//
	//	Target Data
	//
	// ----------------------------------------------------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = Ability)
	FDNAAbilityTargetingLocationInfo MakeTargetLocationInfoFromOwnerActor();

	UFUNCTION(BlueprintPure, Category = Ability)
	FDNAAbilityTargetingLocationInfo MakeTargetLocationInfoFromOwnerSkeletalMeshComponent(FName SocketName);

	// ----------------------------------------------------------------------------------------------------------------
	//
	//	Ability Levels
	// 
	// ----------------------------------------------------------------------------------------------------------------
	
public:
	/** Returns current level of the Ability */
	UFUNCTION(BlueprintCallable, Category = Ability)
	int32 GetAbilityLevel() const;

	/** Returns current ability level for non instanced abilities. You must call this version in these contexts! */
	int32 GetAbilityLevel(FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo) const;

	/** Retrieves the SourceObject associated with this ability. Can only be called on instanced abilities. */
	UFUNCTION(BlueprintCallable, Category = Ability)
	UObject* GetCurrentSourceObject() const;

	/** Retrieves the SourceObject associated with this ability. Callable on non instanced */
	UObject* GetSourceObject(FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo) const;

protected:

	/** 
	 *  This is shared, cached information about the thing using us
	 *	 E.g, Actor*, MovementComponent*, AnimInstance, etc.
	 *	 This is hopefully allocated once per actor and shared by many abilities.
	 *	 The actual struct may be overridden per game to include game specific data.
	 *	 (E.g, child classes may want to cast to FMyGameAbilityActorInfo)
	 */
	mutable const FDNAAbilityActorInfo* CurrentActorInfo;

	mutable FDNAAbilitySpecHandle CurrentSpecHandle;

	/** DNACues that were added during this ability that will get automatically removed when it ends */
	TSet<FDNATag> TrackedDNACues;

	/** Active montage being played by this ability */
	UPROPERTY()
	class UAnimMontage* CurrentMontage;

	/** True if the ability is currently active. For instance per owner abilities */
	UPROPERTY()
	bool bIsActive;

	/** True if the ability is currently cancelable, if not will only be canceled by hard EndAbility calls */
	UPROPERTY()
	bool bIsCancelable;

	/** A count of all the current scope locks. */
	mutable int8 ScopeLockCount;

	/** A list of all the functions waiting for the scope lock to end so they can run. */
	mutable TArray<FPostLockDelegate> WaitingToExecute;

	/** Increases the scope lock count. */
	void IncrementListLock() const;

	/** Decreases the scope lock count. Runs the waiting to execute delegates if the count drops to zero. */
	void DecrementListLock() const;

	/** True if the ability block flags are currently enabled */
	UPROPERTY()
	bool bIsBlockingOtherAbilities;
};
