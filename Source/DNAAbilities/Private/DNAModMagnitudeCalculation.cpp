// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNAModMagnitudeCalculation.h"

UDNAModMagnitudeCalculation::UDNAModMagnitudeCalculation(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bAllowNonNetAuthorityDependencyRegistration(false)
{
}

float UDNAModMagnitudeCalculation::CalculateBaseMagnitude_Implementation(const FDNAEffectSpec& Spec) const
{
	return 0.f;
}

FOnExternalDNAModifierDependencyChange* UDNAModMagnitudeCalculation::GetExternalModifierDependencyMulticast(const FDNAEffectSpec& Spec, UWorld* World) const
{
	return nullptr;
}

bool UDNAModMagnitudeCalculation::ShouldAllowNonNetAuthorityDependencyRegistration() const
{
	ensureMsgf(!bAllowNonNetAuthorityDependencyRegistration || RelevantAttributesToCapture.Num() == 0, TEXT("Cannot have a client-side based custom mod calculation that relies on attribute capture!"));
	return bAllowNonNetAuthorityDependencyRegistration;
}

bool UDNAModMagnitudeCalculation::GetCapturedAttributeMagnitude(const FDNAEffectAttributeCaptureDefinition& Def, const FDNAEffectSpec& Spec, const FAggregatorEvaluateParameters& EvaluationParameters, OUT float& Magnitude) const
{
	const FDNAEffectAttributeCaptureSpec* CaptureSpec = Spec.CapturedRelevantAttributes.FindCaptureSpecByDefinition(Def, true);
	if (CaptureSpec == nullptr)
	{
		ABILITY_LOG(Error, TEXT("GetCapturedAttributeMagnitude unable to get capture spec."));
		return false;
	}
	if (CaptureSpec->AttemptCalculateAttributeMagnitude(EvaluationParameters, Magnitude) == false)
	{
		ABILITY_LOG(Error, TEXT("GetCapturedAttributeMagnitude unable to calculate Health attribute magnitude."));
		return false;
	}

	return true;
}
