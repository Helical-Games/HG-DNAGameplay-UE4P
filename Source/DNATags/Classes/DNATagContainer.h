// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "Stats/Stats.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "DNATagContainer.generated.h"

class UEditableDNATagQuery;
struct FDNATagContainer;
struct FPropertyTag;

DECLARE_LOG_CATEGORY_EXTERN(LogDNATags, Log, All);

DECLARE_STATS_GROUP(TEXT("DNA Tags"), STATGROUP_DNATags, STATCAT_Advanced);

DECLARE_CYCLE_STAT_EXTERN(TEXT("FDNATagContainer::HasTag"), STAT_FDNATagContainer_HasTag, STATGROUP_DNATags, DNATAGS_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("FDNATagContainer::DoesTagContainerMatch"), STAT_FDNATagContainer_DoesTagContainerMatch, STATGROUP_DNATags, DNATAGS_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("UDNATagsManager::DNATagsMatch"), STAT_UDNATagsManager_DNATagsMatch, STATGROUP_DNATags, DNATAGS_API);

struct FDNATagContainer;

// DEPRECATED ENUMS
UENUM(BlueprintType)
namespace EDNATagMatchType
{
	enum Type
	{
		Explicit,			// This will check for a match against just this tag
		IncludeParentTags,	// This will also check for matches against all parent tags
	};
}

UENUM(BlueprintType)
enum class EDNAContainerMatchType : uint8
{
	Any,	//	Means the filter is populated by any tag matches in this container.
	All		//	Means the filter is only populated if all of the tags in this container match.
};

typedef uint16 FDNATagNetIndex;
#define INVALID_TAGNETINDEX MAX_uint16

/** A single DNA tag, which represents a hierarchical name of the form x.y that is registered in the DNATagsManager */
USTRUCT(BlueprintType, meta = (HasNativeMake = "DNATags.BlueprintDNATagLibrary.MakeLiteralDNATag", HasNativeBreak = "DNATags.BlueprintDNATagLibrary.GetTagName"))
struct DNATAGS_API FDNATag
{
	GENERATED_USTRUCT_BODY()

	/** Constructors */
	FDNATag()
	{
	}

	/**
	 * Gets the FDNATag that corresponds to the TagName
	 *
	 * @param TagName The Name of the tag to search for
	 * 
	 * @param ErrorIfNotfound: ensure() that tag exists.
	 * 
	 * @return Will return the corresponding FDNATag or an empty one if not found.
	 */
	static FDNATag RequestDNATag(FName TagName, bool ErrorIfNotFound=true);

	/** Operators */
	FORCEINLINE bool operator==(FDNATag const& Other) const
	{
		return TagName == Other.TagName;
	}

	FORCEINLINE bool operator!=(FDNATag const& Other) const
	{
		return TagName != Other.TagName;
	}

	FORCEINLINE bool operator<(FDNATag const& Other) const
	{
		return TagName < Other.TagName;
	}

	/**
	 * Determine if this tag matches TagToCheck, expanding our parent tags
	 * "A.1".MatchesTag("A") will return True, "A".MatchesTag("A.1") will return False
	 * If TagToCheck is not Valid it will always return False
	 * 
	 * @return True if this tag matches TagToCheck
	 */
	bool MatchesTag(const FDNATag& TagToCheck) const;

	/**
	 * Determine if TagToCheck is valid and exactly matches this tag
	 * "A.1".MatchesTagExact("A") will return False
	 * If TagToCheck is not Valid it will always return False
	 * 
	 * @return True if TagToCheck is Valid and is exactly this tag
	 */
	FORCEINLINE bool MatchesTagExact(const FDNATag& TagToCheck) const
	{
		if (!TagToCheck.IsValid())
		{
			return false;
		}
		// Only check check explicit tag list
		return TagName == TagToCheck.TagName;
	}

	/**
	 * Check to see how closely two FDNATags match. Higher values indicate more matching terms in the tags.
	 *
	 * @param TagToCheck	Tag to match against
	 *
	 * @return The depth of the match, higher means they are closer to an exact match
	 */
	int32 MatchesTagDepth(const FDNATag& TagToCheck) const;

	/**
	 * Checks if this tag matches ANY of the tags in the specified container, also checks against our parent tags
	 * "A.1".MatchesAny({"A","B"}) will return True, "A".MatchesAny({"A.1","B"}) will return False
	 * If ContainerToCheck is empty/invalid it will always return False
	 *
	 * @return True if this tag matches ANY of the tags of in ContainerToCheck
	 */
	bool MatchesAny(const FDNATagContainer& ContainerToCheck) const;

	/**
	 * Checks if this tag matches ANY of the tags in the specified container, only allowing exact matches
	 * "A.1".MatchesAny({"A","B"}) will return False
	 * If ContainerToCheck is empty/invalid it will always return False
	 *
	 * @return True if this tag matches ANY of the tags of in ContainerToCheck exactly
	 */
	bool MatchesAnyExact(const FDNATagContainer& ContainerToCheck) const;

	/** Returns whether the tag is valid or not; Invalid tags are set to NAME_None and do not exist in the game-specific global dictionary */
	FORCEINLINE bool IsValid() const
	{
		return (TagName != NAME_None);
	}

	/** Returns reference to a DNATagContainer containing only this tag */
	const FDNATagContainer& GetSingleTagContainer() const;

	/** Returns direct parent DNATag of this DNATag, calling on x.y will return x */
	FDNATag RequestDirectParent() const;

	/** Returns a new container explicitly containing the tags of this tag */
	FDNATagContainer GetDNATagParents() const;

	/** Used so we can have a TMap of this struct */
	FORCEINLINE friend uint32 GetTypeHash(const FDNATag& Tag)
	{
		return ::GetTypeHash(Tag.TagName);
	}

