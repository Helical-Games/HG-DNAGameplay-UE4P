// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Engine/DataAsset.h"
#include "Abilities/DNAAbility.h"
#include "DNAAbilitySet.generated.h"

class UDNAAbilitySystemComponent;

/**
 *	This is an example input binding enum for DNAAbilities. Your project may want to create its own.
 *	The MetaData default bind keys (LMB, RMB, Q, E, etc) are a convenience for designers setting up abilities sets
 *	or whatever other data you have that gives an ability with a default key binding. Actual key binding is up to DefaultInput.ini
 *	
 *	E.g., "Ability1" is the command string that is bound to DNAAbilitySystemComponent::ActivateAbility(1). The Meta data only *suggests*
 *	that you are binding "Ability1" to LMB by default in your projects DefaultInput.ini.
 */
UENUM(BlueprintType)
namespace EDNAAbilityInputBinds
{
	enum Type
	{
		Ability1				UMETA(DisplayName = "Ability1 (LMB)"),
		Ability2				UMETA(DisplayName = "Ability2 (RMB)"),
		Ability3				UMETA(DisplayName = "Ability3 (Q)"),
		Ability4				UMETA(DisplayName = "Ability4 (E)"),
		Ability5				UMETA(DisplayName = "Ability5 (R)"),
		Ability6				UMETA(DisplayName = "Ability6 (Shift)"),
		Ability7				UMETA(DisplayName = "Ability7 (Space)"),
		Ability8				UMETA(DisplayName = "Ability8 (B)"),
		Ability9				UMETA(DisplayName = "Ability9 (T)"),
	};
}

/**
 *	Example struct that pairs a enum input command to a DNAAbilityClass.6
 */
USTRUCT()
struct FDNAAbilityBindInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = BindInfo)
	TEnumAsByte<EDNAAbilityInputBinds::Type>	Command;

	UPROPERTY(EditAnywhere, Category = BindInfo)
	TSubclassOf<UDNAAbility>	DNAAbilityClass;
};

/**
 *	This is an example DataAsset that could be used for defining a set of abilities to give to an DNAAbilitySystemComponent and bind to an input command.
 *	Your project is free to implement this however it wants!
 *	
 *	
 */
UCLASS()
class DNAABILITIES_API UDNAAbilitySet : public UDataAsset
{
	GENERATED_UCLASS_BODY()

	
	UPROPERTY(EditAnywhere, Category = AbilitySet)
	TArray<FDNAAbilityBindInfo>	Abilities;

	void GiveAbilities(UDNAAbilitySystemComponent* DNAAbilitySystemComponent) const;
};
