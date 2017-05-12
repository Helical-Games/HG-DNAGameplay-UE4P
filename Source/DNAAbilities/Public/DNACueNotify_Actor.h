// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "DNATagContainer.h"
#include "DNACue_Types.h"
#include "DNACueNotify_Actor.generated.h"

/**
 *	An instantiated Actor that acts as a handler of a DNACue. Since they are instantiated, they can maintain state and tick/update every frame if necessary. 
 */

UCLASS(Blueprintable, meta = (ShowWorldContextPin), hidecategories = (Replication))
class DNAABILITIES_API ADNACueNotify_Actor : public AActor
{
	GENERATED_UCLASS_BODY()

	/** Does this DNACueNotify handle this type of DNACueEvent? */
	virtual bool HandlesEvent(EDNACueEvent::Type EventType) const;

	UFUNCTION()
	virtual void OnOwnerDestroyed(AActor* DestroyedActor);

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	virtual void BeginPlay() override;

public:
	virtual void SetOwner( AActor* NewOwner ) override;

	virtual void PostInitProperties() override;

	virtual void Serialize(FArchive& Ar) override;

	virtual void HandleDNACue(AActor* MyTarget, EDNACueEvent::Type EventType, const FDNACueParameters& Parameters);

	/** Called when the GC is finished. It may be about to go back to the recyle pool, or it may be about to be destroyed. */
	virtual void DNACueFinishedCallback();

	virtual bool DNACuePendingRemove();

	/** Called when returning to the recycled pool. Reset all state so that it can be reused. Return false if this class cannot be recycled. */
	virtual bool Recycle();

	/** Called when we are about to reuse the GC. Should undo anything done in Recycle like hiding the actor */
	virtual void ReuseAfterRecycle();

	/** Ends the DNA cue: either destroying it or recycling it. You must call this manually only if you do not use bAutoDestroyOnRemove/AutoDestroyDelay  */
	UFUNCTION(BlueprintCallable, Category="DNACueNotify", DisplayName="End (Recycle) DNACue")
	virtual void K2_EndDNACue();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

	/** We will auto destroy (recycle) this DNACueActor when the OnRemove event fires (after OnRemove is called). */
	UPROPERTY(EditDefaultsOnly, Category = Cleanup)
	bool bAutoDestroyOnRemove;

	/** If bAutoDestroyOnRemove is true, the actor will stay alive for this many seconds before being auto destroyed. */
	UPROPERTY(EditAnywhere, Category = Cleanup)
	float AutoDestroyDelay;

	/** Generic Event Graph event that will get called for every event type */
	UFUNCTION(BlueprintImplementableEvent, Category = "DNACueNotify", DisplayName = "HandleDNACue")
	void K2_HandleDNACue(AActor* MyTarget, EDNACueEvent::Type EventType, const FDNACueParameters& Parameters);

	UFUNCTION(BlueprintNativeEvent, Category = "DNACueNotify")
	bool OnExecute(AActor* MyTarget, const FDNACueParameters& Parameters);

	UFUNCTION(BlueprintNativeEvent, Category = "DNACueNotify")
	bool OnActive(AActor* MyTarget, const FDNACueParameters& Parameters);

	UFUNCTION(BlueprintNativeEvent, Category = "DNACueNotify")
	bool WhileActive(AActor* MyTarget, const FDNACueParameters& Parameters);

	UFUNCTION(BlueprintNativeEvent, Category = "DNACueNotify")
	bool OnRemove(AActor* MyTarget, const FDNACueParameters& Parameters);

	UPROPERTY(EditDefaultsOnly, Category=DNACue, meta=(Categories="DNACue"))
	FDNATag	DNACueTag;

	/** Mirrors DNACueTag in order to be asset registry searchable */
	UPROPERTY(AssetRegistrySearchable)
	FName DNACueName;

	/** If true, attach this DNACue Actor to the target actor while it is active. Attaching is slightly more expensive than not attaching, so only enable when you need to. */
	UPROPERTY(EditDefaultsOnly, Category = DNACue)
	bool bAutoAttachToOwner;

	/** Does this Cue override other cues, or is it called in addition to them? E.g., If this is Damage.Physical.Slash, we wont call Damage.Physical afer we run this cue. */
	UPROPERTY(EditDefaultsOnly, Category = DNACue)
	bool IsOverride;

	/**
	 *	Does this cue get a new instance for each instigator? For example if two instigators apply a GC to the same source, do we create two of these DNACue Notify actors or just one?
	 *	If the notify is simply playing FX or sounds on the source, it should not need unique instances. If this Notify is attaching a beam from the instigator to the target, it does need a unique instance per instigator.
	 */ 	 
	UPROPERTY(EditDefaultsOnly, Category = DNACue)
	bool bUniqueInstancePerInstigator;

	/**
	 *	Does this cue get a new instance for each source object? For example if two source objects apply a GC to the same source, do we create two of these DNACue Notify actors or just one?
	 *	If the notify is simply playing FX or sounds on the source, it should not need unique instances. If this Notify is attaching a beam from the source object to the target, it does need a unique instance per instigator.
	 */ 	 
	UPROPERTY(EditDefaultsOnly, Category = DNACue)
	bool bUniqueInstancePerSourceObject;

	/**
	 *	Does this cue trigger its OnActive event if it's already been triggered?
	 *  This can occur when the associated tag is triggered by multiple sources and there is no unique instancing.
	 */ 	 
	UPROPERTY(EditDefaultsOnly, Category = DNACue)
	bool bAllowMultipleOnActiveEvents;

	/**
	 *	Does this cue trigger its WhileActive event if it's already been triggered?
	 *  This can occur when the associated tag is triggered by multiple sources and there is no unique instancing.
	 */ 	 
	UPROPERTY(EditDefaultsOnly, Category = DNACue)
	bool bAllowMultipleWhileActiveEvents;

	/** How many instances of the DNA cue to preallocate */
	UPROPERTY(EditDefaultsOnly, Category = DNACue)
	int32 NumPreallocatedInstances;

	FGCNotifyActorKey NotifyKey;

	// Set when the GC actor is in the recycle queue (E.g., not active in world. This is to prevent rentrancy in the recyle code since multiple paths can lead the GC actor there)
	bool bInRecycleQueue;
	
protected:
	FTimerHandle FinishTimerHandle;

	void ClearOwnerDestroyedDelegate();

	bool bHasHandledOnActiveEvent;
	bool bHasHandledWhileActiveEvent;
	bool bHasHandledOnRemoveEvent;

private:
	virtual void DeriveDNACueTagFromAssetName();

	void AttachToOwnerIfNecessary();

};
