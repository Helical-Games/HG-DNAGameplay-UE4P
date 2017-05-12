// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNACueTranslator.h"
#include "HAL/IConsoleManager.h"
#include "DNACueSet.h"
#include "DNATagsManager.h"
#include "DNATagsModule.h"
#include "Stats/StatsMisc.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogDNACueTranslator, Display, All);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
static FAutoConsoleVariable CVarGameplyCueTranslatorDebugTag(TEXT("DNACue.Translator.DebugTag"), TEXT(""), TEXT("Debug Tag in DNA cue translation"), ECVF_Default	);
#endif

FDNACueTranslatorNodeIndex FDNACueTranslationManager::GetTranslationIndexForName(FName Name, bool CreateIfInvalid)
{
	FDNACueTranslatorNodeIndex Idx;
	if (CreateIfInvalid)
	{
		FDNACueTranslatorNodeIndex& MapIndex = TranslationNameToIndexMap.FindOrAdd(Name);
		if (MapIndex.IsValid() == false)
		{
			MapIndex = TranslationLUT.AddDefaulted();			
		}
		
		Idx = MapIndex;

		if (TranslationLUT[Idx].CachedIndex.IsValid() == false)
		{
			TranslationLUT[Idx].CachedIndex = Idx;
			TranslationLUT[Idx].CachedDNATag = TagManager->RequestDNATag(Name, false);
			TranslationLUT[Idx].CachedDNATagName = Name;
		}
	}
	else
	{
		if (FDNACueTranslatorNodeIndex* IdxPtr = TranslationNameToIndexMap.Find(Name))
		{
			Idx = *IdxPtr;
		}
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (Idx.IsValid() && TranslationLUT[Idx].CachedDNATagName.ToString().Contains(CVarGameplyCueTranslatorDebugTag->GetString()))
	{
		UE_LOG(LogDNACueTranslator, Log, TEXT("....."));
	}
#endif


	ensureAlways(!Idx.IsValid() || TranslationLUT[Idx].CachedDNATagName != NAME_None);

#if WITH_EDITOR
	// In the editor tags can be created after the initial creation of the translation data structures.
	// This will update the tag in subsequent requests
	if (Idx.IsValid() && TranslationLUT[Idx].CachedDNATag.IsValid() == false)
	{
		TranslationLUT[Idx].CachedDNATag = TagManager->RequestDNATag(Name, false);
	}
#endif

	return Idx;
}

FDNACueTranslatorNode* FDNACueTranslationManager::GetTranslationNodeForName(FName Name, bool CreateIfInvalid)
{
	FDNACueTranslatorNodeIndex Idx = GetTranslationIndexForName(Name, CreateIfInvalid);
	if (TranslationLUT.IsValidIndex(Idx))
	{
		return &TranslationLUT[Idx];
	}
	return nullptr;
}

FDNACueTranslatorNodeIndex FDNACueTranslationManager::GetTranslationIndexForTag(const FDNATag& Tag, bool CreateIfInvalid)
{
	return GetTranslationIndexForName(Tag.GetTagName(), CreateIfInvalid);
}

FDNACueTranslatorNode* FDNACueTranslationManager::GetTranslationNodeForTag(const FDNATag& Tag, bool CreateIfInvalid)
{
	FDNACueTranslatorNodeIndex Idx = GetTranslationIndexForTag(Tag, CreateIfInvalid);
	if (TranslationLUT.IsValidIndex(Idx))
	{
		return &TranslationLUT[Idx];
	}
	return nullptr;
}

FDNACueTranslationLink& FDNACueTranslatorNode::FindOrCreateLink(const UDNACueTranslator* RuleClassCDO, int32 LookupSize)
{
	int32 InsertIdx = 0;
	int32 NewPriority = RuleClassCDO->GetPriority();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (CVarGameplyCueTranslatorDebugTag->GetString().IsEmpty() == false && this->CachedDNATagName.ToString().Contains(CVarGameplyCueTranslatorDebugTag->GetString()))
	{
		UE_LOG(LogDNACueTranslator, Log, TEXT("....."));
	}
#endif

	for (int32 LinkIdx=0; LinkIdx < Links.Num(); ++LinkIdx)
	{
		if (Links[LinkIdx].RulesCDO == RuleClassCDO)
		{
			// Already here, return
			return Links[LinkIdx];
		}

		if (Links[LinkIdx].RulesCDO->GetPriority() > NewPriority)
		{
			// Insert after the existing one with higher priority
			InsertIdx = LinkIdx+1;
		}
	}

	check(InsertIdx <= Links.Num());

	FDNACueTranslationLink& NewLink = Links[Links.Insert(FDNACueTranslationLink(), InsertIdx)];
	NewLink.RulesCDO = RuleClassCDO;
	NewLink.NodeLookup.SetNum( LookupSize );
	return NewLink;
}

void FDNACueTranslationManager::RefreshNameSwaps()
{
	AllNameSwaps.Reset();
	TArray<UDNACueTranslator*> CDOList;

	// Gather CDOs
	for( TObjectIterator<UClass> It ; It ; ++It )
	{
		UClass* Class = *It;
		if( !Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated) )
		{
			if( Class->IsChildOf(UDNACueTranslator::StaticClass()) )
			{
				UDNACueTranslator* CDO = Class->GetDefaultObject<UDNACueTranslator>();
				if (CDO->IsEnabled())
				{
					CDOList.Add(CDO);
				}
			}
		}
	}

	// Sort and get translated names
	CDOList.Sort([](const UDNACueTranslator& A, const UDNACueTranslator& B) { return (A.GetPriority() > B.GetPriority()); });

	for (UDNACueTranslator* CDO : CDOList)
	{
		FNameSwapData& Data = AllNameSwaps[AllNameSwaps.AddDefaulted()];
		CDO->GetTranslationNameSpawns(Data.NameSwaps);
		if (Data.NameSwaps.Num() > 0)
		{
			Data.ClassCDO = CDO;
		}
		else
		{
			AllNameSwaps.Pop(false);
		}
	}

#if WITH_EDITOR
	// Give UniqueID to each rule
	int32 ID=1;
	for (FNameSwapData& GroupData : AllNameSwaps)
	{
		for (FDNACueTranslationNameSwap& SwapData : GroupData.NameSwaps)
		{
			SwapData.EditorData.UniqueID = ID++;
		}
	}
#endif
}

