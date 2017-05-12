// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "UObject/Package.h"
#include "Misc/AutomationTest.h"
#include "Engine/DataTable.h"
#include "DNATagContainer.h"
#include "DNATagsManager.h"
#include "DNATagsModule.h"
#include "Stats/StatsMisc.h"

#if WITH_DEV_AUTOMATION_TESTS

class FDNATagTestBase : public FAutomationTestBase
{
public:
	FDNATagTestBase(const FString& InName, const bool bInComplexTask)
		: FAutomationTestBase(InName, bInComplexTask)
	{
	}

	UDataTable* CreateDNADataTable()
	{	
		TArray<FString> TestTags;

		TestTags.Add(TEXT("Effect.Damage"));
		TestTags.Add(TEXT("Effect.Damage.Basic"));
		TestTags.Add(TEXT("Effect.Damage.Type1"));
		TestTags.Add(TEXT("Effect.Damage.Type2"));
		TestTags.Add(TEXT("Effect.Damage.Reduce"));
		TestTags.Add(TEXT("Effect.Damage.Buffable"));
		TestTags.Add(TEXT("Effect.Damage.Buff"));
		TestTags.Add(TEXT("Effect.Damage.Physical"));
		TestTags.Add(TEXT("Effect.Damage.Fire"));
		TestTags.Add(TEXT("Effect.Damage.Buffed.FireBuff"));
		TestTags.Add(TEXT("Effect.Damage.Mitigated.Armor"));
		TestTags.Add(TEXT("Effect.Lifesteal"));
		TestTags.Add(TEXT("Effect.Shield"));
		TestTags.Add(TEXT("Effect.Buff"));
		TestTags.Add(TEXT("Effect.Immune"));
		TestTags.Add(TEXT("Effect.FireDamage"));
		TestTags.Add(TEXT("Effect.Shield.Absorb"));
		TestTags.Add(TEXT("Effect.Protect.Damage"));
		TestTags.Add(TEXT("Stackable"));
		TestTags.Add(TEXT("Stack.DiminishingReturns"));
		TestTags.Add(TEXT("DNACue.Burning"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.1"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.2"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.3"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.4"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.5"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.6"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.7"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.8"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.9"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.10"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.11"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.12"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.13"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.14"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.15"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.16"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.17"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.18"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.19"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.20"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.21"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.22"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.23"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.24"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.25"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.26"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.27"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.28"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.29"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.30"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.31"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.32"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.33"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.34"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.35"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.36"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.37"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.38"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.39"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.40"));

		auto DataTable = NewObject<UDataTable>(GetTransientPackage(), FName(TEXT("TempDataTable")));
		DataTable->RowStruct = FDNATagTableRow::StaticStruct();

#if WITH_EDITOR
		FString CSV(TEXT(",Tag,CategoryText,"));
		for (int32 Count = 0; Count < TestTags.Num(); Count++)
		{
			CSV.Append(FString::Printf(TEXT("\r\n%d,%s"), Count, *TestTags[Count]));
		}

		DataTable->CreateTableFromCSVString(CSV);
#else
		// Construct the data table manually
		for (int32 Count = 0; Count < TestTags.Num(); Count++)
		{
			uint8* RowData = (uint8*)FMemory::Malloc(DataTable->RowStruct->PropertiesSize);
			DataTable->RowStruct->InitializeStruct(RowData);

			FDNATagTableRow* TagRow = (FDNATagTableRow*)RowData;
			TagRow->Tag = FName(*TestTags[Count]);

			DataTable->RowMap.Add(FName(*FString::FromInt(Count)), RowData);
		}
#endif
	
		FDNATagTableRow * Row = (FDNATagTableRow*)DataTable->RowMap["0"];
		if (Row)
		{
			check(Row->Tag == TEXT("Effect.Damage"));
		}
		return DataTable;
	}

