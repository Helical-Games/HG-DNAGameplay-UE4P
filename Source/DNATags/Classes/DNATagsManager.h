// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "Stats/Stats.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "DNATagContainer.h"
#include "Engine/DataTable.h"
#include "DNATagsManager.generated.h"

class UDNATagsList;

/** Simple struct for a table row in the DNA tag table and element in the ini list */
USTRUCT()
struct FDNATagTableRow : public FTableRowBase
{
	GENERATED_USTRUCT_BODY()

	/** Tag specified in the table */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=DNATag)
	FName Tag;

	/** Developer comment clarifying the usage of a particular tag, not user facing */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=DNATag)
	FString DevComment;

	/** Constructors */
	FDNATagTableRow() {}
	FDNATagTableRow(FName InTag, const FString& InDevComment = TEXT("")) : Tag(InTag), DevComment(InDevComment) {}
	DNATAGS_API FDNATagTableRow(FDNATagTableRow const& Other);

	/** Assignment/Equality operators */
	DNATAGS_API FDNATagTableRow& operator=(FDNATagTableRow const& Other);
	DNATAGS_API bool operator==(FDNATagTableRow const& Other) const;
	DNATAGS_API bool operator!=(FDNATagTableRow const& Other) const;
	DNATAGS_API bool operator<(FDNATagTableRow const& Other) const;
};

UENUM()
enum class EDNATagSourceType : uint8
{
	Native,				// Was added from C++ code
	DefaultTagList,		// The default tag list in DefaultDNATags.ini
	TagList,			// Another tag list from an ini in tags/*.ini
	DataTable,			// From a DataTable
	Invalid,			// Not a real source
};

/** Struct defining where DNA tags are loaded/saved from. Mostly for the editor */
USTRUCT()
struct FDNATagSource
{
	GENERATED_USTRUCT_BODY()

	/** Name of this source */
	UPROPERTY()
	FName SourceName;

	/** Type of this source */
	UPROPERTY()
	EDNATagSourceType SourceType;

	/** If this is bound to an ini object for saving, this is the one */
	UPROPERTY()
	class UDNATagsList* SourceTagList;

	FDNATagSource() 
		: SourceName(NAME_None), SourceType(EDNATagSourceType::Invalid), SourceTagList(nullptr) 
	{
	}

	FDNATagSource(FName InSourceName, EDNATagSourceType InSourceType, UDNATagsList* InSourceTagList = nullptr) 
		: SourceName(InSourceName), SourceType(InSourceType), SourceTagList(InSourceTagList)
	{
	}

	static FName GetNativeName()
	{
		static FName NativeName = FName(TEXT("Native"));
		return NativeName;
	}

	static FName GetDefaultName()
	{
		static FName DefaultName = FName(TEXT("DefaultDNATags.ini"));
		return DefaultName;
	}
};

/** Simple tree node for DNA tags, this stores metadata about specific tags */
USTRUCT()
struct FDNATagNode
{
	GENERATED_USTRUCT_BODY()
	FDNATagNode(){};

	/** Simple constructor */
	FDNATagNode(FName InTag, TSharedPtr<FDNATagNode> InParentNode);

	/** Returns a correctly constructed container with only this tag, useful for doing container queries */
	FORCEINLINE const FDNATagContainer& GetSingleTagContainer() const { return CompleteTagWithParents; }

	/**
	 * Get the complete tag for the node, including all parent tags, delimited by periods
	 * 
	 * @return Complete tag for the node
	 */
	FORCEINLINE const FDNATag& GetCompleteTag() const { return CompleteTagWithParents.Num() > 0 ? CompleteTagWithParents.DNATags[0] : FDNATag::EmptyTag; }
	FORCEINLINE FName GetCompleteTagName() const { return GetCompleteTag().GetTagName(); }
	FORCEINLINE FString GetCompleteTagString() const { return GetCompleteTag().ToString(); }

