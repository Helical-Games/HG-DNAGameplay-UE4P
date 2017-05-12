// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNATagContainer.h"
#include "HAL/IConsoleManager.h"
#include "UObject/CoreNet.h"
#include "UObject/UnrealType.h"
#include "Engine/PackageMapClient.h"
#include "UObject/Package.h"
#include "Engine/NetConnection.h"
#include "DNATagsManager.h"
#include "DNATagsModule.h"
#include "Misc/OutputDeviceNull.h"

const FDNATag FDNATag::EmptyTag;
const FDNATagContainer FDNATagContainer::EmptyContainer;
const FDNATagQuery FDNATagQuery::EmptyQuery;

DEFINE_STAT(STAT_FDNATagContainer_HasTag);
DEFINE_STAT(STAT_FDNATagContainer_DoesTagContainerMatch);
DEFINE_STAT(STAT_UDNATagsManager_DNATagsMatch);

/**
 *	Replicates a tag in a packed format:
 *	-A segment of NetIndexFirstBitSegment bits are always replicated.
 *	-Another bit is replicated to indicate "more"
 *	-If "more", then another segment of (MaxBits - NetIndexFirstBitSegment) length is replicated.
 *	
 *	This format is basically the same as SerializeIntPacked, except that there are only 2 segments and they are not the same size.
 *	The DNA tag system is able to exploit knoweledge in what tags are frequently replicated to ensure they appear in the first segment.
 *	Making frequently replicated tags as cheap as possible. 
 *	
 *	
 *	Setting up your project to take advantage of the packed format.
 *	-Run a normal networked game on non shipping build. 
 *	-After some time, run console command "DNATags.PrintReport" or set "DNATags.PrintReportOnShutdown 1" cvar.
 *	-This will generate information on the server log about what tags replicate most frequently.
 *	-Take this list and put it in DefaultDNATags.ini.
 *	-CommonlyReplicatedTags is the ordered list of tags.
 *	-NetIndexFirstBitSegment is the number of bits (not including the "more" bit) for the first segment.
 *
 */
void SerializeTagNetIndexPacked(FArchive& Ar, FDNATagNetIndex& Value, const int32 NetIndexFirstBitSegment, const int32 MaxBits)
{
	// Case where we have no segment or the segment is larger than max bits
	if (NetIndexFirstBitSegment <= 0 || NetIndexFirstBitSegment >= MaxBits)
	{
		if (Ar.IsLoading())
		{
			Value = 0;
		}
		Ar.SerializeBits(&Value, MaxBits);
		return;
	}


	const uint32 BitMasks[] = {0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff, 0x1ff, 0x3ff, 0x7ff, 0xfff, 0x1fff, 0x3fff, 0x7fff, 0xffff};
	const uint32 MoreBits[] = {0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000, 0x8000};

	const int32 FirstSegment = NetIndexFirstBitSegment;
	const int32 SecondSegment = MaxBits - NetIndexFirstBitSegment;

	if (Ar.IsSaving())
	{
		uint32 Mask = BitMasks[FirstSegment];
		if (Value > Mask)
		{
			uint32 FirstDataSegment = ((Value & Mask) | MoreBits[FirstSegment+1]);
			uint32 SecondDataSegment = (Value >> FirstSegment);

			uint32 SerializedValue = FirstDataSegment | (SecondDataSegment << (FirstSegment+1));				

			Ar.SerializeBits(&SerializedValue, MaxBits + 1);
		}
		else
		{
			uint32 SerializedValue = Value;
			Ar.SerializeBits(&SerializedValue, NetIndexFirstBitSegment + 1);
		}

	}
	else
	{
		uint32 FirstData = 0;
		Ar.SerializeBits(&FirstData, FirstSegment + 1);
		uint32 More = FirstData & MoreBits[FirstSegment+1];
		if (More)
		{
			uint32 SecondData = 0;
			Ar.SerializeBits(&SecondData, SecondSegment);
			Value = (SecondData << FirstSegment);
			Value |= (FirstData & BitMasks[FirstSegment]);
		}
		else
		{
			Value = FirstData;
		}

	}
}


/** Helper class to parse/eval query token streams. */
class FQueryEvaluator
{
public:
	FQueryEvaluator(FDNATagQuery const& Q)
		: Query(Q), 
		CurStreamIdx(0), 
		Version(EDNATagQueryStreamVersion::LatestVersion), 
		bReadError(false)
	{}

	/** Evaluates the query against the given tag container and returns the result (true if matching, false otherwise). */
	bool Eval(FDNATagContainer const& Tags);

	/** Parses the token stream into an FExpr. */
	void Read(struct FDNATagQueryExpression& E);

private:
	FDNATagQuery const& Query;
	int32 CurStreamIdx;
	int32 Version;
	bool bReadError;

	bool EvalAnyTagsMatch(FDNATagContainer const& Tags, bool bSkip);
	bool EvalAllTagsMatch(FDNATagContainer const& Tags, bool bSkip);
	bool EvalNoTagsMatch(FDNATagContainer const& Tags, bool bSkip);

	bool EvalAnyExprMatch(FDNATagContainer const& Tags, bool bSkip);
	bool EvalAllExprMatch(FDNATagContainer const& Tags, bool bSkip);
	bool EvalNoExprMatch(FDNATagContainer const& Tags, bool bSkip);

	bool EvalExpr(FDNATagContainer const& Tags, bool bSkip = false);
	void ReadExpr(struct FDNATagQueryExpression& E);

#if WITH_EDITOR
public:
	UEditableDNATagQuery* CreateEditableQuery();

private:
	UEditableDNATagQueryExpression* ReadEditableQueryExpr(UObject* ExprOuter);
	void ReadEditableQueryTags(UEditableDNATagQueryExpression* EditableQueryExpr);
	void ReadEditableQueryExprList(UEditableDNATagQueryExpression* EditableQueryExpr);
#endif // WITH_EDITOR

	/** Returns the next token in the stream. If there's a read error, sets bReadError and returns zero, so be sure to check that. */
	uint8 GetToken()
	{
		if (Query.QueryTokenStream.IsValidIndex(CurStreamIdx))
		{
			return Query.QueryTokenStream[CurStreamIdx++];
		}
		
		UE_LOG(LogDNATags, Warning, TEXT("Error parsing FDNATagQuery!"));
		bReadError = true;
		return 0;
	}
};

bool FQueryEvaluator::Eval(FDNATagContainer const& Tags)
{
	CurStreamIdx = 0;

	// start parsing the set
	Version = GetToken();
	if (bReadError)
	{
		return false;
	}
	
	bool bRet = false;

	uint8 const bHasRootExpression = GetToken();
	if (!bReadError && bHasRootExpression)
	{
		bRet = EvalExpr(Tags);

	}

	ensure(CurStreamIdx == Query.QueryTokenStream.Num());
	return bRet;
}

void FQueryEvaluator::Read(FDNATagQueryExpression& E)
{
	E = FDNATagQueryExpression();
	CurStreamIdx = 0;

	if (Query.QueryTokenStream.Num() > 0)
	{
		// start parsing the set
		Version = GetToken();
		if (!bReadError)
		{
			uint8 const bHasRootExpression = GetToken();
			if (!bReadError && bHasRootExpression)
			{
				ReadExpr(E);
			}
		}

		ensure(CurStreamIdx == Query.QueryTokenStream.Num());
	}
}