	/** Displays DNA tag as a string for blueprint graph usage */
	FORCEINLINE FString ToString() const
	{
		return TagName.ToString();
	}

	/** Get the tag represented as a name */
	FORCEINLINE FName GetTagName() const
	{
		return TagName;
	}

	friend FArchive& operator<<(FArchive& Ar, FDNATag& DNATag)
	{
		Ar << DNATag.TagName;
		return Ar;
	}

	/** Overridden for fast serialize */
	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	/** Handles fixup and errors. This is only called when not serializing a full FDNATagContainer */
	void PostSerialize(const FArchive& Ar);
	bool NetSerialize_Packed(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	/** Used to upgrade a Name property to a DNATag struct property */
	bool SerializeFromMismatchedTag(const FPropertyTag& Tag, FArchive& Ar);

	/** Sets from a ImportText string, used in asset registry */
	void FromExportString(FString ExportString);

	/** Handles importing tag strings without (TagName=) in it */
	bool ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText);

	/** An empty DNA Tag */
	static const FDNATag EmptyTag;

	// DEPRECATED

PRAGMA_DISABLE_DEPRECATION_WARNINGS

	/**
	 * Check to see if two FDNATags match with explicit match types
	 *
	 * @param MatchTypeOne	How we compare this tag, Explicitly or a match with any parents as well
	 * @param Other			The second tag to compare against
	 * @param MatchTypeTwo	How we compare Other tag, Explicitly or a match with any parents as well
	 * 
	 * @return True if there is a match according to the specified match types; false if not
	 */
	DEPRECATED(4.15, "Deprecated in favor of MatchesTag")
	FORCEINLINE_DEBUGGABLE bool Matches(TEnumAsByte<EDNATagMatchType::Type> MatchTypeOne, const FDNATag& Other, TEnumAsByte<EDNATagMatchType::Type> MatchTypeTwo) const
	{
		bool bResult = false;
		if (MatchTypeOne == EDNATagMatchType::Explicit && MatchTypeTwo == EDNATagMatchType::Explicit)
		{
			bResult = TagName == Other.TagName;
		}
		else
		{
			bResult = ComplexMatches(MatchTypeOne, Other, MatchTypeTwo);
		}
		return bResult;
	}
	/**
	 * Check to see if two FDNATags match
	 *
	 * @param MatchTypeOne	How we compare this tag, Explicitly or a match with any parents as well
	 * @param Other			The second tag to compare against
	 * @param MatchTypeTwo	How we compare Other tag, Explicitly or a match with any parents as well
	 * 
	 * @return True if there is a match according to the specified match types; false if not
	 */
	DEPRECATED(4.15, "Deprecated in favor of MatchesTag")
	bool ComplexMatches(TEnumAsByte<EDNATagMatchType::Type> MatchTypeOne, const FDNATag& Other, TEnumAsByte<EDNATagMatchType::Type> MatchTypeTwo) const;

PRAGMA_ENABLE_DEPRECATION_WARNINGS

private:

	/** Intentionally private so only the tag manager can use */
	explicit FDNATag(FName InTagName);

	/** This Tags Name */
	UPROPERTY(VisibleAnywhere, Category = DNATags)
	FName TagName;

	friend class UDNATagsManager;
	friend struct FDNATagContainer;
	friend struct FDNATagNode;
};

template<>
struct TStructOpsTypeTraits< FDNATag > : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithNetSerializer = true,
		WithPostSerialize = true,
		WithSerializeFromMismatchedTag = true,
		WithImportTextItem = true,
	};
};

/** A Tag Container holds a collection of FDNATags, tags are included explicitly by adding them, and implicitly from adding child tags */
USTRUCT(BlueprintType, meta = (HasNativeMake = "DNATags.BlueprintDNATagLibrary.MakeDNATagContainerFromArray", HasNativeBreak = "DNATags.BlueprintDNATagLibrary.BreakDNATagContainer"))
struct DNATAGS_API FDNATagContainer
{
	GENERATED_USTRUCT_BODY()

	/** Constructors */
	FDNATagContainer()
	{
	}

	FDNATagContainer(FDNATagContainer const& Other)
	{
		*this = Other;
	}

	/** Explicit to prevent people from accidentally using the wrong type of operation */
	explicit FDNATagContainer(const FDNATag& Tag)
	{
		AddTag(Tag);
	}

	FDNATagContainer(FDNATagContainer&& Other)
		: DNATags(MoveTemp(Other.DNATags))
		, ParentTags(MoveTemp(Other.ParentTags))
	{

	}

	~FDNATagContainer()
	{
	}

	/** Creates a container from an array of tags, this is more efficient than adding them all individually */
	template<class AllocatorType>
	static FDNATagContainer CreateFromArray(const TArray<FDNATag, AllocatorType>& SourceTags)
	{
		FDNATagContainer Container;
		Container.DNATags.Append(SourceTags);
		Container.FillParentTags();
		return Container;
	}

	/** Assignment/Equality operators */
	FDNATagContainer& operator=(FDNATagContainer const& Other);
	FDNATagContainer& operator=(FDNATagContainer&& Other);
	bool operator==(FDNATagContainer const& Other) const;
	bool operator!=(FDNATagContainer const& Other) const;

	/**
	 * Determine if TagToCheck is present in this container, also checking against parent tags
	 * {"A.1"}.HasTag("A") will return True, {"A"}.HasTag("A.1") will return False
	 * If TagToCheck is not Valid it will always return False
	 * 
	 * @return True if TagToCheck is in this container, false if it is not
	 */
	FORCEINLINE_DEBUGGABLE bool HasTag(const FDNATag& TagToCheck) const
	{
		if (!TagToCheck.IsValid())
		{
			return false;
		}
		// Check explicit and parent tag list 
		return DNATags.Contains(TagToCheck) || ParentTags.Contains(TagToCheck);
	}

