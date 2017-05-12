// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
// ActorComponent.cpp: Actor component implementation.

#include "Core.h"
#include "Stats/Stats.h"
#include "HAL/IConsoleManager.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "EngineDefines.h"
#include "Engine/NetSerialization.h"
#include "Templates/SubclassOf.h"
#include "Components/InputComponent.h"
#include "DNATagContainer.h"
#include "TimerManager.h"
#include "AbilitySystemLog.h"
#include "AttributeSet.h"
#include "DNAPrediction.h"
#include "DNAEffectTypes.h"
#include "DNAAbilitySpec.h"
#include "UObject/UObjectHash.h"
#include "GameFramework/PlayerController.h"
#include "Abilities/DNAAbilityTypes.h"
#include "AbilitySystemStats.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Abilities/DNAAbilityTargetTypes.h"
#include "Abilities/DNAAbility.h"
#include "AbilitySystemComponent.h"
#include "Abilities/DNAAbilityTargetActor.h"
#include "TickableAttributeSetInterface.h"
#include "DNATagResponseTable.h"
#define LOCTEXT_NAMESPACE "DNAAbilitySystemComponent"

/** Enable to log out all render state create, destroy and updatetransform events */
#define LOG_RENDER_STATE 0

void UDNAAbilitySystemComponent::InitializeComponent()
{
	Super::InitializeComponent();

	// Look for DSO AttributeSets (note we are currently requiring all attribute sets to be subobjects of the same owner. This doesn't *have* to be the case forever.
	AActor *Owner = GetOwner();
	InitAbilityActorInfo(Owner, Owner);	// Default init to our outer owner

	TArray<UObject*> ChildObjects;
	GetObjectsWithOuter(Owner, ChildObjects, false, RF_NoFlags, EInternalObjectFlags::PendingKill);
	for (UObject* Obj : ChildObjects)
	{
		UAttributeSet* Set = Cast<UAttributeSet>(Obj);
		if (Set)  
		{
			SpawnedAttributes.AddUnique(Set);
		}
	}
}

void UDNAAbilitySystemComponent::UninitializeComponent()
{
	Super::UninitializeComponent();
	
	ActiveDNAEffects.Uninitialize();
}

void UDNAAbilitySystemComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	DestroyActiveState();

	// The MarkPendingKill on these attribute sets used to be done in UninitializeComponent,
	// but it was moved here instead since it's possible for the component to be uninitialized,
	// and later re-initialized, without being destroyed - and the attribute sets need to be preserved
	// in this case. This can happen when the owning actor's level is removed and later re-added
	// to the world, since EndPlay (and therefore UninitializeComponents) will be called on
	// the owning actor when its level is removed.
	for (UAttributeSet* Set : SpawnedAttributes)
	{
		if (Set)
		{
			Set->MarkPendingKill();
		}
	}

	// Call the super at the end, after we've done what we needed to do
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UDNAAbilitySystemComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{	
	SCOPE_CYCLE_COUNTER(STAT_TickDNAAbilityTasks);

	if (IsOwnerActorAuthoritative())
	{
		AnimMontage_UpdateReplicatedData();
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	for (UAttributeSet* AttributeSet : SpawnedAttributes)
	{
		ITickableAttributeSetInterface* TickableSet = Cast<ITickableAttributeSetInterface>(AttributeSet);
		if (TickableSet)
		{
			TickableSet->Tick(DeltaTime);
		}
	}
}

void UDNAAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	check(AbilityActorInfo.IsValid());
	bool AvatarChanged = (InAvatarActor != AbilityActorInfo->AvatarActor);

	AbilityActorInfo->InitFromActor(InOwnerActor, InAvatarActor, this);

	OwnerActor = InOwnerActor;
	AvatarActor = InAvatarActor;

	if (AvatarChanged)
	{
		ABILITYLIST_SCOPE_LOCK();
		for (FDNAAbilitySpec& Spec : ActivatableAbilities.Items)
		{
			if (Spec.Ability)
			{
				Spec.Ability->OnAvatarSet(AbilityActorInfo.Get(), Spec);
			}
		}
	}

	if (UDNATagReponseTable* TagTable = UDNAAbilitySystemGlobals::Get().GetDNATagResponseTable())
	{
		TagTable->RegisterResponseForEvents(this);
	}

	if (bPendingMontagerep)
	{
		OnRep_ReplicatedAnimMontage();
	}
}

bool UDNAAbilitySystemComponent::GetShouldTick() const 
{
	const bool bHasReplicatedMontageInfoToUpdate = (IsOwnerActorAuthoritative() && RepAnimMontageInfo.IsStopped == false);
	
	if (bHasReplicatedMontageInfoToUpdate)
	{
		return true;
	}

	bool bResult = Super::GetShouldTick();	
	if (bResult == false)
	{
		for (const UAttributeSet* AttributeSet : SpawnedAttributes)
		{
			const ITickableAttributeSetInterface* TickableAttributeSet = Cast<const ITickableAttributeSetInterface>(AttributeSet);
			if (TickableAttributeSet && TickableAttributeSet->ShouldTick())
			{
				bResult = true;
				break;
			}
		}
	}
	
	return bResult;
}

void UDNAAbilitySystemComponent::SetAvatarActor(AActor* InAvatarActor)
{
	check(AbilityActorInfo.IsValid());
	InitAbilityActorInfo(OwnerActor, InAvatarActor);
}

void UDNAAbilitySystemComponent::ClearActorInfo()
{
	check(AbilityActorInfo.IsValid());
	AbilityActorInfo->ClearActorInfo();
	OwnerActor = NULL;
	AvatarActor = NULL;
}

void UDNAAbilitySystemComponent::OnRep_OwningActor()
{
	check(AbilityActorInfo.IsValid());

	if (OwnerActor != AbilityActorInfo->OwnerActor || AvatarActor != AbilityActorInfo->AvatarActor)
	{
		if (OwnerActor != NULL)
		{
			InitAbilityActorInfo(OwnerActor, AvatarActor);
		}
		else
		{
			ClearActorInfo();
		}
	}
}

void UDNAAbilitySystemComponent::RefreshAbilityActorInfo()
{
	check(AbilityActorInfo.IsValid());
	AbilityActorInfo->InitFromActor(AbilityActorInfo->OwnerActor.Get(), AbilityActorInfo->AvatarActor.Get(), this);
}

FDNAAbilitySpecHandle UDNAAbilitySystemComponent::GiveAbility(const FDNAAbilitySpec& Spec)
{	
	check(Spec.Ability);
	check(IsOwnerActorAuthoritative());	// Should be called on authority

	// If locked, add to pending list. The Spec.Handle is not regenerated when we receive, so returning this is ok.
	if (AbilityScopeLockCount > 0)
	{
		AbilityPendingAdds.Add(Spec);
		return Spec.Handle;
	}
	
	FDNAAbilitySpec& OwnedSpec = ActivatableAbilities.Items[ActivatableAbilities.Items.Add(Spec)];
	
	if (OwnedSpec.Ability->GetInstancingPolicy() == EDNAAbilityInstancingPolicy::InstancedPerActor)
	{
		// Create the instance at creation time
		CreateNewInstanceOfAbility(OwnedSpec, Spec.Ability);
	}
	
	OnGiveAbility(OwnedSpec);
	MarkAbilitySpecDirty(OwnedSpec);

	return OwnedSpec.Handle;
}

FDNAAbilitySpecHandle UDNAAbilitySystemComponent::GiveAbilityAndActivateOnce(const FDNAAbilitySpec& Spec)
{
	check(Spec.Ability);

	if (Spec.Ability->GetInstancingPolicy() == EDNAAbilityInstancingPolicy::NonInstanced || Spec.Ability->GetNetExecutionPolicy() == EDNAAbilityNetExecutionPolicy::LocalOnly)
	{
		ABILITY_LOG(Error, TEXT("GiveAbilityAndActivateOnce called on ability %s that is non instanced or won't execute on server, not allowed!"), *Spec.Ability->GetName());

		return FDNAAbilitySpecHandle();
	}

	if (!IsOwnerActorAuthoritative())
	{
		ABILITY_LOG(Error, TEXT("GiveAbilityAndActivateOnce called on ability %s on the client, not allowed!"), *Spec.Ability->GetName());

		return FDNAAbilitySpecHandle();
	}

	FDNAAbilitySpecHandle AddedAbilityHandle = GiveAbility(Spec);

	FDNAAbilitySpec* FoundSpec = FindAbilitySpecFromHandle(AddedAbilityHandle);

	if (FoundSpec)
	{
		FoundSpec->RemoveAfterActivation = true;

		if (!InternalTryActivateAbility(AddedAbilityHandle))
		{
			// We failed to activate it, so remove it now
			ClearAbility(AddedAbilityHandle);

			return FDNAAbilitySpecHandle();
		}
	}

	return AddedAbilityHandle;
}

void UDNAAbilitySystemComponent::SetRemoveAbilityOnEnd(FDNAAbilitySpecHandle AbilitySpecHandle)
{
	FDNAAbilitySpec* FoundSpec = FindAbilitySpecFromHandle(AbilitySpecHandle);
	if (FoundSpec)
	{
		if (FoundSpec->IsActive())
		{
			FoundSpec->RemoveAfterActivation = true;
			FoundSpec->InputID = INDEX_NONE;
		}
		else
		{
			ClearAbility(AbilitySpecHandle);
		}
	}
}

void UDNAAbilitySystemComponent::ClearAllAbilities()
{
	check(IsOwnerActorAuthoritative());	// Should be called on authority
	check(AbilityScopeLockCount == 0);	// We should never be calling this from a scoped lock situation.

	// Note we aren't marking any old abilities pending kill. This shouldn't matter since they will be garbage collected.
	for (FDNAAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		OnRemoveAbility(Spec);
	}

	ActivatableAbilities.Items.Empty(ActivatableAbilities.Items.Num());
	ActivatableAbilities.MarkArrayDirty();

	CheckForClearedAbilities();
}

void UDNAAbilitySystemComponent::ClearAbility(const FDNAAbilitySpecHandle& Handle)
{
	check(IsOwnerActorAuthoritative()); // Should be called on authority

	for (int Idx = 0; Idx < ActivatableAbilities.Items.Num(); ++Idx)
	{
		check(ActivatableAbilities.Items[Idx].Handle.IsValid());
		if (ActivatableAbilities.Items[Idx].Handle == Handle)
		{
			if (AbilityScopeLockCount > 0)
			{
				if (ActivatableAbilities.Items[Idx].PendingRemove == false)
				{
					ActivatableAbilities.Items[Idx].PendingRemove = true;
					AbilityPendingRemoves.Add(Handle);
				}
			}
			else
			{

				OnRemoveAbility(ActivatableAbilities.Items[Idx]);
				ActivatableAbilities.Items.RemoveAtSwap(Idx);
				ActivatableAbilities.MarkArrayDirty();
				CheckForClearedAbilities();
			}
			return;
		}
	}
}

void UDNAAbilitySystemComponent::OnGiveAbility(FDNAAbilitySpec& Spec)
{
	if (!Spec.Ability)
	{
		return;
	}

	const UDNAAbility* SpecAbility = Spec.Ability;
	if (SpecAbility->GetInstancingPolicy() == EDNAAbilityInstancingPolicy::InstancedPerActor && SpecAbility->GetReplicationPolicy() == EDNAAbilityReplicationPolicy::ReplicateNo)
	{
		// If we don't replicate and are missing an instance, add one
		if (Spec.NonReplicatedInstances.Num() == 0)
		{
			CreateNewInstanceOfAbility(Spec, SpecAbility);
		}
	}

	for (const FAbilityTriggerData& TriggerData : Spec.Ability->AbilityTriggers)
	{
		FDNATag EventTag = TriggerData.TriggerTag;

		auto& TriggeredAbilityMap = (TriggerData.TriggerSource == EDNAAbilityTriggerSource::DNAEvent) ? DNAEventTriggeredAbilities : OwnedTagTriggeredAbilities;

		if (TriggeredAbilityMap.Contains(EventTag))
		{
			TriggeredAbilityMap[EventTag].AddUnique(Spec.Handle);	// Fixme: is this right? Do we want to trigger the ability directly of the spec?
		}
		else
		{
			TArray<FDNAAbilitySpecHandle> Triggers;
			Triggers.Add(Spec.Handle);
			TriggeredAbilityMap.Add(EventTag, Triggers);
		}

		if (TriggerData.TriggerSource != EDNAAbilityTriggerSource::DNAEvent)
		{
			FOnDNAEffectTagCountChanged& CountChangedEvent = RegisterDNATagEvent(EventTag);
			// Add a change callback if it isn't on it already

			if (!CountChangedEvent.IsBoundToObject(this))
			{
				MonitoredTagChangedDelegateHandle = CountChangedEvent.AddUObject(this, &UDNAAbilitySystemComponent::MonitoredTagChanged);
			}
		}
	}

	// If there's already a primary instance, it should be the one to receive the OnGiveAbility call
	UDNAAbility* PrimaryInstance = Spec.GetPrimaryInstance();
	if (PrimaryInstance)
	{
		PrimaryInstance->OnGiveAbility(AbilityActorInfo.Get(), Spec);
	}
	else
	{
		Spec.Ability->OnGiveAbility(AbilityActorInfo.Get(), Spec);
	}
}

void UDNAAbilitySystemComponent::OnRemoveAbility(FDNAAbilitySpec& Spec)
{
	if (!Spec.Ability)
	{
		return;
	}

	TArray<UDNAAbility*> Instances = Spec.GetAbilityInstances();

	for (auto Instance : Instances)
	{
		if (Instance)
		{
			if (Instance->IsActive())
			{
				// End the ability but don't replicate it, OnRemoveAbility gets replicated
				bool bReplicateEndAbility = false;
				bool bWasCancelled = false;
				Instance->EndAbility(Instance->CurrentSpecHandle, Instance->CurrentActorInfo, Instance->CurrentActivationInfo, bReplicateEndAbility, bWasCancelled);
			}
			else
			{
				// Ability isn't active, but still needs to be destroyed
				if (GetOwnerRole() == ROLE_Authority || Instance->GetReplicationPolicy() == EDNAAbilityReplicationPolicy::ReplicateNo)
				{
					// Only destroy if we're the server or this isn't replicated. Can't destroy on the client or replication will fail when it replicates the end state
					AllReplicatedInstancedAbilities.Remove(Instance);
					Instance->MarkPendingKill();
				}
			}
		}
	}
	Spec.ReplicatedInstances.Empty();
	Spec.NonReplicatedInstances.Empty();
}

