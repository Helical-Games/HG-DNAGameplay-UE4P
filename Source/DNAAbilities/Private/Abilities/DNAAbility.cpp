// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/DNAAbility.h"
#include "TimerManager.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/NetDriver.h"
#include "AbilitySystemStats.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "DNACue_Types.h"

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	UDNAAbility
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

UDNAAbility::UDNAAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	auto ImplementedInBlueprint = [](const UFunction* Func) -> bool
	{
		return Func && ensure(Func->GetOuter())
			&& (Func->GetOuter()->IsA(UBlueprintGeneratedClass::StaticClass()) || Func->GetOuter()->IsA(UDynamicClass::StaticClass()));
	};

	{
		static FName FuncName = FName(TEXT("K2_ShouldAbilityRespondToEvent"));
		UFunction* ShouldRespondFunction = GetClass()->FindFunctionByName(FuncName);
		bHasBlueprintShouldAbilityRespondToEvent = ImplementedInBlueprint(ShouldRespondFunction);
	}
	{
		static FName FuncName = FName(TEXT("K2_CanActivateAbility"));
		UFunction* CanActivateFunction = GetClass()->FindFunctionByName(FuncName);
		bHasBlueprintCanUse = ImplementedInBlueprint(CanActivateFunction);
	}
	{
		static FName FuncName = FName(TEXT("K2_ActivateAbility"));
		UFunction* ActivateFunction = GetClass()->FindFunctionByName(FuncName);
		// FIXME: temp to work around crash
		if (ActivateFunction && (HasAnyFlags(RF_ClassDefaultObject) || ActivateFunction->IsValidLowLevelFast()))
		{
			bHasBlueprintActivate = ImplementedInBlueprint(ActivateFunction);
		}
	}
	{
		static FName FuncName = FName(TEXT("K2_ActivateAbilityFromEvent"));
		UFunction* ActivateFunction = GetClass()->FindFunctionByName(FuncName);
		bHasBlueprintActivateFromEvent = ImplementedInBlueprint(ActivateFunction);
	}
	
#if WITH_EDITOR
	/** Autoregister abilities with the blueprint debugger in the editor.*/
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		UBlueprint* BP = Cast<UBlueprint>(GetClass()->ClassGeneratedBy);
		if (BP && (BP->GetWorldBeingDebugged() == nullptr || BP->GetWorldBeingDebugged() == GetWorld()))
		{
			BP->SetObjectBeingDebugged(this);
		}
	}
#endif

	bServerRespectsRemoteAbilityCancellation = true;
	bReplicateInputDirectly = false;
	RemoteInstanceEnded = false;

	InstancingPolicy = EDNAAbilityInstancingPolicy::InstancedPerExecution;

	ScopeLockCount = 0;
}

int32 UDNAAbility::GetFunctionCallspace(UFunction* Function, void* Parameters, FFrame* Stack)
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return FunctionCallspace::Local;
	}
	check(GetOuter() != NULL);
	return GetOuter()->GetFunctionCallspace(Function, Parameters, Stack);
}

bool UDNAAbility::CallRemoteFunction(UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack)
{
	check(!HasAnyFlags(RF_ClassDefaultObject));
	check(GetOuter() != NULL);

	AActor* Owner = CastChecked<AActor>(GetOuter());
	UNetDriver* NetDriver = Owner->GetNetDriver();
	if (NetDriver)
	{
		NetDriver->ProcessRemoteFunction(Owner, Function, Parameters, OutParms, Stack, this);
		return true;
	}

	return false;
}

// TODO: polymorphic payload
void UDNAAbility::SendDNAEvent(FDNATag EventTag, FDNAEventData Payload)
{
	UDNAAbilitySystemComponent* DNAAbilitySystemComponent = CurrentActorInfo->DNAAbilitySystemComponent.Get();
	if (ensure(DNAAbilitySystemComponent))
	{
		FScopedPredictionWindow NewScopedWindow(DNAAbilitySystemComponent, true);
		DNAAbilitySystemComponent->HandleDNAEvent(EventTag, &Payload);
	}
}

void UDNAAbility::PostNetInit()
{
	/** We were dynamically spawned from replication - we need to init a currentactorinfo by looking at outer.
	 *  This may need to be updated further if we start having abilities live on different outers than player DNAAbilitySystemComponents.
	 */
	
	if (CurrentActorInfo == NULL)
	{
		AActor* OwnerActor = Cast<AActor>(GetOuter());
		if (ensure(OwnerActor))
		{
			UDNAAbilitySystemComponent* DNAAbilitySystemComponent = UDNAAbilitySystemGlobals::GetDNAAbilitySystemComponentFromActor(OwnerActor);
			if (ensure(DNAAbilitySystemComponent))
			{
				CurrentActorInfo = DNAAbilitySystemComponent->AbilityActorInfo.Get();
			}
		}
	}
}

bool UDNAAbility::IsActive() const
{
	// Only Instanced-Per-Actor abilities persist between activations
	if (GetInstancingPolicy() == EDNAAbilityInstancingPolicy::InstancedPerActor)
	{
		return bIsActive;
	}

	// this should not be called on NonInstanced warn about it, Should call IsActive on the ability spec instead
	if (GetInstancingPolicy() == EDNAAbilityInstancingPolicy::NonInstanced)
	{
		ABILITY_LOG(Warning, TEXT("UDNAAbility::IsActive() called on %s NonInstanced ability, call IsActive on the Ability Spec instead"), *GetName());
	}

	// NonInstanced and Instanced-Per-Execution abilities are by definition active unless they are pending kill
	return !IsPendingKill();
}

bool UDNAAbility::IsSupportedForNetworking() const
{
	/**
	 *	We can only replicate references to:
	 *		-CDOs and DataAssets (e.g., static, non-instanced DNA abilities)
	 *		-Instanced abilities that are replicating (and will thus be created on clients).
	 *		
	 *	Otherwise it is not supported, and it will be recreated on the client
	 */

	bool Supported = GetReplicationPolicy() != EDNAAbilityReplicationPolicy::ReplicateNo || GetOuter()->IsA(UPackage::StaticClass());

	return Supported;
}

bool UDNAAbility::DoesAbilitySatisfyTagRequirements(const UDNAAbilitySystemComponent& DNAAbilitySystemComponent, const FDNATagContainer* SourceTags, const FDNATagContainer* TargetTags, OUT FDNATagContainer* OptionalRelevantTags) const
{
	bool bBlocked = false;
	bool bMissing = false;

	const FDNATag& BlockedTag = UDNAAbilitySystemGlobals::Get().ActivateFailTagsBlockedTag;
	const FDNATag& MissingTag = UDNAAbilitySystemGlobals::Get().ActivateFailTagsMissingTag;

	// Check if any of this ability's tags are currently blocked
	if (DNAAbilitySystemComponent.AreAbilityTagsBlocked(AbilityTags))
	{
		bBlocked = true;
	}

	// Check to see the required/blocked tags for this ability
	if (ActivationBlockedTags.Num() || ActivationRequiredTags.Num())
	{
		static FDNATagContainer DNAAbilitySystemComponentTags;
		DNAAbilitySystemComponentTags.Reset();

		DNAAbilitySystemComponent.GetOwnedDNATags(DNAAbilitySystemComponentTags);

		if (DNAAbilitySystemComponentTags.HasAny(ActivationBlockedTags))
		{
			bBlocked = true;
		}

		if (!DNAAbilitySystemComponentTags.HasAll(ActivationRequiredTags))
		{
			bMissing = true;
		}
	}

	if (SourceTags != nullptr)
	{
		if (SourceBlockedTags.Num() || SourceRequiredTags.Num())
		{
			if (SourceTags->HasAny(SourceBlockedTags))
			{
				bBlocked = true;
			}

			if (!SourceTags->HasAll(SourceRequiredTags))
			{
				bMissing = true;
			}
		}
	}

	if (TargetTags != nullptr)
	{
		if (TargetBlockedTags.Num() || TargetRequiredTags.Num())
		{
			if (TargetTags->HasAny(TargetBlockedTags))
			{
				bBlocked = true;
			}

			if (!TargetTags->HasAll(TargetRequiredTags))
			{
				bMissing = true;
			}
		}
	}

	if (bBlocked)
	{
		if (OptionalRelevantTags && BlockedTag.IsValid())
		{
			OptionalRelevantTags->AddTag(BlockedTag);
		}
		return false;
	}
	if (bMissing)
	{
		if (OptionalRelevantTags && MissingTag.IsValid())
		{
			OptionalRelevantTags->AddTag(MissingTag);
		}
		return false;
	}
	
	return true;
}

