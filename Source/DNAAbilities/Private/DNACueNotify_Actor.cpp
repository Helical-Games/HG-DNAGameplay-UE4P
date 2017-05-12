// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNACueNotify_Actor.h"
#include "TimerManager.h"
#include "Engine/Blueprint.h"
#include "Components/TimelineComponent.h"
#include "AbilitySystemStats.h"
#include "AbilitySystemGlobals.h"
#include "DNACueManager.h"

ADNACueNotify_Actor::ADNACueNotify_Actor(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	IsOverride = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bAutoDestroyOnRemove = false;
	AutoDestroyDelay = 0.f;
	bUniqueInstancePerSourceObject = false;
	bUniqueInstancePerInstigator = false;
	bAllowMultipleOnActiveEvents = true;
	bAllowMultipleWhileActiveEvents = true;
	bHasHandledOnRemoveEvent = false;

	NumPreallocatedInstances = 0;

	bHasHandledOnActiveEvent = false;
	bHasHandledWhileActiveEvent = false;
	bInRecycleQueue = false;
	bAutoAttachToOwner = false;
}

void ADNACueNotify_Actor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (EndPlayReason == EEndPlayReason::Destroyed)
	{
		UDNAAbilitySystemGlobals::Get().GetDNACueManager()->NotifyDNACueActorEndPlay(this);
	}

	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
void ADNACueNotify_Actor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	UBlueprint* Blueprint = UBlueprint::GetBlueprintFromClass(GetClass());

	if (PropertyThatChanged && PropertyThatChanged->GetFName() == FName(TEXT("DNACueTag")))
	{
		DeriveDNACueTagFromAssetName();
		UDNAAbilitySystemGlobals::Get().GetDNACueManager()->HandleAssetDeleted(Blueprint);
		UDNAAbilitySystemGlobals::Get().GetDNACueManager()->HandleAssetAdded(Blueprint);
	}
}
#endif

void ADNACueNotify_Actor::DeriveDNACueTagFromAssetName()
{
	UDNAAbilitySystemGlobals::DeriveDNACueTagFromClass<ADNACueNotify_Actor>(this);
}

void ADNACueNotify_Actor::Serialize(FArchive& Ar)
{
	if (Ar.IsSaving())
	{
		DeriveDNACueTagFromAssetName();
	}

	Super::Serialize(Ar);

	if (Ar.IsLoading())
	{
		DeriveDNACueTagFromAssetName();
	}
}

void ADNACueNotify_Actor::BeginPlay()
{
	Super::BeginPlay();
	AttachToOwnerIfNecessary();
}

void ADNACueNotify_Actor::SetOwner( AActor* InNewOwner )
{
	// Remove our old delegate
	ClearOwnerDestroyedDelegate();

	Super::SetOwner(InNewOwner);
	if (AActor* NewOwner = GetOwner())
	{
		NewOwner->OnDestroyed.AddDynamic(this, &ADNACueNotify_Actor::OnOwnerDestroyed);
		AttachToOwnerIfNecessary();
	}
}

void ADNACueNotify_Actor::AttachToOwnerIfNecessary()
{
	if (AActor* MyOwner = GetOwner())
	{
		if (bAutoAttachToOwner)
		{
			AttachToActor(MyOwner, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		}
	}
}

void ADNACueNotify_Actor::ClearOwnerDestroyedDelegate()
{
	AActor* OldOwner = GetOwner();
	if (OldOwner)
	{
		OldOwner->OnDestroyed.RemoveDynamic(this, &ADNACueNotify_Actor::OnOwnerDestroyed);
	}
}

void ADNACueNotify_Actor::PostInitProperties()
{
	Super::PostInitProperties();
	DeriveDNACueTagFromAssetName();
}

bool ADNACueNotify_Actor::HandlesEvent(EDNACueEvent::Type EventType) const
{
	return true;
}

void ADNACueNotify_Actor::K2_EndDNACue()
{
	DNACueFinishedCallback();
}

int32 DNACueNotifyTagCheckOnRemove = 1;
static FAutoConsoleVariableRef CVarDNACueNotifyActorStacking(TEXT("DNAAbilitySystem.DNACueNotifyTagCheckOnRemove"), DNACueNotifyTagCheckOnRemove, TEXT("Check that target no longer has tag when removing GamepalyCues"), ECVF_Default );

void ADNACueNotify_Actor::HandleDNACue(AActor* MyTarget, EDNACueEvent::Type EventType, const FDNACueParameters& Parameters)
{
	SCOPE_CYCLE_COUNTER(STAT_HandleDNACueNotifyActor);

	if (Parameters.MatchedTagName.IsValid() == false)
	{
		ABILITY_LOG(Warning, TEXT("DNACue parameter is none for %s"), *GetNameSafe(this));
	}

	// Handle multiple event gating
	{
		if (EventType == EDNACueEvent::OnActive && !bAllowMultipleOnActiveEvents && bHasHandledOnActiveEvent)
		{
			return;
		}

		if (EventType == EDNACueEvent::WhileActive && !bAllowMultipleWhileActiveEvents && bHasHandledWhileActiveEvent)
		{
			return;
		}

		if (EventType == EDNACueEvent::Removed && bHasHandledOnRemoveEvent)
		{
			return;
		}
	}

	// If cvar is enabled, check that the target no longer has the matched tag before doing remove logic. This is a simple way of supporting stacking, such that if an actor has two sources giving him the same GC tag, it will not be removed when the first one is removed.
	if (DNACueNotifyTagCheckOnRemove > 0 && EventType == EDNACueEvent::Removed)
	{
		if (IDNATagAssetInterface* TagInterface = Cast<IDNATagAssetInterface>(MyTarget))
		{
			if (TagInterface->HasMatchingDNATag(Parameters.MatchedTagName))
			{
				return;
			}			
		}
	}

	if (MyTarget && !MyTarget->IsPendingKill())
	{
		K2_HandleDNACue(MyTarget, EventType, Parameters);

		// Clear any pending auto-destroy that may have occurred from a previous OnRemove
		SetLifeSpan(0.f);

		switch (EventType)
		{
		case EDNACueEvent::OnActive:
			OnActive(MyTarget, Parameters);
			bHasHandledOnActiveEvent = true;
			break;

		case EDNACueEvent::WhileActive:
			WhileActive(MyTarget, Parameters);
			bHasHandledWhileActiveEvent = true;
			break;

		case EDNACueEvent::Executed:
			OnExecute(MyTarget, Parameters);
			break;

		case EDNACueEvent::Removed:
			bHasHandledOnRemoveEvent = true;
			OnRemove(MyTarget, Parameters);

			if (bAutoDestroyOnRemove)
			{
				if (AutoDestroyDelay > 0.f)
				{
					FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this, &ADNACueNotify_Actor::DNACueFinishedCallback);
					GetWorld()->GetTimerManager().SetTimer(FinishTimerHandle, Delegate, AutoDestroyDelay, false);
				}
				else
				{
					DNACueFinishedCallback();
				}
			}
			break;
		};
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("Null Target called for event %d on DNACueNotifyActor %s"), (int32)EventType, *GetName() );
		if (EventType == EDNACueEvent::Removed)
		{
			// Make sure the removed event is handled so that we don't leak GC notify actors
			DNACueFinishedCallback();
		}
	}
}