	FDNATag GetTagForString(const FString& String)
	{
		return UDNATagsManager::Get().RequestDNATag(FName(*String));
	}

	void DNATagTest_SimpleTest()
	{
		FName TagName = FName(TEXT("Stack.DiminishingReturns"));
		FDNATag Tag = UDNATagsManager::Get().RequestDNATag(TagName);
		TestTrueExpr(Tag.GetTagName() == TagName);
	}

	void DNATagTest_TagComparisonTest()
	{
		FDNATag EffectDamageTag = GetTagForString(TEXT("Effect.Damage"));
		FDNATag EffectDamage1Tag = GetTagForString(TEXT("Effect.Damage.Type1"));
		FDNATag EffectDamage2Tag = GetTagForString(TEXT("Effect.Damage.Type2"));
		FDNATag CueTag = GetTagForString(TEXT("DNACue.Burning"));
		FDNATag EmptyTag;

		TestTrueExpr(EffectDamage1Tag == EffectDamage1Tag);
		TestTrueExpr(EffectDamage1Tag != EffectDamage2Tag);
		TestTrueExpr(EffectDamage1Tag != EffectDamageTag);

		TestTrueExpr(EffectDamage1Tag.MatchesTag(EffectDamageTag));
		TestTrueExpr(!EffectDamage1Tag.MatchesTagExact(EffectDamageTag));
		TestTrueExpr(!EffectDamage1Tag.MatchesTag(EmptyTag));
		TestTrueExpr(!EffectDamage1Tag.MatchesTagExact(EmptyTag));
		TestTrueExpr(!EmptyTag.MatchesTag(EmptyTag));
		TestTrueExpr(!EmptyTag.MatchesTagExact(EmptyTag));

		TestTrueExpr(EffectDamage1Tag.RequestDirectParent() == EffectDamageTag);
	}

