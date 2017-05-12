// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "EngineDefines.h"
#include "VisualLogger/VisualLogger.h"

// Intended categories:
//	Log - This happened. What DNA programmers may care about to debug
//	Verbose - This is why this happened. What you may turn on to debug the skill system code.
//  VeryVerbose - This is what didn't happen, and why. Extreme printf debugging
//


DNAABILITIES_API DECLARE_LOG_CATEGORY_EXTERN(LogDNAAbilitySystem, Display, All);
DNAABILITIES_API DECLARE_LOG_CATEGORY_EXTERN(VLogDNAAbilitySystem, Display, All);
DNAABILITIES_API DECLARE_LOG_CATEGORY_EXTERN(LogDNAEffects, Display, All);

#if NO_LOGGING || !PLATFORM_DESKTOP

// Without logging enabled we pass ability system through to UE_LOG which only handles Fatal verbosity in NO_LOGGING
#define ABILITY_LOG(Verbosity, Format, ...) \
{ \
	UE_LOG(LogDNAAbilitySystem, Verbosity, Format, ##__VA_ARGS__); \
}

#define ABILITY_VLOG(Actor, Verbosity, Format, ...) \
{ \
	UE_LOG(LogDNAAbilitySystem, Verbosity, Format, ##__VA_ARGS__); \
	UE_VLOG(Actor, VLogDNAAbilitySystem, Verbosity, Format, ##__VA_ARGS__); \
}

#else

#define ABILITY_LOG(Verbosity, Format, ...) \
{ \
	UE_LOG(LogDNAAbilitySystem, Verbosity, Format, ##__VA_ARGS__); \
}

#define ABILITY_VLOG(Actor, Verbosity, Format, ...) \
{ \
	UE_LOG(LogDNAAbilitySystem, Verbosity, Format, ##__VA_ARGS__); \
	UE_VLOG(Actor, VLogDNAAbilitySystem, Verbosity, Format, ##__VA_ARGS__); \
}

#endif //NO_LOGGING

#if ENABLE_VISUAL_LOG

#define ABILITY_VLOG_ATTRIBUTE_GRAPH(Actor, Verbosity, AttributeName, OldValue, NewValue) \
{ \
	if( FVisualLogger::IsRecording() ) \
	{ \
		static const FName GraphName("Attribute Graph"); \
		const float CurrentTime = Actor->GetWorld() ? Actor->GetWorld()->GetTimeSeconds() : 0.f; \
		const FVector2D OldPt(CurrentTime, OldValue); \
		const FVector2D NewPt(CurrentTime, NewValue); \
		const FName LineName(*AttributeName); \
		UE_VLOG_HISTOGRAM(Actor, VLogDNAAbilitySystem, Log, GraphName, LineName, OldPt); \
		UE_VLOG_HISTOGRAM(OwnerActor, VLogDNAAbilitySystem, Log, GraphName, LineName, NewPt); \
	} \
}

#else

#define ABILITY_VLOG_ATTRIBUTE_GRAPH(Actor, Verbosity, AttributeName, OldValue, NewValue)

#endif //ENABLE_VISUAL_LOG