void UDNAAbilitySystemComponent::CheckForClearedAbilities()
{
	for (auto& Triggered : DNAEventTriggeredAbilities)
	{
		// Make sure all triggered abilities still exist, if not remove
		for (int32 i = 0; i < Triggered.Value.Num(); i++)
		{
			FDNAAbilitySpec* Spec = FindAbilitySpecFromHandle(Triggered.Value[i]);

			if (!Spec)
			{
				Triggered.Value.RemoveAt(i);
				i--;
			}
		}
		
		// We leave around the empty trigger stub, it's likely to be added again
	}

	for (auto& Triggered : OwnedTagTriggeredAbilities)
	{
		bool bRemovedTrigger = false;
		// Make sure all triggered abilities still exist, if not remove
		for (int32 i = 0; i < Triggered.Value.Num(); i++)
		{
			FDNAAbilitySpec* Spec = FindAbilitySpecFromHandle(Triggered.Value[i]);

			if (!Spec)
			{
				Triggered.Value.RemoveAt(i);
				i--;
				bRemovedTrigger = true;
			}
		}
		
		if (bRemovedTrigger && Triggered.Value.Num() == 0)
		{
			// If we removed all triggers, remove the callback
			FOnDNAEffectTagCountChanged& CountChangedEvent = RegisterDNATagEvent(Triggered.Key);
		
			if (CountChangedEvent.IsBoundToObject(this))
			{
				CountChangedEvent.Remove(MonitoredTagChangedDelegateHandle);
			}
		}

		// We leave around the empty trigger stub, it's likely to be added again
	}

	for (int32 i = 0; i < AllReplicatedInstancedAbilities.Num(); i++)
	{
		UDNAAbility* Ability = AllReplicatedInstancedAbilities[i];

		if (!Ability || Ability->IsPendingKill())
		{
			AllReplicatedInstancedAbilities.RemoveAt(i);
			i--;
		}
	}
}

void UDNAAbilitySystemComponent::IncrementAbilityListLock()
{
	AbilityScopeLockCount++;
}
void UDNAAbilitySystemComponent::DecrementAbilityListLock()
{
	if (--AbilityScopeLockCount == 0)
	{
		TArray<FDNAAbilitySpec, TInlineAllocator<2> > LocalPendingAdds(MoveTemp(AbilityPendingAdds));
		for (FDNAAbilitySpec& Spec : LocalPendingAdds)
		{
			GiveAbility(Spec);
		}

		TArray<FDNAAbilitySpecHandle, TInlineAllocator<2> > LocalPendingRemoves(MoveTemp(AbilityPendingRemoves));
		for (FDNAAbilitySpecHandle& Handle : LocalPendingRemoves)
		{
			ClearAbility(Handle);
		}
	}
}

FDNAAbilitySpec* UDNAAbilitySystemComponent::FindAbilitySpecFromHandle(FDNAAbilitySpecHandle Handle)
{
	SCOPE_CYCLE_COUNTER(STAT_FindAbilitySpecFromHandle);

	for (FDNAAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (Spec.Handle == Handle)
		{
			return &Spec;
		}
	}

	return nullptr;
}

FDNAAbilitySpec* UDNAAbilitySystemComponent::FindAbilitySpecFromGEHandle(FActiveDNAEffectHandle Handle)
{
	for (FDNAAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (Spec.DNAEffectHandle == Handle)
		{
			return &Spec;
		}
	}
	return nullptr;
}

FDNAAbilitySpec* UDNAAbilitySystemComponent::FindAbilitySpecFromClass(TSubclassOf<UDNAAbility> InAbilityClass)
{
	SCOPE_CYCLE_COUNTER(STAT_FindAbilitySpecFromHandle);

	for (FDNAAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (Spec.Ability->GetClass() == InAbilityClass)
		{
			return &Spec;
		}
	}

	return nullptr;
}

void UDNAAbilitySystemComponent::MarkAbilitySpecDirty(FDNAAbilitySpec& Spec)
{
	if (IsOwnerActorAuthoritative())
	{
		ActivatableAbilities.MarkItemDirty(Spec);
		AbilitySpecDirtiedCallbacks.Broadcast(Spec);
	}
	else
	{
		// Clients predicting should call MarkArrayDirty to force the internal replication map to be rebuilt.
		ActivatableAbilities.MarkArrayDirty();
	}
}

FDNAAbilitySpec* UDNAAbilitySystemComponent::FindAbilitySpecFromInputID(int32 InputID)
{
	if (InputID != INDEX_NONE)
	{
		for (FDNAAbilitySpec& Spec : ActivatableAbilities.Items)
		{
			if (Spec.InputID == InputID)
			{
				return &Spec;
			}
		}
	}
	return nullptr;
}

FDNAEffectContextHandle UDNAAbilitySystemComponent::GetEffectContextFromActiveGEHandle(FActiveDNAEffectHandle Handle)
{
	FActiveDNAEffect* ActiveGE = ActiveDNAEffects.GetActiveDNAEffect(Handle);
	if (ActiveGE)
	{
		return ActiveGE->Spec.GetEffectContext();
	}

	return FDNAEffectContextHandle();
}

UDNAAbility* UDNAAbilitySystemComponent::CreateNewInstanceOfAbility(FDNAAbilitySpec& Spec, const UDNAAbility* Ability)
{
	check(Ability);
	check(Ability->HasAllFlags(RF_ClassDefaultObject));

	AActor* Owner = GetOwner();
	check(Owner);

	UDNAAbility * AbilityInstance = NewObject<UDNAAbility>(Owner, Ability->GetClass());
	check(AbilityInstance);

	// Add it to one of our instance lists so that it doesn't GC.
	if (AbilityInstance->GetReplicationPolicy() != EDNAAbilityReplicationPolicy::ReplicateNo)
	{
		Spec.ReplicatedInstances.Add(AbilityInstance);
		AllReplicatedInstancedAbilities.Add(AbilityInstance);
	}
	else
	{
		Spec.NonReplicatedInstances.Add(AbilityInstance);
	}
	
	return AbilityInstance;
}

void UDNAAbilitySystemComponent::NotifyAbilityEnded(FDNAAbilitySpecHandle Handle, UDNAAbility* Ability, bool bWasCancelled)
{
	check(Ability);
	FDNAAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
	if (Spec == nullptr)
	{
		// The ability spec may have been removed while we were ending. We can assume everything was cleaned up if the spec isnt here.
		return;
	}
	check(Spec);
	check(Ability);

	check(Ability);
	ENetRole OwnerRole = GetOwnerRole();

	// Broadcast that the ability ended
	AbilityEndedCallbacks.Broadcast(Ability);

	// If AnimatingAbility ended, clear the pointer
	if (LocalAnimMontageInfo.AnimatingAbility == Ability)
	{
		ClearAnimatingAbility(Ability);
	}

	// check to make sure we do not cause a roll over to uint8 by decrementing when it is 0
	if (ensure(Spec->ActiveCount > 0))
	{
		Spec->ActiveCount--;
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("NotifyAbilityEnded called when the Spec->ActiveCount <= 0"));
	}
	
	/** If this is instanced per execution, mark pending kill and remove it from our instanced lists if we are the authority */
	if (Ability->GetInstancingPolicy() == EDNAAbilityInstancingPolicy::InstancedPerExecution)
	{
		check(Ability->HasAnyFlags(RF_ClassDefaultObject) == false);	// Should never be calling this on a CDO for an instanced ability!

		if (Ability->GetReplicationPolicy() != EDNAAbilityReplicationPolicy::ReplicateNo)
		{
			if (OwnerRole == ROLE_Authority)
			{
				Spec->ReplicatedInstances.Remove(Ability);
				AllReplicatedInstancedAbilities.Remove(Ability);
				Ability->MarkPendingKill();
			}
		}
		else
		{
			Spec->NonReplicatedInstances.Remove(Ability);
			Ability->MarkPendingKill();
		}
	}

	if (OwnerRole == ROLE_Authority)
	{
		if (Spec->RemoveAfterActivation && !Spec->IsActive())
		{
			// If we should remove after activation and there are no more active instances, kill it now
			ClearAbility(Handle);
		}
		else
		{
			MarkAbilitySpecDirty(*Spec);
		}
	}
}

void UDNAAbilitySystemComponent::CancelAbility(UDNAAbility* Ability)
{
	ABILITYLIST_SCOPE_LOCK();
	for (FDNAAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (Spec.Ability == Ability)
		{
			CancelAbilitySpec(Spec, nullptr);
		}
	}
}

void UDNAAbilitySystemComponent::CancelAbilityHandle(const FDNAAbilitySpecHandle& AbilityHandle)
{
	ABILITYLIST_SCOPE_LOCK();
	for (FDNAAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (Spec.Handle == AbilityHandle)
		{
			CancelAbilitySpec(Spec, nullptr);
			return;
		}
	}
}

void UDNAAbilitySystemComponent::CancelAbilities(const FDNATagContainer* WithTags, const FDNATagContainer* WithoutTags, UDNAAbility* Ignore)
{
	ABILITYLIST_SCOPE_LOCK();
	for (FDNAAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (!Spec.IsActive() || Spec.Ability == nullptr)
		{
			continue;
		}

		bool WithTagPass = (!WithTags || Spec.Ability->AbilityTags.HasAny(*WithTags));
		bool WithoutTagPass = (!WithoutTags || !Spec.Ability->AbilityTags.HasAny(*WithoutTags));

		if (WithTagPass && WithoutTagPass)
		{
			CancelAbilitySpec(Spec, Ignore);
		}
	}
}

void UDNAAbilitySystemComponent::CancelAbilitySpec(FDNAAbilitySpec& Spec, UDNAAbility* Ignore)
{
	FDNAAbilityActorInfo* ActorInfo = AbilityActorInfo.Get();

	if (Spec.Ability->GetInstancingPolicy() != EDNAAbilityInstancingPolicy::NonInstanced)
	{
		// We need to cancel spawned instance, not the CDO
		TArray<UDNAAbility*> AbilitiesToCancel = Spec.GetAbilityInstances();
		for (UDNAAbility* InstanceAbility : AbilitiesToCancel)
		{
			if (InstanceAbility && Ignore != InstanceAbility)
			{
				InstanceAbility->CancelAbility(Spec.Handle, ActorInfo, InstanceAbility->GetCurrentActivationInfo(), true);
			}
		}
	}
	else
	{
		// Try to cancel the non instanced, this may not necessarily work
		Spec.Ability->CancelAbility(Spec.Handle, ActorInfo, FDNAAbilityActivationInfo(), true);
	}
	MarkAbilitySpecDirty(Spec);
}

void UDNAAbilitySystemComponent::CancelAllAbilities(UDNAAbility* Ignore)
{
	ABILITYLIST_SCOPE_LOCK();
	for (FDNAAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (Spec.Ability && Spec.Ability->IsActive())
		{
			CancelAbilitySpec(Spec, Ignore);
		}
	}
}

void UDNAAbilitySystemComponent::DestroyActiveState()
{
	// If we haven't already begun being destroyed
	if ((GetFlags() & RF_BeginDestroyed) == 0)
	{
		// Cancel all abilities before we are destroyed.
		FDNAAbilityActorInfo* ActorInfo = AbilityActorInfo.Get();
		
		// condition needed since in edge cases canceling abilities
		// while not having valid owner/ability component can crash
		if (ActorInfo && ActorInfo->OwnerActor.IsValid(true) && ActorInfo->DNAAbilitySystemComponent.IsValid(true))
		{
			CancelAbilities();
		}

		// Mark pending kill any remaining instanced abilities
		// (CancelAbilities() will only MarkPending kill InstancePerExecution abilities).
		for (FDNAAbilitySpec& Spec : ActivatableAbilities.Items)
		{
			TArray<UDNAAbility*> AbilitiesToCancel = Spec.GetAbilityInstances();
			for (UDNAAbility* InstanceAbility : AbilitiesToCancel)
			{
				if (InstanceAbility)
				{
					InstanceAbility->MarkPendingKill();
				}
			}

			Spec.ReplicatedInstances.Empty();
			Spec.NonReplicatedInstances.Empty();
		}
	}
}

void UDNAAbilitySystemComponent::ApplyAbilityBlockAndCancelTags(const FDNATagContainer& AbilityTags, UDNAAbility* RequestingAbility, bool bEnableBlockTags, const FDNATagContainer& BlockTags, bool bExecuteCancelTags, const FDNATagContainer& CancelTags)
{
	if (bEnableBlockTags)
	{
		BlockAbilitiesWithTags(BlockTags);
	}
	else
	{
		UnBlockAbilitiesWithTags(BlockTags);
	}

	if (bExecuteCancelTags)
	{
		CancelAbilities(&CancelTags, nullptr, RequestingAbility);
	}
}

bool UDNAAbilitySystemComponent::AreAbilityTagsBlocked(const FDNATagContainer& Tags) const
{
	// Expand the passed in tags to get parents, not the blocked tags
	return Tags.HasAny(BlockedAbilityTags.GetExplicitDNATags());
}

void UDNAAbilitySystemComponent::BlockAbilitiesWithTags(const FDNATagContainer& Tags)
{
	BlockedAbilityTags.UpdateTagCount(Tags, 1);
}

void UDNAAbilitySystemComponent::UnBlockAbilitiesWithTags(const FDNATagContainer& Tags)
{
	BlockedAbilityTags.UpdateTagCount(Tags, -1);
}

void UDNAAbilitySystemComponent::BlockAbilityByInputID(int32 InputID)
{
	if (InputID >= 0 && InputID < BlockedAbilityBindings.Num())
	{
		++BlockedAbilityBindings[InputID];
	}
}

void UDNAAbilitySystemComponent::UnBlockAbilityByInputID(int32 InputID)
{
	if (InputID >= 0 && InputID < BlockedAbilityBindings.Num() && BlockedAbilityBindings[InputID] > 0)
	{
		--BlockedAbilityBindings[InputID];
	}
}

#if !UE_BUILD_SHIPPING
int32 DenyClientActivation = 0;
static FAutoConsoleVariableRef CVarDenyClientActivation(
TEXT("DNAAbilitySystem.DenyClientActivations"),
	DenyClientActivation,
	TEXT("Make server deny the next X ability activations from clients. For testing misprediction."),
	ECVF_Default
	);
#endif // !UE_BUILD_SHIPPING

void UDNAAbilitySystemComponent::OnRep_ActivateAbilities()
{
	for (FDNAAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		const UDNAAbility* SpecAbility = Spec.Ability;
		if (!SpecAbility)
		{
			// Queue up another call to make sure this gets run again, as our abilities haven't replicated yet
			GetWorld()->GetTimerManager().SetTimer(OnRep_ActivateAbilitiesTimerHandle, this, &UDNAAbilitySystemComponent::OnRep_ActivateAbilities, 0.5);
			return;
		}
	}

	CheckForClearedAbilities();

	// Try to run any pending activations that couldn't run before. If they don't work now, kill them

	for (auto PendingAbilityInfo : PendingServerActivatedAbilities)
	{
		if (PendingAbilityInfo.bPartiallyActivated)
		{
			ClientActivateAbilitySucceedWithEventData_Implementation(PendingAbilityInfo.Handle, PendingAbilityInfo.PredictionKey, PendingAbilityInfo.TriggerEventData);
		}
		else
		{
			ClientTryActivateAbility(PendingAbilityInfo.Handle);
		}
	}
	PendingServerActivatedAbilities.Empty();

}