bool UDNAAbility::ShouldActivateAbility(ENetRole Role) const
{
	return Role != ROLE_SimulatedProxy;
}

bool UDNAAbility::CanActivateAbility(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNATagContainer* SourceTags, const FDNATagContainer* TargetTags, OUT FDNATagContainer* OptionalRelevantTags) const
{
	// Don't set the actor info, CanActivate is called on the CDO

	// A valid AvatarActor is required. Simulated proxy check means only authority or autonomous proxies should be executing abilities.
	if (ActorInfo == nullptr || ActorInfo->AvatarActor == nullptr || !ShouldActivateAbility(ActorInfo->AvatarActor->Role))
	{
		return false;
	}

	//make into a reference for simplicity
	static FDNATagContainer DummyContainer;
	DummyContainer.Reset();

	FDNATagContainer& OutTags = OptionalRelevantTags ? *OptionalRelevantTags : DummyContainer;

	// make sure the ActorInfo and its ability system component are valid, if not bail out.
	if (ActorInfo == nullptr || !ActorInfo->DNAAbilitySystemComponent.IsValid())
	{
		return false;
	}

	if (ActorInfo->DNAAbilitySystemComponent->GetUserAbilityActivationInhibited())
	{
		/**
		 *	Input is inhibited (UI is pulled up, another ability may be blocking all other input, etc).
		 *	When we get into triggered abilities, we may need to better differentiate between CanActviate and CanUserActivate or something.
		 *	E.g., we would want LMB/RMB to be inhibited while the user is in the menu UI, but we wouldn't want to prevent a 'buff when I am low health'
		 *	ability to not trigger.
		 *	
		 *	Basically: CanActivateAbility is only used by user activated abilities now. If triggered abilities need to check costs/cooldowns, then we may
		 *	want to split this function up and change the calling API to distinguish between 'can I initiate an ability activation' and 'can this ability be activated'.
		 */ 
		return false;
	}
	
	if (!UDNAAbilitySystemGlobals::Get().ShouldIgnoreCooldowns() && !CheckCooldown(Handle, ActorInfo, OptionalRelevantTags))
	{
		return false;
	}

	if (!UDNAAbilitySystemGlobals::Get().ShouldIgnoreCosts() && !CheckCost(Handle, ActorInfo, OptionalRelevantTags))
	{
		return false;
	}

	if (!DoesAbilitySatisfyTagRequirements(*ActorInfo->DNAAbilitySystemComponent.Get(), SourceTags, TargetTags, OptionalRelevantTags))
	{	// If the ability's tags are blocked, or if it has a "Blocking" tag or is missing a "Required" tag, then it can't activate.
		return false;
	}

	FDNAAbilitySpec* Spec = ActorInfo->DNAAbilitySystemComponent->FindAbilitySpecFromHandle(Handle);
	if (!Spec)
	{
		ABILITY_LOG(Warning, TEXT("CanActivateAbility called with invalid Handle"));
		return false;
	}

	// Check if this ability's input binding is currently blocked
	if (ActorInfo->DNAAbilitySystemComponent->IsAbilityInputBlocked(Spec->InputID))
	{
		return false;
	}

	if (bHasBlueprintCanUse)
	{
		if (K2_CanActivateAbility(*ActorInfo, OutTags) == false)
		{
			ABILITY_LOG(Log, TEXT("CanActivateAbility %s failed, blueprint refused"), *GetName());
			return false;
		}
	}

	return true;
}

bool UDNAAbility::ShouldAbilityRespondToEvent(const FDNAAbilityActorInfo* ActorInfo, const FDNAEventData* Payload) const
{
	if (bHasBlueprintShouldAbilityRespondToEvent)
	{
		if (K2_ShouldAbilityRespondToEvent(*ActorInfo, *Payload) == false)
		{
			ABILITY_LOG(Log, TEXT("ShouldAbilityRespondToEvent %s failed, blueprint refused"), *GetName());
			return false;
		}
	}

	return true;
}

bool UDNAAbility::CommitAbility(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo)
{
	// Last chance to fail (maybe we no longer have resources to commit since we after we started this ability activation)
	if (!CommitCheck(Handle, ActorInfo, ActivationInfo))
	{
		return false;
	}

	CommitExecute(Handle, ActorInfo, ActivationInfo);

	// Fixme: Should we always call this or only if it is implemented? A noop may not hurt but could be bad for perf (storing a HasBlueprintCommit per instance isn't good either)
	K2_CommitExecute();

	// Broadcast this commitment
	ActorInfo->DNAAbilitySystemComponent->NotifyAbilityCommit(this);

	return true;
}

bool UDNAAbility::CommitAbilityCooldown(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo, const bool ForceCooldown)
{
	if (UDNAAbilitySystemGlobals::Get().ShouldIgnoreCooldowns())
	{
		return true;
	}

	if (!ForceCooldown)
	{
		// Last chance to fail (maybe we no longer have resources to commit since we after we started this ability activation)
		if (!CheckCooldown(Handle, ActorInfo))
		{
			return false;
		}
	}

	ApplyCooldown(Handle, ActorInfo, ActivationInfo);
	return true;
}

bool UDNAAbility::CommitAbilityCost(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo)
{
	if (UDNAAbilitySystemGlobals::Get().ShouldIgnoreCosts())
	{
		return true;
	}

	// Last chance to fail (maybe we no longer have resources to commit since we after we started this ability activation)
	if (!CheckCost(Handle, ActorInfo))
	{
		return false;
	}

	ApplyCost(Handle, ActorInfo, ActivationInfo);
	return true;
}

bool UDNAAbility::CommitCheck(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo)
{
	/**
	 *	Checks if we can (still) commit this ability. There are some subtleties here.
	 *		-An ability can start activating, play an animation, wait for a user confirmation/target data, and then actually commit
	 *		-Commit = spend resources/cooldowns. Its possible the source has changed state since he started activation, so a commit may fail.
	 *		-We don't want to just call CanActivateAbility() since right now that also checks things like input inhibition.
	 *			-E.g., its possible the act of starting your ability makes it no longer activatable (CanaCtivateAbility() may be false if called here).
	 */

	const bool bValidHandle = Handle.IsValid();
	const bool bValidActorInfoPieces = (ActorInfo && (ActorInfo->DNAAbilitySystemComponent != nullptr));
	const bool bValidSpecFound = bValidActorInfoPieces && (ActorInfo->DNAAbilitySystemComponent->FindAbilitySpecFromHandle(Handle) != nullptr);

	// Ensure that the ability spec is even valid before trying to process the commit
	if (!bValidHandle || !bValidActorInfoPieces || !bValidSpecFound)
	{
		ensureMsgf(false, TEXT("UDNAAbility::CommitCheck provided an invalid handle or actor info or couldn't find ability spec: %s Handle Valid: %d ActorInfo Valid: %d Spec Not Found: %d"), *GetName(), bValidHandle, bValidActorInfoPieces, bValidSpecFound);
		return false;
	}

	if (!UDNAAbilitySystemGlobals::Get().ShouldIgnoreCooldowns() && !CheckCooldown(Handle, ActorInfo))
	{
		return false;
	}

	if (!UDNAAbilitySystemGlobals::Get().ShouldIgnoreCosts() && !CheckCost(Handle, ActorInfo))
	{
		return false;
	}

	return true;
}

void UDNAAbility::CommitExecute(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo)
{
	ApplyCooldown(Handle, ActorInfo, ActivationInfo);

	ApplyCost(Handle, ActorInfo, ActivationInfo);
}

