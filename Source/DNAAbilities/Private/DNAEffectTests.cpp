// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "AttributeSet.h"
#include "DNAEffectTypes.h"
#include "DNAEffect.h"
#include "DNAAbilitiesModule.h"
#include "DNATagsManager.h"
#include "DNATagsModule.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemTestPawn.h"
#include "AbilitySystemTestAttributeSet.h"
#include "AbilitySystemGlobals.h"

#define SKILL_TEST_TEXT( Format, ... ) FString::Printf(TEXT("%s - %d: %s"), TEXT(__FILE__) , __LINE__ , *FString::Printf(TEXT(Format), ##__VA_ARGS__) )

#if WITH_EDITOR

static UDataTable* CreateDNADataTable()
{
	FString CSV(TEXT(",Tag,CategoryText,"));
	CSV.Append(TEXT("\r\n0,Damage"));
	CSV.Append(TEXT("\r\n1,Damage.Basic"));
	CSV.Append(TEXT("\r\n2,Damage.Type1"));
	CSV.Append(TEXT("\r\n3,Damage.Type2"));
	CSV.Append(TEXT("\r\n4,Damage.Reduce"));
	CSV.Append(TEXT("\r\n5,Damage.Buffable"));
	CSV.Append(TEXT("\r\n6,Damage.Buff"));
	CSV.Append(TEXT("\r\n7,Damage.Physical"));
	CSV.Append(TEXT("\r\n8,Damage.Fire"));
	CSV.Append(TEXT("\r\n9,Damage.Buffed.FireBuff"));
	CSV.Append(TEXT("\r\n10,Damage.Mitigated.Armor"));
	CSV.Append(TEXT("\r\n11,Lifesteal"));
	CSV.Append(TEXT("\r\n12,Shield"));
	CSV.Append(TEXT("\r\n13,Buff"));
	CSV.Append(TEXT("\r\n14,Immune"));
	CSV.Append(TEXT("\r\n15,FireDamage"));
	CSV.Append(TEXT("\r\n16,ShieldAbsorb"));
	CSV.Append(TEXT("\r\n17,Stackable"));
	CSV.Append(TEXT("\r\n18,Stack"));
	CSV.Append(TEXT("\r\n19,Stack.CappedNumber"));
	CSV.Append(TEXT("\r\n20,Stack.DiminishingReturns"));
	CSV.Append(TEXT("\r\n21,Protect.Damage"));
	CSV.Append(TEXT("\r\n22,SpellDmg.Buff"));
	CSV.Append(TEXT("\r\n23,DNACue.Burning"));

	auto DataTable = NewObject<UDataTable>(GetTransientPackage(), FName(TEXT("TempDataTable")));
	DataTable->RowStruct = FDNATagTableRow::StaticStruct();
	DataTable->CreateTableFromCSVString(CSV);

	FDNATagTableRow * Row = (FDNATagTableRow*)DataTable->RowMap["0"];
	if (Row)
	{
		check(Row->Tag == TEXT("Damage"));
	}
	return DataTable;
}

#define GET_FIELD_CHECKED(Class, Field) FindFieldChecked<UProperty>(Class::StaticClass(), GET_MEMBER_NAME_CHECKED(Class, Field))
#define CONSTRUCT_CLASS(Class, Name) Class* Name = NewObject<Class>(GetTransientPackage(), FName(TEXT(#Name)))