void FQueryEvaluator::ReadExpr(FDNATagQueryExpression& E)
{
	E.ExprType = (EDNATagQueryExprType::Type) GetToken();
	if (bReadError)
	{
		return;
	}
	
	if (E.UsesTagSet())
	{
		// parse tag set
		int32 NumTags = GetToken();
		if (bReadError)
		{
			return;
		}

		for (int32 Idx = 0; Idx < NumTags; ++Idx)
		{
			int32 const TagIdx = GetToken();
			if (bReadError)
			{
				return;
			}

			FDNATag Tag = Query.GetTagFromIndex(TagIdx);
			E.AddTag(Tag);
		}
	}
	else
	{
		// parse expr set
		int32 NumExprs = GetToken();
		if (bReadError)
		{
			return;
		}

		for (int32 Idx = 0; Idx < NumExprs; ++Idx)
		{
			FDNATagQueryExpression Exp;
			ReadExpr(Exp);
			Exp.AddExpr(Exp);
		}
	}
}


bool FQueryEvaluator::EvalAnyTagsMatch(FDNATagContainer const& Tags, bool bSkip)
{
	bool bShortCircuit = bSkip;
	bool Result = false;

	// parse tagset
	int32 const NumTags = GetToken();
	if (bReadError)
	{
		return false;
	}

	for (int32 Idx = 0; Idx < NumTags; ++Idx)
	{
		int32 const TagIdx = GetToken();
		if (bReadError)
		{
			return false;
		}

		if (bShortCircuit == false)
		{
			FDNATag Tag = Query.GetTagFromIndex(TagIdx);

			bool bHasTag = Tags.HasTag(Tag);

			if (bHasTag)
			{
				// one match is sufficient for a true result!
				bShortCircuit = true;
				Result = true;
			}
		}
	}

	return Result;
}

bool FQueryEvaluator::EvalAllTagsMatch(FDNATagContainer const& Tags, bool bSkip)
{
	bool bShortCircuit = bSkip;

	// assume true until proven otherwise
	bool Result = true;

	// parse tagset
	int32 const NumTags = GetToken();
	if (bReadError)
	{
		return false;
	}

	for (int32 Idx = 0; Idx < NumTags; ++Idx)
	{
		int32 const TagIdx = GetToken();
		if (bReadError)
		{
			return false;
		}

		if (bShortCircuit == false)
		{
			FDNATag const Tag = Query.GetTagFromIndex(TagIdx);
			bool const bHasTag = Tags.HasTag(Tag);

			if (bHasTag == false)
			{
				// one failed match is sufficient for a false result
				bShortCircuit = true;
				Result = false;
			}
		}
	}

	return Result;
}

bool FQueryEvaluator::EvalNoTagsMatch(FDNATagContainer const& Tags, bool bSkip)
{
	bool bShortCircuit = bSkip;

	// assume true until proven otherwise
	bool Result = true;

	// parse tagset
	int32 const NumTags = GetToken();
	if (bReadError)
	{
		return false;
	}
	
	for (int32 Idx = 0; Idx < NumTags; ++Idx)
	{
		int32 const TagIdx = GetToken();
		if (bReadError)
		{
			return false;
		}

		if (bShortCircuit == false)
		{
			FDNATag const Tag = Query.GetTagFromIndex(TagIdx);
			bool const bHasTag = Tags.HasTag(Tag);

			if (bHasTag == true)
			{
				// one match is sufficient for a false result
				bShortCircuit = true;
				Result = false;
			}
		}
	}

	return Result;
}

bool FQueryEvaluator::EvalAnyExprMatch(FDNATagContainer const& Tags, bool bSkip)
{
	bool bShortCircuit = bSkip;

	// assume false until proven otherwise
	bool Result = false;

	// parse exprset
	int32 const NumExprs = GetToken();
	if (bReadError)
	{
		return false;
	}

	for (int32 Idx = 0; Idx < NumExprs; ++Idx)
	{
		bool const bExprResult = EvalExpr(Tags, bShortCircuit);
		if (bShortCircuit == false)
		{
			if (bExprResult == true)
			{
				// one match is sufficient for true result
				Result = true;
				bShortCircuit = true;
			}
		}
	}

	return Result;
}
bool FQueryEvaluator::EvalAllExprMatch(FDNATagContainer const& Tags, bool bSkip)
{
	bool bShortCircuit = bSkip;

	// assume true until proven otherwise
	bool Result = true;

	// parse exprset
	int32 const NumExprs = GetToken();
	if (bReadError)
	{
		return false;
	}

	for (int32 Idx = 0; Idx < NumExprs; ++Idx)
	{
		bool const bExprResult = EvalExpr(Tags, bShortCircuit);
		if (bShortCircuit == false)
		{
			if (bExprResult == false)
			{
				// one fail is sufficient for false result
				Result = false;
				bShortCircuit = true;
			}
		}
	}

	return Result;
}
bool FQueryEvaluator::EvalNoExprMatch(FDNATagContainer const& Tags, bool bSkip)
{
	bool bShortCircuit = bSkip;

	// assume true until proven otherwise
	bool Result = true;

	// parse exprset
	int32 const NumExprs = GetToken();
	if (bReadError)
	{
		return false;
	}

	for (int32 Idx = 0; Idx < NumExprs; ++Idx)
	{
		bool const bExprResult = EvalExpr(Tags, bShortCircuit);
		if (bShortCircuit == false)
		{
			if (bExprResult == true)
			{
				// one match is sufficient for fail result
				Result = false;
				bShortCircuit = true;
			}
		}
	}

	return Result;
}


bool FQueryEvaluator::EvalExpr(FDNATagContainer const& Tags, bool bSkip)
{
	EDNATagQueryExprType::Type const ExprType = (EDNATagQueryExprType::Type) GetToken();
	if (bReadError)
	{
		return false;
	}

	// emit exprdata
	switch (ExprType)
	{
	case EDNATagQueryExprType::AnyTagsMatch:
		return EvalAnyTagsMatch(Tags, bSkip);
	case EDNATagQueryExprType::AllTagsMatch:
		return EvalAllTagsMatch(Tags, bSkip);
	case EDNATagQueryExprType::NoTagsMatch:
		return EvalNoTagsMatch(Tags, bSkip);

	case EDNATagQueryExprType::AnyExprMatch:
		return EvalAnyExprMatch(Tags, bSkip);
	case EDNATagQueryExprType::AllExprMatch:
		return EvalAllExprMatch(Tags, bSkip);
	case EDNATagQueryExprType::NoExprMatch:
		return EvalNoExprMatch(Tags, bSkip);
	}

	check(false);
	return false;
}


FDNATagContainer& FDNATagContainer::operator=(FDNATagContainer const& Other)
{
	// Guard against self-assignment
	if (this == &Other)
	{
		return *this;
	}
	DNATags.Empty(Other.DNATags.Num());
	DNATags.Append(Other.DNATags);

	ParentTags.Empty(Other.ParentTags.Num());
	ParentTags.Append(Other.ParentTags);

	return *this;
}

FDNATagContainer& FDNATagContainer::operator=(FDNATagContainer&& Other)
{
	DNATags = MoveTemp(Other.DNATags);
	ParentTags = MoveTemp(Other.ParentTags);
	return *this;
}

bool FDNATagContainer::operator==(FDNATagContainer const& Other) const
{
	// This is to handle the case where the two containers are in different orders
	if (DNATags.Num() != Other.DNATags.Num())
	{
		return false;
	}
	return FilterExact(Other).Num() == this->Num();
}

bool FDNATagContainer::operator!=(FDNATagContainer const& Other) const
{
	return !operator==(Other);
}