	/**
	 * Determine if TagToCheck is explicitly present in this container, only allowing exact matches
	 * {"A.1"}.HasTagExact("A") will return False
	 * If TagToCheck is not Valid it will always return False
	 * 
	 * @return True if TagToCheck is in this container, false if it is not
	 */
	FORCEINLINE_DEBUGGABLE bool HasTagExact(const FDNATag& TagToCheck) const
	{
		if (!TagToCheck.IsValid())
		{
			return false;
		}
		// Only check check explicit tag list
		return DNATags.Contains(TagToCheck);
	}

	/**
	 * Checks if this container contains ANY of the tags in the specified container, also checks against parent tags
	 * {"A.1"}.HasAny({"A","B"}) will return True, {"A"}.HasAny({"A.1","B"}) will return False
	 * If ContainerToCheck is empty/invalid it will always return False
	 *
	 * @return True if this container has ANY of the tags of in ContainerToCheck
	 */
	FORCEINLINE_DEBUGGABLE bool HasAny(const FDNATagContainer& ContainerToCheck) const
	{
		if (ContainerToCheck.IsEmpty())
		{
			return false;
		}
		for (const FDNATag& OtherTag : ContainerToCheck.DNATags)
		{
			if (DNATags.Contains(OtherTag) || ParentTags.Contains(OtherTag))
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * Checks if this container contains ANY of the tags in the specified container, only allowing exact matches
	 * {"A.1"}.HasAny({"A","B"}) will return False
	 * If ContainerToCheck is empty/invalid it will always return False
	 *
	 * @return True if this container has ANY of the tags of in ContainerToCheck
	 */
	FORCEINLINE_DEBUGGABLE bool HasAnyExact(const FDNATagContainer& ContainerToCheck) const
	{
		if (ContainerToCheck.IsEmpty())
		{
			return false;
		}
		for (const FDNATag& OtherTag : ContainerToCheck.DNATags)
		{
			if (DNATags.Contains(OtherTag))
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * Checks if this container contains ALL of the tags in the specified container, also checks against parent tags
	 * {"A.1","B.1"}.HasAll({"A","B"}) will return True, {"A","B"}.HasAll({"A.1","B.1"}) will return False
	 * If ContainerToCheck is empty/invalid it will always return True, because there were no failed checks
	 *
	 * @return True if this container has ALL of the tags of in ContainerToCheck, including if ContainerToCheck is empty
	 */
	FORCEINLINE_DEBUGGABLE bool HasAll(const FDNATagContainer& ContainerToCheck) const
	{
		if (ContainerToCheck.IsEmpty())
		{
			return true;
		}
		for (const FDNATag& OtherTag : ContainerToCheck.DNATags)
		{
			if (!DNATags.Contains(OtherTag) && !ParentTags.Contains(OtherTag))
			{
				return false;
			}
		}
		return true;
	}

	/**
	 * Checks if this container contains ALL of the tags in the specified container, only allowing exact matches
	 * {"A.1","B.1"}.HasAll({"A","B"}) will return False
	 * If ContainerToCheck is empty/invalid it will always return True, because there were no failed checks
	 *
	 * @return True if this container has ALL of the tags of in ContainerToCheck, including if ContainerToCheck is empty
	 */
	FORCEINLINE_DEBUGGABLE bool HasAllExact(const FDNATagContainer& ContainerToCheck) const
	{
		if (ContainerToCheck.IsEmpty())
		{
			return true;
		}
		for (const FDNATag& OtherTag : ContainerToCheck.DNATags)
		{
			if (!DNATags.Contains(OtherTag))
			{
				return false;
			}
		}
		return true;
	}

	/** Returns the number of explicitly added tags */
	FORCEINLINE int32 Num() const
	{
		return DNATags.Num();
	}

	/** Returns whether the container has any valid tags */
	FORCEINLINE bool IsValid() const
	{
		return DNATags.Num() > 0;
	}

	/** Returns true if container is empty */
	FORCEINLINE bool IsEmpty() const
	{
		return DNATags.Num() == 0;
	}

	/** Returns a new container explicitly containing the tags of this container and all of their parent tags */
	FDNATagContainer GetDNATagParents() const;

	/**
	 * Returns a filtered version of this container, returns all tags that match against any of the tags in OtherContainer, expanding parents
	 *
	 * @param OtherContainer		The Container to filter against
	 *
	 * @return A FDNATagContainer containing the filtered tags
	 */
	FDNATagContainer Filter(const FDNATagContainer& OtherContainer) const;

	/**
	 * Returns a filtered version of this container, returns all tags that match exactly one in OtherContainer
	 *
	 * @param OtherContainer		The Container to filter against
	 *
	 * @return A FDNATagContainer containing the filtered tags
	 */
	FDNATagContainer FilterExact(const FDNATagContainer& OtherContainer) const;

	/** 
	 * Checks if this container matches the given query.
	 *
	 * @param Query		Query we are checking against
	 *
	 * @return True if this container matches the query, false otherwise.
	 */
	bool MatchesQuery(const struct FDNATagQuery& Query) const;

	/** 
	 * Adds all the tags from one container to this container 
	 *
	 * @param Other TagContainer that has the tags you want to add to this container 
	 */
	void AppendTags(FDNATagContainer const& Other);

	/** 
	 * Adds all the tags that match between the two specified containers to this container 
	 *
	 * @param OtherA TagContainer that has the matching tags you want to add to this container, these tags have their parents expanded
	 * @param OtherB TagContainer used to check for matching tags
	 */
	void AppendMatchingTags(FDNATagContainer const& OtherA, FDNATagContainer const& OtherB);

	/**
	 * Add the specified tag to the container
	 *
	 * @param TagToAdd Tag to add to the container
	 */
	void AddTag(const FDNATag& TagToAdd);

	/**
	 * Add the specified tag to the container without checking for uniqueness
	 *
	 * @param TagToAdd Tag to add to the container
	 * 
	 * Useful when building container from another data struct (TMap for example)
	 */
	void AddTagFast(const FDNATag& TagToAdd);

	/**
	 * Adds a tag to the container and removes any direct parents, wont add if child already exists
	 *
	 * @param Tag			The tag to try and add to this container
	 * 
	 * @return True if tag was added
	 */
	bool AddLeafTag(const FDNATag& TagToAdd);

	/**
	 * Tag to remove from the container
	 * 
	 * @param TagToRemove	Tag to remove from the container
	 */
	bool RemoveTag(FDNATag TagToRemove);

	/**
	 * Removes all tags in TagsToRemove from this container
	 *
	 * @param TagsToRemove	Tags to remove from the container
	 */
	void RemoveTags(FDNATagContainer TagsToRemove);

	/** Remove all tags from the container. Will maintain slack by default */
	void Reset(int32 Slack = 0);
	
	/** Serialize the tag container */
	bool Serialize(FArchive& Ar);

	/** Efficient network serialize, takes advantage of the dictionary */
	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	/** Handles fixup after importing from text */
	bool ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText);

	/** Returns string version of container in ImportText format */
	FString ToString() const;

	/** Sets from a ImportText string, used in asset registry */
	void FromExportString(FString ExportString);

	/** Returns abbreviated human readable Tag list without parens or property names */
	FString ToStringSimple() const;

	/** Returns human readable description of what match is being looked for on the readable tag list. */
	FText ToMatchingText(EDNAContainerMatchType MatchType, bool bInvertCondition) const;

	/** Gets the explicit list of DNA tags */
	void GetDNATagArray(TArray<FDNATag>& InOutDNATags) const
	{
		InOutDNATags = DNATags;
	}

	/** Creates a const iterator for the contents of this array */
	TArray<FDNATag>::TConstIterator CreateConstIterator() const
	{
		return DNATags.CreateConstIterator();
	}

	bool IsValidIndex(int32 Index) const
	{
		return DNATags.IsValidIndex(Index);
	}

	FDNATag GetByIndex(int32 Index) const
	{
		if (IsValidIndex(Index))
		{
			return DNATags[Index];
		}
		return FDNATag();
	}	

	FDNATag First() const
	{
		return DNATags.Num() > 0 ? DNATags[0] : FDNATag();
	}

	FDNATag Last() const
	{
		return DNATags.Num() > 0 ? DNATags.Last() : FDNATag();
	}

	/** An empty DNA Tag Container */
	static const FDNATagContainer EmptyContainer;

	// DEPRECATED FUNCTIONALITY

PRAGMA_DISABLE_DEPRECATION_WARNINGS

	DEPRECATED(4.15, "Deprecated in favor of Reset")
		void RemoveAllTags(int32 Slack = 0)
	{
		Reset(Slack);
	}

	DEPRECATED(4.15, "Deprecated in favor of Reset")
		void RemoveAllTagsKeepSlack()
	{
		Reset();
	}

	/**
	 * Determine if the container has the specified tag. This forces an explicit match. 
	 * This function exists for convenience and brevity. We do not wish to use default values for ::HasTag match type parameters, to avoid confusion on what the default behavior is. (E.g., we want people to think and use the right match type).
	 * 
	 * @param TagToCheck			Tag to check if it is present in the container
	 * 
	 * @return True if the tag is in the container, false if it is not
	 */
	DEPRECATED(4.15, "Deprecated in favor of HasTagExact")
	FORCEINLINE_DEBUGGABLE bool HasTagExplicit(FDNATag const& TagToCheck) const
	{
		return HasTag(TagToCheck, EDNATagMatchType::Explicit, EDNATagMatchType::Explicit);
	}

	/**
	 * Determine if the container has the specified tag
	 * 
	 * @param TagToCheck			Tag to check if it is present in the container
	 * @param TagMatchType			Type of match to use for the tags in this container
	 * @param TagToCheckMatchType	Type of match to use for the TagToCheck Param
	 * 
	 * @return True if the tag is in the container, false if it is not
	 */
	DEPRECATED(4.15, "Deprecated in favor of HasTag with no parameters")
	FORCEINLINE_DEBUGGABLE bool HasTag(FDNATag const& TagToCheck, TEnumAsByte<EDNATagMatchType::Type> TagMatchType, TEnumAsByte<EDNATagMatchType::Type> TagToCheckMatchType) const
	{
		SCOPE_CYCLE_COUNTER(STAT_FDNATagContainer_HasTag);
		if (!TagToCheck.IsValid())
		{
			return false;
		}

		return HasTagFast(TagToCheck, TagMatchType, TagToCheckMatchType);
	}

	/** Version of above that is called from conditions where you know tag is valid */
	FORCEINLINE_DEBUGGABLE bool HasTagFast(FDNATag const& TagToCheck, TEnumAsByte<EDNATagMatchType::Type> TagMatchType, TEnumAsByte<EDNATagMatchType::Type> TagToCheckMatchType) const
	{
		bool bResult;
		if (TagToCheckMatchType == EDNATagMatchType::Explicit)
		{
			// Always check explicit
			bResult = DNATags.Contains(TagToCheck);

			if (!bResult && TagMatchType == EDNATagMatchType::IncludeParentTags)
			{
				// Check parent tags as well
				bResult = ParentTags.Contains(TagToCheck);
			}
		}
		else
		{
			bResult = ComplexHasTag(TagToCheck, TagMatchType, TagToCheckMatchType);
		}
		return bResult;
	}

	/**
	 * Determine if the container has the specified tag
	 * 
	 * @param TagToCheck			Tag to check if it is present in the container
	 * @param TagMatchType			Type of match to use for the tags in this container
	 * @param TagToCheckMatchType	Type of match to use for the TagToCheck Param
	 * 
	 * @return True if the tag is in the container, false if it is not
	 */
	bool ComplexHasTag(FDNATag const& TagToCheck, TEnumAsByte<EDNATagMatchType::Type> TagMatchType, TEnumAsByte<EDNATagMatchType::Type> TagToCheckMatchType) const;

	/**
	 * Checks if this container matches ANY of the tags in the specified container. Performs matching by expanding this container out
	 * to include its parent tags.
	 *
	 * @param Other					Container we are checking against
	 * @param bCountEmptyAsMatch	If true, the parameter tag container will count as matching even if it's empty
	 *
	 * @return True if this container has ANY the tags of the passed in container
	 */
	DEPRECATED(4.15, "Deprecated in favor of HasAny")
	FORCEINLINE_DEBUGGABLE bool MatchesAny(const FDNATagContainer& Other, bool bCountEmptyAsMatch) const
	{
		if (Other.IsEmpty())
		{
			return bCountEmptyAsMatch;
		}
		return DoesTagContainerMatch(Other, EDNATagMatchType::IncludeParentTags, EDNATagMatchType::Explicit, EDNAContainerMatchType::Any);
	}

	/**
	 * Checks if this container matches ALL of the tags in the specified container. Performs matching by expanding this container out to
	 * include its parent tags.
	 *
	 * @param Other				Container we are checking against
	 * @param bCountEmptyAsMatch	If true, the parameter tag container will count as matching even if it's empty
	 * 
	 * @return True if this container has ALL the tags of the passed in container
	 */
	DEPRECATED(4.15, "Deprecated in favor of HasAll")
	FORCEINLINE_DEBUGGABLE bool MatchesAll(const FDNATagContainer& Other, bool bCountEmptyAsMatch) const
	{
		if (Other.IsEmpty())
		{
			return bCountEmptyAsMatch;
		}
		return DoesTagContainerMatch(Other, EDNATagMatchType::IncludeParentTags, EDNATagMatchType::Explicit, EDNAContainerMatchType::All);
	}

	/**
	 * Returns true if the tags in this container match the tags in OtherContainer for the specified matching types.
	 *
	 * @param OtherContainer		The Container to filter against
	 * @param TagMatchType			Type of match to use for the tags in this container
	 * @param OtherTagMatchType		Type of match to use for the tags in the OtherContainer param
	 * @param ContainerMatchType	Type of match to use for filtering
	 *
	 * @return Returns true if ContainerMatchType is Any and any of the tags in OtherContainer match the tags in this or ContainerMatchType is All and all of the tags in OtherContainer match at least one tag in this. Returns false otherwise.
	 */
	FORCEINLINE_DEBUGGABLE bool DoesTagContainerMatch(const FDNATagContainer& OtherContainer, TEnumAsByte<EDNATagMatchType::Type> TagMatchType, TEnumAsByte<EDNATagMatchType::Type> OtherTagMatchType, EDNAContainerMatchType ContainerMatchType) const
	{
		SCOPE_CYCLE_COUNTER(STAT_FDNATagContainer_DoesTagContainerMatch);
		bool bResult;
		if (OtherTagMatchType == EDNATagMatchType::Explicit)
		{
			// Start true for all, start false for any
			bResult = (ContainerMatchType == EDNAContainerMatchType::All);
			for (const FDNATag& OtherTag : OtherContainer.DNATags)
			{
				if (HasTagFast(OtherTag, TagMatchType, OtherTagMatchType))
				{
					if (ContainerMatchType == EDNAContainerMatchType::Any)
					{
						bResult = true;
						break;
					}
				}
				else if (ContainerMatchType == EDNAContainerMatchType::All)
				{
					bResult = false;
					break;
				}
			}			
		}
		else
		{
			FDNATagContainer OtherExpanded = OtherContainer.GetDNATagParents();
			return DoesTagContainerMatch(OtherExpanded, TagMatchType, EDNATagMatchType::Explicit, ContainerMatchType);
		}
		return bResult;
	}

	/**
	 * Returns a filtered version of this container, as if the container were filtered by matches from the parameter container
	 *
	 * @param OtherContainer		The Container to filter against
	 * @param TagMatchType			Type of match to use for the tags in this container
	 * @param OtherTagMatchType		Type of match to use for the tags in the OtherContainer param
	 *
	 * @return A FDNATagContainer containing the filtered tags
	 */
	DEPRECATED(4.15, "Deprecated in favor of HasAll")
	FDNATagContainer Filter(const FDNATagContainer& OtherContainer, TEnumAsByte<EDNATagMatchType::Type> TagMatchType, TEnumAsByte<EDNATagMatchType::Type> OtherTagMatchType) const;

PRAGMA_ENABLE_DEPRECATION_WARNINGS

protected:

	/**
	 * Returns true if the tags in this container match the tags in OtherContainer for the specified matching types.
	 *
	 * @param OtherContainer		The Container to filter against
	 * @param TagMatchType			Type of match to use for the tags in this container
	 * @param OtherTagMatchType		Type of match to use for the tags in the OtherContainer param
	 * @param ContainerMatchType	Type of match to use for filtering
	 *
	 * @return Returns true if ContainerMatchType is Any and any of the tags in OtherContainer match the tags in this or ContainerMatchType is All and all of the tags in OtherContainer match at least one tag in this. Returns false otherwise.
	 */
	bool DoesTagContainerMatchComplex(const FDNATagContainer& OtherContainer, TEnumAsByte<EDNATagMatchType::Type> TagMatchType, TEnumAsByte<EDNATagMatchType::Type> OtherTagMatchType, EDNAContainerMatchType ContainerMatchType) const;

	/**
	 * If a Tag with the specified tag name explicitly exists, it will remove that tag and return true.  Otherwise, it 
	   returns false.  It does NOT check the TagName for validity (i.e. the tag could be obsolete and so not exist in
	   the table). It also does NOT check parents (because it cannot do so for a tag that isn't in the table).
	   NOTE: This function should ONLY ever be used by DNATagsManager when redirecting tags.  Do NOT make this
	   function public!
	 */
	bool RemoveTagByExplicitName(const FName& TagName);

	/** Adds parent tags for a single tag */
	void AddParentsForTag(const FDNATag& Tag);

	/** Fills in ParentTags from DNATags */
	void FillParentTags();

	/** Array of DNA tags */
	UPROPERTY(BlueprintReadWrite, Category=DNATags) // Change to VisibleAnywhere after fixing up games
	TArray<FDNATag> DNATags;

	/** Array of expanded parent tags, in addition to DNATags. Used to accelerate parent searches. May contain duplicates in some cases */
	UPROPERTY(Transient)
	TArray<FDNATag> ParentTags;

	friend class UDNATagsManager;
	friend struct FDNATagQuery;
	friend struct FDNATagQueryExpression;
	friend struct FDNATagNode;
	friend struct FDNATag;
	
private:

	/**
	 * DO NOT USE DIRECTLY
	 * STL-like iterators to enable range-based for loop support.
	 */
	
	FORCEINLINE friend TArray<FDNATag>::TConstIterator begin(const FDNATagContainer& Array) { return Array.CreateConstIterator(); }
	FORCEINLINE friend TArray<FDNATag>::TConstIterator end(const FDNATagContainer& Array) { return TArray<FDNATag>::TConstIterator(Array.DNATags, Array.DNATags.Num()); }
};

FORCEINLINE bool FDNATag::MatchesAnyExact(const FDNATagContainer& ContainerToCheck) const
{
	if (ContainerToCheck.IsEmpty())
	{
		return false;
	}
	return ContainerToCheck.DNATags.Contains(*this);
}

template<>
struct TStructOpsTypeTraits<FDNATagContainer> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithSerializer = true,
		WithIdenticalViaEquality = true,
		WithNetSerializer = true,
		WithImportTextItem = true,
		WithCopy = true
	};
};


/** Enumerates the list of supported query expression types. */
UENUM()
namespace EDNATagQueryExprType
{
	enum Type
	{
		Undefined = 0,
		AnyTagsMatch,
		AllTagsMatch,
		NoTagsMatch,
		AnyExprMatch,
		AllExprMatch,
		NoExprMatch,
	};
}

namespace EDNATagQueryStreamVersion
{
	enum Type
	{
		InitialVersion = 0,