class DNAEffectsTestSuite
{
public:
	DNAEffectsTestSuite(UWorld* WorldIn, FAutomationTestBase* TestIn)
	: World(WorldIn)
	, Test(TestIn)
	{
		// run before each test

		const float StartingHealth = 100.f;
		const float StartingMana = 200.f;

		// set up the source actor
		SourceActor = World->SpawnActor<ADNAAbilitySystemTestPawn>();
		SourceComponent = SourceActor->GetDNAAbilitySystemComponent();
		SourceComponent->GetSet<UDNAAbilitySystemTestAttributeSet>()->Health = StartingHealth;
		SourceComponent->GetSet<UDNAAbilitySystemTestAttributeSet>()->MaxHealth = StartingHealth;
		SourceComponent->GetSet<UDNAAbilitySystemTestAttributeSet>()->Mana = StartingMana;
		SourceComponent->GetSet<UDNAAbilitySystemTestAttributeSet>()->MaxMana = StartingMana;

		// set up the destination actor
		DestActor = World->SpawnActor<ADNAAbilitySystemTestPawn>();
		DestComponent = DestActor->GetDNAAbilitySystemComponent();
		DestComponent->GetSet<UDNAAbilitySystemTestAttributeSet>()->Health = StartingHealth;
		DestComponent->GetSet<UDNAAbilitySystemTestAttributeSet>()->MaxHealth = StartingHealth;
		DestComponent->GetSet<UDNAAbilitySystemTestAttributeSet>()->Mana = StartingMana;
		DestComponent->GetSet<UDNAAbilitySystemTestAttributeSet>()->MaxMana = StartingMana;
	}

	~DNAEffectsTestSuite()
	{
		// run after each test

		// destroy the actors
		if (SourceActor)
		{
			World->EditorDestroyActor(SourceActor, false);
		}
		if (DestActor)
		{
			World->EditorDestroyActor(DestActor, false);
		}
	}

public: // the tests

	void Test_InstantDamage()
	{
		const float DamageValue = 5.f;
		const float StartingHealth = DestComponent->GetSet<UDNAAbilitySystemTestAttributeSet>()->Health;

		// just try and reduce the health attribute
		{
			
			CONSTRUCT_CLASS(UDNAEffect, BaseDmgEffect);
			AddModifier(BaseDmgEffect, GET_FIELD_CHECKED(UDNAAbilitySystemTestAttributeSet, Health), EDNAModOp::Additive, FScalableFloat(-DamageValue));
			BaseDmgEffect->DurationPolicy = EDNAEffectDurationType::Instant;
			
			SourceComponent->ApplyDNAEffectToTarget(BaseDmgEffect, DestComponent, 1.f);
		}

		// make sure health was reduced
		TestEqual(SKILL_TEST_TEXT("Health Reduced"), DestComponent->GetSet<UDNAAbilitySystemTestAttributeSet>()->Health, StartingHealth - DamageValue);
	}

	void Test_InstantDamageRemap()
	{
		const float DamageValue = 5.f;
		const float StartingHealth = DestComponent->GetSet<UDNAAbilitySystemTestAttributeSet>()->Health;

		// This is the same as DNAEffectsTest_InstantDamage but modifies the Damage attribute and confirms it is remapped to -Health by UDNAAbilitySystemTestAttributeSet::PostAttributeModify
		{
			CONSTRUCT_CLASS(UDNAEffect, BaseDmgEffect);
			AddModifier(BaseDmgEffect, GET_FIELD_CHECKED(UDNAAbilitySystemTestAttributeSet, Damage), EDNAModOp::Additive, FScalableFloat(DamageValue));
			BaseDmgEffect->DurationPolicy = EDNAEffectDurationType::Instant;

			SourceComponent->ApplyDNAEffectToTarget(BaseDmgEffect, DestComponent, 1.f);
		}

		// Now we should have lost some health
		TestEqual(SKILL_TEST_TEXT("Health Reduced"), DestComponent->GetSet<UDNAAbilitySystemTestAttributeSet>()->Health, StartingHealth - DamageValue);

		// Confirm the damage attribute itself was reset to 0 when it was applied to health
		TestEqual(SKILL_TEST_TEXT("Damage Applied"), DestComponent->GetSet<UDNAAbilitySystemTestAttributeSet>()->Damage, 0.f);
	}

