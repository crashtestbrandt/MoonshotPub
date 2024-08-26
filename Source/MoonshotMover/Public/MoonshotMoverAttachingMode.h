// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MoonshotMoverCommonMovementSettings.h"
#include "CoreMinimal.h"
#include "Mover/Public/MovementMode.h"
#include "Mover/Public/MoverDataModelTypes.h"
#include "MoonshotMoverAttachingMode.generated.h"

struct FProposedMove;
struct FFloorCheckResult;

// Fired after the actor lands on a valid surface. First param is the name of the mode this actor is in after landing. Second param is the hit result from hitting the floor.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMover_OnAttach, const FName&, NextMovementModeName, const FHitResult&, HitResult);

UCLASS(Blueprintable, BlueprintType)
class MOONSHOTMOVER_API UMoonshotMoverAttachingMode : public UBaseMovementMode
{
	GENERATED_UCLASS_BODY()


public:
	UFUNCTION(BlueprintCallable, Category=Mover)
	virtual void OnGenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;

	UFUNCTION(BlueprintCallable, Category=Mover)
	virtual void OnSimulationTick(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;

	// Broadcast when this actor lands on a valid surface.
	UPROPERTY(BlueprintAssignable, Category = Mover)
	FMover_OnAttach OnAttach;
	
	/**
	 * When falling, amount of movement control available to the actor.
	 * 0 = no control, 1 = full control
	 */
	UPROPERTY(Category=Mover, EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1.0"))
	float AirControlPercentage;
	
	/**
 	 * Deceleration to apply to air movement when falling slower than terminal velocity.
 	 * Note: This is NOT applied to vertical velocity, only movement plane velocity
 	 */
	UPROPERTY(Category=Mover, EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ForceUnits = "cm/s^2"))
	float FallingDeceleration;

	/**
     * Deceleration to apply to air movement when falling faster than terminal velocity
	 * Note: This is NOT applied to vertical velocity, only movement plane velocity
     */
    UPROPERTY(Category=Mover, EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ForceUnits = "cm/s^2"))
    float OverTerminalSpeedFallingDeceleration;
	
	/**
	 * If the actor's movement plane velocity is greater than this speed falling will start applying OverTerminalSpeedFallingDeceleration instead of FallingDeceleration
	 * The expected behavior is to set OverTerminalSpeedFallingDeceleration higher than FallingDeceleration so the actor will slow down faster
	 * when going over TerminalMovementPlaneSpeed.
	 */
	UPROPERTY(Category=Mover, EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ForceUnits = "cm/s"))
	float TerminalMovementPlaneSpeed;

	/** When exceeding maximum vertical speed, should it be enforced via a hard clamp? If false, VerticalFallingDeceleration will be used for a smoother transition to the terminal speed limit. */
	UPROPERTY(Category = Mover, EditAnywhere, BlueprintReadWrite)
	bool bShouldClampTerminalVerticalSpeed;

	/** Deceleration to apply to vertical velocity when it's greater than TerminalVerticalSpeed. Only used if bShouldClampTerminalVerticalSpeed is false. */
	UPROPERTY(Category=Mover, EditAnywhere, BlueprintReadWrite, meta=(EditCondition="!bShouldClampTerminalVerticalSpeed", ClampMin="0", ForceUnits = "cm/s^2"))
	float VerticalFallingDeceleration;
	
	/**
	 * If the actors vertical velocity is greater than this speed VerticalFallingDeceleration will be applied to vertical velocity
	 */
	UPROPERTY(Category=Mover, EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ForceUnits = "cm/s"))
	float TerminalVerticalSpeed;

protected:
	virtual void OnRegistered(const FName ModeName) override;
	virtual void OnUnregistered() override;

	virtual bool AttemptTeleport(USceneComponent* UpdatedComponent, const FVector& TeleportPos, const FRotator& TeleportRot, const FMoverDefaultSyncState& StartingSyncState, FMoverTickEndData& Output);

    UFUNCTION(BlueprintCallable, Category=Mover)
	virtual void ProcessLanded(const FFloorCheckResult& FloorResult, FVector& Velocity, FRelativeBaseInfo& BaseInfo, FMoverTickEndData& TickEndData) const;

	void CaptureFinalState(USceneComponent* UpdatedComponent, const FMoverDefaultSyncState& StartSyncState, const FFloorCheckResult& FloorResult, float DeltaSeconds, float DeltaSecondsUsed, FMoverDefaultSyncState& OutputSyncState, FMoverTickEndData& TickEndData, FMovementRecord& Record) const;

	TObjectPtr<const UMoonshotMoverCommonMovementSettings> CommonMovementSettings;
};

// Input parameters for controlled ZeroG movement function
USTRUCT(BlueprintType)
struct MOONSHOTMOVER_API FAttachingModeParams
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	EMoveInputType MoveInputType = EMoveInputType::DirectionalIntent;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	FRotator ControlRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	FVector MoveInput = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	FRotator OrientationIntent = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	FRotator AngularVelocity = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	FVector PriorVelocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	FRotator PriorOrientation = FRotator::ZeroRotator;

    UPROPERTY(BluePrintReadOnly, EditAnywhere, Category = Mover)
    FVector GravityAcceleration = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	float MaxSpeed = 6400.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	float Acceleration = 400.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	float Deceleration = 0.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	float TurningBoost = 8.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	float TurningRate = 500.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	float DeltaSeconds = 0.f;

    UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
    float RollRate = 120.0f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	float DeadZoneAngle = 10.0f;
};

/**
 * AttachingModeUtils: a collection of stateless static BP-accessible functions for a variety of falling movement-related operations
 */
UCLASS()
class MOONSHOTMOVER_API UAttachingModeUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
    static constexpr double SMALL_MOVE_DISTANCE = 1e-3;

	/** Generate a new movement based on move/orientation intents and the prior state, unconstrained like when flying */
//	UFUNCTION(BlueprintCallable, Category = Mover)
//	static FProposedMove ComputeControlledFreeMove(const FAttachingModeParams& InParams,  FTransform OwnerTransform, UWorld* World);
	
    // Checks if a hit result represents a walkable location that an actor can land on
    UFUNCTION(BlueprintCallable, Category=Mover)
    static bool IsValidLandingSpot(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, const FVector& Location, const FHitResult& Hit, float FloorSweepDistance, float MaxWalkSlopeCosine, FFloorCheckResult& OutFloorResult);
    
    /** Attempts to move a component along a surface, while checking for landing on a walkable surface. Intended for use while falling. Returns the percent of time applied, with 0.0 meaning no movement occurred. */
    UFUNCTION(BlueprintCallable, Category=Mover)
    static float TryMoveToFallAlongSurface(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, UMoverComponent* MoverComponent, const FVector& Delta, float PctOfDeltaToMove, const FQuat Rotation, const FVector& Normal, FHitResult& Hit, bool bHandleImpact, float FloorSweepDistance, float MaxWalkSlopeCosine, FFloorCheckResult& OutFloorResult, FMovementRecord& MoveRecord);

};