		// -----<new versions can be added before this line>-------------------------------------------------
		// - this needs to be the last line (see note below)
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};
}

/**
 * An FDNATagQuery is a logical query that can be run against an FDNATagContainer.  A query that succeeds is said to "match".
 * Queries are logical expressions that can test the intersection properties of another tag container (all, any, or none), or the matching state of a set of sub-expressions
 * (all, any, or none). This allows queries to be arbitrarily recursive and very expressive.  For instance, if you wanted to test if a given tag container contained tags 
 * ((A && B) || (C)) && (!D), you would construct your query in the form ALL( ANY( ALL(A,B), ALL(C) ), NONE(D) )
 * 
 * You can expose the query structs to Blueprints and edit them with a custom editor, or you can construct them natively in code. 
 * 
 * Example of how to build a query via code:
 *	FDNATagQuery Q;
 *	Q.BuildQuery(
 *		FDNATagQueryExpression()
 * 		.AllTagsMatch()
 *		.AddTag(FDNATag::RequestDNATag(FName(TEXT("Animal.Mammal.Dog.Corgi"))))
 *		.AddTag(FDNATag::RequestDNATag(FName(TEXT("Plant.Tree.Spruce"))))
 *		);
 * 
 * Queries are internally represented as a byte stream that is memory-efficient and can be evaluated quickly at runtime.
 * Note: these have an extensive details and graph pin customization for editing, so there is no need to expose the internals to Blueprints.
 */
USTRUCT(BlueprintType, meta=(HasNativeMake="DNATags.BlueprintDNATagLibrary.MakeDNATagQuery"))
struct DNATAGS_API FDNATagQuery
{
	GENERATED_BODY();

public:
	FDNATagQuery();