bool FDNATagContainer::ComplexHasTag(FDNATag const& TagToCheck, TEnumAsByte<EDNATagMatchType::Type> TagMatchType, TEnumAsByte<EDNATagMatchType::Type> TagToCheckMatchType) const
{
	check(TagMatchType != EDNATagMatchType::Explicit || TagToCheckMatchType != EDNATagMatchType::Explicit);

	if (TagMatchType == EDNATagMatchType::IncludeParentTags)
	{
		FDNATagContainer ExpandedConatiner = GetDNATagParents();
		return ExpandedConatiner.HasTagFast(TagToCheck, EDNATagMatchType::Explicit, TagToCheckMatchType);
	}
	else
	{
		const FDNATagContainer* SingleContainer = UDNATagsManager::Get().GetSingleTagContainer(TagToCheck);
		if (SingleContainer && SingleContainer->DoesTagContainerMatch(*this, EDNATagMatchType::IncludeParentTags, EDNATagMatchType::Explicit, EDNAContainerMatchType::Any))
		{
			return true;
		}

	}
	return false;
}

DECLARE_CYCLE_STAT(TEXT("FDNATagContainer::RemoveTagByExplicitName"), STAT_FDNATagContainer_RemoveTagByExplicitName, STATGROUP_DNATags);

bool FDNATagContainer::RemoveTagByExplicitName(const FName& TagName)
{
	SCOPE_CYCLE_COUNTER(STAT_FDNATagContainer_RemoveTagByExplicitName);

	for (auto DNATag : this->DNATags)
	{
		if (DNATag.GetTagName() == TagName)
		{
			RemoveTag(DNATag);
			return true;
		}
	}

	return false;
}

FORCEINLINE_DEBUGGABLE void FDNATagContainer::AddParentsForTag(const FDNATag& Tag)
{
	const FDNATagContainer* SingleContainer = UDNATagsManager::Get().GetSingleTagContainer(Tag);

	if (SingleContainer)
	{
		// Add Parent tags from this tag to our own
		for (const FDNATag& ParentTag : SingleContainer->ParentTags)
		{
			ParentTags.AddUnique(ParentTag);
		}
	}
}

DECLARE_CYCLE_STAT(TEXT("FDNATagContainer::FillParentTags"), STAT_FDNATagContainer_FillParentTags, STATGROUP_DNATags);

void FDNATagContainer::FillParentTags()
{
	SCOPE_CYCLE_COUNTER(STAT_FDNATagContainer_FillParentTags);

	ParentTags.Reset();

	for (const FDNATag& Tag : DNATags)
	{
		AddParentsForTag(Tag);
	}
}

FDNATagContainer FDNATagContainer::GetDNATagParents() const
{
	FDNATagContainer ResultContainer;
	ResultContainer.DNATags = DNATags;

	// Add parent tags to explicit tags, the rest got copied over already
	for (const FDNATag& Tag : ParentTags)
	{
		ResultContainer.DNATags.AddUnique(Tag);
	}

	return ResultContainer;
}

DECLARE_CYCLE_STAT(TEXT("FDNATagContainer::Filter"), STAT_FDNATagContainer_Filter, STATGROUP_DNATags);

FDNATagContainer FDNATagContainer::Filter(const FDNATagContainer& OtherContainer, TEnumAsByte<EDNATagMatchType::Type> TagMatchType, TEnumAsByte<EDNATagMatchType::Type> OtherTagMatchType) const
{
	SCOPE_CYCLE_COUNTER(STAT_FDNATagContainer_Filter);

	FDNATagContainer ResultContainer;

	for (const FDNATag& Tag : DNATags)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		// Check to see if all of these tags match other container, with types swapped
		if (OtherContainer.HasTag(Tag, OtherTagMatchType, TagMatchType))
		{
			ResultContainer.AddTagFast(Tag);
		}
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	return ResultContainer;
}

FDNATagContainer FDNATagContainer::Filter(const FDNATagContainer& OtherContainer) const
{
	SCOPE_CYCLE_COUNTER(STAT_FDNATagContainer_Filter);

	FDNATagContainer ResultContainer;

	for (const FDNATag& Tag : DNATags)
	{
		if (Tag.MatchesAny(OtherContainer))
		{
			ResultContainer.AddTagFast(Tag);
		}
	}

	return ResultContainer;
}

FDNATagContainer FDNATagContainer::FilterExact(const FDNATagContainer& OtherContainer) const
{
	SCOPE_CYCLE_COUNTER(STAT_FDNATagContainer_Filter);

	FDNATagContainer ResultContainer;

	for (const FDNATag& Tag : DNATags)
	{
		if (Tag.MatchesAnyExact(OtherContainer))
		{
			ResultContainer.AddTagFast(Tag);
		}
	}

	return ResultContainer;
}

bool FDNATagContainer::DoesTagContainerMatchComplex(const FDNATagContainer& OtherContainer, TEnumAsByte<EDNATagMatchType::Type> TagMatchType, TEnumAsByte<EDNATagMatchType::Type> OtherTagMatchType, EDNAContainerMatchType ContainerMatchType) const
{
	UDNATagsManager& TagManager = UDNATagsManager::Get();

	for (TArray<FDNATag>::TConstIterator OtherIt(OtherContainer.DNATags); OtherIt; ++OtherIt)
	{
		bool bTagFound = false;

		for (TArray<FDNATag>::TConstIterator It(this->DNATags); It; ++It)
		{
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			if (TagManager.DNATagsMatch(*It, TagMatchType, *OtherIt, OtherTagMatchType) == true)
			{
				if (ContainerMatchType == EDNAContainerMatchType::Any)
				{
					return true;
				}

				bTagFound = true;

				// we only need one match per tag in OtherContainer, so don't bother looking for more
				break;
			}
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}

		if (ContainerMatchType == EDNAContainerMatchType::All && bTagFound == false)
		{
			return false;
		}
	}

	// if we've reached this far then either we are looking for any match and didn't find one (return false) or we're looking for all matches and didn't miss one (return true).
	check(ContainerMatchType == EDNAContainerMatchType::All || ContainerMatchType == EDNAContainerMatchType::Any);
	return ContainerMatchType == EDNAContainerMatchType::All;
}

bool FDNATagContainer::MatchesQuery(const FDNATagQuery& Query) const
{
	return Query.Matches(*this);
}

DECLARE_CYCLE_STAT(TEXT("FDNATagContainer::AppendTags"), STAT_FDNATagContainer_AppendTags, STATGROUP_DNATags);

void FDNATagContainer::AppendTags(FDNATagContainer const& Other)
{
	SCOPE_CYCLE_COUNTER(STAT_FDNATagContainer_AppendTags);

	DNATags.Reserve(DNATags.Num() + Other.DNATags.Num());
	ParentTags.Reserve(ParentTags.Num() + Other.ParentTags.Num());

	// Add other container's tags to our own
	for(const FDNATag& OtherTag : Other.DNATags)
	{
		DNATags.AddUnique(OtherTag);
	}

	for (const FDNATag& OtherTag : Other.ParentTags)
	{
		ParentTags.AddUnique(OtherTag);
	}
}

DECLARE_CYCLE_STAT(TEXT("FDNATagContainer::AppendMatchingTags"), STAT_FDNATagContainer_AppendMatchingTags, STATGROUP_DNATags);


void FDNATagContainer::AppendMatchingTags(FDNATagContainer const& OtherA, FDNATagContainer const& OtherB)
{
	SCOPE_CYCLE_COUNTER(STAT_FDNATagContainer_AppendMatchingTags);

	for(const FDNATag& OtherATag : OtherA.DNATags)
	{
		if (OtherATag.MatchesAny(OtherB))
		{
			AddTag(OtherATag);
		}
	}
}

DECLARE_CYCLE_STAT(TEXT("FDNATagContainer::AddTag"), STAT_FDNATagContainer_AddTag, STATGROUP_DNATags);

static UDNATagsManager* CachedTagManager = nullptr;


void FDNATagContainer::AddTag(const FDNATag& TagToAdd)
{
	SCOPE_CYCLE_COUNTER(STAT_FDNATagContainer_AddTag);

	if (TagToAdd.IsValid())
	{
		// Don't want duplicate tags
		DNATags.AddUnique(TagToAdd);

		AddParentsForTag(TagToAdd);
	}
}