	/**
	 * Get the simple tag for the node (doesn't include any parent tags)
	 * 
	 * @return Simple tag for the node
	 */
	FORCEINLINE FName GetSimpleTagName() const { return Tag; }

	/**
	 * Get the children nodes of this node
	 * 
	 * @return Reference to the array of the children nodes of this node
	 */
	FORCEINLINE TArray< TSharedPtr<FDNATagNode> >& GetChildTagNodes() { return ChildTags; }

	/**
	 * Get the children nodes of this node
	 * 
	 * @return Reference to the array of the children nodes of this node
	 */
	FORCEINLINE const TArray< TSharedPtr<FDNATagNode> >& GetChildTagNodes() const { return ChildTags; }

	/**
	 * Get the parent tag node of this node
	 * 
	 * @return The parent tag node of this node
	 */
	FORCEINLINE TSharedPtr<FDNATagNode> GetParentTagNode() const { return ParentNode; }

	/**
	* Get the net index of this node
	*
	* @return The net index of this node
	*/
	FORCEINLINE FDNATagNetIndex GetNetIndex() const { return NetIndex; }

	/** Reset the node of all of its values */
	DNATAGS_API void ResetNode();

private:
	/** Raw name for this tag at current rank in the tree */
	FName Tag;

	/** This complete tag is at DNATags[0], with parents in ParentTags[] */
	FDNATagContainer CompleteTagWithParents;

	/** Child DNA tag nodes */
	TArray< TSharedPtr<FDNATagNode> > ChildTags;

	/** Owner DNA tag node, if any */
	TSharedPtr<FDNATagNode> ParentNode;
	
	/** Net Index of this node */
	FDNATagNetIndex NetIndex;

#if WITH_EDITORONLY_DATA
	/** Package or config file this tag came from. This is the first one added. If None, this is an implicitly added tag */
	FName SourceName;

	/** Comment for this tag */
	FString DevComment;
#endif 

	friend class UDNATagsManager;
};