	FDNATagQuery(FDNATagQuery const& Other);
	FDNATagQuery(FDNATagQuery&& Other);

	/** Assignment/Equality operators */
	FDNATagQuery& operator=(FDNATagQuery const& Other);
	FDNATagQuery& operator=(FDNATagQuery&& Other);

private:
	/** Versioning for future token stream protocol changes. See EDNATagQueryStreamVersion. */
	UPROPERTY()
	int32 TokenStreamVersion;

	/** List of tags referenced by this entire query. Token stream stored indices into this list. */
	UPROPERTY()
	TArray<FDNATag> TagDictionary;

	/** Stream representation of the actual hierarchical query */
	UPROPERTY()
	TArray<uint8> QueryTokenStream;

	/** User-provided string describing the query */
	UPROPERTY()
	FString UserDescription;

	/** Auto-generated string describing the query */
	UPROPERTY()
	FString AutoDescription;

	/** Returns a DNA tag from the tag dictionary */
	FDNATag GetTagFromIndex(int32 TagIdx) const
	{
		ensure(TagDictionary.IsValidIndex(TagIdx));
		return TagDictionary[TagIdx];
	}

public:

	/** Replaces existing tags with passed in tags. Does not modify the tag query expression logic. Useful when you need to cache off and update often used query. Must use same sized tag container! */
	void ReplaceTagsFast(FDNATagContainer const& Tags)
	{
		ensure(Tags.Num() == TagDictionary.Num());
		TagDictionary.Reset();
		TagDictionary.Append(Tags.DNATags);
	}

