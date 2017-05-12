// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNATagsManager.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Stats/StatsMisc.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/LinkerLoad.h"
#include "UObject/Package.h"
#include "DNATagsSettings.h"
#include "DNATagsModule.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Engine/Engine.h"

#if WITH_EDITOR
#include "SourceControlHelpers.h"
#include "ISourceControlModule.h"
#include "Editor.h"
#endif

#define LOCTEXT_NAMESPACE "DNATagManager"

UDNATagsManager* UDNATagsManager::SingletonManager = nullptr;

UDNATagsManager::UDNATagsManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bUseFastReplication = false;
	bShouldWarnOnInvalidTags = true;
	bDoneAddingNativeTags = false;
	NetIndexFirstBitSegment = 16;
	NetIndexTrueBitNum = 16;
	NumBitsForContainerSize = 6;
}

void UDNATagsManager::LoadDNATagTables()
{
	DNATagTables.Empty();

	UDNATagsSettings* MutableDefault = GetMutableDefault<UDNATagsSettings>();

	for (FStringAssetReference DataTablePath : MutableDefault->DNATagTableList)
	{
		UDataTable* TagTable = LoadObject<UDataTable>(nullptr, *DataTablePath.ToString(), nullptr, LOAD_None, nullptr);

		// Handle case where the module is dynamically-loaded within a LoadPackage stack, which would otherwise
		// result in the tag table not having its RowStruct serialized in time. Without the RowStruct, the tags manager
		// will not be initialized correctly.
		if (TagTable && IsLoading())
		{
			FLinkerLoad* TagLinker = TagTable->GetLinker();
			if (TagLinker)
			{
				TagTable->GetLinker()->Preload(TagTable);
			}
		}
		DNATagTables.Add(TagTable);
	}
}

struct FCompareFDNATagNodeByTag
{
	FORCEINLINE bool operator()( const TSharedPtr<FDNATagNode>& A, const TSharedPtr<FDNATagNode>& B ) const
	{
		return (A->GetSimpleTagName().Compare(B->GetSimpleTagName())) < 0;
	}
};

