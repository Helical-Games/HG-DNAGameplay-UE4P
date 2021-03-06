// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "DNAPrediction.h"
#include "DNAAbilitySpec.h"
#include "Abilities/DNAAbilityTypes.h"
#include "DNATask.h"
#include "Abilities/DNAAbility.h"
#include "AbilityTask.generated.h"

class UDNAAbilitySystemComponent;
class UDNATasksComponent;

/**
 *	DNAAbilityTasks are small, self contained operations that can be performed while executing an ability.
 *	They are latent/asynchronous is nature. They will generally follow the pattern of 'start something and wait until it is finished or interrupted'
 *	
 *	We have code in K2Node_LatentAbilityCall to make using these in blueprints streamlined. The best way to become familiar with DNAAbilityTasks is to 
 *	look at existing tasks like UDNAAbilityTask_WaitOverlap (very simple) and UDNAAbilityTask_WaitTargetData (much more complex).
 *	
 *	These are the basic requirements for using an ability task:
 *	
 *	1) Define dynamic multicast, BlueprintAssignable delegates in your UDNAAbilityTask. These are the OUTPUTs of your task. When these delegates fire,
 *	execution resumes in the calling blueprints.
 *	
 *	2) Your inputs are defined by a static factory function which will instantiate an instance of your task. The parameters of this function define
 *	the INPUTs into your task. All the factory function should do is instantiate your task and possibly set starting parameters. It should NOT invoke
 *	any of the callback delegates!
 *	
 *	3) Implement a Activate() function (defined here in base class). This function should actually start/execute your task logic. It is safe to invoke
 *	callback delegates here.
 *	
 *	
 *	This is all you need for basic DNAAbilityTasks. 
 *	
 *	
 *	CheckList:
 *		-Override ::OnDestroy() and unregister any callbacks that the task registered. Call Super::EndTask too!
 *		-Implemented an Activate function which truly 'starts' the task. Do not 'start' the task in your static factory function!
 *	
 *	
 *	--------------------------------------
 *	
 *	We have additional support for DNAAbilityTasks that want to spawn actors. Though this could be accomplished in an Activate() function, it would not be
 *	possible to pass in dynamic "ExposeOnSpawn" actor properties. This is a powerful feature of blueprints, in order to support this, you need to implement 
 *	a different step 3:
 *	
 *	Instead of an Activate() function, you should implement a BeginSpawningActor() and FinishSpawningActor() function.
 *	
 *	BeginSpawningActor() must take in a TSubclassOf<YourActorClassToSpawn> parameters named 'Class'. It must also have a out reference parameters of type 
 *	YourActorClassToSpawn*& named SpawnedActor. This function is allowed to decide whether it wants to spawn the actor or not (useful if wishing to
 *	predicate actor spawning on network authority).
 *	
 *	BeginSpawningActor() can instantiate an actor with SpawnActorDefferred. This is important, otherwise the UCS will run before spawn parameters are set.
 *	BeginSpawningActor() should also set the SpawnedActor parameter to the actor it spawned.
 *	
 *	[Next, the generated byte code will set the expose on spawn parameters to whatever the user has set]
 *	
 *	If you spawned something, FinishSpawningActor() will be called and pass in the same actor that was just spawned. You MUST call ExecuteConstruction + PostActorConstruction
 *	on this actor!
 *	
 *	This is a lot of steps but in general, DNAAbilityTask_SpawnActor() gives a clear, minimal example.
 *	
 *	
 */

/**
 *	Latent tasks are waiting on something. This is to differeniate waiting on the user to do something vs waiting on the game to do something.
 *	Tasks start WaitingOnGame, and are set to WaitingOnUser when appropriate (see WaitTargetData, WaitIiputPress, etc)
 */
UENUM()
enum class EDNAAbilityTaskWaitState : uint8
{
	/** Task is waiting for the game to do something */
	WaitingOnGame = 0x01,

	/** Waiting for the user to do something */
	WaitingOnUser = 0x02,

	/** Waiting on Avatar (Character/Pawn/Actor) to do something (usually something physical in the world, like land, move, etc) */
	WaitingOnAvatar = 0x04
};