	/** Replaces existing tags with passed in tag. Does not modify the tag query expression logic. Useful when you need to cache off and update often used query. */		 
	void ReplaceTagFast(FDNATag const& Tag)
	{
		ensure(1 == TagDictionary.Num());
		TagDictionary.Reset();
		TagDictionary.Add(Tag);
	}

	/** Returns true if the given tags match this query, or false otherwise. */
	bool Matches(FDNATagContainer const& Tags) const;

	/** Returns true if this query is empty, false otherwise. */
	bool IsEmpty() const;

	/** Resets this query to its default empty state. */
	void Clear();

	/** Creates this query with the given root expression. */
	void Build(struct FDNATagQueryExpression& RootQueryExpr, FString InUserDescription = FString());

	/** Static function to assemble and return a query. */
	static FDNATagQuery BuildQuery(struct FDNATagQueryExpression& RootQueryExpr, FString InDescription = FString());

	/** Builds a FDNATagQueryExpression from this query. */
	void GetQueryExpr(struct FDNATagQueryExpression& OutExpr) const;

	/** Returns description string. */
	FString GetDescription() const { return UserDescription.IsEmpty() ? AutoDescription : UserDescription; };

#if WITH_EDITOR
	/** Creates this query based on the given EditableQuery object */
	void BuildFromEditableQuery(class UEditableDNATagQuery& EditableQuery); 