void FDNATagContainer::AddTagFast(const FDNATag& TagToAdd)
{
	DNATags.Add(TagToAdd);
	AddParentsForTag(TagToAdd);
}

bool FDNATagContainer::AddLeafTag(const FDNATag& TagToAdd)
{
	UDNATagsManager& TagManager = UDNATagsManager::Get();

	// Check tag is not already explicitly in container
	if (HasTagExact(TagToAdd))
	{
		return true;
	}

	// If this tag is parent of explicitly added tag, fail
	if (HasTag(TagToAdd))
	{
		return false;
	}

	const FDNATagContainer* TagToAddContainer = UDNATagsManager::Get().GetSingleTagContainer(TagToAdd);

	// This should always succeed
	if (!ensure(TagToAddContainer))
	{
		return false;
	}

	// Remove any tags in the container that are a parent to TagToAdd
	for (const FDNATag& ParentTag : TagToAddContainer->ParentTags)
	{
		if (HasTagExact(ParentTag))
		{
			RemoveTag(ParentTag);
		}
	}

	// Add the tag
	AddTag(TagToAdd);
	return true;
}

DECLARE_CYCLE_STAT(TEXT("FDNATagContainer::RemoveTag"), STAT_FDNATagContainer_RemoveTag, STATGROUP_DNATags);

bool FDNATagContainer::RemoveTag(FDNATag TagToRemove)
{
	SCOPE_CYCLE_COUNTER(STAT_FDNATagContainer_RemoveTag);

	int32 NumChanged = DNATags.RemoveSingle(TagToRemove);

	if (NumChanged > 0)
	{
		// Have to recompute parent table from scratch because there could be duplicates providing the same parent tag
		FillParentTags();
		return true;
	}
	return false;
}

DECLARE_CYCLE_STAT(TEXT("FDNATagContainer::RemoveTags"), STAT_FDNATagContainer_RemoveTags, STATGROUP_DNATags);

void FDNATagContainer::RemoveTags(FDNATagContainer TagsToRemove)
{
	SCOPE_CYCLE_COUNTER(STAT_FDNATagContainer_RemoveTags);

	int32 NumChanged = 0;

	for (auto Tag : TagsToRemove)
	{
		NumChanged += DNATags.RemoveSingle(Tag);
	}

	if (NumChanged > 0)
	{
		// Recompute once at the end
		FillParentTags();
	}
}

void FDNATagContainer::Reset(int32 Slack)
{
	DNATags.Reset(Slack);

	// ParentTags is usually around size of DNATags on average
	ParentTags.Reset(Slack);
}

bool FDNATagContainer::Serialize(FArchive& Ar)
{
	const bool bOldTagVer = Ar.UE4Ver() < VER_UE4_DNA_TAG_CONTAINER_TAG_TYPE_CHANGE;
	
	if (bOldTagVer)
	{
		TArray<FName> Tags_DEPRECATED;
		Ar << Tags_DEPRECATED;
		// Too old to deal with
		UE_LOG(LogDNATags, Error, TEXT("Failed to load old DNATag container, too old to migrate correctly"));
	}
	else
	{
		Ar << DNATags;
	}
	
	// Only do redirects for real loads, not for duplicates or recompiles
	if (Ar.IsLoading() )
	{
		if (Ar.IsPersistent() && !(Ar.GetPortFlags() & PPF_Duplicate) && !(Ar.GetPortFlags() & PPF_DuplicateForPIE))
		{
			// Rename any tags that may have changed by the ini file.  Redirects can happen regardless of version.
			// Regardless of version, want loading to have a chance to handle redirects
			UDNATagsManager::Get().RedirectTagsForContainer(*this, Ar.GetSerializedProperty());
		}

		FillParentTags();
	}

	if (Ar.IsSaving())
	{
		// This marks the saved name for later searching
		for (const FDNATag& Tag : DNATags)
		{
			Ar.MarkSearchableName(FDNATag::StaticStruct(), Tag.TagName);
		}
	}

	return true;
}

FString FDNATagContainer::ToString() const
{
	FString ExportString;
	FDNATagContainer::StaticStruct()->ExportText(ExportString, this, this, nullptr, 0, nullptr);

	return ExportString;
}

void FDNATagContainer::FromExportString(FString ExportString)
{
	Reset();

	FOutputDeviceNull NullOut;
	FDNATagContainer::StaticStruct()->ImportText(*ExportString, this, nullptr, 0, &NullOut, TEXT("FDNATagContainer"), true);
}

bool FDNATagContainer::ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText)
{
	// Call default import, but skip the native callback to avoid recursion
	Buffer = FDNATagContainer::StaticStruct()->ImportText(Buffer, this, Parent, PortFlags, ErrorText, TEXT("FDNATagContainer"), false);

	if (Buffer)
	{
		// Compute parent tags
		FillParentTags();	
	}
	return true;
}

FString FDNATagContainer::ToStringSimple() const
{
	FString RetString;
	for (int i = 0; i < DNATags.Num(); ++i)
	{
		RetString += TEXT("\"");
		RetString += DNATags[i].ToString();
		RetString += TEXT("\"");
		if (i < DNATags.Num() - 1)
		{
			RetString += TEXT(", ");
		}
	}
	return RetString;
}

bool FDNATagContainer::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	// 1st bit to indicate empty tag container or not (empty tag containers are frequently replicated). Early out if empty.
	uint8 IsEmpty = DNATags.Num() == 0;
	Ar.SerializeBits(&IsEmpty, 1);
	if (IsEmpty)
	{
		if (DNATags.Num() > 0)
		{
			Reset();
		}
		bOutSuccess = true;
		return true;
	}

	// -------------------------------------------------------

	int32 NumBitsForContainerSize = UDNATagsManager::Get().NumBitsForContainerSize;

	if (Ar.IsSaving())
	{
		uint8 NumTags = DNATags.Num();
		uint8 MaxSize = (1 << NumBitsForContainerSize);
		if (!ensureMsgf(NumTags < MaxSize, TEXT("TagContainer has %d elements when max is %d! Tags: %s"), NumTags, MaxSize, *ToStringSimple()))
		{
			NumTags = MaxSize - 1;
		}
		
		Ar.SerializeBits(&NumTags, NumBitsForContainerSize);
		for (int32 idx=0; idx < NumTags;++idx)
		{
			FDNATag& Tag = DNATags[idx];
			Tag.NetSerialize_Packed(Ar, Map, bOutSuccess);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			UDNATagsManager::Get().NotifyTagReplicated(Tag, true);
#endif
		}
	}
	else
	{
		// No Common Container tags, just replicate this like normal
		uint8 NumTags = 0;
		Ar.SerializeBits(&NumTags, NumBitsForContainerSize);

		DNATags.Empty(NumTags);
		DNATags.AddDefaulted(NumTags);
		for (uint8 idx = 0; idx < NumTags; ++idx)
		{
			DNATags[idx].NetSerialize_Packed(Ar, Map, bOutSuccess);
		}
		FillParentTags();
	}


	bOutSuccess  = true;
	return true;
}