void UDNAAbilitySystemComponent::GetActivatableDNAAbilitySpecsByAllMatchingTags(const FDNATagContainer& DNATagContainer, TArray < struct FDNAAbilitySpec* >& MatchingDNAAbilities, bool bOnlyAbilitiesThatSatisfyTagRequirements) const
{
	if (!DNATagContainer.IsValid())
	{
		return;
	}

	for (const FDNAAbilitySpec& Spec : ActivatableAbilities.Items)
	{		
		if (Spec.Ability && Spec.Ability->AbilityTags.HasAll(DNATagContainer))
		{
			// Consider abilities that are blocked by tags currently if we're supposed to (default behavior).  
			// That way, we can use the blocking to find an appropriate ability based on tags when we have more than 
			// one ability that match the DNATagContainer.
			if (!bOnlyAbilitiesThatSatisfyTagRequirements || Spec.Ability->DoesAbilitySatisfyTagRequirements(*this))
			{
				MatchingDNAAbilities.Add(const_cast<FDNAAbilitySpec*>(&Spec));
			}
		}
	}
}

bool UDNAAbilitySystemComponent::TryActivateAbilitiesByTag(const FDNATagContainer& DNATagContainer, bool bAllowRemoteActivation)
{
	TArray<FDNAAbilitySpec*> AbilitiesToActivate;
	GetActivatableDNAAbilitySpecsByAllMatchingTags(DNATagContainer, AbilitiesToActivate);

	bool bSuccess = false;

	for (auto DNAAbilitySpec : AbilitiesToActivate)
	{
		bSuccess |= TryActivateAbility(DNAAbilitySpec->Handle, bAllowRemoteActivation);
	}

	return bSuccess;
}

bool UDNAAbilitySystemComponent::TryActivateAbilityByClass(TSubclassOf<UDNAAbility> InAbilityToActivate, bool bAllowRemoteActivation)
{
	bool bSuccess = false;

	const UDNAAbility* const InAbilityCDO = InAbilityToActivate.GetDefaultObject();

	for (const FDNAAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (Spec.Ability == InAbilityCDO)
		{
			bSuccess |= TryActivateAbility(Spec.Handle, bAllowRemoteActivation);
			break;
		}
	}

	return bSuccess;
}

bool UDNAAbilitySystemComponent::TryActivateAbility(FDNAAbilitySpecHandle AbilityToActivate, bool bAllowRemoteActivation)
{
	FDNATagContainer FailureTags;
	FDNAAbilitySpec* Spec = FindAbilitySpecFromHandle(AbilityToActivate);
	if (!Spec)
	{
		ABILITY_LOG(Warning, TEXT("TryActivateAbility called with invalid Handle"));
		return false;
	}

	UDNAAbility* Ability = Spec->Ability;

	if (!Ability)
	{
		ABILITY_LOG(Warning, TEXT("TryActivateAbility called with invalid Ability"));
		return false;
	}

	const FDNAAbilityActorInfo* ActorInfo = AbilityActorInfo.Get();

	// make sure the ActorInfo and then Actor on that FDNAAbilityActorInfo are valid, if not bail out.
	if (ActorInfo == NULL || !ActorInfo->OwnerActor.IsValid() || !ActorInfo->AvatarActor.IsValid())
	{
		return false;
	}

		
	ENetRole NetMode = ActorInfo->AvatarActor->Role;

	// This should only come from button presses/local instigation (AI, etc).
	if (NetMode == ROLE_SimulatedProxy)
	{
		return false;
	}

	bool bIsLocal = AbilityActorInfo->IsLocallyControlled();

	// Check to see if this a local only or server only ability, if so either remotely execute or fail
	if (!bIsLocal && (Ability->GetNetExecutionPolicy() == EDNAAbilityNetExecutionPolicy::LocalOnly || Ability->GetNetExecutionPolicy() == EDNAAbilityNetExecutionPolicy::LocalPredicted))
	{
		if (bAllowRemoteActivation)
		{
			ClientTryActivateAbility(AbilityToActivate);
			return true;
		}

		ABILITY_LOG(Log, TEXT("Can't activate LocalOnly or LocalPredicted ability %s when not local."), *Ability->GetName());
		return false;
	}

	if (NetMode != ROLE_Authority && (Ability->GetNetExecutionPolicy() == EDNAAbilityNetExecutionPolicy::ServerOnly || Ability->GetNetExecutionPolicy() == EDNAAbilityNetExecutionPolicy::ServerInitiated))
	{
		if (bAllowRemoteActivation)
		{
			if (Ability->CanActivateAbility(AbilityToActivate, ActorInfo, nullptr, nullptr, &FailureTags))
			{
				// No prediction key, server will assign a server-generated key
				ServerTryActivateAbility(AbilityToActivate, Spec->InputPressed, FPredictionKey());
				return true;
			}
			else
			{
				NotifyAbilityFailed(AbilityToActivate, Ability, FailureTags);
				return false;
			}
		}

		ABILITY_LOG(Log, TEXT("Can't activate ServerOnly or ServerInitiated ability %s when not the server."), *Ability->GetName());
		return false;
	}

	return InternalTryActivateAbility(AbilityToActivate);
}

bool UDNAAbilitySystemComponent::IsAbilityInputBlocked(int32 InputID) const
{
	// Check if this ability's input binding is currently blocked
	if (InputID >= 0 && InputID < BlockedAbilityBindings.Num() && BlockedAbilityBindings[InputID] > 0)
	{
		return true;
	}

	return false;
}

/**
 * Attempts to activate the ability.
 *	-This function calls CanActivateAbility
 *	-This function handles instancing
 *	-This function handles networking and prediction
 *	-If all goes well, CallActivateAbility is called next.
 */
bool UDNAAbilitySystemComponent::InternalTryActivateAbility(FDNAAbilitySpecHandle Handle, FPredictionKey InPredictionKey, UDNAAbility** OutInstancedAbility, FOnDNAAbilityEnded::FDelegate* OnDNAAbilityEndedDelegate, const FDNAEventData* TriggerEventData)
{
	const FDNATag& NetworkFailTag = UDNAAbilitySystemGlobals::Get().ActivateFailNetworkingTag;

	static FDNATagContainer FailureTags;
	FailureTags.Reset();

	FDNAAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
	if (!Spec)
	{
		ABILITY_LOG(Warning, TEXT("InternalTryActivateAbility called with invalid Handle"));
		return false;
	}

	const FDNAAbilityActorInfo* ActorInfo = AbilityActorInfo.Get();

	// make sure the ActorInfo and then Actor on that FDNAAbilityActorInfo are valid, if not bail out.
	if (ActorInfo == NULL || !ActorInfo->OwnerActor.IsValid() || !ActorInfo->AvatarActor.IsValid())
	{
		return false;
	}

	// This should only come from button presses/local instigation (AI, etc)
	ENetRole NetMode = ROLE_SimulatedProxy;

	// Use PC netmode if its there
	if (APlayerController* PC = ActorInfo->PlayerController.Get())
	{
		NetMode = PC->Role;
	}
	// Fallback to avataractor otherwise. Edge case: avatar "dies" and becomes torn off and ROLE_Authority. We don't want to use this case (use PC role instead).
	else if (AvatarActor)
	{
		NetMode = AvatarActor->Role;
	}

	if (NetMode == ROLE_SimulatedProxy)
	{
		return false;
	}

	bool bIsLocal = AbilityActorInfo->IsLocallyControlled();

	UDNAAbility* Ability = Spec->Ability;

	if (!Ability)
	{
		ABILITY_LOG(Warning, TEXT("InternalTryActivateAbility called with invalid Ability"));
		return false;
	}

	// Check to see if this a local only or server only ability, if so don't execute
	if (!bIsLocal)
	{
		if (Ability->GetNetExecutionPolicy() == EDNAAbilityNetExecutionPolicy::LocalOnly || (Ability->GetNetExecutionPolicy() == EDNAAbilityNetExecutionPolicy::LocalPredicted && !InPredictionKey.IsValidKey()))
		{
			// If we have a valid prediction key, the ability was started on the local client so it's okay

			ABILITY_LOG(Warning, TEXT("Can't activate LocalOnly or LocalPredicted ability %s when not local! Net Execution Policy is %d."), *Ability->GetName(), (int32)Ability->GetNetExecutionPolicy());

			if (NetworkFailTag.IsValid())
			{
				FailureTags.AddTag(NetworkFailTag);
				NotifyAbilityFailed(Handle, Ability, FailureTags);
			}

			return false;
		}		
	}

	if (NetMode != ROLE_Authority && (Ability->GetNetExecutionPolicy() == EDNAAbilityNetExecutionPolicy::ServerOnly || Ability->GetNetExecutionPolicy() == EDNAAbilityNetExecutionPolicy::ServerInitiated))
	{
		ABILITY_LOG(Warning, TEXT("Can't activate ServerOnly or ServerInitiated ability %s when not the server! Net Execution Policy is %d."), *Ability->GetName(), (int32)Ability->GetNetExecutionPolicy());

		if (NetworkFailTag.IsValid())
		{
			FailureTags.AddTag(NetworkFailTag);
			NotifyAbilityFailed(Handle, Ability, FailureTags);
		}

		return false;
	}

	// If it's instance once the instanced ability will be set, otherwise it will be null
	UDNAAbility* InstancedAbility = Spec->GetPrimaryInstance();

	const FDNATagContainer* SourceTags = nullptr;
	const FDNATagContainer* TargetTags = nullptr;
	if (TriggerEventData != nullptr)
	{
		SourceTags = &TriggerEventData->InstigatorTags;
		TargetTags = &TriggerEventData->TargetTags;
	}

	{
		// If we have an instanced ability, call CanActivateAbility on it.
		// Otherwise we always do a non instanced CanActivateAbility check using the CDO of the Ability.
		UDNAAbility* const CanActivateAbilitySource = InstancedAbility ? InstancedAbility : Ability;

		if (!CanActivateAbilitySource->CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, &FailureTags))
		{
			NotifyAbilityFailed(Handle, CanActivateAbilitySource, FailureTags);
			return false;
		}
	}

	// If we're instance per actor and we're already active, don't let us activate again as this breaks the graph
	if (Ability->GetInstancingPolicy() == EDNAAbilityInstancingPolicy::InstancedPerActor)
	{
		if (Spec->IsActive())
		{
			if (Ability->bRetriggerInstancedAbility && InstancedAbility)
			{
				bool bReplicateEndAbility = true;
				bool bWasCancelled = false;
				InstancedAbility->EndAbility(Handle, ActorInfo, Spec->ActivationInfo, bReplicateEndAbility, bWasCancelled);
			}
			else
			{
				ABILITY_LOG(Verbose, TEXT("Can't activate instanced per actor ability %s when their is already a currently active instance for this actor."), *Ability->GetName());
				return false;
			}
		}
	}

	// Make sure we have a primary
	if (Ability->GetInstancingPolicy() == EDNAAbilityInstancingPolicy::InstancedPerActor && !InstancedAbility)
	{
		ABILITY_LOG(Warning, TEXT("InternalTryActivateAbility called but instanced ability is missing! NetMode: %d. Ability: %s"), (int32)NetMode, *Ability->GetName());
		return false;
	}

	// make sure we do not incur a roll over if we go over the uint8 max, this will need to be updated if the var size changes
	if (ensure(Spec->ActiveCount < UINT8_MAX))
	{
		Spec->ActiveCount++;
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("TryActivateAbility %s called when the Spec->ActiveCount (%d) >= UINT8_MAX"), *Ability->GetName(), (int32)Spec->ActiveCount);
	}

	// Setup a fresh ActivationInfo for this AbilitySpec.
	Spec->ActivationInfo = FDNAAbilityActivationInfo(ActorInfo->OwnerActor.Get());
	FDNAAbilityActivationInfo &ActivationInfo = Spec->ActivationInfo;

	// If we are the server or this is local only
	if (Ability->GetNetExecutionPolicy() == EDNAAbilityNetExecutionPolicy::LocalOnly || (NetMode == ROLE_Authority))
	{
		// if we're the server and don't have a valid key or this ability should be started on the server create a new activation key
		bool bCreateNewServerKey = NetMode == ROLE_Authority &&
			(!InPredictionKey.IsValidKey() ||
			 (Ability->GetNetExecutionPolicy() == EDNAAbilityNetExecutionPolicy::ServerInitiated ||
			  Ability->GetNetExecutionPolicy() == EDNAAbilityNetExecutionPolicy::ServerOnly));
		if (bCreateNewServerKey)
		{
			ActivationInfo.ServerSetActivationPredictionKey(FPredictionKey::CreateNewServerInitiatedKey(this));
		}
		else if (InPredictionKey.IsValidKey())
		{
			// Otherwise if available, set the prediction key to what was passed up
			ActivationInfo.ServerSetActivationPredictionKey(InPredictionKey);
		}

		// we may have changed the prediction key so we need to update the scoped key to match
		FScopedPredictionWindow ScopedPredictionWindow(this, ActivationInfo.GetActivationPredictionKey());

		// ----------------------------------------------
		// Tell the client that you activated it (if we're not local and not server only)
		// ----------------------------------------------
		if (!bIsLocal && Ability->GetNetExecutionPolicy() != EDNAAbilityNetExecutionPolicy::ServerOnly)
		{
			if (TriggerEventData)
			{
				ClientActivateAbilitySucceedWithEventData(Handle, ActivationInfo.GetActivationPredictionKey(), *TriggerEventData);
			}
			else
			{
				ClientActivateAbilitySucceed(Handle, ActivationInfo.GetActivationPredictionKey());
			}
			
			// This will get copied into the instanced abilities
			ActivationInfo.bCanBeEndedByOtherInstance = Ability->bServerRespectsRemoteAbilityCancellation;
		}

		// ----------------------------------------------
		//	Call ActivateAbility (note this could end the ability too!)
		// ----------------------------------------------

		// Create instance of this ability if necessary
		if (Ability->GetInstancingPolicy() == EDNAAbilityInstancingPolicy::InstancedPerExecution)
		{
			InstancedAbility = CreateNewInstanceOfAbility(*Spec, Ability);
			InstancedAbility->CallActivateAbility(Handle, ActorInfo, ActivationInfo, OnDNAAbilityEndedDelegate, TriggerEventData);
		}
		else if (InstancedAbility)
		{
			InstancedAbility->CallActivateAbility(Handle, ActorInfo, ActivationInfo, OnDNAAbilityEndedDelegate, TriggerEventData);
		}
		else
		{
			Ability->CallActivateAbility(Handle, ActorInfo, ActivationInfo, OnDNAAbilityEndedDelegate, TriggerEventData);
		}
	}
	else if (Ability->GetNetExecutionPolicy() == EDNAAbilityNetExecutionPolicy::LocalPredicted)
	{
		// This execution is now officially EDNAAbilityActivationMode:Predicting and has a PredictionKey
		FScopedPredictionWindow ScopedPredictionWindow(this, true);

		ActivationInfo.SetPredicting(ScopedPredictionKey);
		
		// This must be called immediately after GeneratePredictionKey to prevent problems with recursively activating abilities
		if (TriggerEventData)
		{
			ServerTryActivateAbilityWithEventData(Handle, Spec->InputPressed, ScopedPredictionKey, *TriggerEventData);
		}
		else
		{
			ServerTryActivateAbility(Handle, Spec->InputPressed, ScopedPredictionKey);
		}

		// When this prediction key is caught up, we better know if the ability was confirmed or rejected
		ScopedPredictionKey.NewCaughtUpDelegate().BindUObject(this, &UDNAAbilitySystemComponent::OnClientActivateAbilityCaughtUp, Handle, ScopedPredictionKey.Current);

		if (Ability->GetInstancingPolicy() == EDNAAbilityInstancingPolicy::InstancedPerExecution)
		{
			// For now, only NonReplicated + InstancedPerExecution abilities can be Predictive.
			// We lack the code to predict spawning an instance of the execution and then merge/combine
			// with the server spawned version when it arrives.

			if (Ability->GetReplicationPolicy() == EDNAAbilityReplicationPolicy::ReplicateNo)
			{
				InstancedAbility = CreateNewInstanceOfAbility(*Spec, Ability);
				InstancedAbility->CallActivateAbility(Handle, ActorInfo, ActivationInfo, OnDNAAbilityEndedDelegate, TriggerEventData);
			}
			else
			{
				ABILITY_LOG(Error, TEXT("InternalTryActivateAbility called on ability %s that is InstancePerExecution and Replicated. This is an invalid configuration."), *Ability->GetName() );
			}
		}
		else if (InstancedAbility)
		{
			InstancedAbility->CallActivateAbility(Handle, ActorInfo, ActivationInfo, OnDNAAbilityEndedDelegate, TriggerEventData);
		}
		else 
		{
			Ability->CallActivateAbility(Handle, ActorInfo, ActivationInfo, OnDNAAbilityEndedDelegate, TriggerEventData);
		}
	}
	
	if (InstancedAbility)
	{
		if (OutInstancedAbility)
		{
			*OutInstancedAbility = InstancedAbility;
		}

		InstancedAbility->SetCurrentActivationInfo(ActivationInfo);	// Need to push this to the ability if it was instanced.
	}

	MarkAbilitySpecDirty(*Spec);

	AbilityLastActivatedTime = GetWorld()->GetTimeSeconds();

	return true;
}