	void DNATagTest_TagContainerTest()
	{
		FDNATag EffectDamageTag = GetTagForString(TEXT("Effect.Damage"));
		FDNATag EffectDamage1Tag = GetTagForString(TEXT("Effect.Damage.Type1"));
		FDNATag EffectDamage2Tag = GetTagForString(TEXT("Effect.Damage.Type2"));
		FDNATag CueTag = GetTagForString(TEXT("DNACue.Burning"));
		FDNATag EmptyTag;

		FDNATagContainer EmptyContainer;

		FDNATagContainer TagContainer;
		TagContainer.AddTag(EffectDamage1Tag);
		TagContainer.AddTag(CueTag);

		FDNATagContainer ReverseTagContainer;
		ReverseTagContainer.AddTag(CueTag);
		ReverseTagContainer.AddTag(EffectDamage1Tag);
	
		FDNATagContainer TagContainer2;
		TagContainer2.AddTag(EffectDamage2Tag);
		TagContainer2.AddTag(CueTag);

		TestTrueExpr(TagContainer == TagContainer);
		TestTrueExpr(TagContainer == ReverseTagContainer);
		TestTrueExpr(TagContainer != TagContainer2);

		FDNATagContainer TagContainerCopy = TagContainer;

		TestTrueExpr(TagContainerCopy == TagContainer);
		TestTrueExpr(TagContainerCopy != TagContainer2);

		TagContainerCopy.Reset();
		TagContainerCopy.AppendTags(TagContainer);

		TestTrueExpr(TagContainerCopy == TagContainer);
		TestTrueExpr(TagContainerCopy != TagContainer2);

		TestTrueExpr(TagContainer.HasAny(TagContainer2));
		TestTrueExpr(TagContainer.HasAnyExact(TagContainer2));
		TestTrueExpr(!TagContainer.HasAll(TagContainer2));
		TestTrueExpr(!TagContainer.HasAllExact(TagContainer2));
		TestTrueExpr(TagContainer.HasAll(TagContainerCopy));
		TestTrueExpr(TagContainer.HasAllExact(TagContainerCopy));

		TestTrueExpr(TagContainer.HasAll(EmptyContainer));
		TestTrueExpr(TagContainer.HasAllExact(EmptyContainer));
		TestTrueExpr(!TagContainer.HasAny(EmptyContainer));
		TestTrueExpr(!TagContainer.HasAnyExact(EmptyContainer));

		TestTrueExpr(EmptyContainer.HasAll(EmptyContainer));
		TestTrueExpr(EmptyContainer.HasAllExact(EmptyContainer));
		TestTrueExpr(!EmptyContainer.HasAny(EmptyContainer));
		TestTrueExpr(!EmptyContainer.HasAnyExact(EmptyContainer));

		TestTrueExpr(!EmptyContainer.HasAll(TagContainer));
		TestTrueExpr(!EmptyContainer.HasAllExact(TagContainer));
		TestTrueExpr(!EmptyContainer.HasAny(TagContainer));
		TestTrueExpr(!EmptyContainer.HasAnyExact(TagContainer));

		TestTrueExpr(TagContainer.HasTag(EffectDamageTag));
		TestTrueExpr(!TagContainer.HasTagExact(EffectDamageTag));
		TestTrueExpr(!TagContainer.HasTag(EmptyTag));
		TestTrueExpr(!TagContainer.HasTagExact(EmptyTag));

		TestTrueExpr(EffectDamage1Tag.MatchesAny(FDNATagContainer(EffectDamageTag)));
		TestTrueExpr(!EffectDamage1Tag.MatchesAnyExact(FDNATagContainer(EffectDamageTag)));

		TestTrueExpr(EffectDamage1Tag.MatchesAny(TagContainer));

		FDNATagContainer FilteredTagContainer = TagContainer.FilterExact(TagContainer2);
		TestTrueExpr(FilteredTagContainer.HasTagExact(CueTag));
		TestTrueExpr(!FilteredTagContainer.HasTagExact(EffectDamage1Tag));

		FilteredTagContainer = TagContainer.Filter(FDNATagContainer(EffectDamageTag));
		TestTrueExpr(!FilteredTagContainer.HasTagExact(CueTag));
		TestTrueExpr(FilteredTagContainer.HasTagExact(EffectDamage1Tag));

		FilteredTagContainer.Reset();
		FilteredTagContainer.AppendMatchingTags(TagContainer, TagContainer2);
	
		TestTrueExpr(FilteredTagContainer.HasTagExact(CueTag));
		TestTrueExpr(!FilteredTagContainer.HasTagExact(EffectDamage1Tag));
	}

