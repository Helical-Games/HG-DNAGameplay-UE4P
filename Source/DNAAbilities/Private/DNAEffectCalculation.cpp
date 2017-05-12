// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "DNAEffectCalculation.h"

UDNAEffectCalculation::UDNAEffectCalculation(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

const TArray<FDNAEffectAttributeCaptureDefinition>& UDNAEffectCalculation::GetAttributeCaptureDefinitions() const
{
	return RelevantAttributesToCapture;
}