	void Test_ManaBuff()
	{
		const float BuffValue = 30.f;
		const float StartingMana = DestComponent->GetSet<UDNAAbilitySystemTestAttributeSet>()->Mana;

		FActiveDNAEffectHandle BuffHandle;

		// apply the buff
		{
			CONSTRUCT_CLASS(UDNAEffect, DamageBuffEffect);
			AddModifier(DamageBuffEffect, GET_FIELD_CHECKED(UDNAAbilitySystemTestAttributeSet, Mana), EDNAModOp::Additive, FScalableFloat(BuffValue));
			DamageBuffEffect->DurationPolicy = EDNAEffectDurationType::Infinite;

			BuffHandle = SourceComponent->ApplyDNAEffectToTarget(DamageBuffEffect, DestComponent, 1.f);
		}

		// check that the value changed
		TestEqual(SKILL_TEST_TEXT("Mana Buffed"), DestComponent->GetSet<UDNAAbilitySystemTestAttributeSet>()->Mana, StartingMana + BuffValue);

		// remove the effect
		{
			DestComponent->RemoveActiveDNAEffect(BuffHandle);
		}

		// check that the value changed back
		TestEqual(SKILL_TEST_TEXT("Mana Restored"), DestComponent->GetSet<UDNAAbilitySystemTestAttributeSet>()->Mana, StartingMana);
	}

	void Test_PeriodicDamage()
	{
		const int32 NumPeriods = 10;
		const float PeriodSecs = 1.0f;
		const float DamagePerPeriod = 5.f; 
		const float StartingHealth = DestComponent->GetSet<UDNAAbilitySystemTestAttributeSet>()->Health;

		// just try and reduce the health attribute
		{
			CONSTRUCT_CLASS(UDNAEffect, BaseDmgEffect);
			AddModifier(BaseDmgEffect, GET_FIELD_CHECKED(UDNAAbilitySystemTestAttributeSet, Health), EDNAModOp::Additive, FScalableFloat(-DamagePerPeriod));
			BaseDmgEffect->DurationPolicy = EDNAEffectDurationType::HasDuration;
			BaseDmgEffect->DurationMagnitude = FDNAEffectModifierMagnitude(FScalableFloat(NumPeriods * PeriodSecs));
			BaseDmgEffect->Period.Value = PeriodSecs;

			SourceComponent->ApplyDNAEffectToTarget(BaseDmgEffect, DestComponent, 1.f);
		}

		int32 NumApplications = 0;

		// Tick a small number to verify the application tick
		TickWorld(SMALL_NUMBER);
		++NumApplications;

		TestEqual(SKILL_TEST_TEXT("Health Reduced"), DestComponent->GetSet<UDNAAbilitySystemTestAttributeSet>()->Health, StartingHealth - (DamagePerPeriod * NumApplications));

		// Tick a bit more to address possible floating point issues
		TickWorld(PeriodSecs * .1f);

		for (int32 i = 0; i < NumPeriods; ++i)
		{
			// advance time by one period
			TickWorld(PeriodSecs);

			++NumApplications;

			// check that health has been reduced
			TestEqual(SKILL_TEST_TEXT("Health Reduced"), DestComponent->GetSet<UDNAAbilitySystemTestAttributeSet>()->Health, StartingHealth - (DamagePerPeriod * NumApplications));
		}

		// advance time by one extra period
		TickWorld(PeriodSecs);

		// should not have reduced further
		TestEqual(SKILL_TEST_TEXT("Health Reduced"), DestComponent->GetSet<UDNAAbilitySystemTestAttributeSet>()->Health, StartingHealth - (DamagePerPeriod * NumApplications));

		// TODO: test that the effect is no longer applied
	}

private: // test helpers

	void TestEqual(const FString& TestText, float Actual, float Expected)
	{
		Test->TestEqual(FString::Printf(TEXT("%s: %f (actual) != %f (expected)"), *TestText, Actual, Expected), Actual, Expected);
	}

	template<typename MODIFIER_T>
	FDNAModifierInfo& AddModifier(UDNAEffect* Effect, UProperty* Property, EDNAModOp::Type Op, const MODIFIER_T& Magnitude)
	{
		int32 Idx = Effect->Modifiers.Num();
		Effect->Modifiers.SetNum(Idx+1);
		FDNAModifierInfo& Info = Effect->Modifiers[Idx];
		Info.ModifierMagnitude = Magnitude;
		Info.ModifierOp = Op;
		Info.Attribute.SetUProperty(Property);
		return Info;
	}

