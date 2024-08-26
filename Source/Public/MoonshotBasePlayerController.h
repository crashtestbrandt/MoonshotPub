// Copyright 2024 Frazimuth, LLC.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MoonshotBasePlayerController.generated.h"

/**
 * A Player Controller class which adds input-handling functionality for
 * CharacterMovementController's custom gravity mechanics.
 */
UCLASS()
class AMoonshotBasePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	
	virtual void UpdateRotation(float DeltaTime) override;

	// Converts a rotation from world space to gravity relative space.
	UFUNCTION(BlueprintPure)
	static FRotator GetGravityRelativeRotation(FRotator Rotation, FVector GravityDirection);

	// Converts a rotation from gravity relative space to world space.
	UFUNCTION(BlueprintPure)
	static FRotator GetGravityWorldRotation(FRotator Rotation, FVector GravityDirection);

private:
	FVector LastFrameGravity = FVector::ZeroVector;
};