bool UDNAAbility::CanBeCanceled() const
{
	if (GetInstancingPolicy() != EDNAAbilityInstancingPolicy::NonInstanced)
	{
		return bIsCancelable;
	}

	// Non instanced are always cancelable
	return true;
}

void UDNAAbility::SetCanBeCanceled(bool bCanBeCanceled)
{
	if (GetInstancingPolicy() != EDNAAbilityInstancingPolicy::NonInstanced && bCanBeCanceled != bIsCancelable)
	{
		bIsCancelable = bCanBeCanceled;

		UDNAAbilitySystemComponent* Comp = CurrentActorInfo->DNAAbilitySystemComponent.Get();
		if (Comp)
		{
			Comp->HandleChangeAbilityCanBeCanceled(AbilityTags, this, bCanBeCanceled);
		}
	}
}

bool UDNAAbility::IsBlockingOtherAbilities() const
{
	if (GetInstancingPolicy() != EDNAAbilityInstancingPolicy::NonInstanced)
	{
		return bIsBlockingOtherAbilities;
	}

	// Non instanced are always marked as blocking other abilities
	return true;
}

void UDNAAbility::SetShouldBlockOtherAbilities(bool bShouldBlockAbilities)
{
	if (bIsActive && GetInstancingPolicy() != EDNAAbilityInstancingPolicy::NonInstanced && bShouldBlockAbilities != bIsBlockingOtherAbilities)
	{
		bIsBlockingOtherAbilities = bShouldBlockAbilities;

		UDNAAbilitySystemComponent* Comp = CurrentActorInfo->DNAAbilitySystemComponent.Get();
		if (Comp)
		{
			Comp->ApplyAbilityBlockAndCancelTags(AbilityTags, this, bIsBlockingOtherAbilities, BlockAbilitiesWithTag, false, CancelAbilitiesWithTag);
		}
	}
}

void UDNAAbility::CancelAbility(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility)
{
	if (CanBeCanceled())
	{
		if (ScopeLockCount > 0)
		{
			WaitingToExecute.Add(FPostLockDelegate::CreateUObject(this, &UDNAAbility::CancelAbility, Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility));
			return;
		}

		// Replicate the the server/client if needed
		if (bReplicateCancelAbility)
		{
			ActorInfo->DNAAbilitySystemComponent->ReplicateEndOrCancelAbility(Handle, ActivationInfo, this, true);
		}

		// Gives the Ability BP a chance to perform custom logic/cleanup when any active ability states are active
		if (OnDNAAbilityCancelled.IsBound())
		{
			OnDNAAbilityCancelled.Broadcast();
		}

		// End the ability but don't replicate it, we replicate the CancelAbility call directly
		bool bReplicateEndAbility = false;
		bool bWasCancelled = true;
		EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	}
}

bool UDNAAbility::IsEndAbilityValid(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo) const
{
	// Protect against EndAbility being called multiple times
	// Ending an AbilityState may cause this to be invoked again
	if (bIsActive == false && GetInstancingPolicy() != EDNAAbilityInstancingPolicy::NonInstanced)
	{
		return false;
	}

	// check if ability has valid owner
	UDNAAbilitySystemComponent* AbilityComp = ActorInfo ? ActorInfo->DNAAbilitySystemComponent.Get() : nullptr;
	if (AbilityComp == nullptr)
	{
		return false;
	}

	// check to see if this is an NonInstanced or if the ability is active.
	const FDNAAbilitySpec* Spec = AbilityComp ? AbilityComp->FindAbilitySpecFromHandle(Handle) : nullptr;
	const bool bIsSpecActive = Spec ? Spec->IsActive() : IsActive();

	if (!bIsSpecActive)
	{
		return false;
	}

	return true;
}

void UDNAAbility::EndAbility(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (IsEndAbilityValid(Handle, ActorInfo))
	{
		if (ScopeLockCount > 0)
		{
			WaitingToExecute.Add(FPostLockDelegate::CreateUObject(this, &UDNAAbility::EndAbility, Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled));
			return;
		}

		// Give blueprint a chance to react
		K2_OnEndAbility();

		// Protect against blueprint causing us to EndAbility already
		if (bIsActive == false && GetInstancingPolicy() != EDNAAbilityInstancingPolicy::NonInstanced)
		{
			return;
		}

		// Stop any timers or latent actions for the ability
		UWorld* MyWorld = GetWorld();
		if (MyWorld)
		{
			MyWorld->GetLatentActionManager().RemoveActionsForObject(this);
			MyWorld->GetTimerManager().ClearAllTimersForObject(this);
		}

		// Execute our delegate and unbind it, as we are no longer active and listeners can re-register when we become active again.
		OnDNAAbilityEnded.Broadcast(this);
		OnDNAAbilityEnded.Clear();

		if (GetInstancingPolicy() != EDNAAbilityInstancingPolicy::NonInstanced)
		{
			bIsActive = false;
		}

		// Tell all our tasks that we are finished and they should cleanup
		for (int32 TaskIdx = ActiveTasks.Num() - 1; TaskIdx >= 0 && ActiveTasks.Num() > 0; --TaskIdx)
		{
			UDNATask* Task = ActiveTasks[TaskIdx];
			if (Task)
			{
				Task->TaskOwnerEnded();
			}
		}
		ActiveTasks.Reset();	// Empty the array but dont resize memory, since this object is probably going to be destroyed very soon anyways.

		// TODO: is this condition still required? validity of DNAAbilitySystemComponent is checked by IsEndAbilityValid()
		if (ActorInfo && ActorInfo->DNAAbilitySystemComponent.IsValid())
		{
			if (bReplicateEndAbility)
			{
				ActorInfo->DNAAbilitySystemComponent->ReplicateEndOrCancelAbility(Handle, ActivationInfo, this, false);
			}

			// Remove tags we added to owner
			ActorInfo->DNAAbilitySystemComponent->RemoveLooseDNATags(ActivationOwnedTags);

			// Remove tracked DNACues that we added
			for (FDNATag& DNACueTag : TrackedDNACues)
			{
				ActorInfo->DNAAbilitySystemComponent->RemoveDNACue(DNACueTag);
			}
			TrackedDNACues.Empty();

			if (CanBeCanceled())
			{
				// If we're still cancelable, cancel it now
				ActorInfo->DNAAbilitySystemComponent->HandleChangeAbilityCanBeCanceled(AbilityTags, this, false);
			}
			
			if (IsBlockingOtherAbilities())
			{
				// If we're still blocking other abilities, cancel now
				ActorInfo->DNAAbilitySystemComponent->ApplyAbilityBlockAndCancelTags(AbilityTags, this, false, BlockAbilitiesWithTag, false, CancelAbilitiesWithTag);
			}

			// Tell owning DNAAbilitySystemComponent that we ended so it can do stuff (including MarkPendingKill us)
			ActorInfo->DNAAbilitySystemComponent->NotifyAbilityEnded(Handle, this, bWasCancelled);
		}
	}
}

void UDNAAbility::ActivateAbility(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo, const FDNAEventData* TriggerEventData)
{
	if (bHasBlueprintActivate)
	{
		// A Blueprinted ActivateAbility function must call CommitAbility somewhere in its execution chain.
		K2_ActivateAbility();
	}
	else if (bHasBlueprintActivateFromEvent)
	{
		if (TriggerEventData)
		{
			// A Blueprinted ActivateAbility function must call CommitAbility somewhere in its execution chain.
			K2_ActivateAbilityFromEvent(*TriggerEventData);
		}
		else
		{
			UE_LOG(LogDNAAbilitySystem, Warning, TEXT("Ability %s expects event data but none is being supplied. Use Activate Ability instead of Activate Ability From Event."), *GetName());
			bool bReplicateEndAbility = false;
			bool bWasCancelled = true;
			EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
		}
	}
	else
	{
		// Native child classes may want to override ActivateAbility and do something like this:

		// Do stuff...

		if (CommitAbility(Handle, ActorInfo, ActivationInfo))		// ..then commit the ability...
		{			
			//	Then do more stuff...
		}
	}
}