void FDNACueTranslationManager::ResetTranslationLUT()
{
	TranslationNameToIndexMap.Reset();
	TranslationLUT.Reset();
}

void FDNACueTranslationManager::BuildTagTranslationTable()
{
#if WITH_EDITOR
	//SCOPE_LOG_TIME_IN_SECONDS(*FString::Printf(TEXT("FDNACueTranslatorManager::BuildTagTranslationTables")), nullptr)
#endif

	TagManager = &UDNATagsManager::Get();
	check(TagManager);
	
	FDNATagContainer AllDNACueTags = TagManager->RequestDNATagChildren(UDNACueSet::BaseDNACueTag());
	
	ResetTranslationLUT();
	RefreshNameSwaps();

	// ----------------------------------------------------------------------------------------------
	
	// Find what tags may be derived from swap rules. Note how we work backwards.
	// If we worked forward, by expanding out all possible tags and then seeing if they exist, 
	// this would take much much longer!

	TArray<FName> SplitNames;
	SplitNames.Reserve(10);
	
	// All DNAcue tags
	for (const FDNATag& Tag : AllDNACueTags)
	{
		SplitNames.Reset();
		TagManager->SplitDNATagFName(Tag, SplitNames);

		BuildTagTranslationTable_r(Tag.GetTagName(), SplitNames);
	}

	// ----------------------------------------------------------------------------------------------
}