void UDNAAbilitySystemComponent::ServerTryActivateAbility_Implementation(FDNAAbilitySpecHandle Handle, bool InputPressed, FPredictionKey PredictionKey)
{
	InternalServerTryActiveAbility(Handle, InputPressed, PredictionKey, nullptr);
}

bool UDNAAbilitySystemComponent::ServerTryActivateAbility_Validate(FDNAAbilitySpecHandle Handle, bool InputPressed, FPredictionKey PredictionKey)
{
	return true;
}

void UDNAAbilitySystemComponent::ServerTryActivateAbilityWithEventData_Implementation(FDNAAbilitySpecHandle Handle, bool InputPressed, FPredictionKey PredictionKey, FDNAEventData TriggerEventData)
{
	InternalServerTryActiveAbility(Handle, InputPressed, PredictionKey, &TriggerEventData);
}

bool UDNAAbilitySystemComponent::ServerTryActivateAbilityWithEventData_Validate(FDNAAbilitySpecHandle Handle, bool InputPressed, FPredictionKey PredictionKey, FDNAEventData TriggerEventData)
{
	return true;
}

void UDNAAbilitySystemComponent::ClientTryActivateAbility_Implementation(FDNAAbilitySpecHandle Handle)
{
	FDNAAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
	if (!Spec)
	{
		// Can happen if the client gets told to activate an ability the same frame that abilities are added on the server
		FPendingAbilityInfo AbilityInfo;
		AbilityInfo.Handle = Handle;
		AbilityInfo.bPartiallyActivated = false;

		// This won't add it if we're currently being called from the pending list
		PendingServerActivatedAbilities.AddUnique(AbilityInfo);
		return;
	}

	InternalTryActivateAbility(Handle);
}

void UDNAAbilitySystemComponent::InternalServerTryActiveAbility(FDNAAbilitySpecHandle Handle, bool InputPressed, const FPredictionKey& PredictionKey, const FDNAEventData* TriggerEventData)
{
#if WITH_SERVER_CODE
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (DenyClientActivation > 0)
	{
		DenyClientActivation--;
		ClientActivateAbilityFailed(Handle, PredictionKey.Current);
		return;
	}
#endif

	FDNAAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
	if (!Spec)
	{
		// Can potentially happen in race conditions where client tries to activate ability that is removed server side before it is received.
		ABILITY_LOG(Display, TEXT("InternalServerTryActiveAbility. Rejecting ClientActivation of ability with invalid SpecHandle!"));
		ClientActivateAbilityFailed(Handle, PredictionKey.Current);
		return;
	}

	// Consume any pending target info, to clear out cancels from old executions
	ConsumeAllReplicatedData(Handle, PredictionKey);

	FScopedPredictionWindow ScopedPredictionWindow(this, PredictionKey);

	const UDNAAbility* AbilityToActivate = Spec->Ability;

	ensure(AbilityToActivate);
	ensure(AbilityActorInfo.IsValid());

	UDNAAbility* InstancedAbility = nullptr;
	Spec->InputPressed = true;

	// Attempt to activate the ability (server side) and tell the client if it succeeded or failed.
	if (InternalTryActivateAbility(Handle, PredictionKey, &InstancedAbility, nullptr, TriggerEventData))
	{
		// TryActivateAbility handles notifying the client of success
	}
	else
	{
		ABILITY_LOG(Display, TEXT("InternalServerTryActiveAbility. Rejecting ClientActivation of %s. InternalTryActivateAbility failed"), *GetNameSafe(Spec->Ability) );
		ClientActivateAbilityFailed(Handle, PredictionKey.Current);
		Spec->InputPressed = false;
	}
	MarkAbilitySpecDirty(*Spec);
#endif
}

void UDNAAbilitySystemComponent::ReplicateEndOrCancelAbility(FDNAAbilitySpecHandle Handle, FDNAAbilityActivationInfo ActivationInfo, UDNAAbility* Ability, bool bWasCanceled)
{
	if (Ability->GetNetExecutionPolicy() == EDNAAbilityNetExecutionPolicy::LocalPredicted || Ability->GetNetExecutionPolicy() == EDNAAbilityNetExecutionPolicy::ServerInitiated)
	{
		// Only replicate ending if policy is predictive
		if (GetOwnerRole() == ROLE_Authority)
		{
			if (!AbilityActorInfo->IsLocallyControlled())
			{
				// Only tell the client about the end/cancel ability if we're not the local controller
				if (bWasCanceled)
				{
					ClientCancelAbility(Handle, ActivationInfo);
				}
				else
				{
					ClientEndAbility(Handle, ActivationInfo);
				}
			}
		}
		else
		{
			// This passes up the current prediction key if we have one
			if (bWasCanceled)
			{
				ServerCancelAbility(Handle, ActivationInfo);
			}
			else
			{
				ServerEndAbility(Handle, ActivationInfo, ScopedPredictionKey);
			}
		}
	}
}

// This is only called when ending or canceling an ability in response to a remote instruction.
void UDNAAbilitySystemComponent::RemoteEndOrCancelAbility(FDNAAbilitySpecHandle AbilityToEnd, FDNAAbilityActivationInfo ActivationInfo, bool bWasCanceled)
{
	FDNAAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(AbilityToEnd);
	if (AbilitySpec && AbilitySpec->Ability && AbilitySpec->IsActive())
	{
		// Handle non-instanced case, which cannot perform prediction key validation
		if (AbilitySpec->Ability->GetInstancingPolicy() == EDNAAbilityInstancingPolicy::NonInstanced)
		{
			// End/Cancel the ability but don't replicate it back to whoever called us
			if (bWasCanceled)
			{
				AbilitySpec->Ability->CancelAbility(AbilityToEnd, AbilityActorInfo.Get(), ActivationInfo, false);
			}
			else
			{
				AbilitySpec->Ability->EndAbility(AbilityToEnd, AbilityActorInfo.Get(), ActivationInfo, false, bWasCanceled);
			}
		}
		else
		{
			TArray<UDNAAbility*> Instances = AbilitySpec->GetAbilityInstances();

			for (auto Instance : Instances)
			{
				// Check if the ability is the same prediction key (can both by 0) and has been confirmed. If so cancel it.
				if (Instance->GetCurrentActivationInfoRef().GetActivationPredictionKey() == ActivationInfo.GetActivationPredictionKey())
				{
					// Let the ability know that the remote instance has ended, even if we aren't about to end it here.
					Instance->SetRemoteInstanceHasEnded();

					if (Instance->GetCurrentActivationInfoRef().bCanBeEndedByOtherInstance)
					{
						// End/Cancel the ability but don't replicate it back to whoever called us
						if (bWasCanceled)
						{
							ForceCancelAbilityDueToReplication(Instance);
						}
						else
						{
							Instance->EndAbility(Instance->CurrentSpecHandle, Instance->CurrentActorInfo, Instance->CurrentActivationInfo, false, bWasCanceled);
						}
					}
				}
			}
		}
	}
}

/** Force cancels the ability and does not replicate this to the other side. This should be called when the ability is cancelled by the other side. */
void UDNAAbilitySystemComponent::ForceCancelAbilityDueToReplication(UDNAAbility* Instance)
{
	check(Instance);

	// Since this was a remote cancel, we should force it through. We do not support 'server says ability was cancelled but client disagrees that it can be'.
	Instance->SetCanBeCanceled(true);
	Instance->CancelAbility(Instance->CurrentSpecHandle, Instance->CurrentActorInfo, Instance->CurrentActivationInfo, false);
}

void UDNAAbilitySystemComponent::ServerEndAbility_Implementation(FDNAAbilitySpecHandle AbilityToEnd, FDNAAbilityActivationInfo ActivationInfo, FPredictionKey PredictionKey)
{
	FScopedPredictionWindow ScopedPrediction(this, PredictionKey);
	
	RemoteEndOrCancelAbility(AbilityToEnd, ActivationInfo, false);
}

bool UDNAAbilitySystemComponent::ServerEndAbility_Validate(FDNAAbilitySpecHandle AbilityToEnd, FDNAAbilityActivationInfo ActivationInfo, FPredictionKey PredictionKey)
{
	return true;
}

void UDNAAbilitySystemComponent::ClientEndAbility_Implementation(FDNAAbilitySpecHandle AbilityToEnd, FDNAAbilityActivationInfo ActivationInfo)
{
	RemoteEndOrCancelAbility(AbilityToEnd, ActivationInfo, false);
}

void UDNAAbilitySystemComponent::ServerCancelAbility_Implementation(FDNAAbilitySpecHandle AbilityToCancel, FDNAAbilityActivationInfo ActivationInfo)
{
	RemoteEndOrCancelAbility(AbilityToCancel, ActivationInfo, true);
}

bool UDNAAbilitySystemComponent::ServerCancelAbility_Validate(FDNAAbilitySpecHandle AbilityToCancel, FDNAAbilityActivationInfo ActivationInfo)
{
	return true;
}

void UDNAAbilitySystemComponent::ClientCancelAbility_Implementation(FDNAAbilitySpecHandle AbilityToCancel, FDNAAbilityActivationInfo ActivationInfo)
{
	RemoteEndOrCancelAbility(AbilityToCancel, ActivationInfo, true);
}

static_assert(sizeof(int16) == sizeof(FPredictionKey::KeyType), "Sizeof PredictionKey::KeyType does not match RPC parameters in DNAAbilitySystemComponent ClientActivateAbilityFailed_Implementation");

void UDNAAbilitySystemComponent::ClientActivateAbilityFailed_Implementation(FDNAAbilitySpecHandle Handle, int16 PredictionKey)
{
	// Tell anything else listening that this was rejected
	if (PredictionKey > 0)
	{
		FPredictionKeyDelegates::BroadcastRejectedDelegate(PredictionKey);
	}

	// Find the actual UDNAAbility		
	FDNAAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
	if (Spec == nullptr)
	{
		ABILITY_LOG(Display, TEXT("ClientActivateAbilityFailed_Implementation. PredictionKey :%d Ability: Could not find!"), PredictionKey);
		return;
	}

	ABILITY_LOG(Display, TEXT("ClientActivateAbilityFailed_Implementation. PredictionKey :%d Ability:"), PredictionKey, *GetNameSafe(Spec->Ability));

	// The ability should be either confirmed or rejected by the time we get here
	if (Spec->ActivationInfo.GetActivationPredictionKey().Current == PredictionKey)
	{
		Spec->ActivationInfo.SetActivationRejected();
	}

	TArray<UDNAAbility*> Instances = Spec->GetAbilityInstances();
	for (UDNAAbility* Ability : Instances)
	{
		if (Ability->CurrentActivationInfo.GetActivationPredictionKey().Current == PredictionKey)
		{
			Ability->K2_EndAbility();
		}
	}
}

void UDNAAbilitySystemComponent::OnClientActivateAbilityCaughtUp(FDNAAbilitySpecHandle Handle, FPredictionKey::KeyType PredictionKey)
{
	FDNAAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
	if (Spec && Spec->IsActive())
	{
		// The ability should be either confirmed or rejected by the time we get here
		if (Spec->ActivationInfo.ActivationMode == EDNAAbilityActivationMode::Predicting && Spec->ActivationInfo.GetActivationPredictionKey().Current == PredictionKey)
		{
			// It is possible to have this happen under bad network conditions. (Reliable Confirm/Reject RPC is lost, but separate property bunch makes it through before the reliable resend happens)
			ABILITY_LOG(Display, TEXT("UDNAAbilitySystemComponent::OnClientActivateAbilityCaughtUp. Ability %s caught up to PredictionKey %d but instance is still active and in predicting state."), *GetNameSafe(Spec->Ability), PredictionKey);
		}
	}
}

void UDNAAbilitySystemComponent::ClientActivateAbilitySucceed_Implementation(FDNAAbilitySpecHandle Handle, FPredictionKey PredictionKey)
{
	ClientActivateAbilitySucceedWithEventData_Implementation(Handle, PredictionKey, FDNAEventData());
}