void UDNAAbility::PreActivate(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo, FOnDNAAbilityEnded::FDelegate* OnDNAAbilityEndedDelegate)
{
	UDNAAbilitySystemComponent* Comp = ActorInfo->DNAAbilitySystemComponent.Get();

	if (GetInstancingPolicy() != EDNAAbilityInstancingPolicy::NonInstanced)
	{
		bIsActive = true;
		bIsBlockingOtherAbilities = true;
		bIsCancelable = true;
	}

	Comp->HandleChangeAbilityCanBeCanceled(AbilityTags, this, true);
	Comp->ApplyAbilityBlockAndCancelTags(AbilityTags, this, true, BlockAbilitiesWithTag, true, CancelAbilitiesWithTag);
	Comp->AddLooseDNATags(ActivationOwnedTags);

	if (OnDNAAbilityEndedDelegate)
	{
		OnDNAAbilityEnded.Add(*OnDNAAbilityEndedDelegate);
	}

	SetCurrentInfo(Handle, ActorInfo, ActivationInfo);

	Comp->NotifyAbilityActivated(Handle, this);
}

void UDNAAbility::CallActivateAbility(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo, FOnDNAAbilityEnded::FDelegate* OnDNAAbilityEndedDelegate, const FDNAEventData* TriggerEventData)
{
	PreActivate(Handle, ActorInfo, ActivationInfo, OnDNAAbilityEndedDelegate);
	ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UDNAAbility::ConfirmActivateSucceed()
{
	// On instanced abilities, update CurrentActivationInfo and call any registered delegates.
	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		PostNetInit();
		check(CurrentActorInfo);
		CurrentActivationInfo.SetActivationConfirmed();

		OnConfirmDelegate.Broadcast(this);
		OnConfirmDelegate.Clear();
	}
}

UDNAEffect* UDNAAbility::GetCooldownDNAEffect() const
{
	if ( CooldownDNAEffectClass )
	{
		return CooldownDNAEffectClass->GetDefaultObject<UDNAEffect>();
	}
	else
	{
		return nullptr;
	}
}

UDNAEffect* UDNAAbility::GetCostDNAEffect() const
{
	if ( CostDNAEffectClass )
	{
		return CostDNAEffectClass->GetDefaultObject<UDNAEffect>();
	}
	else
	{
		return nullptr;
	}
}

bool UDNAAbility::CheckCooldown(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, OUT FDNATagContainer* OptionalRelevantTags) const
{
	const FDNATagContainer* CooldownTags = GetCooldownTags();
	if (CooldownTags)
	{
		check(ActorInfo->DNAAbilitySystemComponent.IsValid());
		if (CooldownTags->Num() > 0 && ActorInfo->DNAAbilitySystemComponent->HasAnyMatchingDNATags(*CooldownTags))
		{
			const FDNATag& CooldownTag = UDNAAbilitySystemGlobals::Get().ActivateFailCooldownTag;

			if (OptionalRelevantTags && CooldownTag.IsValid())
			{
				OptionalRelevantTags->AddTag(CooldownTag);
			}

			return false;
		}
	}
	return true;
}

void UDNAAbility::ApplyCooldown(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo) const
{
	UDNAEffect* CooldownGE = GetCooldownDNAEffect();
	if (CooldownGE)
	{
		ApplyDNAEffectToOwner(Handle, ActorInfo, ActivationInfo, CooldownGE, GetAbilityLevel(Handle, ActorInfo));
	}
}

bool UDNAAbility::CheckCost(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, OUT FDNATagContainer* OptionalRelevantTags) const
{
	UDNAEffect* CostGE = GetCostDNAEffect();
	if (CostGE)
	{
		check(ActorInfo->DNAAbilitySystemComponent.IsValid());
		if (!ActorInfo->DNAAbilitySystemComponent->CanApplyAttributeModifiers(CostGE, GetAbilityLevel(Handle, ActorInfo), MakeEffectContext(Handle, ActorInfo)))
		{
			const FDNATag& CostTag = UDNAAbilitySystemGlobals::Get().ActivateFailCostTag;

			if (OptionalRelevantTags && CostTag.IsValid())
			{
				OptionalRelevantTags->AddTag(CostTag);
			}
			return false;
		}
	}
	return true;
}

void UDNAAbility::ApplyCost(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo) const
{
	UDNAEffect* CostGE = GetCostDNAEffect();
	if (CostGE)
	{
		ApplyDNAEffectToOwner(Handle, ActorInfo, ActivationInfo, CostGE, GetAbilityLevel(Handle, ActorInfo));
	}
}

void UDNAAbility::SetMovementSyncPoint(FName SyncName)
{
}

float UDNAAbility::GetCooldownTimeRemaining(const FDNAAbilityActorInfo* ActorInfo) const
{
	SCOPE_CYCLE_COUNTER(STAT_DNAAbilityGetCooldownTimeRemaining);

	if (ActorInfo->DNAAbilitySystemComponent.IsValid())
	{
		const FDNATagContainer* CooldownTags = GetCooldownTags();
		if (CooldownTags && CooldownTags->Num() > 0)
		{
			FDNAEffectQuery const Query = FDNAEffectQuery::MakeQuery_MatchAnyOwningTags(*CooldownTags);
			TArray< float > Durations = ActorInfo->DNAAbilitySystemComponent->GetActiveEffectsTimeRemaining(Query);
			if (Durations.Num() > 0)
			{
				Durations.Sort();
				return Durations[Durations.Num() - 1];
			}
		}
	}

	return 0.f;
}

void UDNAAbility::InvalidateClientPredictionKey() const
{
	if (CurrentActorInfo && CurrentActorInfo->DNAAbilitySystemComponent.IsValid())
	{
		CurrentActorInfo->DNAAbilitySystemComponent->ScopedPredictionKey = FPredictionKey();
	}
}

void UDNAAbility::GetCooldownTimeRemainingAndDuration(FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, float& TimeRemaining, float& CooldownDuration) const
{
	SCOPE_CYCLE_COUNTER(STAT_DNAAbilityGetCooldownTimeRemainingAndDuration);

	check(ActorInfo->DNAAbilitySystemComponent.IsValid());

	TimeRemaining = 0.f;
	CooldownDuration = 0.f;
	
	const FDNATagContainer* CooldownTags = GetCooldownTags();
	if (CooldownTags && CooldownTags->Num() > 0)
	{
		FDNAEffectQuery const Query = FDNAEffectQuery::MakeQuery_MatchAnyOwningTags(*CooldownTags);
		TArray< TPair<float,float> > DurationAndTimeRemaining = ActorInfo->DNAAbilitySystemComponent->GetActiveEffectsTimeRemainingAndDuration(Query);
		if (DurationAndTimeRemaining.Num() > 0)
		{
			int32 BestIdx = 0;
			float LongestTime = DurationAndTimeRemaining[0].Key;
			for (int32 Idx = 1; Idx < DurationAndTimeRemaining.Num(); ++Idx)
			{
				if (DurationAndTimeRemaining[Idx].Key > LongestTime)
				{
					LongestTime = DurationAndTimeRemaining[Idx].Key;
					BestIdx = Idx;
				}
			}

			TimeRemaining = DurationAndTimeRemaining[BestIdx].Key;
			CooldownDuration = DurationAndTimeRemaining[BestIdx].Value;
		}
	}
}

const FDNATagContainer* UDNAAbility::GetCooldownTags() const
{
	UDNAEffect* CDGE = GetCooldownDNAEffect();
	return CDGE ? &CDGE->InheritableOwnedTagsContainer.CombinedTags : nullptr;
}

FDNAAbilityActorInfo UDNAAbility::GetActorInfo() const
{
	if (!ensure(CurrentActorInfo))
	{
		return FDNAAbilityActorInfo();
	}
	return *CurrentActorInfo;
}

AActor* UDNAAbility::GetOwningActorFromActorInfo() const
{
	if (!ensureMsgf(IsInstantiated(), TEXT("%s: GetOwningActorFromActorInfo can not be called on a non-instanced ability"), *GetName()))
	{
		ABILITY_LOG(Warning, TEXT("%s: GetOwningActorFromActorInfo can not be called on a non-instanced ability"), *GetName());
		return nullptr;
	}

	if (!ensure(CurrentActorInfo))
	{
		return nullptr;
	}
	return CurrentActorInfo->OwnerActor.Get();
}