	/** Creates editable query object tree based on this query */
	UEditableDNATagQuery* CreateEditableQuery();
#endif // WITH_EDITOR

	static const FDNATagQuery EmptyQuery;

	/**
	* Shortcuts for easily creating common query types
	* @todo: add more as dictated by use cases
	*/

	/** Creates a tag query that will match if there are any common tags between the given tags and the tags being queries against. */
	static FDNATagQuery MakeQuery_MatchAnyTags(FDNATagContainer const& InTags);
	static FDNATagQuery MakeQuery_MatchAllTags(FDNATagContainer const& InTags);
	static FDNATagQuery MakeQuery_MatchNoTags(FDNATagContainer const& InTags);

	friend class FQueryEvaluator;
};

struct DNATAGS_API FDNATagQueryExpression
{
	/** 
	 * Fluid syntax approach for setting the type of this expression. 
	 */

	FDNATagQueryExpression& AnyTagsMatch()
	{
		ExprType = EDNATagQueryExprType::AnyTagsMatch;
		return *this;
	}

	FDNATagQueryExpression& AllTagsMatch()
	{
		ExprType = EDNATagQueryExprType::AllTagsMatch;
		return *this;
	}

	FDNATagQueryExpression& NoTagsMatch()
	{
		ExprType = EDNATagQueryExprType::NoTagsMatch;
		return *this;
	}

	FDNATagQueryExpression& AnyExprMatch()
	{
		ExprType = EDNATagQueryExprType::AnyExprMatch;
		return *this;
	}

	FDNATagQueryExpression& AllExprMatch()
	{
		ExprType = EDNATagQueryExprType::AllExprMatch;
		return *this;
	}

	FDNATagQueryExpression& NoExprMatch()
	{
		ExprType = EDNATagQueryExprType::NoExprMatch;
		return *this;
	}

	FDNATagQueryExpression& AddTag(TCHAR const* TagString)
	{
		return AddTag(FName(TagString));
	}
	FDNATagQueryExpression& AddTag(FName TagName);
	FDNATagQueryExpression& AddTag(FDNATag Tag)
	{
		ensure(UsesTagSet());
		TagSet.Add(Tag);
		return *this;
	}
	
	FDNATagQueryExpression& AddTags(FDNATagContainer const& Tags)
	{
		ensure(UsesTagSet());
		TagSet.Append(Tags.DNATags);
		return *this;
	}

	FDNATagQueryExpression& AddExpr(FDNATagQueryExpression& Expr)
	{
		ensure(UsesExprSet());
		ExprSet.Add(Expr);
		return *this;
	}
	
	/** Writes this expression to the given token stream. */
	void EmitTokens(TArray<uint8>& TokenStream, TArray<FDNATag>& TagDictionary) const;

	/** Which type of expression this is. */
	EDNATagQueryExprType::Type ExprType;
	/** Expression list, for expression types that need it */
	TArray<struct FDNATagQueryExpression> ExprSet;
	/** Tag list, for expression types that need it */
	TArray<FDNATag> TagSet;

	/** Returns true if this expression uses the tag data. */
	FORCEINLINE bool UsesTagSet() const
	{
		return (ExprType == EDNATagQueryExprType::AllTagsMatch) || (ExprType == EDNATagQueryExprType::AnyTagsMatch) || (ExprType == EDNATagQueryExprType::NoTagsMatch);
	}
	/** Returns true if this expression uses the expression list data. */
	FORCEINLINE bool UsesExprSet() const
	{
		return (ExprType == EDNATagQueryExprType::AllExprMatch) || (ExprType == EDNATagQueryExprType::AnyExprMatch) || (ExprType == EDNATagQueryExprType::NoExprMatch);
	}
};

template<>
struct TStructOpsTypeTraits<FDNATagQuery> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithCopy = true
	};
};