void UDNAAbilitySystemComponent::ClientActivateAbilitySucceedWithEventData_Implementation(FDNAAbilitySpecHandle Handle, FPredictionKey PredictionKey, FDNAEventData TriggerEventData)
{
	FDNAAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
	if (!Spec)
	{
		// Can happen if the client gets told to activate an ability the same frame that abilities are added on the server
		FPendingAbilityInfo AbilityInfo;
		AbilityInfo.PredictionKey = PredictionKey;
		AbilityInfo.Handle = Handle;
		AbilityInfo.TriggerEventData = TriggerEventData;
		AbilityInfo.bPartiallyActivated = true;

		// This won't add it if we're currently being called from the pending list
		PendingServerActivatedAbilities.AddUnique(AbilityInfo);
		return;
	}

	UDNAAbility* AbilityToActivate = Spec->Ability;

	check(AbilityToActivate);
	ensure(AbilityActorInfo.IsValid());

	Spec->ActivationInfo.SetActivationConfirmed();

	// Fixme: We need a better way to link up/reconcile predictive replicated abilities. It would be ideal if we could predictively spawn an
	// ability and then replace/link it with the server spawned one once the server has confirmed it.

	if (AbilityToActivate->NetExecutionPolicy == EDNAAbilityNetExecutionPolicy::LocalPredicted)
	{
		if (AbilityToActivate->GetInstancingPolicy() == EDNAAbilityInstancingPolicy::NonInstanced)
		{
			// AbilityToActivate->ConfirmActivateSucceed(); // This doesn't do anything for non instanced
		}
		else
		{
			// Find the one we predictively spawned, tell them we are confirmed
			bool found = false;
			TArray<UDNAAbility*> Instances = Spec->GetAbilityInstances();
			for (UDNAAbility* LocalAbility : Instances)
			{
				if (LocalAbility->GetCurrentActivationInfo().GetActivationPredictionKey() == PredictionKey)
				{
					LocalAbility->ConfirmActivateSucceed();
					found = true;
					break;
				}
			}

			if (!found)
			{
				ABILITY_LOG(Verbose, TEXT("Ability %s was confirmed by server but no longer exists on client (replication key: %d"), *AbilityToActivate->GetName(), PredictionKey.Current);
			}
		}
	}
	else
	{
		// We haven't already executed this ability at all, so kick it off.

		// The spec will now be active, and we need to keep track on the client as well.  Since we cannot call TryActivateAbility, which will increment ActiveCount on the server, we have to do this here.
		++Spec->ActiveCount;

		if (PredictionKey.bIsServerInitiated)
		{
			// We have an active server key, set our key equal to it
			Spec->ActivationInfo.ServerSetActivationPredictionKey(PredictionKey);
		}
		
		if (AbilityToActivate->GetInstancingPolicy() == EDNAAbilityInstancingPolicy::InstancedPerExecution)
		{
			// Need to instantiate this in order to execute
			UDNAAbility* InstancedAbility = CreateNewInstanceOfAbility(*Spec, AbilityToActivate);
			InstancedAbility->CallActivateAbility(Handle, AbilityActorInfo.Get(), Spec->ActivationInfo, nullptr, TriggerEventData.EventTag.IsValid() ?  &TriggerEventData : nullptr);
		}
		else if (AbilityToActivate->GetInstancingPolicy() != EDNAAbilityInstancingPolicy::NonInstanced)
		{
			UDNAAbility* InstancedAbility = Spec->GetPrimaryInstance();

			if (!InstancedAbility)
			{
				ABILITY_LOG(Warning, TEXT("Ability %s cannot be activated on the client because it's missing a primary instance!"), *AbilityToActivate->GetName());
				return;
			}
			InstancedAbility->CallActivateAbility(Handle, AbilityActorInfo.Get(), Spec->ActivationInfo, nullptr, TriggerEventData.EventTag.IsValid() ? &TriggerEventData : nullptr);
		}
		else
		{
			AbilityToActivate->CallActivateAbility(Handle, AbilityActorInfo.Get(), Spec->ActivationInfo, nullptr, TriggerEventData.EventTag.IsValid() ? &TriggerEventData : nullptr);
		}
	}
}

bool UDNAAbilitySystemComponent::TriggerAbilityFromDNAEvent(FDNAAbilitySpecHandle Handle, FDNAAbilityActorInfo* ActorInfo, FDNATag EventTag, const FDNAEventData* Payload, UDNAAbilitySystemComponent& Component)
{
	FDNAAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
	if (!ensure(Spec))
	{
		return false;
	}

	const UDNAAbility* InstancedAbility = Spec->GetPrimaryInstance();
	const UDNAAbility* Ability = InstancedAbility ? InstancedAbility : Spec->Ability;
	if (!ensure(Ability))
	{
		return false;
	}

	if (!ensure(Payload))
	{
		return false;
	}

	if (!HasNetworkAuthorityToActivateTriggeredAbility(*Spec))
	{
		// The server or client will handle activating the trigger
		return false;
	}

	// Make a temp copy of the payload, and copy the event tag into it
	FDNAEventData TempEventData = *Payload;
	TempEventData.EventTag = EventTag;

	// Run on the non-instanced ability
	if (Ability->ShouldAbilityRespondToEvent(ActorInfo, &TempEventData))
	{
		int32 ExecutingAbilityIndex = -1;

		// if we're the server and this is coming from a predicted event we should check if the client has already predicted it
		if (ScopedPredictionKey.IsValidKey()
			&& Ability->GetNetExecutionPolicy() == EDNAAbilityNetExecutionPolicy::LocalPredicted
			&& ActorInfo->OwnerActor->Role == ROLE_Authority)
		{
			bool bPendingClientAbilityFound = false;
			for (auto PendingAbilityInfo : Component.PendingClientActivatedAbilities)
			{
				if (ScopedPredictionKey.Current == PendingAbilityInfo.PredictionKey.Base && Handle == PendingAbilityInfo.Handle) // found a match
				{
					Component.PendingClientActivatedAbilities.RemoveSingleSwap(PendingAbilityInfo);
					bPendingClientAbilityFound = true;
					break;
				}
			}

			// we haven't received the client's copy of the triggered ability
			// keep track of this so we can associate the prediction keys when it comes in
			if (bPendingClientAbilityFound == false)
			{
				UDNAAbilitySystemComponent::FExecutingAbilityInfo Info;
				Info.PredictionKey = ScopedPredictionKey;
				Info.Handle = Handle;

				ExecutingAbilityIndex = Component.ExecutingServerAbilities.Add(Info);
			}
		}

		if (InternalTryActivateAbility(Handle, ScopedPredictionKey, nullptr, nullptr, &TempEventData))
		{
			if (ExecutingAbilityIndex >= 0)
			{
				Component.ExecutingServerAbilities[ExecutingAbilityIndex].State = UDNAAbilitySystemComponent::EAbilityExecutionState::Succeeded;
			}
			return true;
		}
		else if (ExecutingAbilityIndex >= 0)
		{
			Component.ExecutingServerAbilities[ExecutingAbilityIndex].State = UDNAAbilitySystemComponent::EAbilityExecutionState::Failed;
		}
	}
	return false;
}

// ----------------------------------------------------------------------------------------------------------------------------------------------------
//								Input 
// ----------------------------------------------------------------------------------------------------------------------------------------------------

bool UDNAAbilitySystemComponent::GetUserAbilityActivationInhibited() const
{
	return UserAbilityActivationInhibited;
}

void UDNAAbilitySystemComponent::SetUserAbilityActivationInhibited(bool NewInhibit)
{
	if(AbilityActorInfo->IsLocallyControlled())
	{
		if (NewInhibit && UserAbilityActivationInhibited)
		{
			// This could cause problems if two sources try to inhibit ability activation, it is not clear when the ability should be uninhibited
			ABILITY_LOG(Warning, TEXT("Call to SetUserAbilityActivationInhibited(true) when UserAbilityActivationInhibited was already true"));
		}

		UserAbilityActivationInhibited = NewInhibit;
	}
}

void UDNAAbilitySystemComponent::NotifyAbilityCommit(UDNAAbility* Ability)
{
	AbilityCommitedCallbacks.Broadcast(Ability);
}

void UDNAAbilitySystemComponent::NotifyAbilityActivated(FDNAAbilitySpecHandle Handle, UDNAAbility* Ability)
{
	AbilityActivatedCallbacks.Broadcast(Ability);
}

void UDNAAbilitySystemComponent::NotifyAbilityFailed(const FDNAAbilitySpecHandle Handle, UDNAAbility* Ability, const FDNATagContainer& FailureReason)
{
	AbilityFailedCallbacks.Broadcast(Ability, FailureReason);
}

int32 UDNAAbilitySystemComponent::HandleDNAEvent(FDNATag EventTag, const FDNAEventData* Payload)
{
	int32 TriggeredCount = 0;
	FDNATag CurrentTag = EventTag;
	while (CurrentTag.IsValid())
	{
		if (DNAEventTriggeredAbilities.Contains(CurrentTag))
		{
			TArray<FDNAAbilitySpecHandle> TriggeredAbilityHandles = DNAEventTriggeredAbilities[CurrentTag];

			for (auto AbilityHandle : TriggeredAbilityHandles)
			{
				if (TriggerAbilityFromDNAEvent(AbilityHandle, AbilityActorInfo.Get(), EventTag, Payload, *this))
				{
					TriggeredCount++;
				}
			}
		}

		CurrentTag = CurrentTag.RequestDirectParent();
	}

	if (FDNAEventMulticastDelegate* Delegate = GenericDNAEventCallbacks.Find(EventTag))
	{
		Delegate->Broadcast(Payload);
	}

	return TriggeredCount;
}

void UDNAAbilitySystemComponent::MonitoredTagChanged(const FDNATag Tag, int32 NewCount)
{
	int32 TriggeredCount = 0;
	if (OwnedTagTriggeredAbilities.Contains(Tag))
	{
		TArray<FDNAAbilitySpecHandle> TriggeredAbilityHandles = OwnedTagTriggeredAbilities[Tag];

		for (auto AbilityHandle : TriggeredAbilityHandles)
		{
			FDNAAbilitySpec* Spec = FindAbilitySpecFromHandle(AbilityHandle);

			if (!Spec || !HasNetworkAuthorityToActivateTriggeredAbility(*Spec))
			{
				return;
			}

			for (const FAbilityTriggerData& TriggerData : Spec->Ability->AbilityTriggers)
			{
				FDNATag EventTag = TriggerData.TriggerTag;

				if (EventTag == Tag)
				{
					if (NewCount > 0)
					{
						// Populate event data so this will use the same blueprint node to activate as DNA triggers
						FDNAEventData EventData;
						EventData.EventMagnitude = NewCount;
						EventData.EventTag = EventTag;
						EventData.Instigator = OwnerActor;
						EventData.Target = OwnerActor;
						// Try to activate it
						InternalTryActivateAbility(Spec->Handle, FPredictionKey(), nullptr, nullptr, &EventData);

						// TODO: Check client/server type
					}
					else if (NewCount == 0 && TriggerData.TriggerSource == EDNAAbilityTriggerSource::OwnedTagPresent)
					{
						// Try to cancel, but only if the type is right
						CancelAbilitySpec(*Spec, nullptr);
					}
				}
			}
		}
	}
}

bool UDNAAbilitySystemComponent::HasNetworkAuthorityToActivateTriggeredAbility(const FDNAAbilitySpec &Spec) const
{
	bool bIsAuthority = IsOwnerActorAuthoritative();
	bool bIsLocal = AbilityActorInfo->IsLocallyControlled();

	switch (Spec.Ability->GetNetExecutionPolicy())
	{
	case EDNAAbilityNetExecutionPolicy::LocalOnly:
	case EDNAAbilityNetExecutionPolicy::LocalPredicted:
		return bIsLocal;
	case EDNAAbilityNetExecutionPolicy::ServerOnly:
	case EDNAAbilityNetExecutionPolicy::ServerInitiated:
		return bIsAuthority;
	}

	return false;
}

// ----------------------------------------------------------------------------------------------------------------------------------------------------
//								Input 
// ----------------------------------------------------------------------------------------------------------------------------------------------------


void UDNAAbilitySystemComponent::BindToInputComponent(UInputComponent* InputComponent)
{
	static const FName ConfirmBindName(TEXT("AbilityConfirm"));
	static const FName CancelBindName(TEXT("AbilityCancel"));

	// Pressed event
	{
		FInputActionBinding AB(ConfirmBindName, IE_Pressed);
		AB.ActionDelegate.GetDelegateForManualSet().BindUObject(this, &UDNAAbilitySystemComponent::LocalInputConfirm);
		InputComponent->AddActionBinding(AB);
	}

	// 
	{
		FInputActionBinding AB(CancelBindName, IE_Pressed);
		AB.ActionDelegate.GetDelegateForManualSet().BindUObject(this, &UDNAAbilitySystemComponent::LocalInputCancel);
		InputComponent->AddActionBinding(AB);
	}
}

void UDNAAbilitySystemComponent::BindAbilityActivationToInputComponent(UInputComponent* InputComponent, FDNAAbiliyInputBinds BindInfo)
{
	UEnum* EnumBinds = BindInfo.GetBindEnum();

	SetBlockAbilityBindingsArray(BindInfo);

	for(int32 idx=0; idx < EnumBinds->NumEnums(); ++idx)
	{
		FString FullStr = EnumBinds->GetEnum(idx).ToString();
		FString BindStr;

		FullStr.Split(TEXT("::"), nullptr, &BindStr);

		// Pressed event
		{
			FInputActionBinding AB(FName(*BindStr), IE_Pressed);
			AB.ActionDelegate.GetDelegateForManualSet().BindUObject(this, &UDNAAbilitySystemComponent::AbilityLocalInputPressed, idx);
			InputComponent->AddActionBinding(AB);
		}

		// Released event
		{
			FInputActionBinding AB(FName(*BindStr), IE_Released);
			AB.ActionDelegate.GetDelegateForManualSet().BindUObject(this, &UDNAAbilitySystemComponent::AbilityLocalInputReleased, idx);
			InputComponent->AddActionBinding(AB);
		}
	}

	// Bind Confirm/Cancel. Note: these have to come last!
	if (BindInfo.ConfirmTargetCommand.IsEmpty() == false)
	{
		FInputActionBinding AB(FName(*BindInfo.ConfirmTargetCommand), IE_Pressed);
		AB.ActionDelegate.GetDelegateForManualSet().BindUObject(this, &UDNAAbilitySystemComponent::LocalInputConfirm);
		InputComponent->AddActionBinding(AB);
	}
	
	if (BindInfo.CancelTargetCommand.IsEmpty() == false)
	{
		FInputActionBinding AB(FName(*BindInfo.CancelTargetCommand), IE_Pressed);
		AB.ActionDelegate.GetDelegateForManualSet().BindUObject(this, &UDNAAbilitySystemComponent::LocalInputCancel);
		InputComponent->AddActionBinding(AB);
	}

	if (BindInfo.CancelTargetInputID >= 0)
	{
		GenericCancelInputID = BindInfo.CancelTargetInputID;
	}
	if (BindInfo.ConfirmTargetInputID >= 0)
	{
		GenericConfirmInputID = BindInfo.ConfirmTargetInputID;
	}
}

void UDNAAbilitySystemComponent::SetBlockAbilityBindingsArray(FDNAAbiliyInputBinds BindInfo)
{
	UEnum* EnumBinds = BindInfo.GetBindEnum();
	BlockedAbilityBindings.SetNumZeroed(EnumBinds->NumEnums());
}