AActor* UDNAAbility::GetAvatarActorFromActorInfo() const
{
	if (!ensure(CurrentActorInfo))
	{
		return nullptr;
	}
	return CurrentActorInfo->AvatarActor.Get();
}

USkeletalMeshComponent* UDNAAbility::GetOwningComponentFromActorInfo() const
{
	if (!ensure(CurrentActorInfo))
	{
		return nullptr;
	}

	return CurrentActorInfo->SkeletalMeshComponent.Get();
}

FDNAEffectSpecHandle UDNAAbility::MakeOutgoingDNAEffectSpec(TSubclassOf<UDNAEffect> DNAEffectClass, float Level) const
{
	check(CurrentActorInfo && CurrentActorInfo->DNAAbilitySystemComponent.IsValid());
	return MakeOutgoingDNAEffectSpec(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, DNAEffectClass, Level);
}

int32 DNAAbilitySystemShowMakeOutgoingDNAEffectSpecs = 0;
static FAutoConsoleVariableRef CVarDNAAbilitySystemShowMakeOutgoingDNAEffectSpecs(TEXT("DNAAbilitySystem.ShowClientMakeOutgoingSpecs"), DNAAbilitySystemShowMakeOutgoingDNAEffectSpecs, TEXT("Displays all DNAEffect specs created on non authority clients"), ECVF_Default );

FDNAEffectSpecHandle UDNAAbility::MakeOutgoingDNAEffectSpec(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo, TSubclassOf<UDNAEffect> DNAEffectClass, float Level) const
{
	check(ActorInfo);

	
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (DNAAbilitySystemShowMakeOutgoingDNAEffectSpecs && HasAuthority(&ActivationInfo) == false)
	{
		ABILITY_LOG(Warning, TEXT("%s, MakeOutgoingDNAEffectSpec: %s"), *ActorInfo->DNAAbilitySystemComponent->GetFullName(),  *DNAEffectClass->GetName()); 
	}
#endif

	FDNAEffectSpecHandle NewHandle = ActorInfo->DNAAbilitySystemComponent->MakeOutgoingSpec(DNAEffectClass, Level, MakeEffectContext(Handle, ActorInfo));
	if (NewHandle.IsValid())
	{
		FDNAAbilitySpec* AbilitySpec =  ActorInfo->DNAAbilitySystemComponent->FindAbilitySpecFromHandle(Handle);
		ApplyAbilityTagsToDNAEffectSpec(*NewHandle.Data.Get(), AbilitySpec);
	}
	return NewHandle;
}

void UDNAAbility::ApplyAbilityTagsToDNAEffectSpec(FDNAEffectSpec& Spec, FDNAAbilitySpec* AbilitySpec) const
{
	Spec.CapturedSourceTags.GetSpecTags().AppendTags(AbilityTags);

	// Allow the source object of the ability to propagate tags along as well
	if (AbilitySpec)
	{
		const IDNATagAssetInterface* SourceObjAsTagInterface = Cast<IDNATagAssetInterface>(AbilitySpec->SourceObject);
		if (SourceObjAsTagInterface)
		{
			FDNATagContainer SourceObjTags;
			SourceObjAsTagInterface->GetOwnedDNATags(SourceObjTags);

			Spec.CapturedSourceTags.GetSpecTags().AppendTags(SourceObjTags);
		}
	}
}

/** Fixme: Naming is confusing here */

bool UDNAAbility::K2_CommitAbility()
{
	check(CurrentActorInfo);
	return CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo);
}

bool UDNAAbility::K2_CommitAbilityCooldown(bool BroadcastCommitEvent, bool ForceCooldown)
{
	check(CurrentActorInfo);
	if (BroadcastCommitEvent)
	{
		CurrentActorInfo->DNAAbilitySystemComponent->NotifyAbilityCommit(this);
	}
	return CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, ForceCooldown);
}

bool UDNAAbility::K2_CommitAbilityCost(bool BroadcastCommitEvent)
{
	check(CurrentActorInfo);
	if (BroadcastCommitEvent)
	{
		CurrentActorInfo->DNAAbilitySystemComponent->NotifyAbilityCommit(this);
	}
	return CommitAbilityCost(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo);
}

bool UDNAAbility::K2_CheckAbilityCooldown()
{
	check(CurrentActorInfo);
	return UDNAAbilitySystemGlobals::Get().ShouldIgnoreCooldowns() || CheckCooldown(CurrentSpecHandle, CurrentActorInfo);
}

bool UDNAAbility::K2_CheckAbilityCost()
{
	check(CurrentActorInfo);
	return UDNAAbilitySystemGlobals::Get().ShouldIgnoreCosts() || CheckCost(CurrentSpecHandle, CurrentActorInfo);
}

