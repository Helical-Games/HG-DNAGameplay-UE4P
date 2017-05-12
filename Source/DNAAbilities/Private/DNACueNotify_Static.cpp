// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNACueNotify_Static.h"
#include "Engine/Blueprint.h"
#include "AbilitySystemStats.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "DNACueManager.h"

UDNACueNotify_Static::UDNACueNotify_Static(const FObjectInitializer& PCIP)
: Super(PCIP)
{
	IsOverride = true;
}

#if WITH_EDITOR
void UDNACueNotify_Static::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	UBlueprint* Blueprint = UBlueprint::GetBlueprintFromClass(GetClass());

	if (PropertyThatChanged && PropertyThatChanged->GetFName() == FName(TEXT("DNACueTag")))
	{
		DeriveDNACueTagFromAssetName();
		UDNAAbilitySystemGlobals::Get().GetDNACueManager()->HandleAssetDeleted(Blueprint);
		UDNAAbilitySystemGlobals::Get().GetDNACueManager()->HandleAssetAdded(Blueprint);
	}
}
#endif

void UDNACueNotify_Static::DeriveDNACueTagFromAssetName()
{
	UDNAAbilitySystemGlobals::DeriveDNACueTagFromClass<UDNACueNotify_Static>(this);
}

void UDNACueNotify_Static::Serialize(FArchive& Ar)
{
	if (Ar.IsSaving())
	{
		DeriveDNACueTagFromAssetName();
	}

	Super::Serialize(Ar);

	if (Ar.IsLoading())
	{
		DeriveDNACueTagFromAssetName();
	}
}

void UDNACueNotify_Static::PostInitProperties()
{
	Super::PostInitProperties();
	DeriveDNACueTagFromAssetName();
}

bool UDNACueNotify_Static::HandlesEvent(EDNACueEvent::Type EventType) const
{
	return true;
}

void UDNACueNotify_Static::HandleDNACue(AActor* MyTarget, EDNACueEvent::Type EventType, const FDNACueParameters& Parameters)
{
	SCOPE_CYCLE_COUNTER(STAT_HandleDNACueNotifyStatic);

	if (MyTarget && !MyTarget->IsPendingKill())
	{
		K2_HandleDNACue(MyTarget, EventType, Parameters);

		switch (EventType)
		{
		case EDNACueEvent::OnActive:
			OnActive(MyTarget, Parameters);
			break;

		case EDNACueEvent::WhileActive:
			WhileActive(MyTarget, Parameters);
			break;

		case EDNACueEvent::Executed:
			OnExecute(MyTarget, Parameters);
			break;

		case EDNACueEvent::Removed:
			OnRemove(MyTarget, Parameters);
			break;
		};
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("Null Target"));
	}
}

void UDNACueNotify_Static::OnOwnerDestroyed()
{
}

bool UDNACueNotify_Static::OnExecute_Implementation(AActor* MyTarget, const FDNACueParameters& Parameters) const
{
	return false;
}

bool UDNACueNotify_Static::OnActive_Implementation(AActor* MyTarget, const FDNACueParameters& Parameters) const
{
	return false;
}

bool UDNACueNotify_Static::WhileActive_Implementation(AActor* MyTarget, const FDNACueParameters& Parameters) const
{
	return false;
}

bool UDNACueNotify_Static::OnRemove_Implementation(AActor* MyTarget, const FDNACueParameters& Parameters) const
{
	return false;
}

UWorld* UDNACueNotify_Static::GetWorld() const
{
	static TWeakObjectPtr<UDNACueManager> CueManager = UDNAAbilitySystemGlobals::Get().GetDNACueManager();
	return CueManager.IsValid() ? CueManager->GetWorld() : nullptr;
}