bool FDNACueTranslationManager::BuildTagTranslationTable_r(const FName& TagName, const TArray<FName>& SplitNames)
{

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (CVarGameplyCueTranslatorDebugTag->GetString().IsEmpty() == false && TagName.ToString().Contains(CVarGameplyCueTranslatorDebugTag->GetString()))
	{
		// 
		UE_LOG(LogDNACueTranslator, Log, TEXT("....."));
	}
#endif

	bool HasValidRootTag = false;

	TArray<FName> SwappedNames;
	SwappedNames.Reserve(10);

	// Every NameSwap Rule/Class that gave us data
	for (FNameSwapData& NameSwapData : AllNameSwaps)
	{
		// Avoid rule recursion
		{
			if (FDNACueTranslatorNode* ChildNode = GetTranslationNodeForName(TagName, false))
			{
				if (ChildNode->UsedTranslators.Contains(NameSwapData.ClassCDO))
				{
					continue;
				}
			}
		}

		// Every Swap that this Rule/Class gave us
		for (int32 SwapRuleIdx=0; SwapRuleIdx < NameSwapData.NameSwaps.Num(); ++SwapRuleIdx)
		{
			const FDNACueTranslationNameSwap& SwapRule = NameSwapData.NameSwaps[SwapRuleIdx];

#if WITH_EDITOR
			if (SwapRule.EditorData.Enabled == false)
			{
				continue;
			}
#endif

			// Walk through the original tag's elements
			for (int32 TagIdx=0; TagIdx < SplitNames.Num(); ++TagIdx)
			{
				// Walk through the potential new tag's elemnts
				for (int32 ToNameIdx=0; ToNameIdx < SwapRule.ToNames.Num() && TagIdx < SplitNames.Num(); ++ToNameIdx)
				{
					// If they match
					if (SwapRule.ToNames[ToNameIdx] == SplitNames[TagIdx])
					{
						// If we are at the end
						if (ToNameIdx == SwapRule.ToNames.Num()-1)
						{
							// *Possible* tag translation found! This tag can be derived from out name swapping rules, 
							// but we don't know if there actually is a tag that matches the tag which it would be translated *from*

							// Don't operator on SplitNames, since subsequent rules and name swaps use the same array
							SwappedNames = SplitNames;

							// Remove the "To Names" from the SwappedNames array, replace with the single "From Name"
							// E.g, GC.{Steel.Master} -> GC.{Hero}
							int32 NumRemoves = SwapRule.ToNames.Num(); // We are going to remove as many tags 
							int32 RemoveAtIdx = TagIdx - (SwapRule.ToNames.Num() - 1);
							check(SwappedNames.IsValidIndex(RemoveAtIdx));

							SwappedNames.RemoveAt(RemoveAtIdx, NumRemoves, false);
							SwappedNames.Insert(SwapRule.FromName, RemoveAtIdx);

							// Compose a string from the new name
							FString ComposedString = SwappedNames[0].ToString();							
							for (int32 ComposeIdx=1; ComposeIdx < SwappedNames.Num(); ++ ComposeIdx)
							{
								ComposedString += FString::Printf(TEXT(".%s"), *SwappedNames[ComposeIdx].ToString());
							}
								
							UE_LOG(LogDNACueTranslator, Log, TEXT("Found possible expanded tag. Original Child Tag: %s. Possible Parent Tag: %s"), *TagName.ToString(), *ComposedString);
							FName ComposedName = FName(*ComposedString);

							// Look for this tag - is it an actual real tag? If not, continue on
							{
								FDNATag ComposedTag = TagManager->RequestDNATag(ComposedName, false);
								if (ComposedTag.IsValid() == false)
								{
									UE_LOG(LogDNACueTranslator, Log, TEXT("   No tag match found, recursing..."));
									
									FDNACueTranslatorNodeIndex ParentIdx = GetTranslationIndexForName( ComposedName, false );
									if (ParentIdx.IsValid() == false)
									{
										ParentIdx = GetTranslationIndexForName( ComposedName, true );
										check(ParentIdx.IsValid());
										TranslationLUT[ParentIdx].UsedTranslators.Add( NameSwapData.ClassCDO );
																		
										HasValidRootTag |= BuildTagTranslationTable_r(ComposedName, SwappedNames);
									}
								}
								else
								{
									HasValidRootTag = true;
								}
							}

							if (HasValidRootTag)
							{
								// Add it to our data structures
								FDNACueTranslatorNodeIndex ParentIdx = GetTranslationIndexForName(ComposedName, true);
								check(ParentIdx.IsValid());
								
								UE_LOG(LogDNACueTranslator, Log, TEXT("   Matches real tags! Adding to translation tree"));

								FDNACueTranslatorNodeIndex ChildIdx = GetTranslationIndexForName(TagName, true);
								ensure(ChildIdx != INDEX_NONE);

								// Note: important to do this after getting ChildIdx since allocating idx can move TranslationMap memory around
								FDNACueTranslatorNode& ParentNode = TranslationLUT[ParentIdx];

								FDNACueTranslationLink& NewLink = ParentNode.FindOrCreateLink(NameSwapData.ClassCDO, NameSwapData.NameSwaps.Num());
										
								// Verify this link hasn't already been established
								ensure(NewLink.NodeLookup[SwapRuleIdx] != ChildIdx);

								// Setup the link
								NewLink.NodeLookup[SwapRuleIdx] = ChildIdx;

								// Now make sure we don't reapply this rule to this child node or any of its child nodes
								FDNACueTranslatorNode& ChildNode = TranslationLUT[ChildIdx];
								ChildNode.UsedTranslators.Append( ParentNode.UsedTranslators );
								ChildNode.UsedTranslators.Add( NameSwapData.ClassCDO );
							}
							else
							{
								UE_LOG(LogDNACueTranslator, Log, TEXT("   No tag match found after recursing. Dead end."));
							}

							break;
						}
						else
						{
							// Keep going, advance TagIdx
							TagIdx++;
							continue;
						}
					}
					else
					{
						// Match failed
						break;
					}
				}
			}
		}
	}

	return HasValidRootTag;
}