void UDNAAbility::K2_EndAbility()
{
	check(CurrentActorInfo);

	bool bReplicateEndAbility = true;
	bool bWasCancelled = false;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// --------------------------------------------------------------------

void UDNAAbility::MontageJumpToSection(FName SectionName)
{
	check(CurrentActorInfo);

	if (CurrentActorInfo->DNAAbilitySystemComponent->IsAnimatingAbility(this))
	{
		CurrentActorInfo->DNAAbilitySystemComponent->CurrentMontageJumpToSection(SectionName);
	}
}

void UDNAAbility::MontageSetNextSectionName(FName FromSectionName, FName ToSectionName)
{
	check(CurrentActorInfo);

	if (CurrentActorInfo->DNAAbilitySystemComponent->IsAnimatingAbility(this))
	{
		CurrentActorInfo->DNAAbilitySystemComponent->CurrentMontageSetNextSectionName(FromSectionName, ToSectionName);
	}
}

void UDNAAbility::MontageStop(float OverrideBlendOutTime)
{
	check(CurrentActorInfo);

	UDNAAbilitySystemComponent* DNAAbilitySystemComponent = CurrentActorInfo->DNAAbilitySystemComponent.Get();
	if (DNAAbilitySystemComponent != NULL)
	{
		// We should only stop the current montage if we are the animating ability
		if (DNAAbilitySystemComponent->IsAnimatingAbility(this))
		{
			DNAAbilitySystemComponent->CurrentMontageStop();
		}
	}
}

void UDNAAbility::SetCurrentMontage(class UAnimMontage* InCurrentMontage)
{
	ensure(IsInstantiated());
	CurrentMontage = InCurrentMontage;
}

UAnimMontage* UDNAAbility::GetCurrentMontage() const
{
	return CurrentMontage;
}

// --------------------------------------------------------------------

FDNAAbilityTargetingLocationInfo UDNAAbility::MakeTargetLocationInfoFromOwnerActor()
{
	FDNAAbilityTargetingLocationInfo ReturnLocation;
	ReturnLocation.LocationType = EDNAAbilityTargetingLocationType::ActorTransform;
	ReturnLocation.SourceActor = GetActorInfo().AvatarActor.Get();
	ReturnLocation.SourceAbility = this;
	return ReturnLocation;
}

FDNAAbilityTargetingLocationInfo UDNAAbility::MakeTargetLocationInfoFromOwnerSkeletalMeshComponent(FName SocketName)
{
	FDNAAbilityTargetingLocationInfo ReturnLocation;
	ReturnLocation.LocationType = EDNAAbilityTargetingLocationType::SocketTransform;
	ReturnLocation.SourceComponent = GetActorInfo().SkeletalMeshComponent.Get();
	ReturnLocation.SourceAbility = this;
	ReturnLocation.SourceSocketName = SocketName;
	return ReturnLocation;
}

//----------------------------------------------------------------------

UDNATasksComponent* UDNAAbility::GetDNATasksComponent(const UDNATask& Task) const
{
	return GetCurrentActorInfo() ? GetCurrentActorInfo()->DNAAbilitySystemComponent.Get() : nullptr;
}

AActor* UDNAAbility::GetDNATaskOwner(const UDNATask* Task) const
{
	const FDNAAbilityActorInfo* Info = GetCurrentActorInfo();
	return Info ? Info->OwnerActor.Get() : nullptr;
}

AActor* UDNAAbility::GetDNATaskAvatar(const UDNATask* Task) const
{
	const FDNAAbilityActorInfo* Info = GetCurrentActorInfo();
	return Info ? Info->AvatarActor.Get() : nullptr;
}

void UDNAAbility::OnDNATaskInitialized(UDNATask& Task)
{
	UDNAAbilityTask* UDNAAbilityTask = Cast<UDNAAbilityTask>(&Task);
	if (UDNAAbilityTask)
	{
		UDNAAbilityTask->SetDNAAbilitySystemComponent(GetCurrentActorInfo()->DNAAbilitySystemComponent.Get());
		UDNAAbilityTask->Ability = this;
	}
}

void UDNAAbility::OnDNATaskActivated(UDNATask& Task)
{
	ABILITY_VLOG(CastChecked<AActor>(GetOuter()), Log, TEXT("Task Started %s"), *Task.GetName());

	ActiveTasks.Add(&Task);
}

void UDNAAbility::OnDNATaskDeactivated(UDNATask& Task)
{
	ABILITY_VLOG(CastChecked<AActor>(GetOuter()), Log, TEXT("Task Ended %s"), *Task.GetName());

	ActiveTasks.Remove(&Task);

	if (ENABLE_DNAAbilityTask_DEBUGMSG)
	{
		AddDNAAbilityTaskDebugMessage(&Task, TEXT("Ended."));
	}
}

void UDNAAbility::ConfirmTaskByInstanceName(FName InstanceName, bool bEndTask)
{
	TArray<UDNATask*, TInlineAllocator<8> > NamedTasks;

	for (UDNATask* Task : ActiveTasks)
	{
		if (Task && Task->GetInstanceName() == InstanceName)
		{
			NamedTasks.Add(Task);
		}
	}
	
	for (int32 i = NamedTasks.Num() - 1; i >= 0; --i)
	{
		UDNATask* CurrentTask = NamedTasks[i];
		if (CurrentTask && CurrentTask->IsPendingKill() == false)
		{
			CurrentTask->ExternalConfirm(bEndTask);
		}
	}
}

void UDNAAbility::EndOrCancelTasksByInstanceName()
{
	// Static array for avoiding memory allocations
	TArray<UDNATask*, TInlineAllocator<8> > NamedTasks;

	// Call Endtask on everything in EndTaskInstanceNames list
	for (int32 j = 0; j < EndTaskInstanceNames.Num(); ++j)
	{
		FName InstanceName = EndTaskInstanceNames[j];
		NamedTasks.Reset();

		// Find every current task that needs to end before ending any
		for (UDNATask* Task : ActiveTasks)
		{
			if (Task && Task->GetInstanceName() == InstanceName)
			{
				NamedTasks.Add(Task);
			}
		}
		
		// End each one individually. Not ending a task may do "anything" including killing other tasks or the ability itself
		for (int32 i = NamedTasks.Num() - 1; i >= 0; --i)
		{
			UDNATask* CurrentTask = NamedTasks[i];
			if (CurrentTask && CurrentTask->IsPendingKill() == false)
			{
				CurrentTask->EndTask();
			}
		}
	}
	EndTaskInstanceNames.Empty();


	// Call ExternalCancel on everything in CancelTaskInstanceNames list
	for (int32 j = 0; j < CancelTaskInstanceNames.Num(); ++j)
	{
		FName InstanceName = CancelTaskInstanceNames[j];
		NamedTasks.Reset();
		
		// Find every current task that needs to cancel before cancelling any
		for (UDNATask* Task : ActiveTasks)
		{
			if (Task && Task->GetInstanceName() == InstanceName)
			{
				NamedTasks.Add(Task);
			}
		}

		// Cancel each one individually.  Not canceling a task may do "anything" including killing other tasks or the ability itself
		for (int32 i = NamedTasks.Num() - 1; i >= 0; --i)
		{
			UDNATask* CurrentTask = NamedTasks[i];
			if (CurrentTask && CurrentTask->IsPendingKill() == false)
			{
				CurrentTask->ExternalCancel();
			}
		}
	}
	CancelTaskInstanceNames.Empty();
}

void UDNAAbility::EndTaskByInstanceName(FName InstanceName)
{
	//Avoid race condition by delaying for one frame
	EndTaskInstanceNames.AddUnique(InstanceName);
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UDNAAbility::EndOrCancelTasksByInstanceName);
}

void UDNAAbility::CancelTaskByInstanceName(FName InstanceName)
{
	//Avoid race condition by delaying for one frame
	CancelTaskInstanceNames.AddUnique(InstanceName);
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UDNAAbility::EndOrCancelTasksByInstanceName);
}

void UDNAAbility::EndAbilityState(FName OptionalStateNameToEnd)
{
	check(CurrentActorInfo);

	if (OnDNAAbilityStateEnded.IsBound())
	{
		OnDNAAbilityStateEnded.Broadcast(OptionalStateNameToEnd);
	}
}

void UDNAAbility::AddDNAAbilityTaskDebugMessage(UDNATask* UDNAAbilityTask, FString DebugMessage)
{
	TaskDebugMessages.AddDefaulted();
	FDNAAbilityTaskDebugMessage& Msg = TaskDebugMessages.Last();
	Msg.FromTask = UDNAAbilityTask;
	Msg.Message = FString::Printf(TEXT("{%s} %s"), UDNAAbilityTask ? *UDNAAbilityTask->GetDebugString() : TEXT(""), *DebugMessage);
}

/**
 *	Helper methods for adding GaAmeplayCues without having to go through DNAEffects.
 *	For now, none of these will happen predictively. We can eventually build this out more to 
 *	work with the PredictionKey system.
 */

void UDNAAbility::K2_ExecuteDNACue(FDNATag DNACueTag, FDNAEffectContextHandle Context)
{
	check(CurrentActorInfo);
	CurrentActorInfo->DNAAbilitySystemComponent->ExecuteDNACue(DNACueTag, Context);
}

void UDNAAbility::K2_ExecuteDNACueWithParams(FDNATag DNACueTag, const FDNACueParameters& DNACueParameters)
{
	check(CurrentActorInfo);
	const_cast<FDNACueParameters&>(DNACueParameters).AbilityLevel = GetAbilityLevel();
	CurrentActorInfo->DNAAbilitySystemComponent->ExecuteDNACue(DNACueTag, DNACueParameters);
}

void UDNAAbility::K2_AddDNACue(FDNATag DNACueTag, FDNAEffectContextHandle Context, bool bRemoveOnAbilityEnd)
{
	check(CurrentActorInfo);

	// Make default context if nothing is passed in
	if (Context.IsValid() == false)
	{
		Context = MakeEffectContext(CurrentSpecHandle, CurrentActorInfo);
	}

	Context.SetAbility(this);

	CurrentActorInfo->DNAAbilitySystemComponent->AddDNACue(DNACueTag, Context);

	if (bRemoveOnAbilityEnd)
	{
		TrackedDNACues.Add(DNACueTag);
	}
}

void UDNAAbility::K2_RemoveDNACue(FDNATag DNACueTag)
{
	check(CurrentActorInfo);
	CurrentActorInfo->DNAAbilitySystemComponent->RemoveDNACue(DNACueTag);

	TrackedDNACues.Remove(DNACueTag);
}

FDNAEffectContextHandle UDNAAbility::GetContextFromOwner(FDNAAbilityTargetDataHandle OptionalTargetData) const
{
	check(CurrentActorInfo);
	FDNAEffectContextHandle Context = MakeEffectContext(CurrentSpecHandle, CurrentActorInfo);
	
	for (auto Data : OptionalTargetData.Data)
	{
		if (Data.IsValid())
		{
			Data->AddTargetDataToContext(Context, true);
		}
	}

	return Context;
}

int32 UDNAAbility::GetAbilityLevel() const
{
	if (IsInstantiated() == false || CurrentActorInfo == nullptr)
	{
		return 1;
	}
	
	return GetAbilityLevel(CurrentSpecHandle, CurrentActorInfo);
}