	void TickWorld(float Time)
	{
		const float step = 0.1f;
		while (Time > 0.f)
		{
			World->Tick(ELevelTick::LEVELTICK_All, FMath::Min(Time, step));
			Time -= step;

			// This is terrible but required for subticking like this.
			// we could always cache the real GFrameCounter at the start of our tests and restore it when finished.
			GFrameCounter++;
		}
	}

private:
	UWorld* World;
	FAutomationTestBase* Test;

	ADNAAbilitySystemTestPawn* SourceActor;
	UDNAAbilitySystemComponent* SourceComponent;

	ADNAAbilitySystemTestPawn* DestActor;
	UDNAAbilitySystemComponent* DestComponent;
};

#define ADD_TEST(Name) \
	TestFunctions.Add(&DNAEffectsTestSuite::Name); \
	TestFunctionNames.Add(TEXT(#Name))

class FDNAEffectsTest : public FAutomationTestBase
{
public:
	typedef void (DNAEffectsTestSuite::*TestFunc)();

	FDNAEffectsTest(const FString& InName)
	: FAutomationTestBase(InName, false)
	{
		// list all test functions here
		ADD_TEST(Test_InstantDamage);
		ADD_TEST(Test_InstantDamageRemap);
		ADD_TEST(Test_ManaBuff);
		ADD_TEST(Test_PeriodicDamage);
	}

	virtual uint32 GetTestFlags() const override { return EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter; }
	virtual bool IsStressTest() const { return false; }
	virtual uint32 GetRequiredDeviceNum() const override { return 1; }

protected:
	virtual FString GetBeautifiedTestName() const override { return "System.DNAAbilitySystem.DNAEffects"; }
	virtual void GetTests(TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands) const override
	{
		for (const FString& TestFuncName : TestFunctionNames)
		{
			OutBeautifiedNames.Add(TestFuncName);
			OutTestCommands.Add(TestFuncName);
		}
	}

	bool RunTest(const FString& Parameters) override
	{
		// find the matching test
		TestFunc TestFunction = nullptr;
		for (int32 i = 0; i < TestFunctionNames.Num(); ++i)
		{
			if (TestFunctionNames[i] == Parameters)
			{
				TestFunction = TestFunctions[i];
				break;
			}
		}
		if (TestFunction == nullptr)
		{
			return false;
		}

		// get the current curve and data table (to restore later)
		UCurveTable *CurveTable = IDNAAbilitiesModule::Get().GetDNAAbilitySystemGlobals()->GetGlobalCurveTable();
		UDataTable *DataTable = IDNAAbilitiesModule::Get().GetDNAAbilitySystemGlobals()->GetGlobalAttributeMetaDataTable();

		// setup required DNATags
		UDataTable* TagTable = CreateDNADataTable();

		UDNATagsManager::Get().PopulateTreeFromDataTable(TagTable);

		UWorld *World = UWorld::CreateWorld(EWorldType::Game, false);
		FWorldContext &WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
		WorldContext.SetCurrentWorld(World);

		FURL URL;
		World->InitializeActorsForPlay(URL);
		World->BeginPlay();

		// run the matching test
		uint64 InitialFrameCounter = GFrameCounter;
		{
			DNAEffectsTestSuite Tester(World, this);
			(Tester.*TestFunction)();
		}
		GFrameCounter = InitialFrameCounter;

		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);

		IDNAAbilitiesModule::Get().GetDNAAbilitySystemGlobals()->AutomationTestOnly_SetGlobalCurveTable(CurveTable);
		IDNAAbilitiesModule::Get().GetDNAAbilitySystemGlobals()->AutomationTestOnly_SetGlobalAttributeDataTable(DataTable);
		return true;
	}

	TArray<TestFunc> TestFunctions;
	TArray<FString> TestFunctionNames;
};

namespace
{
	FDNAEffectsTest FDNAEffectsTestAutomationTestInstance(TEXT("FDNAEffectsTest"));
}

#endif //WITH_EDITOR
