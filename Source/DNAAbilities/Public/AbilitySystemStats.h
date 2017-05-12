// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "Stats/Stats.h"

DECLARE_STATS_GROUP(TEXT("DNAAbilitySystem"), STATGROUP_DNAAbilitySystem, STATCAT_Advanced);

DECLARE_CYCLE_STAT_EXTERN(TEXT("DNAEffectsHasAllTagsTime"), STAT_DNAEffectsHasAllTags, STATGROUP_DNAAbilitySystem, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("DNAEffectsHasAnyTagTime"), STAT_DNAEffectsHasAnyTag, STATGROUP_DNAAbilitySystem, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("DNAEffectsGetOwnedTags"), STAT_DNAEffectsGetOwnedTags, STATGROUP_DNAAbilitySystem, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("DNAEffectsTick"), STAT_DNAEffectsTick, STATGROUP_DNAAbilitySystem, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("CanApplyAttributeModifiers"), STAT_DNAEffectsCanApplyAttributeModifiers, STATGROUP_DNAAbilitySystem, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("GetActiveEffectsData"), STAT_DNAEffectsGetActiveEffectsTimeRemaining, STATGROUP_DNAAbilitySystem, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("GetActiveEffectsData"), STAT_DNAEffectsGetActiveEffectsDuration, STATGROUP_DNAAbilitySystem, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("GetActiveEffectsData"), STAT_DNAEffectsGetActiveEffectsTimeRemainingAndDuration, STATGROUP_DNAAbilitySystem, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("GetActiveEffectsData"), STAT_DNAEffectsGetActiveEffects, STATGROUP_DNAAbilitySystem, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("GetActiveEffectsData"), STAT_DNAEffectsGetAllActiveEffectHandles, STATGROUP_DNAAbilitySystem, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("SetActiveEffectsData"), STAT_DNAEffectsModifyActiveEffectStartTime, STATGROUP_DNAAbilitySystem, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("GetCooldownTimeRemaining"), STAT_DNAAbilityGetCooldownTimeRemaining, STATGROUP_DNAAbilitySystem, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("GetCooldownTimeRemaining"), STAT_DNAAbilityGetCooldownTimeRemainingAndDuration, STATGROUP_DNAAbilitySystem, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("RemoveActiveDNAEffect"), STAT_RemoveActiveDNAEffect, STATGROUP_DNAAbilitySystem, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("ApplyDNAEffectSpec"), STAT_ApplyDNAEffectSpec, STATGROUP_DNAAbilitySystem, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("GetDNACueFunction"), STAT_GetDNACueFunction, STATGROUP_DNAAbilitySystem, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("GetOutgoingSpec"), STAT_GetOutgoingSpec, STATGROUP_DNAAbilitySystem, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("InitAttributeSetDefaults"), STAT_InitAttributeSetDefaults, STATGROUP_DNAAbilitySystem, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("TickDNAAbilityTasks"), STAT_TickDNAAbilityTasks, STATGROUP_DNAAbilitySystem, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("FindAbilitySpecFromHandle"), STAT_FindAbilitySpecFromHandle, STATGROUP_DNAAbilitySystem, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Aggregator Evaluate"), STAT_AggregatorEvaluate, STATGROUP_DNAAbilitySystem, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Has Application Immunity To Spec"), STAT_HasApplicationImmunityToSpec, STATGROUP_DNAAbilitySystem, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Has Matching DNATag"), STAT_HasMatchingDNATag, STATGROUP_DNAAbilitySystem, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("DNACueNotify Static"), STAT_HandleDNACueNotifyStatic, STATGROUP_DNAAbilitySystem, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("DNACueNotify Actor"), STAT_HandleDNACueNotifyActor, STATGROUP_DNAAbilitySystem, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("ApplyDNAEffectToTarget"), STAT_ApplyDNAEffectToTarget, STATGROUP_DNAAbilitySystem, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("ActiveDNAEffect Added"), STAT_OnActiveDNAEffectAdded, STATGROUP_DNAAbilitySystem, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("ActiveDNAEffect Removed"), STAT_OnActiveDNAEffectRemoved, STATGROUP_DNAAbilitySystem, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("DNACueInterface HandleDNACue"), STAT_DNACueInterface_HandleDNACue, STATGROUP_DNAAbilitySystem, );