/** Returns current ability level for non instanced abilities. You m ust call this version in these contexts! */
int32 UDNAAbility::GetAbilityLevel(FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo) const
{
	FDNAAbilitySpec* Spec = ActorInfo->DNAAbilitySystemComponent->FindAbilitySpecFromHandle(Handle);
	check(Spec);

	return Spec->Level;
}

FDNAAbilitySpec* UDNAAbility::GetCurrentAbilitySpec() const
{
	check(IsInstantiated()); // You should not call this on non instanced abilities.
	check(CurrentActorInfo);
	return CurrentActorInfo->DNAAbilitySystemComponent->FindAbilitySpecFromHandle(CurrentSpecHandle);
}

FDNAEffectContextHandle UDNAAbility::GetGrantedByEffectContext() const
{
	check(IsInstantiated()); // You should not call this on non instanced abilities.
	check(CurrentActorInfo);
	if (CurrentActorInfo)
	{
		FActiveDNAEffectHandle ActiveHandle = CurrentActorInfo->DNAAbilitySystemComponent->FindActiveDNAEffectHandle(GetCurrentAbilitySpecHandle());
		if (ActiveHandle.IsValid())
		{
			return CurrentActorInfo->DNAAbilitySystemComponent->GetEffectContextFromActiveGEHandle(ActiveHandle);
		}
	}

	return FDNAEffectContextHandle();
}

void UDNAAbility::RemoveGrantedByEffect()
{
	check(IsInstantiated()); // You should not call this on non instanced abilities.
	check(CurrentActorInfo);
	if (CurrentActorInfo)
	{
		FActiveDNAEffectHandle ActiveHandle = CurrentActorInfo->DNAAbilitySystemComponent->FindActiveDNAEffectHandle(GetCurrentAbilitySpecHandle());
		if (ActiveHandle.IsValid())
		{
			CurrentActorInfo->DNAAbilitySystemComponent->RemoveActiveDNAEffect(ActiveHandle);
		}
	}
}

UObject* UDNAAbility::GetSourceObject(FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo) const
{
	if (ActorInfo != NULL)
	{
		UDNAAbilitySystemComponent* DNAAbilitySystemComponent = ActorInfo->DNAAbilitySystemComponent.Get();
		if (DNAAbilitySystemComponent != NULL)
		{
			FDNAAbilitySpec* AbilitySpec = DNAAbilitySystemComponent->FindAbilitySpecFromHandle(Handle);
			if (AbilitySpec)
			{
				return AbilitySpec->SourceObject;
			}
		}
	}
	return nullptr;
}

UObject* UDNAAbility::GetCurrentSourceObject() const
{
	FDNAAbilitySpec* AbilitySpec = GetCurrentAbilitySpec();
	if (AbilitySpec)
	{
		return AbilitySpec->SourceObject;
	}
	return nullptr;
}

FDNAEffectContextHandle UDNAAbility::MakeEffectContext(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo *ActorInfo) const
{
	check(ActorInfo);
	FDNAEffectContextHandle Context = FDNAEffectContextHandle(UDNAAbilitySystemGlobals::Get().AllocDNAEffectContext());
	// By default use the owner and avatar as the instigator and causer
	Context.AddInstigator(ActorInfo->OwnerActor.Get(), ActorInfo->AvatarActor.Get());

	// add in the ability tracking here.
	Context.SetAbility(this);

	return Context;
}

bool UDNAAbility::IsTriggered() const
{
	// Assume that if there is triggered data, then we are triggered. 
	// If we need to support abilities that can be both, this will need to be expanded.
	return AbilityTriggers.Num() > 0;
}

bool UDNAAbility::IsPredictingClient() const
{
	if (GetCurrentActorInfo()->OwnerActor.IsValid())
	{
		bool bIsLocallyControlled = GetCurrentActorInfo()->IsLocallyControlled();
		bool bIsAuthority = GetCurrentActorInfo()->IsNetAuthority();

		// LocalPredicted and ServerInitiated are both valid because in both those modes the ability also runs on the client
		if (!bIsAuthority && bIsLocallyControlled && (GetNetExecutionPolicy() == EDNAAbilityNetExecutionPolicy::LocalPredicted || GetNetExecutionPolicy() == EDNAAbilityNetExecutionPolicy::ServerInitiated))
		{
			return true;
		}
	}

	return false;
}

bool UDNAAbility::IsForRemoteClient() const
{
	if (GetCurrentActorInfo()->OwnerActor.IsValid())
	{
		bool bIsLocallyControlled = GetCurrentActorInfo()->IsLocallyControlled();
		bool bIsAuthority = GetCurrentActorInfo()->IsNetAuthority();

		if (bIsAuthority && !bIsLocallyControlled)
		{
			return true;
		}
	}

	return false;
}

bool UDNAAbility::IsLocallyControlled() const
{
	if (GetCurrentActorInfo()->OwnerActor.IsValid())
	{
		return GetCurrentActorInfo()->IsLocallyControlled();
	}

	return false;
}

bool UDNAAbility::HasAuthority(const FDNAAbilityActivationInfo* ActivationInfo) const
{
	return (ActivationInfo->ActivationMode == EDNAAbilityActivationMode::Authority);
}

bool UDNAAbility::HasAuthorityOrPredictionKey(const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo* ActivationInfo) const
{
	return ActorInfo->DNAAbilitySystemComponent->HasAuthorityOrPredictionKey(ActivationInfo);
}

void UDNAAbility::OnGiveAbility(const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilitySpec& Spec)
{
	SetCurrentActorInfo(Spec.Handle, ActorInfo);

	// If we already have an avatar set, call the OnAvatarSet event as well
	if (ActorInfo && ActorInfo->AvatarActor.IsValid())
	{
		OnAvatarSet(ActorInfo, Spec);
	}
}

void UDNAAbility::OnAvatarSet(const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilitySpec& Spec)
{
	// Projects may want to initiate passives or do other "BeginPlay" type of logic here.
}

// -------------------------------------------------------

FActiveDNAEffectHandle UDNAAbility::BP_ApplyDNAEffectToOwner(TSubclassOf<UDNAEffect> DNAEffectClass, int32 DNAEffectLevel, int32 Stacks)
{
	check(CurrentActorInfo);
	check(CurrentSpecHandle.IsValid());

	if ( DNAEffectClass )
	{
		const UDNAEffect* DNAEffect = DNAEffectClass->GetDefaultObject<UDNAEffect>();
		return ApplyDNAEffectToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, DNAEffect, DNAEffectLevel, Stacks);
	}

	ABILITY_LOG(Error, TEXT("BP_ApplyDNAEffectToOwner called on ability %s with no DNAEffectClass."), *GetName());
	return FActiveDNAEffectHandle();
}

FActiveDNAEffectHandle UDNAAbility::ApplyDNAEffectToOwner(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo, const UDNAEffect* DNAEffect, float DNAEffectLevel, int32 Stacks) const
{
	if (DNAEffect && (HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo)))
	{
		FDNAEffectSpecHandle SpecHandle = MakeOutgoingDNAEffectSpec(Handle, ActorInfo, ActivationInfo, DNAEffect->GetClass(), DNAEffectLevel);
		if (SpecHandle.IsValid())
		{
			SpecHandle.Data->StackCount = Stacks;
			return ApplyDNAEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
		}
	}

	// We cannot apply DNAEffects in this context. Return an empty handle.
	return FActiveDNAEffectHandle();
}

FActiveDNAEffectHandle UDNAAbility::K2_ApplyDNAEffectSpecToOwner(const FDNAEffectSpecHandle EffectSpecHandle)
{
	return ApplyDNAEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, EffectSpecHandle);
}

FActiveDNAEffectHandle UDNAAbility::ApplyDNAEffectSpecToOwner(const FDNAAbilitySpecHandle AbilityHandle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo, const FDNAEffectSpecHandle SpecHandle) const
{
	// This batches all created cues together
	FScopedDNACueSendContext DNACueSendContext;

	if (SpecHandle.IsValid() && (HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo)))
	{
		return ActorInfo->DNAAbilitySystemComponent->ApplyDNAEffectSpecToSelf(*SpecHandle.Data.Get(), ActorInfo->DNAAbilitySystemComponent->GetPredictionKeyForNewAction());

	}
	return FActiveDNAEffectHandle();
}