void UDNAAbilitySystemComponent::AbilityLocalInputPressed(int32 InputID)
{
	// Consume the input if this InputID is overloaded with GenericConfirm/Cancel and the GenericConfim/Cancel callback is bound
	if (IsGenericConfirmInputBound(InputID))
	{
		LocalInputConfirm();
		return;
	}

	if (IsGenericCancelInputBound(InputID))
	{
		LocalInputCancel();
		return;
	}

	// ---------------------------------------------------------

	ABILITYLIST_SCOPE_LOCK();
	for (FDNAAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (Spec.InputID == InputID)
		{
			if (Spec.Ability)
			{
				Spec.InputPressed = true;
				if (Spec.IsActive())
				{
					if (Spec.Ability->bReplicateInputDirectly && IsOwnerActorAuthoritative() == false)
					{
						ServerSetInputPressed(Spec.Handle);
					}

					AbilitySpecInputPressed(Spec);

					// Invoke the InputPressed event. This is not replicated here. If someone is listening, they may replicate the InputPressed event to the server.
					InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, Spec.Handle, Spec.ActivationInfo.GetActivationPredictionKey());					
				}
				else
				{
					// Ability is not active, so try to activate it
					TryActivateAbility(Spec.Handle);
				}
			}
		}
	}
}

void UDNAAbilitySystemComponent::AbilityLocalInputReleased(int32 InputID)
{
	ABILITYLIST_SCOPE_LOCK();
	for (FDNAAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (Spec.InputID == InputID)
		{
			Spec.InputPressed = false;
			if (Spec.Ability && Spec.IsActive())
			{
				if (Spec.Ability->bReplicateInputDirectly && IsOwnerActorAuthoritative() == false)
				{
					ServerSetInputReleased(Spec.Handle);
				}

				AbilitySpecInputReleased(Spec);
				
				InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, Spec.Handle, Spec.ActivationInfo.GetActivationPredictionKey());
			}
		}
	}
}

void UDNAAbilitySystemComponent::ServerSetInputPressed_Implementation(FDNAAbilitySpecHandle AbilityHandle)
{
	FDNAAbilitySpec* Spec = FindAbilitySpecFromHandle(AbilityHandle);
	if (Spec)
	{
		AbilitySpecInputPressed(*Spec);
	}

}

void UDNAAbilitySystemComponent::ServerSetInputReleased_Implementation(FDNAAbilitySpecHandle AbilityHandle)
{
	FDNAAbilitySpec* Spec = FindAbilitySpecFromHandle(AbilityHandle);
	if (Spec)
	{
		AbilitySpecInputReleased(*Spec);
	}
}

bool UDNAAbilitySystemComponent::ServerSetInputPressed_Validate(FDNAAbilitySpecHandle AbilityHandle)
{
	return true;
}

bool UDNAAbilitySystemComponent::ServerSetInputReleased_Validate(FDNAAbilitySpecHandle AbilityHandle)
{
	return true;
}

void UDNAAbilitySystemComponent::AbilitySpecInputPressed(FDNAAbilitySpec& Spec)
{
	Spec.InputPressed = true;
	if (Spec.IsActive())
	{
		// The ability is active, so just pipe the input event to it
		if (Spec.Ability->GetInstancingPolicy() == EDNAAbilityInstancingPolicy::NonInstanced)
		{
			Spec.Ability->InputPressed(Spec.Handle, AbilityActorInfo.Get(), Spec.ActivationInfo);
		}
		else
		{
			TArray<UDNAAbility*> Instances = Spec.GetAbilityInstances();
			for (UDNAAbility* Instance : Instances)
			{
				Instance->InputPressed(Spec.Handle, AbilityActorInfo.Get(), Spec.ActivationInfo);
			}
		}
	}
}

void UDNAAbilitySystemComponent::AbilitySpecInputReleased(FDNAAbilitySpec& Spec)
{
	Spec.InputPressed = false;
	if (Spec.IsActive())
	{
		// The ability is active, so just pipe the input event to it
		if (Spec.Ability->GetInstancingPolicy() == EDNAAbilityInstancingPolicy::NonInstanced)
		{
			Spec.Ability->InputReleased(Spec.Handle, AbilityActorInfo.Get(), Spec.ActivationInfo);
		}
		else
		{
			TArray<UDNAAbility*> Instances = Spec.GetAbilityInstances();
			for (UDNAAbility* Instance : Instances)
			{
				Instance->InputReleased(Spec.Handle, AbilityActorInfo.Get(), Spec.ActivationInfo);
			}
		}
	}
}

void UDNAAbilitySystemComponent::LocalInputConfirm()
{
	FAbilityConfirmOrCancel Temp = GenericLocalConfirmCallbacks;
	GenericLocalConfirmCallbacks.Clear();
	Temp.Broadcast();
}

void UDNAAbilitySystemComponent::LocalInputCancel()
{	
	FAbilityConfirmOrCancel Temp = GenericLocalCancelCallbacks;
	GenericLocalCancelCallbacks.Clear();
	Temp.Broadcast();
}

void UDNAAbilitySystemComponent::TargetConfirm()
{
	TArray<ADNAAbilityTargetActor*> LeftoverTargetActors;
	for (ADNAAbilityTargetActor* TargetActor : SpawnedTargetActors)
	{
		if (TargetActor)
		{
			if (TargetActor->IsConfirmTargetingAllowed())
			{
				//TODO: There might not be any cases where this bool is false
				if (!TargetActor->bDestroyOnConfirmation)
				{
					LeftoverTargetActors.Add(TargetActor);
				}
				TargetActor->ConfirmTargeting();
			}
			else
			{
				LeftoverTargetActors.Add(TargetActor);
			}
		}
	}
	SpawnedTargetActors = LeftoverTargetActors;		//These actors declined to confirm targeting, or are allowed to fire multiple times, so keep contact with them.
}

void UDNAAbilitySystemComponent::TargetCancel()
{
	for (ADNAAbilityTargetActor* TargetActor : SpawnedTargetActors)
	{
		if (TargetActor)
		{
			TargetActor->CancelTargeting();
		}
	}

	SpawnedTargetActors.Empty();
}

// --------------------------------------------------------------------------

#if ENABLE_VISUAL_LOG
void UDNAAbilitySystemComponent::ClearDebugInstantEffects()
{
	ActiveDNAEffects.DebugExecutedDNAEffects.Empty();
}
#endif // ENABLE_VISUAL_LOG

// ---------------------------------------------------------------------------

float UDNAAbilitySystemComponent::PlayMontage(UDNAAbility* InAnimatingAbility, FDNAAbilityActivationInfo ActivationInfo, UAnimMontage* NewAnimMontage, float InPlayRate, FName StartSectionName)
{
	float Duration = -1.f;

	UAnimInstance* AnimInstance = AbilityActorInfo.IsValid() ? AbilityActorInfo->GetAnimInstance() : nullptr;
	if (AnimInstance && NewAnimMontage)
	{
		Duration = AnimInstance->Montage_Play(NewAnimMontage, InPlayRate);
		if (Duration > 0.f)
		{
			if (LocalAnimMontageInfo.AnimatingAbility && LocalAnimMontageInfo.AnimatingAbility != InAnimatingAbility)
			{
				// The ability that was previously animating will have already gotten the 'interrupted' callback.
				// It may be a good idea to make this a global policy and 'cancel' the ability.
				// 
				// For now, we expect it to end itself when this happens.
			}

			if (NewAnimMontage->HasRootMotion() && AnimInstance->GetOwningActor())
			{
				UE_LOG(LogRootMotion, Log, TEXT("UDNAAbilitySystemComponent::PlayMontage %s, Role: %s")
					, *GetNameSafe(NewAnimMontage)
					, *UEnum::GetValueAsString(TEXT("Engine.ENetRole"), AnimInstance->GetOwningActor()->Role)
					);
			}

			LocalAnimMontageInfo.AnimMontage = NewAnimMontage;
			LocalAnimMontageInfo.AnimatingAbility = InAnimatingAbility;
			
			if (InAnimatingAbility)
			{
				InAnimatingAbility->SetCurrentMontage(NewAnimMontage);
			}
			
			// Start at a given Section.
			if (StartSectionName != NAME_None)
			{
				AnimInstance->Montage_JumpToSection(StartSectionName, NewAnimMontage);
			}

			// Replicate to non owners
			if (IsOwnerActorAuthoritative())
			{
				// Those are static parameters, they are only set when the montage is played. They are not changed after that.
				RepAnimMontageInfo.AnimMontage = NewAnimMontage;
				RepAnimMontageInfo.ForcePlayBit = !bool(RepAnimMontageInfo.ForcePlayBit);

				// Update parameters that change during Montage life time.
				AnimMontage_UpdateReplicatedData();
			}
			else
			{
				// If this prediction key is rejected, we need to end the preview
				FPredictionKey PredictionKey = GetPredictionKeyForNewAction();
				if (PredictionKey.IsValidKey())
				{
					PredictionKey.NewRejectedDelegate().BindUObject(this, &UDNAAbilitySystemComponent::OnPredictiveMontageRejected, NewAnimMontage);
				}
			}
		}
	}

	return Duration;
}

float UDNAAbilitySystemComponent::PlayMontageSimulated(UAnimMontage* NewAnimMontage, float InPlayRate, FName StartSectionName)
{
	float Duration = -1.f;
	UAnimInstance* AnimInstance = AbilityActorInfo.IsValid() ? AbilityActorInfo->GetAnimInstance() : nullptr;
	if (AnimInstance && NewAnimMontage)
	{
		Duration = AnimInstance->Montage_Play(NewAnimMontage, InPlayRate);
		if (Duration > 0.f)
		{
			LocalAnimMontageInfo.AnimMontage = NewAnimMontage;
		}
	}

	return Duration;
}

void UDNAAbilitySystemComponent::AnimMontage_UpdateReplicatedData()
{
	check(IsOwnerActorAuthoritative());

	UAnimInstance* AnimInstance = AbilityActorInfo.IsValid() ? AbilityActorInfo->GetAnimInstance() : nullptr;
	if (AnimInstance && LocalAnimMontageInfo.AnimMontage)
	{
		RepAnimMontageInfo.AnimMontage = LocalAnimMontageInfo.AnimMontage;
		RepAnimMontageInfo.PlayRate = AnimInstance->Montage_GetPlayRate(LocalAnimMontageInfo.AnimMontage);
		RepAnimMontageInfo.Position = AnimInstance->Montage_GetPosition(LocalAnimMontageInfo.AnimMontage);
		RepAnimMontageInfo.BlendTime = AnimInstance->Montage_GetBlendTime(LocalAnimMontageInfo.AnimMontage);

		// Compressed Flags
		bool bIsStopped = AnimInstance->Montage_GetIsStopped(LocalAnimMontageInfo.AnimMontage);

		if (RepAnimMontageInfo.IsStopped != bIsStopped)
		{
			// Set this prior to calling UpdateShouldTick, so we start ticking if we are playing a Montage
			RepAnimMontageInfo.IsStopped = bIsStopped;

			// When we start or stop an animation, update the clients right away for the Avatar Actor
			if (AbilityActorInfo->AvatarActor != nullptr)
			{
				AbilityActorInfo->AvatarActor->ForceNetUpdate();
			}

			// When this changes, we should update whether or not we should be ticking
			UpdateShouldTick();
		}

		// Replicate NextSectionID to keep it in sync.
		// We actually replicate NextSectionID+1 on a BYTE to put INDEX_NONE in there.
		int32 CurrentSectionID = LocalAnimMontageInfo.AnimMontage->GetSectionIndexFromPosition(RepAnimMontageInfo.Position);
		if (CurrentSectionID != INDEX_NONE)
		{
			int32 NextSectionID = AnimInstance->Montage_GetNextSectionID(LocalAnimMontageInfo.AnimMontage, CurrentSectionID);
			if (NextSectionID >= (256 - 1))
			{
				ABILITY_LOG( Error, TEXT("AnimMontage_UpdateReplicatedData. NextSectionID = %d.  RepAnimMontageInfo.Position: %.2f, CurrentSectionID: %d. LocalAnimMontageInfo.AnimMontage %s"), 
					NextSectionID, RepAnimMontageInfo.Position, CurrentSectionID, *GetNameSafe(LocalAnimMontageInfo.AnimMontage) );
				ensure(NextSectionID < (256 - 1));
			}
			RepAnimMontageInfo.NextSectionID = uint8(NextSectionID + 1);
		}
		else
		{
			RepAnimMontageInfo.NextSectionID = 0;
		}
	}
}

void UDNAAbilitySystemComponent::OnPredictiveMontageRejected(UAnimMontage* PredictiveMontage)
{
	static const float MONTAGE_PREDICTION_REJECT_FADETIME = 0.25f;

	UAnimInstance* AnimInstance = AbilityActorInfo.IsValid() ? AbilityActorInfo->GetAnimInstance() : nullptr;
	if (AnimInstance && PredictiveMontage)
	{
		// If this montage is still playing: kill it
		if (AnimInstance->Montage_IsPlaying(PredictiveMontage))
		{
			AnimInstance->Montage_Stop(MONTAGE_PREDICTION_REJECT_FADETIME, PredictiveMontage);
		}
	}
}

bool UDNAAbilitySystemComponent::IsReadyForReplicatedMontage()
{
	/** Children may want to override this for additional checks (e.g, "has skin been applied") */
	return true;
}

