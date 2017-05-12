// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "DNATagContainer.h"
#include "Engine/DataAsset.h"
#include "DNAEffectTypes.h"
#include "DNAEffect.h"
#include "DNATagResponseTable.generated.h"

class UDNAAbilitySystemComponent;

USTRUCT()
struct FDNATagReponsePair
{
	GENERATED_USTRUCT_BODY()

	/** Tag that triggers this response */
	UPROPERTY(EditAnywhere, Category="Response")
	FDNATag	Tag;
	
	/** Deprecated. Replaced with ResponseDNAEffects */
	UPROPERTY()
	TSubclassOf<UDNAEffect> ResponseDNAEffect;

	/** The DNAEffects to apply in reponse to the tag */
	UPROPERTY(EditAnywhere, Category="Response")
	TArray<TSubclassOf<UDNAEffect> > ResponseDNAEffects;

	/** The max "count" this response can achieve. This will not prevent counts from being applied, but will be used when calculating the net count of a tag. 0=no cap. */
	UPROPERTY(EditAnywhere, Category="Response", meta=(ClampMin = "0"))
	int32 SoftCountCap;
};

USTRUCT()
struct FDNATagResponseTableEntry
{
	GENERATED_USTRUCT_BODY()

	/** Tags that count as "positive" toward to final response count. If the overall count is positive, this ResponseDNAEffect is applied. */
	UPROPERTY(EditAnywhere, Category="Response")
	FDNATagReponsePair		Positive;

	/** Tags that count as "negative" toward to final response count. If the overall count is negative, this ResponseDNAEffect is applied. */
	UPROPERTY(EditAnywhere, Category="Response")
	FDNATagReponsePair		Negative;
};

/**
 *	A data driven table for applying DNA effects based on tag count. This allows designers to map a 
 *	"tag count" -> "response DNA Effect" relationship.
 *	
 *	For example, "for every count of "Status.Haste" I get 1 level of GE_Response_Haste. This class facilitates
 *	building this table and automatically registering and responding to tag events on the ability system component.
 */
UCLASS()
class DNAABILITIES_API UDNATagReponseTable : public UDataAsset
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category="Response")
	TArray<FDNATagResponseTableEntry>	Entries;

	/** Registers for tag events for the given ability system component. Note this will happen to every spawned ASC, we may want to allow games
	 *	to limit what classe this is called on, or potentially build into the table class restrictions for each response entry.
	 */
	void RegisterResponseForEvents(UDNAAbilitySystemComponent* ASC);

	virtual void PostLoad() override;

protected:

	UFUNCTION()
	void TagResponseEvent(const FDNATag Tag, int32 NewCount, UDNAAbilitySystemComponent* ASC, int32 idx);
	
	/** Temporary structs to avoid extra heap allocations every time we recalculate tag count */
	mutable FDNAEffectQuery Query;

	FDNAEffectQuery& MakeQuery(const FDNATag& Tag) const
	{
		Query.OwningTagQuery.ReplaceTagFast(Tag);
		return Query;
	}

	// ----------------------------------------------------

	struct FDNATagResponseAppliedInfo
	{
		TArray<FActiveDNAEffectHandle> PositiveHandles;
		TArray<FActiveDNAEffectHandle> NegativeHandles;
	};

	TMap< TWeakObjectPtr<UDNAAbilitySystemComponent>, TArray< FDNATagResponseAppliedInfo> > RegisteredASCs;

	float LastASCPurgeTime;

	void Remove(UDNAAbilitySystemComponent* ASC, TArray<FActiveDNAEffectHandle>& Handles);

	void AddOrUpdate(UDNAAbilitySystemComponent* ASC, const TArray<TSubclassOf<UDNAEffect> >& ResponseDNAEffects, int32 TotalCount, TArray<FActiveDNAEffectHandle>& Handles);

	int32 GetCount(const FDNATagReponsePair& Pair, UDNAAbilitySystemComponent* ASC) const;
};