void UDNATagsManager::ConstructDNATagTree()
{
	if (!DNARootTag.IsValid())
	{
		DNARootTag = MakeShareable(new FDNATagNode());

		// Add native tags first
		for (FName TagToAdd : NativeTagsToAdd)
		{
			AddTagTableRow(FDNATagTableRow(TagToAdd), FDNATagSource::GetNativeName());
		}

		{
#if STATS
			FString PerfMessage = FString::Printf(TEXT("UDNATagsManager::ConstructDNATagTree: Construct from data asset"));
			SCOPE_LOG_TIME_IN_SECONDS(*PerfMessage, nullptr)
#endif
		
			for (auto It(DNATagTables.CreateIterator()); It; It++)
			{
				if (*It)
				{
					PopulateTreeFromDataTable(*It);
				}
			}
		}

		UDNATagsSettings* MutableDefault = GetMutableDefault<UDNATagsSettings>();
		FString DefaultEnginePath = FString::Printf(TEXT("%sDefaultEngine.ini"), *FPaths::SourceConfigDir());

		// Create native source
		FindOrAddTagSource(FDNATagSource::GetNativeName(), EDNATagSourceType::Native);

		if (ShouldImportTagsFromINI())
		{
#if STATS
			FString PerfMessage = FString::Printf(TEXT("UDNATagsManager::ConstructDNATagTree: ImportINI"));
			SCOPE_LOG_TIME_IN_SECONDS(*PerfMessage, nullptr)
#endif
			// Copy from deprecated list in DefaultEngine.ini
			TArray<FString> EngineConfigTags;
			GConfig->GetArray(TEXT("/Script/DNATags.DNATagsSettings"), TEXT("+DNATags"), EngineConfigTags, DefaultEnginePath);
			
			for (const FString& EngineConfigTag : EngineConfigTags)
			{
				MutableDefault->DNATagList.AddUnique(FDNATagTableRow(FName(*EngineConfigTag)));
			}

			// Copy from deprecated list in DefaultGamplayTags.ini
			EngineConfigTags.Empty();
			GConfig->GetArray(TEXT("/Script/DNATags.DNATagsSettings"), TEXT("+DNATags"), EngineConfigTags, MutableDefault->GetDefaultConfigFilename());

			for (const FString& EngineConfigTag : EngineConfigTags)
			{
				MutableDefault->DNATagList.AddUnique(FDNATagTableRow(FName(*EngineConfigTag)));
			}

#if WITH_EDITOR
			MutableDefault->SortTags();
#endif

			FName TagSource = FDNATagSource::GetDefaultName();
			FDNATagSource* DefaultSource = FindOrAddTagSource(TagSource, EDNATagSourceType::DefaultTagList);

			for (const FDNATagTableRow& TableRow : MutableDefault->DNATagList)
			{
				AddTagTableRow(TableRow, TagSource);
			}

			// Extra tags
		
			// Read all tags from the ini
			TArray<FString> FilesInDirectory;
			IFileManager::Get().FindFilesRecursive(FilesInDirectory, *(FPaths::GameConfigDir() / TEXT("Tags")), TEXT("*.ini"), true, false);
			FilesInDirectory.Sort();
			for (FString& FileName : FilesInDirectory)
			{
				TagSource = FName(*FPaths::GetCleanFilename(FileName));
				FDNATagSource* FoundSource = FindOrAddTagSource(TagSource, EDNATagSourceType::TagList);

				UE_LOG(LogDNATags, Display, TEXT("Loading Tag File: %s"), *FileName);

				// Check deprecated locations
				TArray<FString> Tags;
				if (GConfig->GetArray(TEXT("UserTags"), TEXT("DNATags"), Tags, FileName))
				{
					for (const FString& Tag : Tags)
					{
						FoundSource->SourceTagList->DNATagList.AddUnique(FDNATagTableRow(FName(*Tag)));
					}
				}
				else
				{
					// Load from new ini
					FoundSource->SourceTagList->LoadConfig(UDNATagsList::StaticClass(), *FileName);
				}

#if WITH_EDITOR
				if (GIsEditor || IsRunningCommandlet()) // Sort tags for UI Purposes but don't sort in -game scenerio since this would break compat with noneditor cooked builds
				{
					FoundSource->SourceTagList->SortTags();
				}
#endif

				for (const FDNATagTableRow& TableRow : FoundSource->SourceTagList->DNATagList)
				{
					AddTagTableRow(TableRow, TagSource);
				}
			}
		}

		// Grab the commonly replicated tags
		CommonlyReplicatedTags.Empty();
		for (FName TagName : MutableDefault->CommonlyReplicatedTags)
		{
			FDNATag Tag = RequestDNATag(TagName);
			if (Tag.IsValid())
			{
				CommonlyReplicatedTags.Add(Tag);
			}
			else
			{
				UE_LOG(LogDNATags, Warning, TEXT("%s was found in the CommonlyReplicatedTags list but doesn't appear to be a valid tag!"), *TagName.ToString());
			}
		}

		bUseFastReplication = MutableDefault->FastReplication;
		bShouldWarnOnInvalidTags = MutableDefault->WarnOnInvalidTags;
		NumBitsForContainerSize = MutableDefault->NumBitsForContainerSize;
		NetIndexFirstBitSegment = MutableDefault->NetIndexFirstBitSegment;

		if (ShouldUseFastReplication())
		{
#if STATS
			FString PerfMessage = FString::Printf(TEXT("UDNATagsManager::ConstructDNATagTree: Reconstruct NetIndex"));
			SCOPE_LOG_TIME_IN_SECONDS(*PerfMessage, nullptr)
#endif
			ConstructNetIndex();
		}

		{
#if STATS
			FString PerfMessage = FString::Printf(TEXT("UDNATagsManager::ConstructDNATagTree: DNATagTreeChangedEvent.Broadcast"));
			SCOPE_LOG_TIME_IN_SECONDS(*PerfMessage, nullptr)
#endif
			IDNATagsModule::OnDNATagTreeChanged.Broadcast();
		}

		// Update the TagRedirects map
		TagRedirects.Empty();

		// Check the deprecated location
		bool bFoundDeprecated = false;
		FConfigSection* PackageRedirects = GConfig->GetSectionPrivate(TEXT("/Script/Engine.Engine"), false, true, DefaultEnginePath);

		if (PackageRedirects)
		{
			for (FConfigSection::TIterator It(*PackageRedirects); It; ++It)
			{
				if (It.Key() == TEXT("+DNATagRedirects"))
				{
					FName OldTagName = NAME_None;
					FName NewTagName;

					if (FParse::Value(*It.Value().GetValue(), TEXT("OldTagName="), OldTagName))
					{
						if (FParse::Value(*It.Value().GetValue(), TEXT("NewTagName="), NewTagName))
						{
							FDNATagRedirect Redirect;
							Redirect.OldTagName = OldTagName;
							Redirect.NewTagName = NewTagName;

							MutableDefault->DNATagRedirects.AddUnique(Redirect);

							bFoundDeprecated = true;
						}
					}
				}
			}
		}

		if (bFoundDeprecated)
		{
			UE_LOG(LogDNATags, Log, TEXT("DNATagRedirects is in a deprecated location, after editing DNATags developer settings you must remove these manually"));
		}

		// Check settings object
		for (const FDNATagRedirect& Redirect : MutableDefault->DNATagRedirects)
		{
			FName OldTagName = Redirect.OldTagName;
			FName NewTagName = Redirect.NewTagName;

			if (ensureMsgf(!TagRedirects.Contains(OldTagName), TEXT("Old tag %s is being redirected to more than one tag. Please remove all the redirections except for one."), *OldTagName.ToString()))
			{
				FDNATag OldTag = RequestDNATag(OldTagName, false); //< This only succeeds if OldTag is in the Table!
				if (OldTag.IsValid())
				{
					UE_LOG(LogDNATags, Warning,
						TEXT("Old tag (%s) which is being redirected still exists in the table!  Generally you should "
						TEXT("remove the old tags from the table when you are redirecting to new tags, or else users will ")
						TEXT("still be able to add the old tags to containers.")), *OldTagName.ToString()
						);
				}

				FDNATag NewTag = (NewTagName != NAME_None) ? RequestDNATag(NewTagName, false) : FDNATag();

				// Basic infinite recursion guard
				int32 IterationsLeft = 10;
				while (!NewTag.IsValid() && NewTagName != NAME_None)
				{
					bool bFoundRedirect = false;

					// See if it got redirected again
					for (const FDNATagRedirect& SecondRedirect : MutableDefault->DNATagRedirects)
					{
						if (SecondRedirect.OldTagName == NewTagName)
						{
							NewTagName = SecondRedirect.NewTagName;
							NewTag = RequestDNATag(NewTagName, false);
							bFoundRedirect = true;
							break;
						}
					}
					IterationsLeft--;

					if (!bFoundRedirect || IterationsLeft <= 0)
					{
						UE_LOG(LogDNATags, Warning, TEXT("Invalid new tag %s!  Cannot replace old tag %s."),
							*Redirect.NewTagName.ToString(), *Redirect.OldTagName.ToString());
						break;
					}
				}

				if (NewTag.IsValid())
				{
					// Populate the map
					TagRedirects.Add(OldTagName, NewTag);
				}
			}
		}
	}
}

