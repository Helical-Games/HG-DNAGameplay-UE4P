// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "BlueprintDNATagLibrary.h"
#include "DNATagsModule.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"

UBlueprintDNATagLibrary::UBlueprintDNATagLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UBlueprintDNATagLibrary::MatchesTag(FDNATag TagOne, FDNATag TagTwo, bool bExactMatch)
{
	if (bExactMatch)
	{
		return TagOne.MatchesTagExact(TagTwo);
	}

	return TagOne.MatchesTag(TagTwo);
}

bool UBlueprintDNATagLibrary::MatchesAnyTags(FDNATag TagOne, const FDNATagContainer& OtherContainer, bool bExactMatch)
{
	if (bExactMatch)
	{
		return TagOne.MatchesAnyExact(OtherContainer);
	}
	return TagOne.MatchesAny(OtherContainer);
}

bool UBlueprintDNATagLibrary::EqualEqual_DNATag(FDNATag A, FDNATag B)
{
	return A == B;
}

bool UBlueprintDNATagLibrary::NotEqual_DNATag(FDNATag A, FDNATag B)
{
	return A != B;
}

bool UBlueprintDNATagLibrary::IsDNATagValid(FDNATag DNATag)
{
	return DNATag.IsValid();
}

FName UBlueprintDNATagLibrary::GetTagName(const FDNATag& DNATag)
{
	return DNATag.GetTagName();
}

FDNATag UBlueprintDNATagLibrary::MakeLiteralDNATag(FDNATag Value)
{
	return Value;
}

int32 UBlueprintDNATagLibrary::GetNumDNATagsInContainer(const FDNATagContainer& TagContainer)
{
	return TagContainer.Num();
}

bool UBlueprintDNATagLibrary::HasTag(const FDNATagContainer& TagContainer, FDNATag Tag, bool bExactMatch)
{
	if (bExactMatch)
	{
		return TagContainer.HasTagExact(Tag);
	}
	return TagContainer.HasTag(Tag);
}

bool UBlueprintDNATagLibrary::HasAnyTags(const FDNATagContainer& TagContainer, const FDNATagContainer& OtherContainer, bool bExactMatch)
{
	if (bExactMatch)
	{
		return TagContainer.HasAnyExact(OtherContainer);
	}
	return TagContainer.HasAny(OtherContainer);
}

bool UBlueprintDNATagLibrary::HasAllTags(const FDNATagContainer& TagContainer, const FDNATagContainer& OtherContainer, bool bExactMatch)
{
	if (bExactMatch)
	{
		return TagContainer.HasAllExact(OtherContainer);
	}
	return TagContainer.HasAll(OtherContainer);
}

bool UBlueprintDNATagLibrary::DoesContainerMatchTagQuery(const FDNATagContainer& TagContainer, const FDNATagQuery& TagQuery)
{
	return TagQuery.Matches(TagContainer);
}

void UBlueprintDNATagLibrary::GetAllActorsOfClassMatchingTagQuery(UObject* WorldContextObject,
																	   TSubclassOf<AActor> ActorClass,
																	   const FDNATagQuery& DNATagQuery,
																	   TArray<AActor*>& OutActors)
{
	OutActors.Empty();

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);

	// We do nothing if not class provided, rather than giving ALL actors!
	if (ActorClass && World)
	{
		bool bHasLoggedMissingInterface = false;
		for (TActorIterator<AActor> It(World, ActorClass); It; ++It)
		{
			AActor* Actor = *It;
			check(Actor != nullptr);
			if (!Actor->IsPendingKill())
			{
				IDNATagAssetInterface* DNATagAssetInterface = Cast<IDNATagAssetInterface>(Actor);
				if (DNATagAssetInterface != nullptr)
				{
					FDNATagContainer OwnedDNATags;
					DNATagAssetInterface->GetOwnedDNATags(OwnedDNATags);

					if (OwnedDNATags.MatchesQuery(DNATagQuery))
					{
						OutActors.Add(Actor);
					}
				}
				else
				{
					if (!bHasLoggedMissingInterface)
					{
						UE_LOG(LogDNATags, Warning,
							TEXT("At least one actor (%s) of class %s does not implement IGameplTagAssetInterface.  Unable to find owned tags, so cannot determine if actor matches DNA tag query.  Presuming it does not."),
							*Actor->GetName(), *ActorClass->GetName());
						bHasLoggedMissingInterface = true;
					}
				}
			}
		}
	}
}

bool UBlueprintDNATagLibrary::EqualEqual_DNATagContainer(const FDNATagContainer& A, const FDNATagContainer& B)
{
	return A == B;
}

bool UBlueprintDNATagLibrary::NotEqual_DNATagContainer(const FDNATagContainer& A, const FDNATagContainer& B)
{
	return A != B;
}

FDNATagContainer UBlueprintDNATagLibrary::MakeLiteralDNATagContainer(FDNATagContainer Value)
{
	return Value;
}

FDNATagContainer UBlueprintDNATagLibrary::MakeDNATagContainerFromArray(const TArray<FDNATag>& DNATags)
{
	return FDNATagContainer::CreateFromArray(DNATags);
}