FText FDNATagContainer::ToMatchingText(EDNAContainerMatchType MatchType, bool bInvertCondition) const
{
	enum class EMatchingTypes : int8
	{
		Inverted	= 0x01,
		All			= 0x02
	};

#define LOCTEXT_NAMESPACE "FDNATagContainer"
	const FText MatchingDescription[] =
	{
		LOCTEXT("MatchesAnyDNATags", "Has any tags in set: {DNATagSet}"),
		LOCTEXT("NotMatchesAnyDNATags", "Does not have any tags in set: {DNATagSet}"),
		LOCTEXT("MatchesAllDNATags", "Has all tags in set: {DNATagSet}"),
		LOCTEXT("NotMatchesAllDNATags", "Does not have all tags in set: {DNATagSet}")
	};
#undef LOCTEXT_NAMESPACE

	int32 DescriptionIndex = bInvertCondition ? static_cast<int32>(EMatchingTypes::Inverted) : 0;
	switch (MatchType)
	{
		case EDNAContainerMatchType::All:
			DescriptionIndex |= static_cast<int32>(EMatchingTypes::All);
			break;

		case EDNAContainerMatchType::Any:
			break;

		default:
			UE_LOG(LogDNATags, Warning, TEXT("Invalid value for TagsToMatch (EDNAContainerMatchType) %d.  Should only be Any or All."), static_cast<int32>(MatchType));
			break;
	}

	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("DNATagSet"), FText::FromString(*ToString()));
	return FText::Format(MatchingDescription[DescriptionIndex], Arguments);
}

bool FDNATag::ComplexMatches(TEnumAsByte<EDNATagMatchType::Type> MatchTypeOne, const FDNATag& Other, TEnumAsByte<EDNATagMatchType::Type> MatchTypeTwo) const
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return UDNATagsManager::Get().DNATagsMatch(*this, MatchTypeOne, Other, MatchTypeTwo);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

DECLARE_CYCLE_STAT(TEXT("FDNATag::GetSingleTagContainer"), STAT_FDNATag_GetSingleTagContainer, STATGROUP_DNATags);

const FDNATagContainer& FDNATag::GetSingleTagContainer() const
{
	SCOPE_CYCLE_COUNTER(STAT_FDNATag_GetSingleTagContainer);

	const FDNATagContainer* TagContainer = UDNATagsManager::Get().GetSingleTagContainer(*this);

	if (TagContainer)
	{
		return *TagContainer;
	}

	// This should always be invalid if the node is missing
	ensure(!IsValid());

	return FDNATagContainer::EmptyContainer;
}

FDNATag FDNATag::RequestDNATag(FName TagName, bool ErrorIfNotFound)
{
	return UDNATagsManager::Get().RequestDNATag(TagName, ErrorIfNotFound);
}

FDNATagContainer FDNATag::GetDNATagParents() const
{
	return UDNATagsManager::Get().RequestDNATagParents(*this);
}

DECLARE_CYCLE_STAT(TEXT("FDNATag::MatchesTag"), STAT_FDNATag_MatchesTag, STATGROUP_DNATags);

bool FDNATag::MatchesTag(const FDNATag& TagToCheck) const
{
	SCOPE_CYCLE_COUNTER(STAT_FDNATag_MatchesTag);

	const FDNATagContainer* TagContainer = UDNATagsManager::Get().GetSingleTagContainer(*this);

	if (TagContainer)
	{
		return TagContainer->HasTag(TagToCheck);
	}

	// This should always be invalid if the node is missing
	ensure(!IsValid());

	return false;
}

DECLARE_CYCLE_STAT(TEXT("FDNATag::MatchesAny"), STAT_FDNATag_MatchesAny, STATGROUP_DNATags);

bool FDNATag::MatchesAny(const FDNATagContainer& ContainerToCheck) const
{
	SCOPE_CYCLE_COUNTER(STAT_FDNATag_MatchesAny);

	const FDNATagContainer* TagContainer = UDNATagsManager::Get().GetSingleTagContainer(*this);

	if (TagContainer)
	{
		return TagContainer->HasAny(ContainerToCheck);
	}

	// This should always be invalid if the node is missing
	ensure(!IsValid());

	return false;
}

int32 FDNATag::MatchesTagDepth(const FDNATag& TagToCheck) const
{
	return UDNATagsManager::Get().DNATagsMatchDepth(*this, TagToCheck);
}

FDNATag::FDNATag(FName Name)
	: TagName(Name)
{
	// This constructor is used to bypass the table check and is only usable by DNATagManager
}

bool FDNATag::SerializeFromMismatchedTag(const FPropertyTag& Tag, FArchive& Ar)
{
	if (Tag.Type == NAME_NameProperty)
	{
		Ar << TagName;
		return true;
	}
	return false;
}

FDNATag FDNATag::RequestDirectParent() const
{
	return UDNATagsManager::Get().RequestDNATagDirectParent(*this);
}

DECLARE_CYCLE_STAT(TEXT("FDNATag::NetSerialize"), STAT_FDNATag_NetSerialize, STATGROUP_DNATags);

bool FDNATag::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (Ar.IsSaving())
	{
		UDNATagsManager::Get().NotifyTagReplicated(*this, false);
	}
#endif

	NetSerialize_Packed(Ar, Map, bOutSuccess);

	bOutSuccess = true;
	return true;
}

static TSharedPtr<FNetFieldExportGroup> CreateNetfieldExportGroupForNetworkDNATags(const UDNATagsManager& TagManager, const TCHAR* NetFieldExportGroupName)
{
	TSharedPtr<FNetFieldExportGroup> NetFieldExportGroup = TSharedPtr<FNetFieldExportGroup>(new FNetFieldExportGroup());

	const TArray<TSharedPtr<FDNATagNode>>& NetworkDNATagNodeIndex = TagManager.GetNetworkDNATagNodeIndex();

	NetFieldExportGroup->PathName = NetFieldExportGroupName;
	NetFieldExportGroup->NetFieldExports.SetNum(NetworkDNATagNodeIndex.Num());

	for (int32 i = 0; i < NetworkDNATagNodeIndex.Num(); i++)
	{
		FNetFieldExport NetFieldExport(
			i,
			0,
			NetworkDNATagNodeIndex[i]->GetCompleteTagString(),
			TEXT(""));

		NetFieldExportGroup->NetFieldExports[i] = NetFieldExport;
	}

	return NetFieldExportGroup;
}

bool FDNATag::NetSerialize_Packed(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	SCOPE_CYCLE_COUNTER(STAT_FDNATag_NetSerialize);

	UDNATagsManager& TagManager = UDNATagsManager::Get();

	if (TagManager.ShouldUseFastReplication())
	{
		FDNATagNetIndex NetIndex = INVALID_TAGNETINDEX;

		UPackageMapClient* PackageMapClient = Cast<UPackageMapClient>(Map);
		const bool bIsReplay = PackageMapClient && PackageMapClient->GetConnection() && PackageMapClient->GetConnection()->InternalAck;

		TSharedPtr<FNetFieldExportGroup> NetFieldExportGroup;

		if (bIsReplay)
		{
			// For replays, use a net field export group to guarantee we can send the name reliably (without having to rely on the client having a deterministic NetworkDNATagNodeIndex array)
			const TCHAR* NetFieldExportGroupName = TEXT("NetworkDNATagNodeIndex");

			// Find this net field export group
			NetFieldExportGroup = PackageMapClient->GetNetFieldExportGroup(NetFieldExportGroupName);

			if (Ar.IsSaving())
			{
				// If we didn't find it, we need to create it (only when saving though, it should be here on load since it was exported at save time)
				if (!NetFieldExportGroup.IsValid())
				{
					NetFieldExportGroup = CreateNetfieldExportGroupForNetworkDNATags(TagManager, NetFieldExportGroupName);

					PackageMapClient->AddNetFieldExportGroup(NetFieldExportGroupName, NetFieldExportGroup);
				}

				NetIndex = TagManager.GetNetIndexFromTag(*this);

				if (NetIndex != TagManager.InvalidTagNetIndex && NetIndex != INVALID_TAGNETINDEX)
				{
					PackageMapClient->TrackNetFieldExport(NetFieldExportGroup.Get(), NetIndex);
				}
				else
				{
					NetIndex = INVALID_TAGNETINDEX;		// We can't save InvalidTagNetIndex, since the remote side could have a different value for this
				}
			}

			uint32 NetIndex32 = NetIndex;
			Ar.SerializeIntPacked(NetIndex32);
			NetIndex = NetIndex32;

			if (Ar.IsLoading())
			{
				// Get the tag name from the net field export group entry
				if (NetIndex != INVALID_TAGNETINDEX && ensure(NetFieldExportGroup.IsValid()) && ensure(NetIndex < NetFieldExportGroup->NetFieldExports.Num()))
				{
					TagName = FName(*NetFieldExportGroup->NetFieldExports[NetIndex].Name);

					// Validate the tag name
					const FDNATag Tag = UDNATagsManager::Get().RequestDNATag(TagName, false);

					// Warn (once) if the tag isn't found
					if (!Tag.IsValid() && !NetFieldExportGroup->NetFieldExports[NetIndex].bIncompatible)
					{ 
						UE_LOG(LogDNATags, Warning, TEXT( "DNA tag not found (marking incompatible): %s"), *TagName.ToString());
						NetFieldExportGroup->NetFieldExports[NetIndex].bIncompatible = true;
					}

					TagName = Tag.TagName;
				}
				else
				{
					TagName = NAME_None;
				}
			}

			bOutSuccess = true;
			return true;
		}

		if (Ar.IsSaving())
		{
			NetIndex = TagManager.GetNetIndexFromTag(*this);
			
			SerializeTagNetIndexPacked(Ar, NetIndex, TagManager.NetIndexFirstBitSegment, TagManager.NetIndexTrueBitNum);
		}
		else
		{
			SerializeTagNetIndexPacked(Ar, NetIndex, TagManager.NetIndexFirstBitSegment, TagManager.NetIndexTrueBitNum);
			TagName = TagManager.GetTagNameFromNetIndex(NetIndex);
		}
	}
	else
	{
		Ar << TagName;
	}

	bOutSuccess = true;
	return true;
}