void UDNATagsManager::ConstructNetIndex()
{
	NetworkDNATagNodeIndex.Empty();

	DNATagNodeMap.GenerateValueArray(NetworkDNATagNodeIndex);

	NetworkDNATagNodeIndex.Sort(FCompareFDNATagNodeByTag());

	check(CommonlyReplicatedTags.Num() <= NetworkDNATagNodeIndex.Num());

	// Put the common indices up front
	for (int32 CommonIdx=0; CommonIdx < CommonlyReplicatedTags.Num(); ++CommonIdx)
	{
		int32 BaseIdx=0;
		FDNATag& Tag = CommonlyReplicatedTags[CommonIdx];

		bool Found = false;
		for (int32 findidx=0; findidx < NetworkDNATagNodeIndex.Num(); ++findidx)
		{
			if (NetworkDNATagNodeIndex[findidx]->GetCompleteTag() == Tag)
			{
				NetworkDNATagNodeIndex.Swap(findidx, CommonIdx);
				Found = true;
				break;
			}
		}

		// A non fatal error should have been thrown when parsing the CommonlyReplicatedTags list. If we make it here, something is seriously wrong.
		checkf( Found, TEXT("Tag %s not found in NetworkDNATagNodeIndex"), *Tag.ToString() );
	}

	InvalidTagNetIndex = NetworkDNATagNodeIndex.Num()+1;
	NetIndexTrueBitNum = FMath::CeilToInt(FMath::Log2(InvalidTagNetIndex));
	
	// This should never be smaller than NetIndexTrueBitNum
	NetIndexFirstBitSegment = FMath::Min<int64>(NetIndexFirstBitSegment, NetIndexTrueBitNum);

	// This is now sorted and it should be the same on both client and server
	if (NetworkDNATagNodeIndex.Num() >= INVALID_TAGNETINDEX)
	{
		ensureMsgf(false, TEXT("Too many tags in dictionary for networking! Remove tags or increase tag net index size"));

		NetworkDNATagNodeIndex.SetNum(INVALID_TAGNETINDEX - 1);
	}

	for (FDNATagNetIndex i = 0; i < NetworkDNATagNodeIndex.Num(); i++)
	{
		if (NetworkDNATagNodeIndex[i].IsValid())
		{
			NetworkDNATagNodeIndex[i]->NetIndex = i;
		}
	}
}

FName UDNATagsManager::GetTagNameFromNetIndex(FDNATagNetIndex Index) const
{
	if (Index >= NetworkDNATagNodeIndex.Num())
	{
		// Ensure Index is the invalid index. If its higher than that, then something is wrong.
		ensureMsgf(Index == InvalidTagNetIndex, TEXT("Received invalid tag net index %d! Tag index is out of sync on client!"), Index);
		return NAME_None;
	}
	return NetworkDNATagNodeIndex[Index]->GetCompleteTagName();
}

FDNATagNetIndex UDNATagsManager::GetNetIndexFromTag(const FDNATag &InTag) const
{
	TSharedPtr<FDNATagNode> DNATagNode = FindTagNode(InTag);

	if (DNATagNode.IsValid())
	{
		return DNATagNode->GetNetIndex();
	}

	return InvalidTagNetIndex;
}

bool UDNATagsManager::ShouldImportTagsFromINI() const
{
	UDNATagsSettings* MutableDefault = GetMutableDefault<UDNATagsSettings>();
	FString DefaultEnginePath = FString::Printf(TEXT("%sDefaultEngine.ini"), *FPaths::SourceConfigDir());

	// Deprecated path
	bool ImportFromINI = false;
	if (GConfig->GetBool(TEXT("DNATags"), TEXT("ImportTagsFromConfig"), ImportFromINI, DefaultEnginePath))
	{
		if (ImportFromINI)
		{
			MutableDefault->ImportTagsFromConfig = ImportFromINI;
			UE_LOG(LogDNATags, Log, TEXT("ImportTagsFromConfig is in a deprecated location, open and save DNATag settings to fix"));
		}
		return ImportFromINI;
	}

	return MutableDefault->ImportTagsFromConfig;
}