void FDNACueTranslationManager::BuildTagTranslationTable_Forward()
{
#if WITH_EDITOR
	SCOPE_LOG_TIME_IN_SECONDS(*FString::Printf(TEXT("FDNACueTranslatorManager::BuildTagTranslationTable_Forward")), nullptr)
#endif

	// Build the normal TranslationLUT first. This is only done to make sure that UsedTranslators are filled in, giving "real" tags higher priority.
	// Example:
	//	1) GC.Rampage.Enraged
	//	2) GC.Rampage.Elemental.Enraged
	//	
	//	2 is am override for 1, but comes first alphabetically. In the _Forward method, 2 would be handled first and expanded again to GC.Rampage.Elemental.Elemental.Enraged.
	//	rule recursion wouldn't have been hit yet because 2 actually exists and would be encountered before 1.
	//
	//	Since BuildTagTranslationTable_Forward is only called by the editor and BuildTagTranslationTable is already fast, this is the simplest way to avoid the above example.
	//	_Forward() could be made more complicated to test for this itself, but doesn't seem like a good trade off for how it would complicate the function.
	BuildTagTranslationTable();

	TArray<FName> SplitNames;
	SplitNames.Reserve(10);
	
	FDNATagContainer AllDNACueTags = TagManager->RequestDNATagChildren(UDNACueSet::BaseDNACueTag());

	// Each DNACueTag
	for (const FDNATag& Tag : AllDNACueTags)
	{
		SplitNames.Reset();
		TagManager->SplitDNATagFName(Tag, SplitNames);

		BuildTagTranslationTable_Forward_r(Tag.GetTagName(), SplitNames);
	}
}

void FDNACueTranslationManager::BuildTagTranslationTable_Forward_r(const FName& TagName, const TArray<FName>& SplitNames)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (CVarGameplyCueTranslatorDebugTag->GetString().IsEmpty() == false && TagName.ToString().Contains(CVarGameplyCueTranslatorDebugTag->GetString()))
	{
		// 
		UE_LOG(LogDNACueTranslator, Log, TEXT("....."));
	}