void FDNATag::PostSerialize(const FArchive& Ar)
{
	// This only happens for tags that are not nested inside a container, containers handle redirectors themselves
	// Only do redirects for real loads, not for duplicates or recompiles
	if (Ar.IsLoading() && Ar.IsPersistent() && !(Ar.GetPortFlags() & PPF_Duplicate) && !(Ar.GetPortFlags() & PPF_DuplicateForPIE))
	{
		// Rename any tags that may have changed by the ini file.
		UDNATagsManager::Get().RedirectSingleDNATag(*this, Ar.GetSerializedProperty());
	}

	if (Ar.IsSaving() && IsValid())
	{
		// This marks the saved name for later searching
		Ar.MarkSearchableName(FDNATag::StaticStruct(), TagName);
	}
}

bool FDNATag::ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText)
{
	FString ImportedTag = TEXT("");
	const TCHAR* NewBuffer = UPropertyHelpers::ReadToken(Buffer, ImportedTag, true);
	if (!NewBuffer)
	{
		// Failed to read buffer. Maybe normal ImportText will work.
		return false;
	}

	if (ImportedTag == TEXT("None") || ImportedTag.IsEmpty())
	{
		// TagName was none
		TagName = NAME_None;
		return true;
	}

	if (ImportedTag[0] == '(')
	{
		// Let normal ImportText handle this. It appears to be prepared for it.
		return false;
	}

	FName ImportedTagName = FName(*ImportedTag);
	if (UDNATagsManager::Get().ValidateTagCreation(ImportedTagName))
	{
		// We found the tag. Assign it here.
		TagName = ImportedTagName;
		return true;
	}

	// Let normal ImportText try.
	return false;
}

void FDNATag::FromExportString(FString ExportString)
{
	TagName = NAME_None;

	FOutputDeviceNull NullOut;
	FDNATag::StaticStruct()->ImportText(*ExportString, this, nullptr, 0, &NullOut, TEXT("FDNATag"), true);
}

FDNATagQuery::FDNATagQuery()
	: TokenStreamVersion(EDNATagQueryStreamVersion::LatestVersion)
{
}

FDNATagQuery::FDNATagQuery(FDNATagQuery const& Other)
{
	*this = Other;
}

FDNATagQuery::FDNATagQuery(FDNATagQuery&& Other)
{
	*this = MoveTemp(Other);
}

/** Assignment/Equality operators */
FDNATagQuery& FDNATagQuery::operator=(FDNATagQuery const& Other)
{
	if (this != &Other)
	{
		TokenStreamVersion = Other.TokenStreamVersion;
		TagDictionary = Other.TagDictionary;
		QueryTokenStream = Other.QueryTokenStream;
		UserDescription = Other.UserDescription;
		AutoDescription = Other.AutoDescription;
	}
	return *this;
}

FDNATagQuery& FDNATagQuery::operator=(FDNATagQuery&& Other)
{
	TokenStreamVersion = Other.TokenStreamVersion;
	TagDictionary = MoveTemp(Other.TagDictionary);
	QueryTokenStream = MoveTemp(Other.QueryTokenStream);
	UserDescription = MoveTemp(Other.UserDescription);
	AutoDescription = MoveTemp(Other.AutoDescription);
	return *this;
}

bool FDNATagQuery::Matches(FDNATagContainer const& Tags) const
{
	FQueryEvaluator QE(*this);
	return QE.Eval(Tags);
}

bool FDNATagQuery::IsEmpty() const
{
	return (QueryTokenStream.Num() == 0);
}

void FDNATagQuery::Clear()
{
	*this = FDNATagQuery::EmptyQuery;
}

void FDNATagQuery::GetQueryExpr(FDNATagQueryExpression& OutExpr) const
{
	// build the FExpr tree from the token stream and return it
	FQueryEvaluator QE(*this);
	QE.Read(OutExpr);
}

void FDNATagQuery::Build(FDNATagQueryExpression& RootQueryExpr, FString InUserDescription)
{
	TokenStreamVersion = EDNATagQueryStreamVersion::LatestVersion;
	UserDescription = InUserDescription;

	// Reserve size here is arbitrary, goal is to minimizing reallocs while being respectful of mem usage
	QueryTokenStream.Reset(128);
	TagDictionary.Reset();

	// add stream version first
	QueryTokenStream.Add(EDNATagQueryStreamVersion::LatestVersion);

	// emit the query
	QueryTokenStream.Add(1);		// true to indicate is has a root expression
	RootQueryExpr.EmitTokens(QueryTokenStream, TagDictionary);
}

// static 
FDNATagQuery FDNATagQuery::BuildQuery(FDNATagQueryExpression& RootQueryExpr, FString InDescription)
{
	FDNATagQuery Q;
	Q.Build(RootQueryExpr, InDescription);
	return Q;
}

//static 
FDNATagQuery FDNATagQuery::MakeQuery_MatchAnyTags(FDNATagContainer const& InTags)
{
	return FDNATagQuery::BuildQuery
	(
		FDNATagQueryExpression()
		.AnyTagsMatch()
		.AddTags(InTags)
	);
}

//static
FDNATagQuery FDNATagQuery::MakeQuery_MatchAllTags(FDNATagContainer const& InTags)
{
	return FDNATagQuery::BuildQuery
		(
		FDNATagQueryExpression()
		.AllTagsMatch()
		.AddTags(InTags)
		);
}

// static
FDNATagQuery FDNATagQuery::MakeQuery_MatchNoTags(FDNATagContainer const& InTags)
{
	return FDNATagQuery::BuildQuery
		(
		FDNATagQueryExpression()
		.NoTagsMatch()
		.AddTags(InTags)
		);
}


#if WITH_EDITOR