// -------------------------------

TArray<FActiveDNAEffectHandle> UDNAAbility::BP_ApplyDNAEffectToTarget(FDNAAbilityTargetDataHandle Target, TSubclassOf<UDNAEffect> DNAEffectClass, int32 DNAEffectLevel, int32 Stacks)
{
	return ApplyDNAEffectToTarget(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, Target, DNAEffectClass, DNAEffectLevel, Stacks);
}

TArray<FActiveDNAEffectHandle> UDNAAbility::ApplyDNAEffectToTarget(const FDNAAbilitySpecHandle Handle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo, const FDNAAbilityTargetDataHandle& Target, TSubclassOf<UDNAEffect> DNAEffectClass, float DNAEffectLevel, int32 Stacks) const
{
	SCOPE_CYCLE_COUNTER(STAT_ApplyDNAEffectToTarget);

	TArray<FActiveDNAEffectHandle> EffectHandles;

	if (HasAuthority(&ActivationInfo) == false && UDNAAbilitySystemGlobals::Get().ShouldPredictTargetDNAEffects() == false)
	{
		// Early out to avoid making effect specs that we can't apply
		return EffectHandles;
	}

	// This batches all created cues together
	FScopedDNACueSendContext DNACueSendContext;

	if (DNAEffectClass == nullptr)
	{
		ABILITY_LOG(Error, TEXT("ApplyDNAEffectToTarget called on ability %s with no DNAEffect."), *GetName());
	}
	else if (HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo))
	{
		FDNAEffectSpecHandle SpecHandle = MakeOutgoingDNAEffectSpec(Handle, ActorInfo, ActivationInfo, DNAEffectClass, DNAEffectLevel);
		SpecHandle.Data->StackCount = Stacks;
		EffectHandles.Append(ApplyDNAEffectSpecToTarget(Handle, ActorInfo, ActivationInfo, SpecHandle, Target));
	}

	return EffectHandles;
}

TArray<FActiveDNAEffectHandle> UDNAAbility::K2_ApplyDNAEffectSpecToTarget(const FDNAEffectSpecHandle SpecHandle, FDNAAbilityTargetDataHandle TargetData)
{
	return ApplyDNAEffectSpecToTarget(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, SpecHandle, TargetData);
}

TArray<FActiveDNAEffectHandle> UDNAAbility::ApplyDNAEffectSpecToTarget(const FDNAAbilitySpecHandle AbilityHandle, const FDNAAbilityActorInfo* ActorInfo, const FDNAAbilityActivationInfo ActivationInfo, const FDNAEffectSpecHandle SpecHandle, const FDNAAbilityTargetDataHandle& TargetData) const
{
	TArray<FActiveDNAEffectHandle> EffectHandles;
	
	if (SpecHandle.IsValid() && HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo))
	{
		TARGETLIST_SCOPE_LOCK(*ActorInfo->DNAAbilitySystemComponent);
		for (TSharedPtr<FDNAAbilityTargetData> Data : TargetData.Data)
		{
			EffectHandles.Append(Data->ApplyDNAEffectSpec(*SpecHandle.Data.Get(), ActorInfo->DNAAbilitySystemComponent->GetPredictionKeyForNewAction()));
		}
	}
	return EffectHandles;
}

void UDNAAbility::IncrementListLock() const
{
	++ScopeLockCount;
}
void UDNAAbility::DecrementListLock() const
{
	if (--ScopeLockCount == 0)
	{
		// execute delayed functions in the order they came in
		// These may end or cancel this ability
		for (int32 Idx = 0; Idx < WaitingToExecute.Num(); ++Idx)
		{
			WaitingToExecute[Idx].ExecuteIfBound();
		}

		WaitingToExecute.Empty();
	}
}

void UDNAAbility::BP_RemoveDNAEffectFromOwnerWithAssetTags(FDNATagContainer WithTags, int32 StacksToRemove)
{
	if (HasAuthority(&CurrentActivationInfo) == false)
	{
		return;
	}

	FDNAEffectQuery const Query = FDNAEffectQuery::MakeQuery_MatchAnyEffectTags(WithTags);
	CurrentActorInfo->DNAAbilitySystemComponent->RemoveActiveEffects(Query, StacksToRemove);
}

void UDNAAbility::BP_RemoveDNAEffectFromOwnerWithGrantedTags(FDNATagContainer WithGrantedTags, int32 StacksToRemove)
{
	if (HasAuthority(&CurrentActivationInfo) == false)
	{
		return;
	}

	FDNAEffectQuery const Query = FDNAEffectQuery::MakeQuery_MatchAnyOwningTags(WithGrantedTags);
	CurrentActorInfo->DNAAbilitySystemComponent->RemoveActiveEffects(Query, StacksToRemove);
}

float UDNAAbility::GetCooldownTimeRemaining() const
{
	return IsInstantiated() ? GetCooldownTimeRemaining(CurrentActorInfo) : 0.f;
}

void UDNAAbility::SetRemoteInstanceHasEnded()
{
	// This could potentially happen in shutdown corner cases
	if (IsPendingKill() || CurrentActorInfo == nullptr || CurrentActorInfo->DNAAbilitySystemComponent.IsValid() == false)
	{
		return;
	}

	RemoteInstanceEnded = true;
	for (UDNATask* Task : ActiveTasks)
	{
		if (Task && Task->IsPendingKill() == false && Task->IsWaitingOnRemotePlayerdata())
		{
			// We have a task that is waiting for player input, but the remote player has ended the ability, so he will not send it.
			// Kill the ability to avoid getting stuck active.
			
			ABILITY_LOG(Log, TEXT("Ability %s is force cancelling because Task %s is waiting on remote player input and the  remote player has just ended the ability."), *GetName(), *Task->GetDebugString());
			CurrentActorInfo->DNAAbilitySystemComponent->ForceCancelAbilityDueToReplication(this);
			break;
		}
	}
}

void UDNAAbility::NotifyAvatarDestroyed()
{
	// This could potentially happen in shutdown corner cases
	if (IsPendingKill() || CurrentActorInfo == nullptr || CurrentActorInfo->DNAAbilitySystemComponent.IsValid() == false)
	{
		return;
	}

	RemoteInstanceEnded = true;
	for (UDNATask* Task : ActiveTasks)
	{
		if (Task && Task->IsPendingKill() == false && Task->IsWaitingOnAvatar())
		{
			// We have a task waiting on some Avatar state but the avatar is destroyed, so force end the ability to avoid getting stuck on.
			
			ABILITY_LOG(Log, TEXT("Ability %s is force cancelling because Task %s is waiting on avatar data avatar has been destroyed."), *GetName(), *Task->GetDebugString());
			CurrentActorInfo->DNAAbilitySystemComponent->ForceCancelAbilityDueToReplication(this);
			break;
		}
	}
}

void UDNAAbility::NotifyDNAAbilityTaskWaitingOnPlayerData(class UDNAAbilityTask* UDNAAbilityTask)
{
	// This should never happen since it will only be called from actively running ability tasks
	check(CurrentActorInfo && CurrentActorInfo->DNAAbilitySystemComponent.IsValid());

	if (RemoteInstanceEnded)
	{
		ABILITY_LOG(Log, TEXT("Ability %s is force cancelling because Task %s has started after the remote player has ended the ability."), *GetName(), *UDNAAbilityTask->GetDebugString());
		CurrentActorInfo->DNAAbilitySystemComponent->ForceCancelAbilityDueToReplication(this);
	}
}

void UDNAAbility::NotifyDNAAbilityTaskWaitingOnAvatar(class UDNAAbilityTask* UDNAAbilityTask)
{
	if (CurrentActorInfo && CurrentActorInfo->AvatarActor.IsValid() == false)
	{
		ABILITY_LOG(Log, TEXT("Ability %s is force cancelling because Task %s has started while there is no valid AvatarActor"), *GetName(), *UDNAAbilityTask->GetDebugString());
		CurrentActorInfo->DNAAbilitySystemComponent->ForceCancelAbilityDueToReplication(this);
	}
}
