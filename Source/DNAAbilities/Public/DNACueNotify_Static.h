// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "DNATagContainer.h"
#include "DNAEffectTypes.h"
#include "DNACueNotify_Static.generated.h"

/**
 *	A non instantiated UObject that acts as a handler for a DNACue. These are useful for one-off "burst" effects.
 */

UCLASS(Blueprintable, meta = (ShowWorldContextPin), hidecategories = (Replication))
class DNAABILITIES_API UDNACueNotify_Static : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Does this DNACueNotify handle this type of DNACueEvent? */
	virtual bool HandlesEvent(EDNACueEvent::Type EventType) const;

	virtual void OnOwnerDestroyed();

	virtual void PostInitProperties() override;

	virtual void Serialize(FArchive& Ar) override;

	virtual void HandleDNACue(AActor* MyTarget, EDNACueEvent::Type EventType, const FDNACueParameters& Parameters);

	UWorld* GetWorld() const override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

	/** Generic Event Graph event that will get called for every event type */
	UFUNCTION(BlueprintImplementableEvent, Category = "DNACueNotify", DisplayName = "HandleDNACue")
	void K2_HandleDNACue(AActor* MyTarget, EDNACueEvent::Type EventType, const FDNACueParameters& Parameters) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "DNACueNotify")
	bool OnExecute(AActor* MyTarget, const FDNACueParameters& Parameters) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "DNACueNotify")
	bool OnActive(AActor* MyTarget, const FDNACueParameters& Parameters) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "DNACueNotify")
	bool WhileActive(AActor* MyTarget, const FDNACueParameters& Parameters) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "DNACueNotify")
	bool OnRemove(AActor* MyTarget, const FDNACueParameters& Parameters) const;

	UPROPERTY(EditDefaultsOnly, Category = DNACue, meta=(Categories="DNACue"))
	FDNATag	DNACueTag;

	/** Mirrors DNACueTag in order to be asset registry searchable */
	UPROPERTY(AssetRegistrySearchable)
	FName DNACueName;

	/** Does this Cue override other cues, or is it called in addition to them? E.g., If this is Damage.Physical.Slash, we wont call Damage.Physical afer we run this cue. */
	UPROPERTY(EditDefaultsOnly, Category = DNACue)
	bool IsOverride;

private:
	virtual void DeriveDNACueTagFromAssetName();
};
