// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNATagResponseTable.h"
#include "AbilitySystemComponent.h"
#include "Misc/TimeGuard.h"

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	UDNATagReponseTable
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

UDNATagReponseTable::UDNATagReponseTable(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Make an empty tag query. We will actually fill the tag out prior to evaluating the query with ::MakeQuery

	Query.OwningTagQuery = FDNATagQuery::BuildQuery(
		FDNATagQueryExpression()
		.AllTagsMatch()
		.AddTag(FDNATag())
	);
	LastASCPurgeTime = 0;
}

void UDNATagReponseTable::PostLoad()
{
	Super::PostLoad();

	// Fixup
	for (FDNATagResponseTableEntry& Entry :  Entries)
	{
		if (*Entry.Positive.ResponseDNAEffect)
		{
			Entry.Positive.ResponseDNAEffects.Add(Entry.Positive.ResponseDNAEffect);
			Entry.Positive.ResponseDNAEffect = nullptr;
		}
		if (*Entry.Negative.ResponseDNAEffect)
		{
			Entry.Negative.ResponseDNAEffects.Add(Entry.Negative.ResponseDNAEffect);
			Entry.Negative.ResponseDNAEffect = nullptr;
		}
	}
}

void UDNATagReponseTable::RegisterResponseForEvents(UDNAAbilitySystemComponent* ASC)
{
	if (RegisteredASCs.Contains(ASC))
	{
		return;
	}

	TArray<FDNATagResponseAppliedInfo> AppliedInfoList;
	AppliedInfoList.SetNum(Entries.Num());

	RegisteredASCs.Add(ASC, AppliedInfoList );

	for (int32 idx=0; idx < Entries.Num(); ++idx)
	{
		const FDNATagResponseTableEntry& Entry = Entries[idx];
		if (Entry.Positive.Tag.IsValid())
		{
			ASC->RegisterDNATagEvent( Entry.Positive.Tag, EDNATagEventType::AnyCountChange ).AddUObject(this, &UDNATagReponseTable::TagResponseEvent, ASC, idx);
		}
		if (Entry.Negative.Tag.IsValid())
		{
			ASC->RegisterDNATagEvent( Entry.Negative.Tag, EDNATagEventType::AnyCountChange ).AddUObject(this, &UDNATagReponseTable::TagResponseEvent, ASC, idx);
		}
	}

	// Need to periodically cull null entries. We can do this very infrequently as the memory overhead is not great
	if (FPlatformTime::Seconds() - LastASCPurgeTime >= 300.0f)
	{
		SCOPE_TIME_GUARD_MS(TEXT("DNATagReponseTableCleanup"), 1);

		bool Removed = false;
		for (auto It = RegisteredASCs.CreateIterator(); It; ++It)
		{
			if (It.Key().IsValid() == false)
			{
				Removed = true;
				It.RemoveCurrent();
			}
		}

		if (Removed)
		{
			RegisteredASCs.Compact();
		}

		LastASCPurgeTime = FPlatformTime::Seconds();
	}
}

void UDNATagReponseTable::TagResponseEvent(const FDNATag Tag, int32 NewCount, UDNAAbilitySystemComponent* ASC, int32 idx)
{
	if (!ensure(Entries.IsValidIndex(idx)))
	{
		return;
	}

	const FDNATagResponseTableEntry& Entry = Entries[idx];
	int32 TotalCount = 0;

	{
		QUICK_SCOPE_CYCLE_COUNTER(ABILITY_TRT_CALC_COUNT);

		int32 Positive = GetCount(Entry.Positive, ASC);
		int32 Negative = GetCount(Entry.Negative, ASC);

		TotalCount = Positive - Negative;
	}

	TArray<FDNATagResponseAppliedInfo>& InfoList = RegisteredASCs.FindChecked(ASC);
	FDNATagResponseAppliedInfo& Info = InfoList[idx];

	if (TotalCount < 0)
	{
		Remove(ASC, Info.PositiveHandles);
		AddOrUpdate(ASC, Entry.Negative.ResponseDNAEffects, TotalCount, Info.NegativeHandles);
	}
	else if (TotalCount > 0)
	{
		Remove(ASC, Info.NegativeHandles);
		AddOrUpdate(ASC, Entry.Positive.ResponseDNAEffects, TotalCount, Info.PositiveHandles);
	}
	else if (TotalCount == 0)
	{
		Remove(ASC, Info.PositiveHandles);
		Remove(ASC, Info.NegativeHandles);
	}
}

int32 UDNATagReponseTable::GetCount(const FDNATagReponsePair& Pair, UDNAAbilitySystemComponent* ASC) const
{
	int32 Count=0;
	if (Pair.Tag.IsValid())
	{
		Count = ASC->GetAggregatedStackCount(MakeQuery(Pair.Tag));
		if (Pair.SoftCountCap > 0)
		{
			Count = FMath::Min<int32>(Pair.SoftCountCap, Count);
		}
	}

	return Count;
}

void UDNATagReponseTable::Remove(UDNAAbilitySystemComponent* ASC, TArray<FActiveDNAEffectHandle>& Handles)
{
	for (FActiveDNAEffectHandle& Handle : Handles)
	{
		if (Handle.IsValid())
		{
			ASC->RemoveActiveDNAEffect(Handle);
			Handle.Invalidate();
		}
	}
	Handles.Reset();
}

void UDNATagReponseTable::AddOrUpdate(UDNAAbilitySystemComponent* ASC, const TArray<TSubclassOf<UDNAEffect> >& ResponseDNAEffects, int32 TotalCount, TArray<FActiveDNAEffectHandle>& Handles)
{
	if (ResponseDNAEffects.Num() > 0)
	{
		if (Handles.Num() > 0)
		{
			// Already been applied
			for (FActiveDNAEffectHandle& Handle : Handles)
			{
				ASC->SetActiveDNAEffectLevel(Handle, TotalCount);
			}
		}
		else
		{
			for (const TSubclassOf<UDNAEffect>& ResponseDNAEffect : ResponseDNAEffects)
			{
				FActiveDNAEffectHandle NewHandle = ASC->ApplyDNAEffectToSelf(Cast<UDNAEffect>(ResponseDNAEffect->ClassDefaultObject), TotalCount, ASC->MakeEffectContext());
				if (NewHandle.IsValid())
				{
					Handles.Add(NewHandle);
				}
			}
		}
	}
}