	void DNATagTest_PerfTest()
	{
		FDNATag EffectDamageTag = GetTagForString(TEXT("Effect.Damage"));
		FDNATag EffectDamage1Tag = GetTagForString(TEXT("Effect.Damage.Type1"));
		FDNATag EffectDamage2Tag = GetTagForString(TEXT("Effect.Damage.Type2"));
		FDNATag CueTag = GetTagForString(TEXT("DNACue.Burning"));

		FDNATagContainer TagContainer;

		bool bResult = true;

		{
			FScopeLogTime LogTimePtr(*FString::Printf(TEXT("10000 get tag")), nullptr, FScopeLogTime::ScopeLog_Milliseconds);
			for (int32 i = 0; i < 10000; i++)
			{
				UDNATagsManager::Get().RequestDNATag(FName(TEXT("Effect.Damage")));
			}
		}

		{
			FScopeLogTime LogTimePtr(*FString::Printf(TEXT("1000 container constructions")), nullptr, FScopeLogTime::ScopeLog_Milliseconds);
			for (int32 i = 0; i < 1000; i++)
			{
				TagContainer = FDNATagContainer();
				TagContainer.AddTag(EffectDamage1Tag);
				TagContainer.AddTag(EffectDamage2Tag);
				TagContainer.AddTag(CueTag);
				for (int32 j = 1; j <= 40; j++)
				{
					TagContainer.AddTag(GetTagForString(FString::Printf(TEXT("Expensive.Status.Tag.Type.%d"), j)));
				}
			}
		}

		{
			FScopeLogTime LogTimePtr(*FString::Printf(TEXT("1000 container copies")), nullptr, FScopeLogTime::ScopeLog_Milliseconds);
			for (int32 i = 0; i < 1000; i++)
			{
				FDNATagContainer TagContainerNew;

				for (auto It = TagContainer.CreateConstIterator(); It; ++It)
				{
					TagContainerNew.AddTag(*It);
				}
			}
		}

		{
			FScopeLogTime LogTimePtr(*FString::Printf(TEXT("1000 container appends")), nullptr, FScopeLogTime::ScopeLog_Milliseconds);
			for (int32 i = 0; i < 1000; i++)
			{
				FDNATagContainer TagContainerNew;

				TagContainerNew.AppendTags(TagContainer);
			}
		}
	
		FDNATagContainer TagContainer2;
		TagContainer2.AddTag(EffectDamage1Tag);
		TagContainer2.AddTag(EffectDamage2Tag);
		TagContainer2.AddTag(CueTag);

		{
			FScopeLogTime LogTimePtr(*FString::Printf(TEXT("10000 MatchesAnyExact checks")), nullptr, FScopeLogTime::ScopeLog_Milliseconds);
			for (int32 i = 0; i < 10000; i++)
			{
				bResult &= EffectDamage1Tag.MatchesAnyExact(TagContainer);
			}
		}

		{
			FScopeLogTime LogTimePtr(*FString::Printf(TEXT("10000 MatchesAny checks")), nullptr, FScopeLogTime::ScopeLog_Milliseconds);
			for (int32 i = 0; i < 10000; i++)
			{
				bResult &= EffectDamage1Tag.MatchesAny(TagContainer);
			}
		}

		{
			FScopeLogTime LogTimePtr(*FString::Printf(TEXT("10000 HasTagExact checks")), nullptr, FScopeLogTime::ScopeLog_Milliseconds);
			for (int32 i = 0; i < 10000; i++)
			{
				bResult &= TagContainer.HasTagExact(EffectDamage1Tag);
			}
		}

		{
			FScopeLogTime LogTimePtr(*FString::Printf(TEXT("10000 HasTag checks")), nullptr, FScopeLogTime::ScopeLog_Milliseconds);
			for (int32 i = 0; i < 10000; i++)
			{
				bResult &= TagContainer.HasTag(EffectDamage1Tag);
			}
		}

		{
			FScopeLogTime LogTimePtr(*FString::Printf(TEXT("10000 HasAll checks")), nullptr, FScopeLogTime::ScopeLog_Milliseconds);
			for (int32 i = 0; i < 10000; i++)
			{
				bResult &= TagContainer.HasAll(TagContainer2);
			}
		}

		{
			FScopeLogTime LogTimePtr(*FString::Printf(TEXT("10000 HasAny checks")), nullptr, FScopeLogTime::ScopeLog_Milliseconds);
			for (int32 i = 0; i < 10000; i++)
			{
				bResult &= TagContainer.HasAny(TagContainer2);
			}
		}

		TestTrue(TEXT("Performance Tests succeeded"), bResult);
	}

};

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FDNATagTest, FDNATagTestBase, "System.DNATags.DNATag", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FDNATagTest::RunTest(const FString& Parameters)
{
	// Create Test Data 
	UDataTable* DataTable = CreateDNADataTable();

	UDNATagsManager::Get().PopulateTreeFromDataTable(DataTable);

	// Run Tests
	DNATagTest_SimpleTest();
	DNATagTest_TagComparisonTest();
	DNATagTest_TagContainerTest();
	DNATagTest_PerfTest();

	return !HasAnyErrors();
}

#endif //WITH_DEV_AUTOMATION_TESTS