/**	Replicated Event for AnimMontages */
void UDNAAbilitySystemComponent::OnRep_ReplicatedAnimMontage()
{
	static const float MONTAGE_REP_POS_ERR_THRESH = 0.1f;

	UAnimInstance* AnimInstance = AbilityActorInfo.IsValid() ? AbilityActorInfo->GetAnimInstance() : nullptr;
	if (AnimInstance == nullptr || !IsReadyForReplicatedMontage())
	{
		// We can't handle this yet
		bPendingMontagerep = true;
		return;
	}
	bPendingMontagerep = false;

	if (!AbilityActorInfo->IsLocallyControlled())
	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("net.Montage.Debug"));
		bool DebugMontage = (CVar && CVar->GetValueOnGameThread() == 1);
		if (DebugMontage)
		{
			ABILITY_LOG( Warning, TEXT("\n\nOnRep_ReplicatedAnimMontage, %s"), *GetNameSafe(this));
			ABILITY_LOG( Warning, TEXT("\tAnimMontage: %s\n\tPlayRate: %f\n\tPosition: %f\n\tBlendTime: %f\n\tNextSectionID: %d\n\tIsStopped: %d\n\tForcePlayBit: %d"),
				*GetNameSafe(RepAnimMontageInfo.AnimMontage), 
				RepAnimMontageInfo.PlayRate, 
				RepAnimMontageInfo.Position,
				RepAnimMontageInfo.BlendTime,
				RepAnimMontageInfo.NextSectionID, 
				RepAnimMontageInfo.IsStopped, 
				RepAnimMontageInfo.ForcePlayBit);
			ABILITY_LOG( Warning, TEXT("\tLocalAnimMontageInfo.AnimMontage: %s\n\tPosition: %f"),
				*GetNameSafe(LocalAnimMontageInfo.AnimMontage), AnimInstance->Montage_GetPosition(LocalAnimMontageInfo.AnimMontage));
		}

		if( RepAnimMontageInfo.AnimMontage )
		{
			// New Montage to play
			bool ReplicatedPlayBit = bool(RepAnimMontageInfo.ForcePlayBit);
			if ((LocalAnimMontageInfo.AnimMontage != RepAnimMontageInfo.AnimMontage) || (LocalAnimMontageInfo.PlayBit != ReplicatedPlayBit))
			{
				LocalAnimMontageInfo.PlayBit = ReplicatedPlayBit;
				PlayMontageSimulated(RepAnimMontageInfo.AnimMontage, RepAnimMontageInfo.PlayRate);
			}

			if (LocalAnimMontageInfo.AnimMontage == nullptr)
			{ 
				ABILITY_LOG(Warning, TEXT("OnRep_ReplicatedAnimMontage: PlayMontageSimulated failed. Name: %s, AnimMontage: %s"), *GetNameSafe(this), *GetNameSafe(RepAnimMontageInfo.AnimMontage));
				return;
			}

			// Play Rate has changed
			if (AnimInstance->Montage_GetPlayRate(LocalAnimMontageInfo.AnimMontage) != RepAnimMontageInfo.PlayRate)
			{
				AnimInstance->Montage_SetPlayRate(LocalAnimMontageInfo.AnimMontage, RepAnimMontageInfo.PlayRate);
			}

			// Compressed Flags
			const bool bIsStopped = AnimInstance->Montage_GetIsStopped(LocalAnimMontageInfo.AnimMontage);
			const bool bReplicatedIsStopped = bool(RepAnimMontageInfo.IsStopped);

			// Process stopping first, so we don't change sections and cause blending to pop.
			if (bReplicatedIsStopped)
			{
				if (!bIsStopped)
				{
					CurrentMontageStop(RepAnimMontageInfo.BlendTime);
				}
			}
			else
			{
				int32 RepSectionID = LocalAnimMontageInfo.AnimMontage->GetSectionIndexFromPosition(RepAnimMontageInfo.Position);
				int32 RepNextSectionID = int32(RepAnimMontageInfo.NextSectionID) - 1;
		
				// And NextSectionID for the replicated SectionID.
				if( RepSectionID != INDEX_NONE )
				{
					int32 NextSectionID = AnimInstance->Montage_GetNextSectionID(LocalAnimMontageInfo.AnimMontage, RepSectionID);

					// If NextSectionID is different than the replicated one, then set it.
					if( NextSectionID != RepNextSectionID )
					{
						AnimInstance->Montage_SetNextSection(LocalAnimMontageInfo.AnimMontage->GetSectionName(RepSectionID), LocalAnimMontageInfo.AnimMontage->GetSectionName(RepNextSectionID), LocalAnimMontageInfo.AnimMontage);
					}

					// Make sure we haven't received that update too late and the client hasn't already jumped to another section. 
					int32 CurrentSectionID = LocalAnimMontageInfo.AnimMontage->GetSectionIndexFromPosition(AnimInstance->Montage_GetPosition(LocalAnimMontageInfo.AnimMontage));
					if ((CurrentSectionID != RepSectionID) && (CurrentSectionID != RepNextSectionID))
					{
						// Client is in a wrong section, teleport him into the begining of the right section
						const float SectionStartTime = LocalAnimMontageInfo.AnimMontage->GetAnimCompositeSection(RepSectionID).GetTime();
						AnimInstance->Montage_SetPosition(LocalAnimMontageInfo.AnimMontage, SectionStartTime);
					}
				}

				// Update Position. If error is too great, jump to replicated position.
				float CurrentPosition = AnimInstance->Montage_GetPosition(LocalAnimMontageInfo.AnimMontage);
				int32 CurrentSectionID = LocalAnimMontageInfo.AnimMontage->GetSectionIndexFromPosition(CurrentPosition);
				// Only check threshold if we are located in the same section. Different sections require a bit more work as we could be jumping around the timeline.
				if ((CurrentSectionID == RepSectionID) && (FMath::Abs(CurrentPosition - RepAnimMontageInfo.Position) > MONTAGE_REP_POS_ERR_THRESH) && RepAnimMontageInfo.IsStopped == 0)
				{
					// fast forward to server position and trigger notifies
					if (FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(RepAnimMontageInfo.AnimMontage))
					{
						MontageInstance->HandleEvents(CurrentPosition, RepAnimMontageInfo.Position, nullptr);
						AnimInstance->TriggerAnimNotifies(0.f);
					}
					AnimInstance->Montage_SetPosition(LocalAnimMontageInfo.AnimMontage, RepAnimMontageInfo.Position);
				}
			}
		}
	}
}

void UDNAAbilitySystemComponent::CurrentMontageStop(float OverrideBlendOutTime)
{
	UAnimInstance* AnimInstance = AbilityActorInfo.IsValid() ? AbilityActorInfo->GetAnimInstance() : nullptr;
	UAnimMontage* MontageToStop = LocalAnimMontageInfo.AnimMontage;
	bool bShouldStopMontage = AnimInstance && MontageToStop && !AnimInstance->Montage_GetIsStopped(MontageToStop);

	if (bShouldStopMontage)
	{
		const float BlendOutTime = (OverrideBlendOutTime >= 0.0f ? OverrideBlendOutTime : MontageToStop->BlendOut.GetBlendTime());

		AnimInstance->Montage_Stop(MontageToStop->BlendOut.GetBlendTime(), MontageToStop);

		if (IsOwnerActorAuthoritative())
		{
			AnimMontage_UpdateReplicatedData();
		}
	}
}

void UDNAAbilitySystemComponent::ClearAnimatingAbility(UDNAAbility* Ability)
{
	if (LocalAnimMontageInfo.AnimatingAbility == Ability)
	{
		Ability->SetCurrentMontage(NULL);
		LocalAnimMontageInfo.AnimatingAbility = NULL;
	}
}

void UDNAAbilitySystemComponent::CurrentMontageJumpToSection(FName SectionName)
{
	UAnimInstance* AnimInstance = AbilityActorInfo.IsValid() ? AbilityActorInfo->GetAnimInstance() : nullptr;
	if ((SectionName != NAME_None) && AnimInstance && LocalAnimMontageInfo.AnimMontage)
	{
		AnimInstance->Montage_JumpToSection(SectionName, LocalAnimMontageInfo.AnimMontage);
		if (IsOwnerActorAuthoritative())
		{
			AnimMontage_UpdateReplicatedData();
		}
		else
		{
			ServerCurrentMontageJumpToSectionName(LocalAnimMontageInfo.AnimMontage, SectionName);
		}
	}
}

void UDNAAbilitySystemComponent::CurrentMontageSetNextSectionName(FName FromSectionName, FName ToSectionName)
{
	UAnimInstance* AnimInstance = AbilityActorInfo.IsValid() ? AbilityActorInfo->GetAnimInstance() : nullptr;
	if( LocalAnimMontageInfo.AnimMontage && AnimInstance )
	{
		// Set Next Section Name. 
		AnimInstance->Montage_SetNextSection(FromSectionName, ToSectionName, LocalAnimMontageInfo.AnimMontage);

		// Update replicated version for Simulated Proxies if we are on the server.
		if( IsOwnerActorAuthoritative() )
		{
			AnimMontage_UpdateReplicatedData();
		}
		else
		{
			float CurrentPosition = AnimInstance->Montage_GetPosition(LocalAnimMontageInfo.AnimMontage);
			ServerCurrentMontageSetNextSectionName(LocalAnimMontageInfo.AnimMontage, CurrentPosition, FromSectionName, ToSectionName);
		}
	}
}

void UDNAAbilitySystemComponent::CurrentMontageSetPlayRate(float InPlayRate)
{
	UAnimInstance* AnimInstance = AbilityActorInfo.IsValid() ? AbilityActorInfo->GetAnimInstance() : nullptr;
	if (LocalAnimMontageInfo.AnimMontage && AnimInstance)
	{
		// Set Play Rate
		AnimInstance->Montage_SetPlayRate(LocalAnimMontageInfo.AnimMontage, InPlayRate);

		// Update replicated version for Simulated Proxies if we are on the server.
		if (IsOwnerActorAuthoritative())
		{
			AnimMontage_UpdateReplicatedData();
		}
		else
		{
			ServerCurrentMontageSetPlayRate(LocalAnimMontageInfo.AnimMontage, InPlayRate);
		}
	}
}

bool UDNAAbilitySystemComponent::ServerCurrentMontageSetNextSectionName_Validate(UAnimMontage* ClientAnimMontage, float ClientPosition, FName SectionName, FName NextSectionName)
{
	return true;
}

void UDNAAbilitySystemComponent::ServerCurrentMontageSetNextSectionName_Implementation(UAnimMontage* ClientAnimMontage, float ClientPosition, FName SectionName, FName NextSectionName)
{
	UAnimInstance* AnimInstance = AbilityActorInfo.IsValid() ? AbilityActorInfo->GetAnimInstance() : nullptr;
	if (AnimInstance)
	{
		UAnimMontage* CurrentAnimMontage = LocalAnimMontageInfo.AnimMontage;
		if (ClientAnimMontage == CurrentAnimMontage)
		{
			// Set NextSectionName
			AnimInstance->Montage_SetNextSection(SectionName, NextSectionName, CurrentAnimMontage);

			// Correct position if we are in an invalid section
			float CurrentPosition = AnimInstance->Montage_GetPosition(CurrentAnimMontage);
			int32 CurrentSectionID = CurrentAnimMontage->GetSectionIndexFromPosition(CurrentPosition);
			FName CurrentSectionName = CurrentAnimMontage->GetSectionName(CurrentSectionID);

			int32 ClientSectionID = CurrentAnimMontage->GetSectionIndexFromPosition(ClientPosition);
			FName ClientCurrentSectionName = CurrentAnimMontage->GetSectionName(ClientSectionID);
			if ((CurrentSectionName != ClientCurrentSectionName) || (CurrentSectionName != SectionName) || (CurrentSectionName != NextSectionName))
			{
				// We are in an invalid section, jump to client's position.
				AnimInstance->Montage_SetPosition(CurrentAnimMontage, ClientPosition);
			}

			// Update replicated version for Simulated Proxies if we are on the server.
			if (IsOwnerActorAuthoritative())
			{
				AnimMontage_UpdateReplicatedData();
			}
		}
	}
}

bool UDNAAbilitySystemComponent::ServerCurrentMontageJumpToSectionName_Validate(UAnimMontage* ClientAnimMontage, FName SectionName)
{
	return true;
}

void UDNAAbilitySystemComponent::ServerCurrentMontageJumpToSectionName_Implementation(UAnimMontage* ClientAnimMontage, FName SectionName)
{
	UAnimInstance* AnimInstance = AbilityActorInfo.IsValid() ? AbilityActorInfo->GetAnimInstance() : nullptr;
	if (AnimInstance)
	{
		UAnimMontage* CurrentAnimMontage = LocalAnimMontageInfo.AnimMontage;
		if (ClientAnimMontage == CurrentAnimMontage)
		{
			// Set NextSectionName
			AnimInstance->Montage_JumpToSection(SectionName, CurrentAnimMontage);

			// Update replicated version for Simulated Proxies if we are on the server.
			if (IsOwnerActorAuthoritative())
			{
				AnimMontage_UpdateReplicatedData();
			}
		}
	}
}

bool UDNAAbilitySystemComponent::ServerCurrentMontageSetPlayRate_Validate(UAnimMontage* ClientAnimMontage, float InPlayRate)
{
	return true;
}

void UDNAAbilitySystemComponent::ServerCurrentMontageSetPlayRate_Implementation(UAnimMontage* ClientAnimMontage, float InPlayRate)
{
	UAnimInstance* AnimInstance = AbilityActorInfo.IsValid() ? AbilityActorInfo->GetAnimInstance() : nullptr;
	if (AnimInstance)
	{
		UAnimMontage* CurrentAnimMontage = LocalAnimMontageInfo.AnimMontage;
		if (ClientAnimMontage == CurrentAnimMontage)
		{
			// Set PlayRate
			AnimInstance->Montage_SetPlayRate(LocalAnimMontageInfo.AnimMontage, InPlayRate);

			// Update replicated version for Simulated Proxies if we are on the server.
			if (IsOwnerActorAuthoritative())
			{
				AnimMontage_UpdateReplicatedData();
			}
		}
	}
}

UAnimMontage* UDNAAbilitySystemComponent::GetCurrentMontage() const
{
	UAnimInstance* AnimInstance = AbilityActorInfo.IsValid() ? AbilityActorInfo->GetAnimInstance() : nullptr;
	if (LocalAnimMontageInfo.AnimMontage && AnimInstance && AnimInstance->Montage_IsActive(LocalAnimMontageInfo.AnimMontage))
	{
		return LocalAnimMontageInfo.AnimMontage;
	}

	return nullptr;
}

int32 UDNAAbilitySystemComponent::GetCurrentMontageSectionID() const
{
	UAnimInstance* AnimInstance = AbilityActorInfo.IsValid() ? AbilityActorInfo->GetAnimInstance() : nullptr;
	UAnimMontage* CurrentAnimMontage = GetCurrentMontage();

	if (CurrentAnimMontage && AnimInstance)
	{
		float MontagePosition = AnimInstance->Montage_GetPosition(CurrentAnimMontage);
		return CurrentAnimMontage->GetSectionIndexFromPosition(MontagePosition);
	}

	return INDEX_NONE;
}

FName UDNAAbilitySystemComponent::GetCurrentMontageSectionName() const
{
	UAnimInstance* AnimInstance = AbilityActorInfo.IsValid() ? AbilityActorInfo->GetAnimInstance() : nullptr;
	UAnimMontage* CurrentAnimMontage = GetCurrentMontage();

	if (CurrentAnimMontage && AnimInstance)
	{
		float MontagePosition = AnimInstance->Montage_GetPosition(CurrentAnimMontage);
		int32 CurrentSectionID = CurrentAnimMontage->GetSectionIndexFromPosition(MontagePosition);

		return CurrentAnimMontage->GetSectionName(CurrentSectionID);
	}

	return NAME_None;
}

float UDNAAbilitySystemComponent::GetCurrentMontageSectionLength() const
{
	UAnimInstance* AnimInstance = AbilityActorInfo.IsValid() ? AbilityActorInfo->GetAnimInstance() : nullptr;
	UAnimMontage* CurrentAnimMontage = GetCurrentMontage();
	if (CurrentAnimMontage && AnimInstance)
	{
		int32 CurrentSectionID = GetCurrentMontageSectionID();
		if (CurrentSectionID != INDEX_NONE)
		{
			TArray<FCompositeSection>& CompositeSections = CurrentAnimMontage->CompositeSections;

			// If we have another section after us, then take delta between both start times.
			if (CurrentSectionID < (CompositeSections.Num() - 1))
			{
				return (CompositeSections[CurrentSectionID + 1].GetTime() - CompositeSections[CurrentSectionID].GetTime());
			}
			// Otherwise we are the last section, so take delta with Montage total time.
			else
			{
				return (CurrentAnimMontage->SequenceLength - CompositeSections[CurrentSectionID].GetTime());
			}
		}

		// if we have no sections, just return total length of Montage.
		return CurrentAnimMontage->SequenceLength;
	}

	return 0.f;
}