#endif

	TArray<FName> SwappedNames;
	SwappedNames.Reserve(10);

	// Each nameswap rule group
	for (FNameSwapData& NameSwapData : AllNameSwaps)
	{
		// Avoid rule recursion
		{
			if (FDNACueTranslatorNode* ChildNode = GetTranslationNodeForName(TagName, false))
			{
				if (ChildNode->UsedTranslators.Contains(NameSwapData.ClassCDO))
				{
					continue;
				}
			}
		}

		// Each swaprule 
		for (int32 SwapRuleIdx=0; SwapRuleIdx < NameSwapData.NameSwaps.Num(); ++SwapRuleIdx)
		{
			const FDNACueTranslationNameSwap& SwapRule = NameSwapData.NameSwaps[SwapRuleIdx];

#if WITH_EDITOR
			if (SwapRule.EditorData.Enabled == false)
			{
				continue;
			}
#endif

			// Each subtag within this DNATag
			for (int32 TagIdx=0; TagIdx < SplitNames.Num(); ++TagIdx)
			{
				if (SplitNames[TagIdx] == SwapRule.FromName)
				{
					SwappedNames = SplitNames;

					// Possible match!
					// Done - full match found
					SwappedNames.RemoveAt(TagIdx, 1, false);
					for (int32 ToIdx=0; ToIdx < SwapRule.ToNames.Num(); ++ToIdx)
					{
						SwappedNames.Insert(SwapRule.ToNames[ToIdx], TagIdx + ToIdx);
					}

					FString ComposedString = SwappedNames[0].ToString();
					for (int32 ComposeIdx=1; ComposeIdx < SwappedNames.Num(); ++ ComposeIdx)
					{
						ComposedString += FString::Printf(TEXT(".%s"), *SwappedNames[ComposeIdx].ToString());
					}
						
					UE_LOG(LogDNACueTranslator, Log, TEXT("Found possible new expanded tag. Original: %s. Parent: %s"), *TagName.ToString(), *ComposedString);
					FName ComposedName = FName(*ComposedString);

					FDNACueTranslatorNodeIndex ChildIdx = GetTranslationIndexForName( ComposedName, true );
					if ( ChildIdx.IsValid() )
					{
						FDNACueTranslatorNodeIndex ParentIdx = GetTranslationIndexForName( TagName, true );
						if (ParentIdx.IsValid())
						{
							FDNACueTranslatorNode& ParentNode = TranslationLUT[ParentIdx];
							FDNACueTranslatorNode& ChildNode = TranslationLUT[ChildIdx];

							// Find or create the link structure out of the parent node
							FDNACueTranslationLink& NewLink = ParentNode.FindOrCreateLink(NameSwapData.ClassCDO, NameSwapData.NameSwaps.Num());
							
							NewLink.NodeLookup[SwapRuleIdx] = ChildNode.CachedIndex;

							ChildNode.UsedTranslators.Append(ParentNode.UsedTranslators);
							ChildNode.UsedTranslators.Add(NameSwapData.ClassCDO);
						}
					}

					BuildTagTranslationTable_Forward_r(ComposedName, SwappedNames);
				}
			}
		}
	}
}

void FDNACueTranslationManager::TranslateTag(FDNATag& Tag, AActor* TargetActor, const FDNACueParameters& Parameters)
{
	if (FDNACueTranslatorNode* Node = GetTranslationNodeForTag(Tag))
	{
		TranslateTag_Internal(*Node, Tag, Tag.GetTagName(), TargetActor, Parameters);
	}
}

bool FDNACueTranslationManager::TranslateTag_Internal(FDNACueTranslatorNode& Node, FDNATag& OutTag, const FName& TagName, AActor* TargetActor, const FDNACueParameters& Parameters)
{
	for (FDNACueTranslationLink& Link : Node.Links)
	{
		// Have CDO give us TranslationIndex. This is 0 - (number of name swaps this class gave us)
		int32 TranslationIndex = Link.RulesCDO->DNACueToTranslationIndex(TagName, TargetActor, Parameters);
		if (TranslationIndex != INDEX_NONE)
		{
			// Use the link's NodeLookup to get the real NodeIndex
			FDNACueTranslatorNodeIndex NodeIndex = Link.NodeLookup[TranslationIndex];
			if (NodeIndex != INDEX_NONE)
			{
				// Warn if more links?
				FDNACueTranslatorNode& InnerNode = TranslationLUT[NodeIndex];

				UE_LOG(LogDNACueTranslator, Verbose, TEXT("Translating %s --> %s (via %s)"), *TagName.ToString(), *InnerNode.CachedDNATagName.ToString(), *GetNameSafe(Link.RulesCDO));

				OutTag = InnerNode.CachedDNATag;
				
				TranslateTag_Internal(InnerNode, OutTag, InnerNode.CachedDNATagName, TargetActor, Parameters);
				return true;
			}
		}
	}

	return false;
}