/** Holds data about the tag dictionary, is in a singleton UObject */
UCLASS(config=Engine)
class DNATAGS_API UDNATagsManager : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Destructor */
	~UDNATagsManager();

	/** Returns the global UDNATagsManager manager */
	FORCEINLINE static UDNATagsManager& Get()
	{
		if (SingletonManager == nullptr)
		{
			InitializeManager();
		}

		return *SingletonManager;
	}

	/**
	 * Gets the FDNATag that corresponds to the TagName
	 *
	 * @param TagName The Name of the tag to search for
	 * @param ErrorIfNotfound: ensure() that tag exists.
	 * 
	 * @return Will return the corresponding FDNATag or an empty one if not found.
	 */
	FDNATag RequestDNATag(FName TagName, bool ErrorIfNotFound=true) const;

	/**
	 * Registers the given name as a DNA tag, and tracks that it is being directly referenced from code
	 * This can only be called during engine initialization, the table needs to be locked down before replication
	 *
	 * @param TagName The Name of the tag to add
	 * 
	 * @return Will return the corresponding FDNATag
	 */
	FDNATag AddNativeDNATag(FName TagName);

	/** Call to flush the list of native tags, once called it is unsafe to add more */
	void DoneAddingNativeTags();

	/**
	 * Gets a Tag Container containing the supplied tag and all of it's parents as explicit tags
	 *
	 * @param DNATag The Tag to use at the child most tag for this container
	 * 
	 * @return A Tag Container with the supplied tag and all its parents added explicitly
	 */
	FDNATagContainer RequestDNATagParents(const FDNATag& DNATag) const;

	/**
	 * Gets a Tag Container containing the all tags in the hierarchy that are children of this tag. Does not return the original tag
	 *
	 * @param DNATag					The Tag to use at the parent tag
	 * 
	 * @return A Tag Container with the supplied tag and all its parents added explicitly
	 */
	FDNATagContainer RequestDNATagChildren(const FDNATag& DNATag) const;

	/** Returns direct parent DNATag of this DNATag, calling on x.y will return x */
	FDNATag RequestDNATagDirectParent(const FDNATag& DNATag) const;

	/**
	 * Helper function to get the stored TagContainer containing only this tag, which has searchable ParentTags
	 * @param DNATag		Tag to get single container of
	 * @return					Pointer to container with this tag
	 */
	FORCEINLINE_DEBUGGABLE const FDNATagContainer* GetSingleTagContainer(const FDNATag& DNATag) const
	{
		TSharedPtr<FDNATagNode> TagNode = FindTagNode(DNATag);
		if (TagNode.IsValid())
		{
			return &(TagNode->GetSingleTagContainer());
		}
		return nullptr;
	}

	/**
	 * Checks node tree to see if a FDNATagNode with the tag exists
	 *
	 * @param TagName	The name of the tag node to search for
	 *
	 * @return A shared pointer to the FDNATagNode found, or NULL if not found.
	 */
	FORCEINLINE_DEBUGGABLE TSharedPtr<FDNATagNode> FindTagNode(const FDNATag& DNATag) const
	{
		const TSharedPtr<FDNATagNode>* Node = DNATagNodeMap.Find(DNATag);

		if (Node)
		{
			return *Node;
		}
#if WITH_EDITOR
		// Check redirector
		if (GIsEditor && DNATag.IsValid())
		{
			FDNATag RedirectedTag = DNATag;

			RedirectSingleDNATag(RedirectedTag, nullptr);

			Node = DNATagNodeMap.Find(RedirectedTag);

			if (Node)
			{
				return *Node;
			}
		}
#endif
		return nullptr;
	}

	/**
	 * Checks node tree to see if a FDNATagNode with the name exists
	 *
	 * @param TagName	The name of the tag node to search for
	 *
	 * @return A shared pointer to the FDNATagNode found, or NULL if not found.
	 */
	FORCEINLINE_DEBUGGABLE TSharedPtr<FDNATagNode> FindTagNode(FName TagName) const
	{
		FDNATag PossibleTag(TagName);
		return FindTagNode(PossibleTag);
	}

	/** Loads the tag tables referenced in the DNATagSettings object */
	void LoadDNATagTables();

	/** Helper function to construct the DNA tag tree */
	void ConstructDNATagTree();

	/** Helper function to destroy the DNA tag tree */
	void DestroyDNATagTree();

	/** Splits a tag such as x.y.z into an array of names {x,y,z} */
	void SplitDNATagFName(const FDNATag& Tag, TArray<FName>& OutNames) const;

	/** Gets the list of all tags in the dictionary */
	void RequestAllDNATags(FDNATagContainer& TagContainer, bool OnlyIncludeDictionaryTags) const;

	/** Returns true if if the passed in name is in the tag dictionary and can be created */
	bool ValidateTagCreation(FName TagName) const;

	/** Returns the tag source for a given tag source name, or null if not found */
	const FDNATagSource* FindTagSource(FName TagSourceName) const;

	/** Fills in an array with all tag sources of a specific type */
	void FindTagSourcesWithType(EDNATagSourceType TagSourceType, TArray<const FDNATagSource*>& OutArray) const;

	/**
	 * Check to see how closely two FDNATags match. Higher values indicate more matching terms in the tags.
	 *
	 * @param DNATagOne	The first tag to compare
	 * @param DNATagTwo	The second tag to compare
	 *
	 * @return the length of the longest matching pair
	 */
	int32 DNATagsMatchDepth(const FDNATag& DNATagOne, const FDNATag& DNATagTwo) const;

	/** Returns true if we should import tags from UDNATagsSettings objects (configured by INI files) */
	bool ShouldImportTagsFromINI() const;

	/** Should we print loading errors when trying to load invalid tags */
	bool ShouldWarnOnInvalidTags() const
	{
		return bShouldWarnOnInvalidTags;
	}

	/** Should use fast replication */
	bool ShouldUseFastReplication() const
	{
		return bUseFastReplication;
	}

	/** Handles redirectors for an entire container, will also error on invalid tags */
	void RedirectTagsForContainer(FDNATagContainer& Container, UProperty* SerializingProperty) const;

	/** Handles redirectors for a single tag, will also error on invalid tag. This is only called for when individual tags are serialized on their own */
	void RedirectSingleDNATag(FDNATag& Tag, UProperty* SerializingProperty) const;

	/** Gets a tag name from net index and vice versa, used for replication efficiency */
	FName GetTagNameFromNetIndex(FDNATagNetIndex Index) const;
	FDNATagNetIndex GetNetIndexFromTag(const FDNATag &InTag) const;

	/** Cached number of bits we need to replicate tags. That is, Log2(Number of Tags). Will always be <= 16. */
	int32 NetIndexTrueBitNum;
	
	/** The length in bits of the first segment when net serializing tags. We will serialize NetIndexFirstBitSegment + 1 bit to indicatore "more" (more = second segment that is NetIndexTrueBitNum - NetIndexFirstBitSegment) */
	int32 NetIndexFirstBitSegment;

	/** Numbers of bits to use for replicating container size. This can be set via config. */
	int32 NumBitsForContainerSize;

	/** This is the actual value for an invalid tag "None". This is computed at runtime as (Total number of tags) + 1 */
	FDNATagNetIndex InvalidTagNetIndex;

	const TArray<TSharedPtr<FDNATagNode>>& GetNetworkDNATagNodeIndex() const { return NetworkDNATagNodeIndex; }