FDNATagContainer UBlueprintDNATagLibrary::MakeDNATagContainerFromTag(FDNATag SingleTag)
{
	return FDNATagContainer(SingleTag);
}

void UBlueprintDNATagLibrary::BreakDNATagContainer(const FDNATagContainer& DNATagContainer, TArray<FDNATag>& DNATags)
{
	DNATagContainer.GetDNATagArray(DNATags);
}

FDNATagQuery UBlueprintDNATagLibrary::MakeDNATagQuery(FDNATagQuery TagQuery)
{
	return TagQuery;
}

bool UBlueprintDNATagLibrary::HasAllMatchingDNATags(TScriptInterface<IDNATagAssetInterface> TagContainerInterface, const FDNATagContainer& OtherContainer)
{
	if (TagContainerInterface.GetInterface() == NULL)
	{
		return (OtherContainer.Num() == 0);
	}

	FDNATagContainer OwnedTags;
	TagContainerInterface->GetOwnedDNATags(OwnedTags);
	return (OwnedTags.HasAll(OtherContainer));
}

bool UBlueprintDNATagLibrary::DoesTagAssetInterfaceHaveTag(TScriptInterface<IDNATagAssetInterface> TagContainerInterface, FDNATag Tag)
{
	if (TagContainerInterface.GetInterface() == NULL)
	{
		return false;
	}

	FDNATagContainer OwnedTags;
	TagContainerInterface->GetOwnedDNATags(OwnedTags);
	return (OwnedTags.HasTag(Tag));
}

void UBlueprintDNATagLibrary::AppendDNATagContainers(FDNATagContainer& InOutTagContainer, const FDNATagContainer& InTagContainer)
{
	InOutTagContainer.AppendTags(InTagContainer);
}

void UBlueprintDNATagLibrary::AddDNATag(FDNATagContainer& InOutTagContainer, FDNATag Tag)
{
	InOutTagContainer.AddTag(Tag);
}

bool UBlueprintDNATagLibrary::NotEqual_TagTag(FDNATag A, FString B)
{
	return A.ToString() != B;
}

bool UBlueprintDNATagLibrary::NotEqual_TagContainerTagContainer(FDNATagContainer A, FString B)
{
	FDNATagContainer TagContainer;

	const FString OpenParenthesesStr(TEXT("("));
	const FString CloseParenthesesStr(TEXT(")"));

	// Convert string to Tag Container before compare
	FString TagString = MoveTemp(B);
	if (TagString.StartsWith(OpenParenthesesStr, ESearchCase::CaseSensitive) && TagString.EndsWith(CloseParenthesesStr, ESearchCase::CaseSensitive))
	{
		TagString = TagString.LeftChop(1);
		TagString = TagString.RightChop(1);

		const FString EqualStr(TEXT("="));

		TagString.Split(EqualStr, nullptr, &TagString, ESearchCase::CaseSensitive);

		TagString = TagString.LeftChop(1);
		TagString = TagString.RightChop(1);

		FString ReadTag;
		FString Remainder;

		const FString CommaStr(TEXT(","));
		const FString QuoteStr(TEXT("\""));

		while (TagString.Split(CommaStr, &ReadTag, &Remainder, ESearchCase::CaseSensitive))
		{
			ReadTag.Split(EqualStr, nullptr, &ReadTag, ESearchCase::CaseSensitive);
			if (ReadTag.EndsWith(CloseParenthesesStr, ESearchCase::CaseSensitive))
			{
				ReadTag = ReadTag.LeftChop(1);
				if (ReadTag.StartsWith(QuoteStr, ESearchCase::CaseSensitive) && ReadTag.EndsWith(QuoteStr, ESearchCase::CaseSensitive))
				{
					ReadTag = ReadTag.LeftChop(1);
					ReadTag = ReadTag.RightChop(1);
				}
			}
			TagString = Remainder;

			const FDNATag Tag = FDNATag::RequestDNATag(FName(*ReadTag));
			TagContainer.AddTag(Tag);
		}
		if (Remainder.IsEmpty())
		{
			Remainder = MoveTemp(TagString);
		}
		if (!Remainder.IsEmpty())
		{
			Remainder.Split(EqualStr, nullptr, &Remainder, ESearchCase::CaseSensitive);
			if (Remainder.EndsWith(CloseParenthesesStr, ESearchCase::CaseSensitive))
			{
				Remainder = Remainder.LeftChop(1);
				if (Remainder.StartsWith(QuoteStr, ESearchCase::CaseSensitive) && Remainder.EndsWith(QuoteStr, ESearchCase::CaseSensitive))
				{
					Remainder = Remainder.LeftChop(1);
					Remainder = Remainder.RightChop(1);
				}
			}
			const FDNATag Tag = FDNATag::RequestDNATag(FName(*Remainder));
			TagContainer.AddTag(Tag);
		}
	}

	return A != TagContainer;
}
FString UBlueprintDNATagLibrary::GetDebugStringFromDNATagContainer(const FDNATagContainer& TagContainer)
{
	return TagContainer.ToStringSimple();
}

FString UBlueprintDNATagLibrary::GetDebugStringFromDNATag(FDNATag DNATag)
{
	return DNATag.ToString();
}
