// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Abilities/Tasks/AbilityTask_MoveToLocation.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Curves/CurveVector.h"
#include "Net/UnrealNetwork.h"

UDNAAbilityTask_MoveToLocation::UDNAAbilityTask_MoveToLocation(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bTickingTask = true;
	bSimulatedTask = true;
	bIsFinished = false;
}

UDNAAbilityTask_MoveToLocation* UDNAAbilityTask_MoveToLocation::MoveToLocation(UDNAAbility* OwningAbility, FName TaskInstanceName, FVector Location, float Duration, UCurveFloat* OptionalInterpolationCurve, UCurveVector* OptionalVectorInterpolationCurve)
{
	auto MyObj = NewDNAAbilityTask<UDNAAbilityTask_MoveToLocation>(OwningAbility, TaskInstanceName);

	if (MyObj->GetAvatarActor() != nullptr)
	{
		MyObj->StartLocation = MyObj->GetAvatarActor()->GetActorLocation();
	}

	MyObj->TargetLocation = Location;
	MyObj->DurationOfMovement = FMath::Max(Duration, 0.001f);		// Avoid negative or divide-by-zero cases
	MyObj->TimeMoveStarted = MyObj->GetWorld()->GetTimeSeconds();
	MyObj->TimeMoveWillEnd = MyObj->TimeMoveStarted + MyObj->DurationOfMovement;
	MyObj->LerpCurve = OptionalInterpolationCurve;
	MyObj->LerpCurveVector = OptionalVectorInterpolationCurve;

	return MyObj;
}

void UDNAAbilityTask_MoveToLocation::Activate()
{

}

void UDNAAbilityTask_MoveToLocation::InitSimulatedTask(UDNATasksComponent& InDNATasksComponent)
{
	Super::InitSimulatedTask(InDNATasksComponent);

	TimeMoveStarted = GetWorld()->GetTimeSeconds();
	TimeMoveWillEnd = TimeMoveStarted + DurationOfMovement;
}

//TODO: This is still an awful way to do this and we should scrap this task or do it right.
void UDNAAbilityTask_MoveToLocation::TickTask(float DeltaTime)
{
	if (bIsFinished)
	{
		return;
	}

	Super::TickTask(DeltaTime);
	AActor* MyActor = GetAvatarActor();
	if (MyActor)
	{
		ACharacter* MyCharacter = Cast<ACharacter>(MyActor);
		if (MyCharacter)
		{
			UCharacterMovementComponent* CharMoveComp = Cast<UCharacterMovementComponent>(MyCharacter->GetMovementComponent());
			if (CharMoveComp)
			{
				CharMoveComp->SetMovementMode(MOVE_Custom, 0);
			}
		}


		float CurrentTime = GetWorld()->GetTimeSeconds();

		if (CurrentTime >= TimeMoveWillEnd)
		{
			bIsFinished = true;

			// Teleport in attempt to find a valid collision spot
			MyActor->TeleportTo(TargetLocation, MyActor->GetActorRotation());
			if (!bIsSimulating)
			{
				MyActor->ForceNetUpdate();
				OnTargetLocationReached.Broadcast();
				EndTask();
			}
		}
		else
		{
			FVector NewLocation;

			float MoveFraction = (CurrentTime - TimeMoveStarted) / DurationOfMovement;
			if (LerpCurveVector)
			{
				const FVector ComponentInterpolationFraction = LerpCurveVector->GetVectorValue(MoveFraction);
				NewLocation = FMath::Lerp<FVector, FVector>(StartLocation, TargetLocation, ComponentInterpolationFraction);
			}
			else
			{
				if (LerpCurve)
				{
					MoveFraction = LerpCurve->GetFloatValue(MoveFraction);
				}

				NewLocation = FMath::Lerp<FVector, float>(StartLocation, TargetLocation, MoveFraction);
			}

			MyActor->SetActorLocation(NewLocation);
		}
	}
	else
	{
		bIsFinished = true;
		EndTask();
	}
}

void UDNAAbilityTask_MoveToLocation::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	DOREPLIFETIME(UDNAAbilityTask_MoveToLocation, StartLocation);
	DOREPLIFETIME(UDNAAbilityTask_MoveToLocation, TargetLocation);
	DOREPLIFETIME(UDNAAbilityTask_MoveToLocation, DurationOfMovement);
	DOREPLIFETIME(UDNAAbilityTask_MoveToLocation, LerpCurve);
	DOREPLIFETIME(UDNAAbilityTask_MoveToLocation, LerpCurveVector);
}

void UDNAAbilityTask_MoveToLocation::OnDestroy(bool AbilityIsEnding)
{
	AActor* MyActor = GetAvatarActor();
	if (MyActor)
	{
		ACharacter* MyCharacter = Cast<ACharacter>(MyActor);
		if (MyCharacter)
		{
			UCharacterMovementComponent* CharMoveComp = Cast<UCharacterMovementComponent>(MyCharacter->GetMovementComponent());
			if (CharMoveComp && CharMoveComp->MovementMode == MOVE_Custom)
			{
				CharMoveComp->SetMovementMode(MOVE_Falling);
			}
		}
	}

	Super::OnDestroy(AbilityIsEnding);
}