UCLASS(Abstract)
class DNAABILITIES_API UDNAAbilityTask : public UDNATask
{
	GENERATED_UCLASS_BODY()
	
	/** Returns spec handle for owning ability */
	FDNAAbilitySpecHandle GetAbilitySpecHandle() const;

	void SetDNAAbilitySystemComponent(UDNAAbilitySystemComponent* InDNAAbilitySystemComponent);

	/** DNAAbility that created us */
	UPROPERTY()
	UDNAAbility* Ability;

	UPROPERTY()
	UDNAAbilitySystemComponent* DNAAbilitySystemComponent;

	/** Returns true if the ability is a locally predicted ability running on a client. Usually this means we need to tell the server something. */
	bool IsPredictingClient() const;

	/** Returns true if we are executing the ability on the server for a non locally controlled client */
	bool IsForRemoteClient() const;

	/** Returns true if we are executing the ability on the locally controlled client */
	bool IsLocallyControlled() const;

	/** Returns ActivationPredictionKey of owning ability */
	FPredictionKey GetActivationPredictionKey() const;

	virtual void InitSimulatedTask(UDNATasksComponent& InDNATasksComponent) override;

	/** Helper function for instantiating and initializing a new task */
	template <class T>
	static T* NewDNAAbilityTask(UDNAAbility* ThisAbility, FName InstanceName = FName())
	{
		check(ThisAbility);

		T* MyObj = NewObject<T>();
		MyObj->InitTask(*ThisAbility, ThisAbility->GetDNATaskDefaultPriority());
		MyObj->InstanceName = InstanceName;
		return MyObj;
	}

	template<typename T>
	static bool DelayedFalse()
	{
		return false;
	}

	// this function has been added to make sure DNAAbilityTasks don't use this method
	template <class T>
	FORCEINLINE static T* NewTask(UObject* WorldContextObject, FName InstanceName = FName())
	{
		static_assert(DelayedFalse<T>(), "UDNAAbilityTask::NewTask should never be used. Use NewDNAAbilityTask instead");
	}

	/** Called when the ability task is waiting on remote player data. IF the remote player ends the ability prematurely, and a task with this set is still running, the ability is killed. */
	void SetWaitingOnRemotePlayerData();
	void ClearWaitingOnRemotePlayerData();
	virtual bool IsWaitingOnRemotePlayerdata() const override;

	/** same as RemotePlayerData but for ACharacter type of state (movement state, etc) */
	void SetWaitingOnAvatar();
	void ClearWaitingOnAvatar();
	virtual bool IsWaitingOnAvatar() const override;

	/** What we are waiting on */
	uint8 WaitStateBitMask;

protected:
	/** Helper method for registering client replicated callbacks */
	bool CallOrAddReplicatedDelegate(EAbilityGenericReplicatedEvent::Type Event, FSimpleMulticastDelegate::FDelegate Delegate);
};

//For searching through lists of ability instances
struct FAbilityInstanceNamePredicate
{
	FAbilityInstanceNamePredicate(FName DesiredInstanceName)
	{
		InstanceName = DesiredInstanceName;
	}

	bool operator()(const TWeakObjectPtr<UDNAAbilityTask> A) const
	{
		return (A.IsValid() && !A.Get()->GetInstanceName().IsNone() && A.Get()->GetInstanceName().IsValid() && (A.Get()->GetInstanceName() == InstanceName));
	}

	FName InstanceName;
};

struct FAbilityInstanceClassPredicate
{
	FAbilityInstanceClassPredicate(TSubclassOf<UDNAAbilityTask> Class)
	{
		TaskClass = Class;
	}

	bool operator()(const TWeakObjectPtr<UDNAAbilityTask> A) const
	{
		return (A.IsValid() && (A.Get()->GetClass() == TaskClass));
	}

	TSubclassOf<UDNAAbilityTask> TaskClass;
};

#define ABILITYTASK_MSG(Format, ...) \
	if (ENABLE_ABILITYTASK_DEBUGMSG) \
	{ \
		if (Ability) \
			Ability->AddDNAAbilityTaskDebugMessage(this, FString::Printf(TEXT(Format), ##__VA_ARGS__)); \
	} 