void UDNATagsManager::RedirectTagsForContainer(FDNATagContainer& Container, UProperty* SerializingProperty) const
{
	TSet<FName> NamesToRemove;
	TSet<const FDNATag*> TagsToAdd;

	// First populate the NamesToRemove and TagsToAdd sets by finding tags in the container that have redirects
	for (auto TagIt = Container.CreateConstIterator(); TagIt; ++TagIt)
	{
		const FName TagName = TagIt->GetTagName();
		const FDNATag* NewTag = TagRedirects.Find(TagName);
		if (NewTag)
		{
			NamesToRemove.Add(TagName);
			if (NewTag->IsValid())
			{
				TagsToAdd.Add(NewTag);
			}
		}
#if WITH_EDITOR
		else if (SerializingProperty)
		{
			// Warn about invalid tags at load time in editor builds, too late to fix it in cooked builds
			FDNATag OldTag = RequestDNATag(TagName, false);
			if (!OldTag.IsValid() && ShouldWarnOnInvalidTags())
			{
				UE_LOG(LogDNATags, Warning, TEXT("Invalid DNATag %s found while loading property %s."), *TagName.ToString(), *GetPathNameSafe(SerializingProperty));
			}
		}
#endif
	}

	// Remove all tags from the NamesToRemove set
	for (FName RemoveName : NamesToRemove)
	{
		FDNATag OldTag = RequestDNATag(RemoveName, false);
		if (OldTag.IsValid())
		{
			Container.RemoveTag(OldTag);
		}
		else
		{
			Container.RemoveTagByExplicitName(RemoveName);
		}
	}

	// Add all tags from the TagsToAdd set
	for (const FDNATag* AddTag : TagsToAdd)
	{
		check(AddTag);
		Container.AddTag(*AddTag);
	}
}

void UDNATagsManager::RedirectSingleDNATag(FDNATag& Tag, UProperty* SerializingProperty) const
{
	const FName TagName = Tag.GetTagName();
	const FDNATag* NewTag = TagRedirects.Find(TagName);
	if (NewTag)
	{
		if (NewTag->IsValid())
		{
			Tag = *NewTag;
		}
	}
#if WITH_EDITOR
	else if (TagName != NAME_None && SerializingProperty)
	{
		// Warn about invalid tags at load time in editor builds, too late to fix it in cooked builds
		FDNATag OldTag = RequestDNATag(TagName, false);
		if (!OldTag.IsValid() && ShouldWarnOnInvalidTags())
		{
			UE_LOG(LogDNATags, Warning, TEXT("Invalid DNATag %s found while loading property %s."), *TagName.ToString(), *GetPathNameSafe(SerializingProperty));
		}
	}
#endif
}

void UDNATagsManager::InitializeManager()
{
	check(!SingletonManager);

	SingletonManager = NewObject<UDNATagsManager>(GetTransientPackage(), NAME_None);
	SingletonManager->AddToRoot();

	UDNATagsSettings* MutableDefault = GetMutableDefault<UDNATagsSettings>();
	FString DefaultEnginePath = FString::Printf(TEXT("%sDefaultEngine.ini"), *FPaths::SourceConfigDir());

	TArray<FString> DNATagTables;
	GConfig->GetArray(TEXT("DNATags"), TEXT("+DNATagTableList"), DNATagTables, DefaultEnginePath);

	// Report deprecation
	if (DNATagTables.Num() > 0)
	{
		UE_LOG(LogDNATags, Log, TEXT("DNATagTableList is in a deprecated location, open and save DNATag settings to fix"));
		for (const FString& DataTable : DNATagTables)
		{
			MutableDefault->DNATagTableList.AddUnique(DataTable);
		}
	}

	SingletonManager->LoadDNATagTables();
	SingletonManager->ConstructDNATagTree();

	// Bind to end of engine init to be done adding native tags
	UEngine::OnPostEngineInit.AddUObject(SingletonManager, &UDNATagsManager::DoneAddingNativeTags);
}

void UDNATagsManager::PopulateTreeFromDataTable(class UDataTable* InTable)
{
	checkf(DNARootTag.IsValid(), TEXT("ConstructDNATagTree() must be called before PopulateTreeFromDataTable()"));
	static const FString ContextString(TEXT("UDNATagsManager::PopulateTreeFromDataTable"));
	
	TArray<FDNATagTableRow*> TagTableRows;
	InTable->GetAllRows<FDNATagTableRow>(ContextString, TagTableRows);

	FName SourceName = InTable->GetOutermost()->GetFName();

	FDNATagSource* FoundSource = FindOrAddTagSource(SourceName, EDNATagSourceType::DataTable);

	for (const FDNATagTableRow* TagRow : TagTableRows)
	{
		if (TagRow)
		{
			AddTagTableRow(*TagRow, SourceName);
		}
	}
}

void UDNATagsManager::AddTagTableRow(const FDNATagTableRow& TagRow, FName SourceName)
{
	TSharedPtr<FDNATagNode> CurNode = DNARootTag;

	// Split the tag text on the "." delimiter to establish tag depth and then insert each tag into the
	// DNA tag tree
	TArray<FString> SubTags;
	TagRow.Tag.ToString().ParseIntoArray(SubTags, TEXT("."), true);

	for (int32 SubTagIdx = 0; SubTagIdx < SubTags.Num(); ++SubTagIdx)
	{
		TArray< TSharedPtr<FDNATagNode> >& ChildTags = CurNode.Get()->GetChildTagNodes();

		bool bFromDictionary = (SubTagIdx == (SubTags.Num() - 1));
		int32 InsertionIdx = InsertTagIntoNodeArray(*SubTags[SubTagIdx], CurNode, ChildTags, bFromDictionary ? SourceName : NAME_None, TagRow.DevComment);

		CurNode = ChildTags[InsertionIdx];
	}
}

UDNATagsManager::~UDNATagsManager()
{
	DestroyDNATagTree();
	SingletonManager = nullptr;
}

void UDNATagsManager::DestroyDNATagTree()
{
	if (DNARootTag.IsValid())
	{
		DNARootTag->ResetNode();
		DNARootTag.Reset();
		DNATagNodeMap.Reset();
	}
}