void FDNACueTranslationManager::PrintTranslationTable()
{
	UE_LOG(LogDNACueTranslator, Display, TEXT("Printing DNACue Translation Table. * means tag is not created but could be."));

	TotalNumTranslations = 0;
	TotalNumTheoreticalTranslations = 0;
	for (FDNACueTranslatorNode& Node : TranslationLUT)
	{
		PrintTranslationTable_r(Node);
	}

	UE_LOG(LogDNACueTranslator, Display, TEXT(""));
	UE_LOG(LogDNACueTranslator, Display, TEXT("Total Number of Translations with valid tags: %d"), TotalNumTranslations);
	UE_LOG(LogDNACueTranslator, Display, TEXT("Total Number of Translations without  valid tags: %d (theoretical translations)"), TotalNumTheoreticalTranslations);
}

void FDNACueTranslationManager::PrintTranslationTable_r(FDNACueTranslatorNode& Node, FString IdentStr)
{
	if (Node.Links.Num() > 0)
	{
		if (IdentStr.IsEmpty())
		{
			UE_LOG(LogDNACueTranslator, Display, TEXT("%s %s"), *Node.CachedDNATagName.ToString(), Node.CachedDNATag.IsValid() ? TEXT("") : TEXT("*"));
		}

		for (FDNACueTranslationLink& Link : Node.Links)
		{
			for (FDNACueTranslatorNodeIndex& Index : Link.NodeLookup)
			{
				if (Index.IsValid())
				{
					FDNACueTranslatorNode& InnerNode = TranslationLUT[Index];

					if (InnerNode.CachedDNATag.IsValid())
					{
						UE_LOG(LogDNACueTranslator, Display, TEXT("%s -> %s [%s]"), *IdentStr, *InnerNode.CachedDNATag.ToString(), *GetNameSafe(Link.RulesCDO) );
						TotalNumTranslations++;
					}
					else
					{
						UE_LOG(LogDNACueTranslator, Display, TEXT("%s -> %s [%s] *"), *IdentStr, *InnerNode.CachedDNATagName.ToString(), *GetNameSafe(Link.RulesCDO) );
						TotalNumTheoreticalTranslations++;
					}

					PrintTranslationTable_r(InnerNode, IdentStr + TEXT("  "));
				}
			}
		}

		UE_LOG(LogDNACueTranslator, Display, TEXT(""));
	}
}

// --------------------------------------------------------------------------------------------------
#if WITH_EDITOR
bool FDNACueTranslationManager::GetTranslatedTags(const FName& ParentTag, TArray<FDNACueTranslationEditorInfo>& Children)
{
	if (const FDNACueTranslatorNode* Node = GetTranslationNodeForName(ParentTag))
	{
		for (const FDNACueTranslationLink& Link : Node->Links)
		{
			for (int32 LinkIdx=0; LinkIdx < Link.NodeLookup.Num(); ++LinkIdx)
			{
				const FDNACueTranslatorNodeIndex& Index = Link.NodeLookup[LinkIdx];
				if (Index != INDEX_NONE)
				{
					FDNACueTranslatorNode& ChildNode = TranslationLUT[Index];

					// Find the description of the rule this came from
					for (FNameSwapData& SwapData : AllNameSwaps)
					{
						if (SwapData.ClassCDO == Link.RulesCDO)
						{
							check(SwapData.NameSwaps.IsValidIndex(LinkIdx));

							FDNACueTranslationEditorInfo& Info = Children[Children.AddDefaulted()];
							Info.DNATagName = ChildNode.CachedDNATagName;
							Info.DNATag = ChildNode.CachedDNATag;
							Info.EditorData = SwapData.NameSwaps[LinkIdx].EditorData;
							break;
						}
					}
				}
			}
		}
	}
	return Children.Num() > 0;
}
#endif