/** 
 * This is an editor-only representation of a query, designed to be editable with a typical property window. 
 * To edit a query in the editor, an FDNATagQuery is converted to a set of UObjects and edited,  When finished,
 * the query struct is rewritten and these UObjects are discarded.
 * This query representation is not intended for runtime use.
 */
UCLASS(editinlinenew, collapseCategories, Transient) 
class DNATAGS_API UEditableDNATagQuery : public UObject
{
	GENERATED_BODY()

public:
	/** User-supplied description, shown in property details. Auto-generated description is shown if not supplied. */
	UPROPERTY(EditDefaultsOnly, Category = Query)
	FString UserDescription;

	/** Automatically-generated description */
	FString AutoDescription;

	/** The base expression of this query. */
	UPROPERTY(EditDefaultsOnly, Instanced, Category = Query)
	class UEditableDNATagQueryExpression* RootExpression;

#if WITH_EDITOR
	/** Converts this editor query construct into the runtime-usable token stream. */
	void EmitTokens(TArray<uint8>& TokenStream, TArray<FDNATag>& TagDictionary, FString* DebugString=nullptr) const;

	/** Generates and returns the export text for this query. */
	FString GetTagQueryExportText(FDNATagQuery const& TagQuery);
#endif  // WITH_EDITOR

private:
	/** Property to hold a DNA tag query so we can use the exporttext path to get a string representation. */
	UPROPERTY()
	FDNATagQuery TagQueryExportText_Helper;
};

UCLASS(abstract, editinlinenew, collapseCategories, Transient)
class DNATAGS_API UEditableDNATagQueryExpression : public UObject
{
	GENERATED_BODY()

#if WITH_EDITOR
public:
	/** Converts this editor query construct into the runtime-usable token stream. */
	virtual void EmitTokens(TArray<uint8>& TokenStream, TArray<FDNATag>& TagDictionary, FString* DebugString=nullptr) const {};

protected:
	void EmitTagTokens(FDNATagContainer const& TagsToEmit, TArray<uint8>& TokenStream, TArray<FDNATag>& TagDictionary, FString* DebugString) const;
	void EmitExprListTokens(TArray<UEditableDNATagQueryExpression*> const& ExprList, TArray<uint8>& TokenStream, TArray<FDNATag>& TagDictionary, FString* DebugString) const;
#endif  // WITH_EDITOR
};

UCLASS(BlueprintType, editinlinenew, collapseCategories, meta=(DisplayName="Any Tags Match"))
class UEditableDNATagQueryExpression_AnyTagsMatch : public UEditableDNATagQueryExpression
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, Category = Expr)
	FDNATagContainer Tags;

#if WITH_EDITOR
	virtual void EmitTokens(TArray<uint8>& TokenStream, TArray<FDNATag>& TagDictionary, FString* DebugString = nullptr) const override;
#endif  // WITH_EDITOR
};

UCLASS(BlueprintType, editinlinenew, collapseCategories, meta = (DisplayName = "All Tags Match"))
class UEditableDNATagQueryExpression_AllTagsMatch : public UEditableDNATagQueryExpression
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, Category = Expr)
	FDNATagContainer Tags;

#if WITH_EDITOR
	virtual void EmitTokens(TArray<uint8>& TokenStream, TArray<FDNATag>& TagDictionary, FString* DebugString = nullptr) const override;
#endif  // WITH_EDITOR
};

UCLASS(BlueprintType, editinlinenew, collapseCategories, meta = (DisplayName = "No Tags Match"))
class UEditableDNATagQueryExpression_NoTagsMatch : public UEditableDNATagQueryExpression
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, Category = Expr)
	FDNATagContainer Tags;

#if WITH_EDITOR
	virtual void EmitTokens(TArray<uint8>& TokenStream, TArray<FDNATag>& TagDictionary, FString* DebugString = nullptr) const override;
#endif  // WITH_EDITOR
};

UCLASS(BlueprintType, editinlinenew, collapseCategories, meta = (DisplayName = "Any Expressions Match"))
class UEditableDNATagQueryExpression_AnyExprMatch : public UEditableDNATagQueryExpression
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Instanced, Category = Expr)
	TArray<UEditableDNATagQueryExpression*> Expressions;

#if WITH_EDITOR
	virtual void EmitTokens(TArray<uint8>& TokenStream, TArray<FDNATag>& TagDictionary, FString* DebugString = nullptr) const override;
#endif  // WITH_EDITOR
};

UCLASS(BlueprintType, editinlinenew, collapseCategories, meta = (DisplayName = "All Expressions Match"))
class UEditableDNATagQueryExpression_AllExprMatch : public UEditableDNATagQueryExpression
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Instanced, Category = Expr)
	TArray<UEditableDNATagQueryExpression*> Expressions;

#if WITH_EDITOR
	virtual void EmitTokens(TArray<uint8>& TokenStream, TArray<FDNATag>& TagDictionary, FString* DebugString = nullptr) const override;
#endif  // WITH_EDITOR
};

UCLASS(BlueprintType, editinlinenew, collapseCategories, meta = (DisplayName = "No Expressions Match"))
class UEditableDNATagQueryExpression_NoExprMatch : public UEditableDNATagQueryExpression
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Instanced, Category = Expr)
	TArray<UEditableDNATagQueryExpression*> Expressions;

#if WITH_EDITOR
	virtual void EmitTokens(TArray<uint8>& TokenStream, TArray<FDNATag>& TagDictionary, FString* DebugString = nullptr) const override;
#endif  // WITH_EDITOR
};