float UDNAAbilitySystemComponent::GetCurrentMontageSectionTimeLeft() const
{
	UAnimInstance* AnimInstance = AbilityActorInfo.IsValid() ? AbilityActorInfo->GetAnimInstance() : nullptr;
	UAnimMontage* CurrentAnimMontage = GetCurrentMontage();
	if (CurrentAnimMontage && AnimInstance && AnimInstance->Montage_IsActive(CurrentAnimMontage))
	{
		const float CurrentPosition = AnimInstance->Montage_GetPosition(CurrentAnimMontage);
		return CurrentAnimMontage->GetSectionTimeLeftFromPos(CurrentPosition);
	}

	return -1.f;
}

bool UDNAAbilitySystemComponent::IsAnimatingAbility(UDNAAbility* InAbility) const
{
	return (LocalAnimMontageInfo.AnimatingAbility == InAbility);
}

UDNAAbility* UDNAAbilitySystemComponent::GetAnimatingAbility()
{
	return LocalAnimMontageInfo.AnimatingAbility;
}

// -----------------------------------------------------------------------------



void UDNAAbilitySystemComponent::ConfirmAbilityTargetData(FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey, const FDNAAbilityTargetDataHandle& TargetData, const FDNATag& ApplicationTag)
{
	FAbilityReplicatedDataCache* CachedData = AbilityTargetDataMap.Find(FDNAAbilitySpecHandleAndPredictionKey(AbilityHandle, AbilityOriginalPredictionKey));
	if (CachedData)
	{
		CachedData->TargetSetDelegate.Broadcast(TargetData, ApplicationTag);
	}
}

void UDNAAbilitySystemComponent::CancelAbilityTargetData(FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey)
{
	FAbilityReplicatedDataCache* CachedData = AbilityTargetDataMap.Find(FDNAAbilitySpecHandleAndPredictionKey(AbilityHandle, AbilityOriginalPredictionKey));
	if (CachedData)
	{
		CachedData->Reset();
		CachedData->TargetCancelledDelegate.Broadcast();
	}
}

void UDNAAbilitySystemComponent::ConsumeAllReplicatedData(FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey)
{
	FAbilityReplicatedDataCache* CachedData = AbilityTargetDataMap.Find(FDNAAbilitySpecHandleAndPredictionKey(AbilityHandle, AbilityOriginalPredictionKey));
	if (CachedData)
	{
		CachedData->Reset();
	}
}

void UDNAAbilitySystemComponent::ConsumeClientReplicatedTargetData(FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey)
{
	FAbilityReplicatedDataCache* CachedData = AbilityTargetDataMap.Find(FDNAAbilitySpecHandleAndPredictionKey(AbilityHandle, AbilityOriginalPredictionKey));
	if (CachedData)
	{
		CachedData->TargetData.Clear();
		CachedData->bTargetConfirmed = false;
		CachedData->bTargetCancelled = false;
	}
}

void UDNAAbilitySystemComponent::ConsumeGenericReplicatedEvent(EAbilityGenericReplicatedEvent::Type EventType, FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey)
{
	FAbilityReplicatedDataCache* CachedData = AbilityTargetDataMap.Find(FDNAAbilitySpecHandleAndPredictionKey(AbilityHandle, AbilityOriginalPredictionKey));
	if (CachedData)
	{
		CachedData->GenericEvents[EventType].bTriggered = false;
	}
}

FAbilityReplicatedData UDNAAbilitySystemComponent::GetReplicatedDataOfGenericReplicatedEvent(EAbilityGenericReplicatedEvent::Type EventType, FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey)
{
	FAbilityReplicatedData ReturnData;

	FAbilityReplicatedDataCache* CachedData = AbilityTargetDataMap.Find(FDNAAbilitySpecHandleAndPredictionKey(AbilityHandle, AbilityOriginalPredictionKey));
	if (CachedData)
	{
		ReturnData.bTriggered = CachedData->GenericEvents[EventType].bTriggered;
		ReturnData.VectorPayload = CachedData->GenericEvents[EventType].VectorPayload;
	}

	return ReturnData;
}

// --------------------------------------------------------------------------

void UDNAAbilitySystemComponent::ServerSetReplicatedEvent_Implementation(EAbilityGenericReplicatedEvent::Type EventType, FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey,  FPredictionKey CurrentPredictionKey)
{
	FScopedPredictionWindow ScopedPrediction(this, CurrentPredictionKey);

	InvokeReplicatedEvent(EventType, AbilityHandle, AbilityOriginalPredictionKey, CurrentPredictionKey);
}

void UDNAAbilitySystemComponent::ServerSetReplicatedEventWithPayload_Implementation(EAbilityGenericReplicatedEvent::Type EventType, FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey,  FPredictionKey CurrentPredictionKey, FVector_NetQuantize100 VectorPayload)
{
	FScopedPredictionWindow ScopedPrediction(this, CurrentPredictionKey);

	InvokeReplicatedEventWithPayload(EventType, AbilityHandle, AbilityOriginalPredictionKey, CurrentPredictionKey, VectorPayload);
}

bool UDNAAbilitySystemComponent::InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::Type EventType, FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey, FPredictionKey CurrentPredictionKey)
{
	FAbilityReplicatedDataCache& ReplicatedData = AbilityTargetDataMap.FindOrAdd(FDNAAbilitySpecHandleAndPredictionKey(AbilityHandle, AbilityOriginalPredictionKey));
	ReplicatedData.GenericEvents[(uint8)EventType].bTriggered = true;
	ReplicatedData.PredictionKey = CurrentPredictionKey;

	if (ReplicatedData.GenericEvents[EventType].Delegate.IsBound())
	{
		ReplicatedData.GenericEvents[EventType].Delegate.Broadcast();
		return true;
	}
	else
	{
		return false;
	}
}

bool UDNAAbilitySystemComponent::InvokeReplicatedEventWithPayload(EAbilityGenericReplicatedEvent::Type EventType, FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey, FPredictionKey CurrentPredictionKey, FVector_NetQuantize100 VectorPayload)
{
	FAbilityReplicatedDataCache& ReplicatedData = AbilityTargetDataMap.FindOrAdd(FDNAAbilitySpecHandleAndPredictionKey(AbilityHandle, AbilityOriginalPredictionKey));
	ReplicatedData.GenericEvents[(uint8)EventType].bTriggered = true;
	ReplicatedData.GenericEvents[(uint8)EventType].VectorPayload = VectorPayload;
	ReplicatedData.PredictionKey = CurrentPredictionKey;

	if (ReplicatedData.GenericEvents[EventType].Delegate.IsBound())
	{
		ReplicatedData.GenericEvents[EventType].Delegate.Broadcast();
		return true;
	}
	else
	{
		return false;
	}
}

bool UDNAAbilitySystemComponent::ServerSetReplicatedEvent_Validate(EAbilityGenericReplicatedEvent::Type EventType, FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey,  FPredictionKey CurrentPredictionKey)
{
	if (EventType >= EAbilityGenericReplicatedEvent::MAX)
	{
		return false;
	}
	return true;
}

bool UDNAAbilitySystemComponent::ServerSetReplicatedEventWithPayload_Validate(EAbilityGenericReplicatedEvent::Type EventType, FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey,  FPredictionKey CurrentPredictionKey, FVector_NetQuantize100 VectorPayload)
{
	if (EventType >= EAbilityGenericReplicatedEvent::MAX)
	{
		return false;
	}
	return true;
}

// -------

void UDNAAbilitySystemComponent::ClientSetReplicatedEvent_Implementation(EAbilityGenericReplicatedEvent::Type EventType, FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey)
{
	InvokeReplicatedEvent(EventType, AbilityHandle, AbilityOriginalPredictionKey, ScopedPredictionKey);
}

// -------

void UDNAAbilitySystemComponent::ServerSetReplicatedTargetData_Implementation(FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey, const FDNAAbilityTargetDataHandle& ReplicatedTargetDataHandle, FDNATag ApplicationTag, FPredictionKey CurrentPredictionKey)
{
	FScopedPredictionWindow ScopedPrediction(this, CurrentPredictionKey);

	// Always adds to cache to store the new data
	FAbilityReplicatedDataCache& ReplicatedData = AbilityTargetDataMap.FindOrAdd(FDNAAbilitySpecHandleAndPredictionKey(AbilityHandle, AbilityOriginalPredictionKey));

	if (ReplicatedData.TargetData.Num() > 0)
	{
		FDNAAbilitySpec* Spec = FindAbilitySpecFromHandle(AbilityHandle);
		if (Spec && Spec->Ability)
		{
			// Can happen under normal circumstances if ServerForceClientTargetData is hit
			ABILITY_LOG(Display, TEXT("Ability %s is overriding pending replicated target data."), *Spec->Ability->GetName());
		}
	}

	ReplicatedData.TargetData = ReplicatedTargetDataHandle;
	ReplicatedData.ApplicationTag = ApplicationTag;
	ReplicatedData.bTargetConfirmed = true;
	ReplicatedData.bTargetCancelled = false;
	ReplicatedData.PredictionKey = CurrentPredictionKey;
	ReplicatedData.TargetSetDelegate.Broadcast(ReplicatedData.TargetData, ReplicatedData.ApplicationTag);
}

bool UDNAAbilitySystemComponent::ServerSetReplicatedTargetData_Validate(FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey, const FDNAAbilityTargetDataHandle& ReplicatedTargetDataHandle, FDNATag ApplicationTag, FPredictionKey CurrentPredictionKey)
{
	return true;
}

// -------

void UDNAAbilitySystemComponent::ServerSetReplicatedTargetDataCancelled_Implementation(FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey, FPredictionKey CurrentPredictionKey)
{
	FScopedPredictionWindow ScopedPrediction(this, CurrentPredictionKey);

	// Always adds to cache to store the new data
	FAbilityReplicatedDataCache& ReplicatedData = AbilityTargetDataMap.FindOrAdd(FDNAAbilitySpecHandleAndPredictionKey(AbilityHandle, AbilityOriginalPredictionKey));

	ReplicatedData.Reset();
	ReplicatedData.bTargetCancelled = true;
	ReplicatedData.PredictionKey = CurrentPredictionKey;
	ReplicatedData.TargetCancelledDelegate.Broadcast();
}

bool UDNAAbilitySystemComponent::ServerSetReplicatedTargetDataCancelled_Validate(FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey, FPredictionKey CurrentPredictionKey)
{
	return true;
}

void UDNAAbilitySystemComponent::CallAllReplicatedDelegatesIfSet(FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey)
{
	FAbilityReplicatedDataCache* CachedData = AbilityTargetDataMap.Find(FDNAAbilitySpecHandleAndPredictionKey(AbilityHandle, AbilityOriginalPredictionKey));
	if (CachedData)
	{
		FScopedPredictionWindow ScopedWindow(this, CachedData->PredictionKey, false);
		if (CachedData->bTargetConfirmed)
		{
			CachedData->TargetSetDelegate.Broadcast(CachedData->TargetData, CachedData->ApplicationTag);
		}
		else if (CachedData->bTargetCancelled)
		{
			CachedData->TargetCancelledDelegate.Broadcast();
		}

		for (int32 idx=0; idx < EAbilityGenericReplicatedEvent::MAX; ++idx)
		{
			if (CachedData->GenericEvents[idx].bTriggered)
			{
				CachedData->GenericEvents[idx].Delegate.Broadcast();
			}
		}
	}
}

bool UDNAAbilitySystemComponent::CallReplicatedTargetDataDelegatesIfSet(FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey)
{
	bool CalledDelegate = false;
	FAbilityReplicatedDataCache* CachedData = AbilityTargetDataMap.Find(FDNAAbilitySpecHandleAndPredictionKey(AbilityHandle, AbilityOriginalPredictionKey));
	if (CachedData)
	{
		// Use prediction key that was sent to us
		FScopedPredictionWindow ScopedWindow(this, CachedData->PredictionKey, false);

		if (CachedData->bTargetConfirmed)
		{
			CachedData->TargetSetDelegate.Broadcast(CachedData->TargetData, CachedData->ApplicationTag);
			CalledDelegate = true;
		}
		else if (CachedData->bTargetCancelled)
		{
			CachedData->TargetCancelledDelegate.Broadcast();
			CalledDelegate = true;
		}
	}

	return CalledDelegate;
}

bool UDNAAbilitySystemComponent::CallReplicatedEventDelegateIfSet(EAbilityGenericReplicatedEvent::Type EventType, FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey)
{
	FAbilityReplicatedDataCache* CachedData = AbilityTargetDataMap.Find(FDNAAbilitySpecHandleAndPredictionKey(AbilityHandle, AbilityOriginalPredictionKey));
	if (CachedData && CachedData->GenericEvents[EventType].bTriggered)
	{
		FScopedPredictionWindow ScopedWindow(this, CachedData->PredictionKey, false);

		// Already triggered, fire off delegate
		CachedData->GenericEvents[EventType].Delegate.Broadcast();
		return true;
	}
	return false;
}

bool UDNAAbilitySystemComponent::CallOrAddReplicatedDelegate(EAbilityGenericReplicatedEvent::Type EventType, FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey, FSimpleMulticastDelegate::FDelegate Delegate)
{
	FAbilityReplicatedDataCache& CachedData = AbilityTargetDataMap.FindOrAdd(FDNAAbilitySpecHandleAndPredictionKey(AbilityHandle, AbilityOriginalPredictionKey));
	if (CachedData.GenericEvents[EventType].bTriggered)
	{
		FScopedPredictionWindow ScopedWindow(this, CachedData.PredictionKey, false);

		// Already triggered, fire off delegate
		Delegate.Execute();
		return true;
	}
	
	// Not triggered yet, so just add the delegate
	CachedData.GenericEvents[EventType].Delegate.Add(Delegate);
	return false;
}

FAbilityTargetDataSetDelegate& UDNAAbilitySystemComponent::AbilityTargetDataSetDelegate(FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey)
{
	return AbilityTargetDataMap.FindOrAdd(FDNAAbilitySpecHandleAndPredictionKey(AbilityHandle, AbilityOriginalPredictionKey)).TargetSetDelegate;
}

FSimpleMulticastDelegate& UDNAAbilitySystemComponent::AbilityTargetDataCancelledDelegate(FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey)
{
	return AbilityTargetDataMap.FindOrAdd(FDNAAbilitySpecHandleAndPredictionKey(AbilityHandle, AbilityOriginalPredictionKey)).TargetCancelledDelegate;
}

FSimpleMulticastDelegate& UDNAAbilitySystemComponent::AbilityReplicatedEventDelegate(EAbilityGenericReplicatedEvent::Type EventType, FDNAAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey)
{
	return AbilityTargetDataMap.FindOrAdd(FDNAAbilitySpecHandleAndPredictionKey(AbilityHandle, AbilityOriginalPredictionKey)).GenericEvents[EventType].Delegate;
}


#undef LOCTEXT_NAMESPACE

