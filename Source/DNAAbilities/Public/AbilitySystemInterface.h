// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"
#include "AbilitySystemInterface.generated.h"

class UDNAAbilitySystemComponent;

/** Interface for actors that expose access to an ability system component */
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UDNAAbilitySystemInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class DNAABILITIES_API IDNAAbilitySystemInterface
{
	GENERATED_IINTERFACE_BODY()

	virtual UDNAAbilitySystemComponent* GetDNAAbilitySystemComponent() const = 0;
};
