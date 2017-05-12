// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "DNACueInterface.h"
#include "GameFramework/DefaultPawn.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemTestPawn.generated.h"

class UDNAAbilitySystemComponent;

UCLASS(Blueprintable, BlueprintType, notplaceable)
class DNAABILITIES_API ADNAAbilitySystemTestPawn : public ADefaultPawn, public IDNACueInterface, public IDNAAbilitySystemInterface
{
	GENERATED_UCLASS_BODY()

	virtual void PostInitializeComponents() override;

	virtual UDNAAbilitySystemComponent* GetDNAAbilitySystemComponent() const override;
	
private_subobject:
	/** DefaultPawn collision component */
	DEPRECATED_FORGAME(4.6, "DNAAbilitySystemComponent should not be accessed directly, please use GetDNAAbilitySystemComponent() function instead. DNAAbilitySystemComponent will soon be private and your code will not compile.")
	UPROPERTY(Category = DNAAbilitySystem, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UDNAAbilitySystemComponent* DNAAbilitySystemComponent;
public:

	//UPROPERTY(EditDefaultsOnly, Category=DNAEffects)
	//UDNAAbilitySet * DefaultAbilitySet;

	static FName DNAAbilitySystemComponentName;

	/** Returns DNAAbilitySystemComponent subobject **/
	class UDNAAbilitySystemComponent* GetDNAAbilitySystemComponent();
};