#if WITH_EDITOR
	/** Gets a Filtered copy of the DNARootTags Array based on the comma delimited filter string passed in */
	void GetFilteredDNARootTags(const FString& InFilterString, TArray< TSharedPtr<FDNATagNode> >& OutTagArray) const;

	/** Gets a list of all DNA tag nodes added by the specific source */
	void GetAllTagsFromSource(FName TagSource, TArray< TSharedPtr<FDNATagNode> >& OutTagArray) const;

	/** Returns true if this tag is directly in the dictionary already */
	bool IsDictionaryTag(FName TagName) const;

	/** Returns comment and source for tag. If not found return false */
	bool GetTagEditorData(FName TagName, FString& OutComment, FName &OutTagSource) const;

	/** Refresh the DNAtag tree due to an editor change */
	void EditorRefreshDNATagTree();

	/** Gets a Tag Container containing the all tags in the hierarchy that are children of this tag, and were explicitly added to the dictionary */
	FDNATagContainer RequestDNATagChildrenInDictionary(const FDNATag& DNATag) const;

#endif //WITH_EDITOR

	DEPRECATED(4.15, "Call MatchesTag on FDNATag instead")
	FORCEINLINE_DEBUGGABLE bool DNATagsMatch(const FDNATag& DNATagOne, TEnumAsByte<EDNATagMatchType::Type> MatchTypeOne, const FDNATag& DNATagTwo, TEnumAsByte<EDNATagMatchType::Type> MatchTypeTwo) const
	{
		SCOPE_CYCLE_COUNTER(STAT_UDNATagsManager_DNATagsMatch);
		bool bResult = false;
		if (MatchTypeOne == EDNATagMatchType::Explicit && MatchTypeTwo == EDNATagMatchType::Explicit)
		{
			bResult = DNATagOne == DNATagTwo;
		}
		else
		{
			// Convert both to their containers and do that match
			const FDNATagContainer* ContainerOne = GetSingleTagContainer(DNATagOne);
			const FDNATagContainer* ContainerTwo = GetSingleTagContainer(DNATagTwo);
			if (ContainerOne && ContainerTwo)
			{
				bResult = ContainerOne->DoesTagContainerMatch(*ContainerTwo, MatchTypeOne, MatchTypeTwo, EDNAContainerMatchType::Any);
			}
		}
		return bResult;
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/** Mechanism for tracking what tags are frequently replicated */

	void PrintReplicationFrequencyReport();
	void NotifyTagReplicated(FDNATag Tag, bool WasInContainer);

	TMap<FDNATag, int32>	ReplicationCountMap;
	TMap<FDNATag, int32>	ReplicationCountMap_SingleTags;
	TMap<FDNATag, int32>	ReplicationCountMap_Containers;
#endif

private:

	/** Initializes the manager */
	static void InitializeManager();

	/** The Tag Manager singleton */
	static UDNATagsManager* SingletonManager;

	friend class FDNATagTest;
	friend class FDNAEffectsTest;
	friend class FDNATagsModule;
	friend class FDNATagsEditorModule;

	/**
	 * Helper function to insert a tag into a tag node array
	 *
	 * @param Tag					Tag to insert
	 * @param ParentNode			Parent node, if any, for the tag
	 * @param NodeArray				Node array to insert the new node into, if necessary (if the tag already exists, no insertion will occur)
	 * @param SourceName			File tag was added from
	 * @param DevComment			Comment from developer about this tag
	 *
	 * @return Index of the node of the tag
	 */
	int32 InsertTagIntoNodeArray(FName Tag, TSharedPtr<FDNATagNode> ParentNode, TArray< TSharedPtr<FDNATagNode> >& NodeArray, FName SourceName, const FString& DevComment);

	/** Helper function to populate the tag tree from each table */
	void PopulateTreeFromDataTable(class UDataTable* Table);

	void AddTagTableRow(const FDNATagTableRow& TagRow, FName SourceName);

	void AddChildrenTags(FDNATagContainer& TagContainer, TSharedPtr<FDNATagNode> DNATagNode, bool RecurseAll=true, bool OnlyIncludeDictionaryTags=false) const;

	/**
	 * Helper function for DNATagsMatch to get all parents when doing a parent match,
	 * NOTE: Must never be made public as it uses the FNames which should never be exposed
	 * 
	 * @param NameList		The list we are adding all parent complete names too
	 * @param DNATag	The current Tag we are adding to the list
	 */
	void GetAllParentNodeNames(TSet<FName>& NamesList, TSharedPtr<FDNATagNode> DNATag) const;

	/** Returns the tag source for a given tag source name, or null if not found */
	FDNATagSource* FindOrAddTagSource(FName TagSourceName, EDNATagSourceType SourceType);

	/** Constructs the net indices for each tag */
	void ConstructNetIndex();

	/** Roots of DNA tag nodes */
	TSharedPtr<FDNATagNode> DNARootTag;

	/** Map of Tags to Nodes - Internal use only. FDNATag is inside node structure, do not use FindKey! */
	TMap<FDNATag, TSharedPtr<FDNATagNode>> DNATagNodeMap;

	/** Our aggregated, sorted list of commonly replicated tags. These tags are given lower indices to ensure they replicate in the first bit segment. */
	TArray<FDNATag> CommonlyReplicatedTags;

	/** List of DNA tag sources */
	UPROPERTY()
	TArray<FDNATagSource> TagSources;

	/** List of native tags to add when reconstructing tree */
	TSet<FName> NativeTagsToAdd;

	/** Cached runtime value for whether we are using fast replication or not. Initialized from config setting. */
	bool bUseFastReplication;

	/** Cached runtime value for whether we should warn when loading invalid tags */
	bool bShouldWarnOnInvalidTags;

	/** True if native tags have all been added and flushed */
	bool bDoneAddingNativeTags;

#if WITH_EDITOR
	// This critical section is to handle and editor-only issue where tag requests come from another thread when async loading from a background thread in FDNATagContainer::Serialize.
	// This class is not generically threadsafe.
	mutable FCriticalSection DNATagMapCritical;
#endif

	/** Sorted list of nodes, used for network replication */
	TArray<TSharedPtr<FDNATagNode>> NetworkDNATagNodeIndex;

	/** Holds all of the valid DNA-related tags that can be applied to assets */
	UPROPERTY()
	TArray<UDataTable*> DNATagTables;

	/** The map of ini-configured tag redirectors */
	TMap<FName, FDNATag> TagRedirects;
};