int32 UDNATagsManager::InsertTagIntoNodeArray(FName Tag, TSharedPtr<FDNATagNode> ParentNode, TArray< TSharedPtr<FDNATagNode> >& NodeArray, FName SourceName, const FString& DevComment)
{
	int32 InsertionIdx = INDEX_NONE;
	int32 WhereToInsert = INDEX_NONE;

	// See if the tag is already in the array
	for (int32 CurIdx = 0; CurIdx < NodeArray.Num(); ++CurIdx)
	{
		if (NodeArray[CurIdx].IsValid())
		{
			if (NodeArray[CurIdx].Get()->GetSimpleTagName() == Tag)
			{
				InsertionIdx = CurIdx;
				break;
			}
			else if (NodeArray[CurIdx].Get()->GetSimpleTagName() > Tag && WhereToInsert == INDEX_NONE)
			{
				// Insert new node before this
				WhereToInsert = CurIdx;
			}
		}
	}

	if (InsertionIdx == INDEX_NONE)
	{
		if (WhereToInsert == INDEX_NONE)
		{
			// Insert at end
			WhereToInsert = NodeArray.Num();
		}

		// Don't add the root node as parent
		TSharedPtr<FDNATagNode> TagNode = MakeShareable(new FDNATagNode(Tag, ParentNode != DNARootTag ? ParentNode : nullptr));

		// Add at the sorted location
		InsertionIdx = NodeArray.Insert(TagNode, WhereToInsert);

		FDNATag DNATag = TagNode->GetCompleteTag();

		{
#if WITH_EDITOR
			// This critical section is to handle an editor-only issue where tag requests come from another thread when async loading from a background thread in FDNATagContainer::Serialize.
			// This function is not generically threadsafe.
			FScopeLock Lock(&DNATagMapCritical);
#endif
			DNATagNodeMap.Add(DNATag, TagNode);
		}
	}

#if WITH_EDITOR
	static FName NativeSourceName = FDNATagSource::GetNativeName();
	// Set/update editor only data
	if (NodeArray[InsertionIdx]->SourceName.IsNone() && !SourceName.IsNone())
	{
		NodeArray[InsertionIdx]->SourceName = SourceName;
	}
	else if (SourceName == NativeSourceName)
	{
		// Native overrides other types
		NodeArray[InsertionIdx]->SourceName = SourceName;
	}

	if (NodeArray[InsertionIdx]->DevComment.IsEmpty() && !DevComment.IsEmpty())
	{
		NodeArray[InsertionIdx]->DevComment = DevComment;
	}
#endif

	return InsertionIdx;
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
void UDNATagsManager::PrintReplicationFrequencyReport()
{
	UE_LOG(LogDNATags, Warning, TEXT("================================="));
	UE_LOG(LogDNATags, Warning, TEXT("DNA Tags Replication Report"));

	UE_LOG(LogDNATags, Warning, TEXT("\nTags replicated solo:"));
	ReplicationCountMap_SingleTags.ValueSort(TGreater<int32>());
	for (auto& It : ReplicationCountMap_SingleTags)
	{
		UE_LOG(LogDNATags, Warning, TEXT("%s - %d"), *It.Key.ToString(), It.Value);
	}
	
	// ---------------------------------------

	UE_LOG(LogDNATags, Warning, TEXT("\nTags replicated in containers:"));
	ReplicationCountMap_Containers.ValueSort(TGreater<int32>());
	for (auto& It : ReplicationCountMap_Containers)
	{
		UE_LOG(LogDNATags, Warning, TEXT("%s - %d"), *It.Key.ToString(), It.Value);
	}

	// ---------------------------------------

	UE_LOG(LogDNATags, Warning, TEXT("\nAll Tags replicated:"));
	ReplicationCountMap.ValueSort(TGreater<int32>());
	for (auto& It : ReplicationCountMap)
	{
		UE_LOG(LogDNATags, Warning, TEXT("%s - %d"), *It.Key.ToString(), It.Value);
	}

	TMap<int32, int32> SavingsMap;
	int32 BaselineCost = 0;
	for (int32 Bits=1; Bits < NetIndexTrueBitNum; ++Bits)
	{
		int32 TotalSavings = 0;
		BaselineCost = 0;

		FDNATagNetIndex ExpectedNetIndex=0;
		for (auto& It : ReplicationCountMap)
		{
			int32 ExpectedCostBits = 0;
			bool FirstSeg = ExpectedNetIndex < FMath::Pow(2, Bits);
			if (FirstSeg)
			{
				// This would fit in the first Bits segment
				ExpectedCostBits = Bits+1;
			}
			else
			{
				// Would go in the second segment, so we pay the +1 cost
				ExpectedCostBits = NetIndexTrueBitNum+1;
			}

			int32 Savings = (NetIndexTrueBitNum - ExpectedCostBits) * It.Value;
			BaselineCost += NetIndexTrueBitNum * It.Value;

			//UE_LOG(LogDNATags, Warning, TEXT("[Bits: %d] Tag %s would save %d bits"), Bits, *It.Key.ToString(), Savings);
			ExpectedNetIndex++;
			TotalSavings += Savings;
		}

		SavingsMap.FindOrAdd(Bits) = TotalSavings;
	}

	SavingsMap.ValueSort(TGreater<int32>());
	int32 BestBits = 0;
	for (auto& It : SavingsMap)
	{
		if (BestBits == 0)
		{
			BestBits = It.Key;
		}

		UE_LOG(LogDNATags, Warning, TEXT("%d bits would save %d (%.2f)"), It.Key, It.Value, (float)It.Value / (float)BaselineCost);
	}

	UE_LOG(LogDNATags, Warning, TEXT("\nSuggested config:"));

	// Write out a nice copy pastable config
	int32 Count=0;
	for (auto& It : ReplicationCountMap)
	{
		UE_LOG(LogDNATags, Warning, TEXT("+CommonlyReplicatedTags=%s"), *It.Key.ToString());

		if (Count == FMath::Pow(2, BestBits))
		{
			// Print a blank line out, indicating tags after this are not necessary but still may be useful if the user wants to manually edit the list.
			UE_LOG(LogDNATags, Warning, TEXT(""));
		}

		if (Count++ >= FMath::Pow(2, BestBits+1))
		{
			break;
		}
	}

	UE_LOG(LogDNATags, Warning, TEXT("NetIndexFirstBitSegment=%d"), BestBits);

	UE_LOG(LogDNATags, Warning, TEXT("================================="));
}

void UDNATagsManager::NotifyTagReplicated(FDNATag Tag, bool WasInContainer)
{
	ReplicationCountMap.FindOrAdd(Tag)++;

	if (WasInContainer)
	{
		ReplicationCountMap_Containers.FindOrAdd(Tag)++;
	}
	else
	{
		ReplicationCountMap_SingleTags.FindOrAdd(Tag)++;
	}
	
}
#endif

#if WITH_EDITOR

static void RecursiveRootTagSearch(const FString& InFilterString, const TArray<TSharedPtr<FDNATagNode> >& DNARootTags, TArray< TSharedPtr<FDNATagNode> >& OutTagArray)
{
	FString CurrentFilter, RestOfFilter;
	if (!InFilterString.Split(TEXT("."), &CurrentFilter, &RestOfFilter))
	{
		CurrentFilter = InFilterString;
	}

	for (int32 iTag = 0; iTag < DNARootTags.Num(); ++iTag)
	{
		FString RootTagName = DNARootTags[iTag].Get()->GetSimpleTagName().ToString();

		if (RootTagName.Equals(CurrentFilter) == true)
		{
			if (RestOfFilter.IsEmpty())
			{
				// We've reached the end of the filter, add tags
				OutTagArray.Add(DNARootTags[iTag]);
			}
			else
			{
				// Recurse into our children
				RecursiveRootTagSearch(RestOfFilter, DNARootTags[iTag]->GetChildTagNodes(), OutTagArray);
			}
		}		
	}
}

void UDNATagsManager::GetFilteredDNARootTags(const FString& InFilterString, TArray< TSharedPtr<FDNATagNode> >& OutTagArray) const
{
	TArray<FString> Filters;
	TArray<TSharedPtr<FDNATagNode>>& DNARootTags = DNARootTag->GetChildTagNodes();

	OutTagArray.Empty();
	if( InFilterString.ParseIntoArray( Filters, TEXT( "," ), true ) > 0 )
	{
		// Check all filters in the list
		for (int32 iFilter = 0; iFilter < Filters.Num(); ++iFilter)
		{
			RecursiveRootTagSearch(Filters[iFilter], DNARootTags, OutTagArray);
		}
	}
	else
	{
		// No Filters just return them all
		OutTagArray = DNARootTags;
	}
}

void UDNATagsManager::GetAllTagsFromSource(FName TagSource, TArray< TSharedPtr<FDNATagNode> >& OutTagArray) const
{
	for (const TPair<FDNATag, TSharedPtr<FDNATagNode>>& NodePair : DNATagNodeMap)
	{
		if (NodePair.Value->SourceName == TagSource)
		{
			OutTagArray.Add(NodePair.Value);
		}
	}
}

bool UDNATagsManager::IsDictionaryTag(FName TagName) const
{
	TSharedPtr<FDNATagNode> Node = FindTagNode(TagName);
	if (Node.IsValid() && Node->SourceName != NAME_None)
	{
		return true;
	}

	return false;
}

bool UDNATagsManager::GetTagEditorData(FName TagName, FString& OutComment, FName &OutTagSource) const
{
	TSharedPtr<FDNATagNode> Node = FindTagNode(TagName);
	if (Node.IsValid())
	{
		OutComment = Node->DevComment;
		OutTagSource = Node->SourceName;
		return true;
	}
	return false;
}

void UDNATagsManager::EditorRefreshDNATagTree()
{
	DestroyDNATagTree();
	LoadDNATagTables();
	ConstructDNATagTree();
}

FDNATagContainer UDNATagsManager::RequestDNATagChildrenInDictionary(const FDNATag& DNATag) const
{
	// Note this purposefully does not include the passed in DNATag in the container.
	FDNATagContainer TagContainer;

	TSharedPtr<FDNATagNode> DNATagNode = FindTagNode(DNATag);
	if (DNATagNode.IsValid())
	{
		AddChildrenTags(TagContainer, DNATagNode, true, true);
	}
	return TagContainer;
}

#endif // WITH_EDITOR

const FDNATagSource* UDNATagsManager::FindTagSource(FName TagSourceName) const
{
	for (const FDNATagSource& TagSource : TagSources)
	{
		if (TagSource.SourceName == TagSourceName)
		{
			return &TagSource;
		}
	}
	return nullptr;
}

void UDNATagsManager::FindTagSourcesWithType(EDNATagSourceType TagSourceType, TArray<const FDNATagSource*>& OutArray) const
{
	for (const FDNATagSource& TagSource : TagSources)
	{
		if (TagSource.SourceType == TagSourceType)
		{
			OutArray.Add(&TagSource);
		}
	}
}

FDNATagSource* UDNATagsManager::FindOrAddTagSource(FName TagSourceName, EDNATagSourceType SourceType)
{
	const FDNATagSource* FoundSource = FindTagSource(TagSourceName);
	if (FoundSource)
	{
		return const_cast<FDNATagSource*>(FoundSource);
	}

	// Need to make a new one

	FDNATagSource* NewSource = new(TagSources) FDNATagSource(TagSourceName, SourceType);

	if (SourceType == EDNATagSourceType::DefaultTagList)
	{
		NewSource->SourceTagList = GetMutableDefault<UDNATagsSettings>();
	}
	else if (SourceType == EDNATagSourceType::TagList)
	{
		NewSource->SourceTagList = NewObject<UDNATagsList>(this, TagSourceName, RF_Transient);
		NewSource->SourceTagList->ConfigFileName = FString::Printf(TEXT("%sTags/%s"), *FPaths::SourceConfigDir(), *TagSourceName.ToString());
	}

	return NewSource;
}

DECLARE_CYCLE_STAT(TEXT("UDNATagsManager::RequestDNATag"), STAT_UDNATagsManager_RequestDNATag, STATGROUP_DNATags);

FDNATag UDNATagsManager::RequestDNATag(FName TagName, bool ErrorIfNotFound) const
{
	SCOPE_CYCLE_COUNTER(STAT_UDNATagsManager_RequestDNATag);
#if WITH_EDITOR
	// This critical section is to handle and editor-only issue where tag requests come from another thread when async loading from a background thread in FDNATagContainer::Serialize.
	// This function is not generically threadsafe.
	FScopeLock Lock(&DNATagMapCritical);
#endif

	FDNATag PossibleTag(TagName);

	if (DNATagNodeMap.Contains(PossibleTag))
	{
		return PossibleTag;
	}
	else if (ErrorIfNotFound)
	{
		static TSet<FName> MissingTagName;
		if (!MissingTagName.Contains(TagName))
		{
			ensureAlwaysMsgf(false, TEXT("Requested Tag %s was not found. Check tag data table."), *TagName.ToString());
			MissingTagName.Add(TagName);
		}
	}
	return FDNATag();
}

FDNATag UDNATagsManager::AddNativeDNATag(FName TagName)
{
	if (TagName.IsNone())
	{
		return FDNATag();
	}

	// Unsafe to call after done adding
	if (ensure(!bDoneAddingNativeTags))
	{
		FDNATag NewTag = FDNATag(TagName);

		if (!NativeTagsToAdd.Contains(TagName))
		{
			NativeTagsToAdd.Add(TagName);
		}

		AddTagTableRow(FDNATagTableRow(TagName), FDNATagSource::GetNativeName());

		return NewTag;
	}

	return FDNATag();
}

void UDNATagsManager::DoneAddingNativeTags()
{
	// Safe to call multiple times, only works the first time
	if (!bDoneAddingNativeTags)
	{
		bDoneAddingNativeTags = true;

		if (ShouldUseFastReplication())
		{
			ConstructNetIndex();
		}
	}
}

FDNATagContainer UDNATagsManager::RequestDNATagParents(const FDNATag& DNATag) const
{
	const FDNATagContainer* ParentTags = GetSingleTagContainer(DNATag);

	if (ParentTags)
	{
		return ParentTags->GetDNATagParents();
	}
	return FDNATagContainer();
}

void UDNATagsManager::RequestAllDNATags(FDNATagContainer& TagContainer, bool OnlyIncludeDictionaryTags) const
{
	TArray<TSharedPtr<FDNATagNode>> ValueArray;
	DNATagNodeMap.GenerateValueArray(ValueArray);
	for (const TSharedPtr<FDNATagNode>& TagNode : ValueArray)
	{
#if WITH_EDITOR
		bool DictTag = IsDictionaryTag(TagNode->GetSimpleTagName());
#else
		bool DictTag = false;
#endif 
		if (!OnlyIncludeDictionaryTags || DictTag)
		{
			const FDNATag* Tag = DNATagNodeMap.FindKey(TagNode);
			check(Tag);
			TagContainer.AddTagFast(*Tag);
		}
	}
}

FDNATagContainer UDNATagsManager::RequestDNATagChildren(const FDNATag& DNATag) const
{
	FDNATagContainer TagContainer;
	// Note this purposefully does not include the passed in DNATag in the container.
	TSharedPtr<FDNATagNode> DNATagNode = FindTagNode(DNATag);
	if (DNATagNode.IsValid())
	{
		AddChildrenTags(TagContainer, DNATagNode, true, false);
	}
	return TagContainer;
}

FDNATag UDNATagsManager::RequestDNATagDirectParent(const FDNATag& DNATag) const
{
	TSharedPtr<FDNATagNode> DNATagNode = FindTagNode(DNATag);
	if (DNATagNode.IsValid())
	{
		TSharedPtr<FDNATagNode> Parent = DNATagNode->GetParentTagNode();
		if (Parent.IsValid())
		{
			return Parent->GetCompleteTag();
		}
	}
	return FDNATag();
}

void UDNATagsManager::AddChildrenTags(FDNATagContainer& TagContainer, TSharedPtr<FDNATagNode> DNATagNode, bool RecurseAll, bool OnlyIncludeDictionaryTags) const
{
	if (DNATagNode.IsValid())
	{
		TArray< TSharedPtr<FDNATagNode> >& ChildrenNodes = DNATagNode->GetChildTagNodes();
		for (TSharedPtr<FDNATagNode> ChildNode : ChildrenNodes)
		{
			if (ChildNode.IsValid())
			{
				bool bShouldInclude = true;

#if WITH_EDITORONLY_DATA
				if (OnlyIncludeDictionaryTags && ChildNode->SourceName == NAME_None)
				{
					// Only have info to do this in editor builds
					bShouldInclude = false;
				}
#endif	
				if (bShouldInclude)
				{
					TagContainer.AddTag(ChildNode->GetCompleteTag());
				}

				if (RecurseAll)
				{
					AddChildrenTags(TagContainer, ChildNode, true, OnlyIncludeDictionaryTags);
				}
			}

		}
	}
}

void UDNATagsManager::SplitDNATagFName(const FDNATag& Tag, TArray<FName>& OutNames) const
{
	TSharedPtr<FDNATagNode> CurNode = FindTagNode(Tag);
	while (CurNode.IsValid())
	{
		OutNames.Insert(CurNode->GetSimpleTagName(), 0);
		CurNode = CurNode->GetParentTagNode();
	}
}

int32 UDNATagsManager::DNATagsMatchDepth(const FDNATag& DNATagOne, const FDNATag& DNATagTwo) const
{
	TSet<FName> Tags1;
	TSet<FName> Tags2;

	TSharedPtr<FDNATagNode> TagNode = FindTagNode(DNATagOne);
	if (TagNode.IsValid())
	{
		GetAllParentNodeNames(Tags1, TagNode);
	}

	TagNode = FindTagNode(DNATagTwo);
	if (TagNode.IsValid())
	{
		GetAllParentNodeNames(Tags2, TagNode);
	}

	return Tags1.Intersect(Tags2).Num();
}

DECLARE_CYCLE_STAT(TEXT("UDNATagsManager::GetAllParentNodeNames"), STAT_UDNATagsManager_GetAllParentNodeNames, STATGROUP_DNATags);

void UDNATagsManager::GetAllParentNodeNames(TSet<FName>& NamesList, TSharedPtr<FDNATagNode> DNATag) const
{
	SCOPE_CYCLE_COUNTER(STAT_UDNATagsManager_GetAllParentNodeNames);

	NamesList.Add(DNATag->GetCompleteTagName());
	TSharedPtr<FDNATagNode> Parent = DNATag->GetParentTagNode();
	if (Parent.IsValid())
	{
		GetAllParentNodeNames(NamesList, Parent);
	}
}

DECLARE_CYCLE_STAT(TEXT("UDNATagsManager::ValidateTagCreation"), STAT_UDNATagsManager_ValidateTagCreation, STATGROUP_DNATags);

bool UDNATagsManager::ValidateTagCreation(FName TagName) const
{
	SCOPE_CYCLE_COUNTER(STAT_UDNATagsManager_ValidateTagCreation);

	return FindTagNode(TagName).IsValid();
}

FDNATagTableRow::FDNATagTableRow(FDNATagTableRow const& Other)
{
	*this = Other;
}

FDNATagTableRow& FDNATagTableRow::operator=(FDNATagTableRow const& Other)
{
	// Guard against self-assignment
	if (this == &Other)
	{
		return *this;
	}

	Tag = Other.Tag;
	DevComment = Other.DevComment;

	return *this;
}

bool FDNATagTableRow::operator==(FDNATagTableRow const& Other) const
{
	return (Tag == Other.Tag);
}

bool FDNATagTableRow::operator!=(FDNATagTableRow const& Other) const
{
	return (Tag != Other.Tag);
}

bool FDNATagTableRow::operator<(FDNATagTableRow const& Other) const
{
	return (Tag < Other.Tag);
}

FDNATagNode::FDNATagNode(FName InTag, TSharedPtr<FDNATagNode> InParentNode)
	: Tag(InTag)
	, ParentNode(InParentNode)
	, NetIndex(INVALID_TAGNETINDEX)
{
	TArray<FDNATag> ParentCompleteTags;

	TSharedPtr<FDNATagNode> CurNode = InParentNode;

	// Stop iterating at root node
	while (CurNode.IsValid() && CurNode->GetSimpleTagName() != NAME_None)
	{
		ParentCompleteTags.Add(CurNode->GetCompleteTag());
		CurNode = CurNode->GetParentTagNode();
	}

	FString CompleteTagString = InTag.ToString();

	if (ParentCompleteTags.Num() > 0)
	{
		// If we have a parent, add parent., which will includes all earlier parents
		CompleteTagString = FString::Printf(TEXT("%s.%s"), *ParentCompleteTags[0].ToString(), *InTag.ToString());
	}
	
	// Manually construct the tag container as we want to bypass the safety checks
	CompleteTagWithParents.DNATags.Add(FDNATag(FName(*CompleteTagString)));
	CompleteTagWithParents.ParentTags = ParentCompleteTags;
}

void FDNATagNode::ResetNode()
{
	Tag = NAME_None;
	CompleteTagWithParents.Reset();
	NetIndex = INVALID_TAGNETINDEX;

	for (int32 ChildIdx = 0; ChildIdx < ChildTags.Num(); ++ChildIdx)
	{
		ChildTags[ChildIdx]->ResetNode();
	}

	ChildTags.Empty();
	ParentNode.Reset();
}

#undef LOCTEXT_NAMESPACE