UEditableDNATagQuery* FQueryEvaluator::CreateEditableQuery()
{
	CurStreamIdx = 0;

	UEditableDNATagQuery* const EditableQuery = NewObject<UEditableDNATagQuery>(GetTransientPackage(), NAME_None, RF_Transactional);

	// start parsing the set
	Version = GetToken();
	if (!bReadError)
	{
		uint8 const bHasRootExpression = GetToken();
		if (!bReadError && bHasRootExpression)
		{
			EditableQuery->RootExpression = ReadEditableQueryExpr(EditableQuery);
		}
	}
	ensure(CurStreamIdx == Query.QueryTokenStream.Num());

	EditableQuery->UserDescription = Query.UserDescription;

	return EditableQuery;
}

UEditableDNATagQueryExpression* FQueryEvaluator::ReadEditableQueryExpr(UObject* ExprOuter)
{
	EDNATagQueryExprType::Type const ExprType = (EDNATagQueryExprType::Type) GetToken();
	if (bReadError)
	{
		return nullptr;
	}

	UClass* ExprClass = nullptr;
	switch (ExprType)
	{
	case EDNATagQueryExprType::AnyTagsMatch:
		ExprClass = UEditableDNATagQueryExpression_AnyTagsMatch::StaticClass();
		break;
	case EDNATagQueryExprType::AllTagsMatch:
		ExprClass = UEditableDNATagQueryExpression_AllTagsMatch::StaticClass();
		break;
	case EDNATagQueryExprType::NoTagsMatch:
		ExprClass = UEditableDNATagQueryExpression_NoTagsMatch::StaticClass();
		break;
	case EDNATagQueryExprType::AnyExprMatch:
		ExprClass = UEditableDNATagQueryExpression_AnyExprMatch::StaticClass();
		break;
	case EDNATagQueryExprType::AllExprMatch:
		ExprClass = UEditableDNATagQueryExpression_AllExprMatch::StaticClass();
		break;
	case EDNATagQueryExprType::NoExprMatch:
		ExprClass = UEditableDNATagQueryExpression_NoExprMatch::StaticClass();
		break;
	}

	UEditableDNATagQueryExpression* NewExpr = nullptr;
	if (ExprClass)
	{
		NewExpr = NewObject<UEditableDNATagQueryExpression>(ExprOuter, ExprClass, NAME_None, RF_Transactional);
		if (NewExpr)
		{
			switch (ExprType)
			{
			case EDNATagQueryExprType::AnyTagsMatch:
			case EDNATagQueryExprType::AllTagsMatch:
			case EDNATagQueryExprType::NoTagsMatch:
				ReadEditableQueryTags(NewExpr);
				break;
			case EDNATagQueryExprType::AnyExprMatch:
			case EDNATagQueryExprType::AllExprMatch:
			case EDNATagQueryExprType::NoExprMatch:
				ReadEditableQueryExprList(NewExpr);
				break;
			}
		}
	}

	return NewExpr;
}

void FQueryEvaluator::ReadEditableQueryTags(UEditableDNATagQueryExpression* EditableQueryExpr)
{
	// find the tag container to read into
	FDNATagContainer* Tags = nullptr;
	if (EditableQueryExpr->IsA(UEditableDNATagQueryExpression_AnyTagsMatch::StaticClass()))
	{
		Tags = &((UEditableDNATagQueryExpression_AnyTagsMatch*)EditableQueryExpr)->Tags;
	}
	else if (EditableQueryExpr->IsA(UEditableDNATagQueryExpression_AllTagsMatch::StaticClass()))
	{
		Tags = &((UEditableDNATagQueryExpression_AllTagsMatch*)EditableQueryExpr)->Tags;
	}
	else if (EditableQueryExpr->IsA(UEditableDNATagQueryExpression_NoTagsMatch::StaticClass()))
	{
		Tags = &((UEditableDNATagQueryExpression_NoTagsMatch*)EditableQueryExpr)->Tags;
	}
	ensure(Tags);

	if (Tags)
	{
		// parse tag set
		int32 const NumTags = GetToken();
		if (bReadError)
		{
			return;
		}

		for (int32 Idx = 0; Idx < NumTags; ++Idx)
		{
			int32 const TagIdx = GetToken();
			if (bReadError)
			{
				return;
			}

			FDNATag const Tag = Query.GetTagFromIndex(TagIdx);
			Tags->AddTag(Tag);
		}
	}
}

void FQueryEvaluator::ReadEditableQueryExprList(UEditableDNATagQueryExpression* EditableQueryExpr)
{
	// find the tag container to read into
	TArray<UEditableDNATagQueryExpression*>* ExprList = nullptr;
	if (EditableQueryExpr->IsA(UEditableDNATagQueryExpression_AnyExprMatch::StaticClass()))
	{
		ExprList = &((UEditableDNATagQueryExpression_AnyExprMatch*)EditableQueryExpr)->Expressions;
	}
	else if (EditableQueryExpr->IsA(UEditableDNATagQueryExpression_AllExprMatch::StaticClass()))
	{
		ExprList = &((UEditableDNATagQueryExpression_AllExprMatch*)EditableQueryExpr)->Expressions;
	}
	else if (EditableQueryExpr->IsA(UEditableDNATagQueryExpression_NoExprMatch::StaticClass()))
	{
		ExprList = &((UEditableDNATagQueryExpression_NoExprMatch*)EditableQueryExpr)->Expressions;
	}
	ensure(ExprList);

	if (ExprList)
	{
		// parse expr set
		int32 const NumExprs = GetToken();
		if (bReadError)
		{
			return;
		}

		for (int32 Idx = 0; Idx < NumExprs; ++Idx)
		{
			UEditableDNATagQueryExpression* const NewExpr = ReadEditableQueryExpr(EditableQueryExpr);
			ExprList->Add(NewExpr);
		}
	}
}

UEditableDNATagQuery* FDNATagQuery::CreateEditableQuery()
{
	FQueryEvaluator QE(*this);
	return QE.CreateEditableQuery();
}

void FDNATagQuery::BuildFromEditableQuery(UEditableDNATagQuery& EditableQuery)
{
	QueryTokenStream.Reset();
	TagDictionary.Reset();

	UserDescription = EditableQuery.UserDescription;

	// add stream version first
	QueryTokenStream.Add(EDNATagQueryStreamVersion::LatestVersion);
	EditableQuery.EmitTokens(QueryTokenStream, TagDictionary, &AutoDescription);
}

FString UEditableDNATagQuery::GetTagQueryExportText(FDNATagQuery const& TagQuery)
{
	TagQueryExportText_Helper = TagQuery;
	UProperty* const TQProperty = FindField<UProperty>(GetClass(), TEXT("TagQueryExportText_Helper"));

	FString OutString;
	TQProperty->ExportTextItem(OutString, (void*)&TagQueryExportText_Helper, (void*)&TagQueryExportText_Helper, this, 0);
	return OutString;
}

void UEditableDNATagQuery::EmitTokens(TArray<uint8>& TokenStream, TArray<FDNATag>& TagDictionary, FString* DebugString) const
{
	if (DebugString)
	{
		// start with a fresh string
		DebugString->Empty();
	}

	if (RootExpression)
	{
		TokenStream.Add(1);		// true if has a root expression
		RootExpression->EmitTokens(TokenStream, TagDictionary, DebugString);
	}
	else
	{
		TokenStream.Add(0);		// false if no root expression
		if (DebugString)
		{
			DebugString->Append(TEXT("undefined"));
		}
	}
}

void UEditableDNATagQueryExpression::EmitTagTokens(FDNATagContainer const& TagsToEmit, TArray<uint8>& TokenStream, TArray<FDNATag>& TagDictionary, FString* DebugString) const
{
	uint8 const NumTags = (uint8)TagsToEmit.Num();
	TokenStream.Add(NumTags);

	bool bFirstTag = true;

	for (auto T : TagsToEmit)
	{
		int32 TagIdx = TagDictionary.AddUnique(T);
		check(TagIdx <= 255);
		TokenStream.Add((uint8)TagIdx);

		if (DebugString)
		{
			if (bFirstTag == false)
			{
				DebugString->Append(TEXT(","));
			}

			DebugString->Append(TEXT(" "));
			DebugString->Append(T.ToString());
		}

		bFirstTag = false;
	}
}

