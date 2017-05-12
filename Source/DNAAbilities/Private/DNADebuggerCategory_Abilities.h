// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#if WITH_DNA_DEBUGGER
#include "DNADebuggerCategory.h"
#endif

class AActor;
class APlayerController;

#if WITH_DNA_DEBUGGER

class FDNADebuggerCategory_Abilities : public FDNADebuggerCategory
{
public:
	FDNADebuggerCategory_Abilities();

	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;
	virtual void DrawData(APlayerController* OwnerPC, FDNADebuggerCanvasContext& CanvasContext) override;

	static TSharedRef<FDNADebuggerCategory> MakeInstance();

protected:
	struct FRepData
	{
		FString OwnedTags;

		struct FDNAAbilityDebug
		{
			FString Ability;
			FString Source;
			int32 Level;
			bool bIsActive;
		};
		TArray<FDNAAbilityDebug> Abilities;

		struct FDNAEffectDebug
		{
			FString Effect;
			FString Context;
			float Duration;
			float Period;
			int32 Stacks;
			float Level;
		};
		TArray<FDNAEffectDebug> DNAEffects;

		void Serialize(FArchive& Ar);
	};
	FRepData DataPack;
};

#endif // WITH_DNA_DEBUGGER