void ADNACueNotify_Actor::OnOwnerDestroyed(AActor* DestroyedActor)
{
	if (bInRecycleQueue)
	{
		// We are already done
		return;
	}

	// May need to do extra cleanup in child classes
	DNACueFinishedCallback();
}

bool ADNACueNotify_Actor::OnExecute_Implementation(AActor* MyTarget, const FDNACueParameters& Parameters)
{
	return false;
}

bool ADNACueNotify_Actor::OnActive_Implementation(AActor* MyTarget, const FDNACueParameters& Parameters)
{
	return false;
}

bool ADNACueNotify_Actor::WhileActive_Implementation(AActor* MyTarget, const FDNACueParameters& Parameters)
{
	return false;
}

bool ADNACueNotify_Actor::OnRemove_Implementation(AActor* MyTarget, const FDNACueParameters& Parameters)
{
	return false;
}

void ADNACueNotify_Actor::DNACueFinishedCallback()
{
	UWorld* MyWorld = GetWorld();
	if (MyWorld) // Teardown cases in PIE may cause the world to be invalid
	{
		if (FinishTimerHandle.IsValid())
		{
			MyWorld->GetTimerManager().ClearTimer(FinishTimerHandle);
			FinishTimerHandle.Invalidate();
		}

		// Make sure OnRemoved has been called at least once if WhileActive was called (for possible cleanup)
		if (bHasHandledWhileActiveEvent && !bHasHandledOnRemoveEvent)
		{
			// Force onremove to be called with null parameters
			bHasHandledOnRemoveEvent = true;
			OnRemove(nullptr, FDNACueParameters());
		}
	}
	
	UDNAAbilitySystemGlobals::Get().GetDNACueManager()->NotifyDNACueActorFinished(this);
}

bool ADNACueNotify_Actor::DNACuePendingRemove()
{
	return GetLifeSpan() > 0.f || FinishTimerHandle.IsValid() || IsPendingKill();
}

bool ADNACueNotify_Actor::Recycle()
{
	bHasHandledOnActiveEvent = false;
	bHasHandledWhileActiveEvent = false;
	bHasHandledOnRemoveEvent = false;
	ClearOwnerDestroyedDelegate();
	if (FinishTimerHandle.IsValid())
	{
		FinishTimerHandle.Invalidate();
	}

	// End timeline components
	TInlineComponentArray<UTimelineComponent*> TimelineComponents(this);
	for (UTimelineComponent* Timeline : TimelineComponents)
	{
		if (Timeline)
		{
			// May be too spammy, but want to call visibility to this. Maybe make this editor only?
			if (Timeline->IsPlaying())
			{
				ABILITY_LOG(Warning, TEXT("DNACueNotify_Actor %s had active timelines when it was recycled."), *GetName());
			}

			Timeline->SetPlaybackPosition(0.f, false, false);
			Timeline->Stop();
		}
	}

	UWorld* MyWorld = GetWorld();
	if (MyWorld)
	{
		// Note, ::Recycle is called on CDOs too, so that even "new" GCs start off in a recycled state.
		// So, its ok if there is no valid world here, just skip the stuff that has to do with worlds.
		if (MyWorld->GetLatentActionManager().GetNumActionsForObject(this))
		{
			// May be too spammy, but want ot call visibility to this. Maybe make this editor only?
			ABILITY_LOG(Warning, TEXT("DNACueNotify_Actor %s has active latent actions (Delays, etc) when it was recycled."), *GetName());
		}

		// End latent actions
		MyWorld->GetLatentActionManager().RemoveActionsForObject(this);

		// End all timers
		MyWorld->GetTimerManager().ClearAllTimersForObject(this);
	}

	// Clear owner, hide, detach from parent
	SetOwner(nullptr);
	SetActorHiddenInGame(true);
	DetachRootComponentFromParent();

	return true;
}

void ADNACueNotify_Actor::ReuseAfterRecycle()
{
	SetActorHiddenInGame(false);
}