void UEditableDNATagQueryExpression::EmitExprListTokens(TArray<UEditableDNATagQueryExpression*> const& ExprList, TArray<uint8>& TokenStream, TArray<FDNATag>& TagDictionary, FString* DebugString) const
{
	uint8 const NumExprs = (uint8)ExprList.Num();
	TokenStream.Add(NumExprs);

	bool bFirstExpr = true;
	
	for (auto E : ExprList)
	{
		if (DebugString)
		{
			if (bFirstExpr == false)
			{
				DebugString->Append(TEXT(","));
			}

			DebugString->Append(TEXT(" "));
		}

		if (E)
		{
			E->EmitTokens(TokenStream, TagDictionary, DebugString);
		}
		else
		{
			// null expression
			TokenStream.Add(EDNATagQueryExprType::Undefined);
			if (DebugString)
			{
				DebugString->Append(TEXT("undefined"));
			}
		}

		bFirstExpr = false;
	}
}

void UEditableDNATagQueryExpression_AnyTagsMatch::EmitTokens(TArray<uint8>& TokenStream, TArray<FDNATag>& TagDictionary, FString* DebugString) const
{
	TokenStream.Add(EDNATagQueryExprType::AnyTagsMatch);

	if (DebugString)
	{
		DebugString->Append(TEXT(" ANY("));
	}

	EmitTagTokens(Tags, TokenStream, TagDictionary, DebugString);

	if (DebugString)
	{
		DebugString->Append(TEXT(" )"));
	}
}

void UEditableDNATagQueryExpression_AllTagsMatch::EmitTokens(TArray<uint8>& TokenStream, TArray<FDNATag>& TagDictionary, FString* DebugString) const
{
	TokenStream.Add(EDNATagQueryExprType::AllTagsMatch);

	if (DebugString)
	{
		DebugString->Append(TEXT(" ALL("));
	}
	
	EmitTagTokens(Tags, TokenStream, TagDictionary, DebugString);

	if (DebugString)
	{
		DebugString->Append(TEXT(" )"));
	}
}

void UEditableDNATagQueryExpression_NoTagsMatch::EmitTokens(TArray<uint8>& TokenStream, TArray<FDNATag>& TagDictionary, FString* DebugString) const
{
	TokenStream.Add(EDNATagQueryExprType::NoTagsMatch);
	EmitTagTokens(Tags, TokenStream, TagDictionary, DebugString);

	if (DebugString)
	{
		DebugString->Append(TEXT(" )"));
	}
}

void UEditableDNATagQueryExpression_AnyExprMatch::EmitTokens(TArray<uint8>& TokenStream, TArray<FDNATag>& TagDictionary, FString* DebugString) const
{
	TokenStream.Add(EDNATagQueryExprType::AnyExprMatch);

	if (DebugString)
	{
		DebugString->Append(TEXT(" ANY("));
	}

	EmitExprListTokens(Expressions, TokenStream, TagDictionary, DebugString);

	if (DebugString)
	{
		DebugString->Append(TEXT(" )"));
	}
}

void UEditableDNATagQueryExpression_AllExprMatch::EmitTokens(TArray<uint8>& TokenStream, TArray<FDNATag>& TagDictionary, FString* DebugString) const
{
	TokenStream.Add(EDNATagQueryExprType::AllExprMatch);

	if (DebugString)
	{
		DebugString->Append(TEXT(" ALL("));
	}

	EmitExprListTokens(Expressions, TokenStream, TagDictionary, DebugString);

	if (DebugString)
	{
		DebugString->Append(TEXT(" )"));
	}
}

void UEditableDNATagQueryExpression_NoExprMatch::EmitTokens(TArray<uint8>& TokenStream, TArray<FDNATag>& TagDictionary, FString* DebugString) const
{
	TokenStream.Add(EDNATagQueryExprType::NoExprMatch);

	if (DebugString)
	{
		DebugString->Append(TEXT(" NONE("));
	}

	EmitExprListTokens(Expressions, TokenStream, TagDictionary, DebugString);

	if (DebugString)
	{
		DebugString->Append(TEXT(" )"));
	}
}
#endif	// WITH_EDITOR


FDNATagQueryExpression& FDNATagQueryExpression::AddTag(FName TagName)
{
	FDNATag const Tag = UDNATagsManager::Get().RequestDNATag(TagName);
	return AddTag(Tag);
}

void FDNATagQueryExpression::EmitTokens(TArray<uint8>& TokenStream, TArray<FDNATag>& TagDictionary) const
{
	// emit exprtype
	TokenStream.Add(ExprType);

	// emit exprdata
	switch (ExprType)
	{
	case EDNATagQueryExprType::AnyTagsMatch:
	case EDNATagQueryExprType::AllTagsMatch:
	case EDNATagQueryExprType::NoTagsMatch:
	{
		// emit tagset
		uint8 NumTags = (uint8)TagSet.Num();
		TokenStream.Add(NumTags);

		for (auto Tag : TagSet)
		{
			int32 TagIdx = TagDictionary.AddUnique(Tag);
			check(TagIdx <= 254);		// we reserve token 255 for internal use, so 254 is max unique tags
			TokenStream.Add((uint8)TagIdx);
		}
	}
	break;

	case EDNATagQueryExprType::AnyExprMatch:
	case EDNATagQueryExprType::AllExprMatch:
	case EDNATagQueryExprType::NoExprMatch:
	{
		// emit tagset
		uint8 NumExprs = (uint8)ExprSet.Num();
		TokenStream.Add(NumExprs);

		for (auto& E : ExprSet)
		{
			E.EmitTokens(TokenStream, TagDictionary);
		}
	}
	break;
	default:
		break;
	}
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

static void DNATagPrintReplicationMap()
{
	UDNATagsManager::Get().PrintReplicationFrequencyReport();
}

FAutoConsoleCommand DNATagPrintReplicationMapCmd(
	TEXT("DNATags.PrintReport"), 
	TEXT( "Prints frequency of DNA tags" ), 
	FConsoleCommandDelegate::CreateStatic(DNATagPrintReplicationMap)
);


static void TagPackingTest()
{
	for (int32 TotalNetIndexBits=1; TotalNetIndexBits <= 16; ++TotalNetIndexBits)
	{
		for (int32 NetIndexBitsPerComponent=0; NetIndexBitsPerComponent <= TotalNetIndexBits; NetIndexBitsPerComponent++)
		{
			for (int32 NetIndex=0; NetIndex < FMath::Pow(2, TotalNetIndexBits); ++NetIndex)
			{
				FDNATagNetIndex NI = NetIndex;

				FNetBitWriter	BitWriter(nullptr, 1024 * 8);
				SerializeTagNetIndexPacked(BitWriter, NI, NetIndexBitsPerComponent, TotalNetIndexBits);

				FNetBitReader	Reader(nullptr, BitWriter.GetData(), BitWriter.GetNumBits());

				FDNATagNetIndex NewIndex;
				SerializeTagNetIndexPacked(Reader, NewIndex, NetIndexBitsPerComponent, TotalNetIndexBits);

				if (ensureAlways((NewIndex == NI)) == false)
				{
					NetIndex--;
					continue;
				}
			}
		}
	}

	UE_LOG(LogDNATags, Warning, TEXT("TagPackingTest completed!"));

}

FAutoConsoleCommand TagPackingTestCmd(
	TEXT("DNATags.PackingTest"), 
	TEXT( "Prints frequency of DNA tags" ), 
	FConsoleCommandDelegate::CreateStatic(TagPackingTest)
);

#endif
