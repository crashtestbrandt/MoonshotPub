// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MoonshotMoverCommonMovementSettings.h"
#include "CoreMinimal.h"
#include "Mover/Public/DefaultMovementSet/LayeredMoves/BasicLayeredMoves.h"
#include "Mover/Public/MovementMode.h"
#include "Mover/Public/MoverDataModelTypes.h"
#include "MoonshotMoverSurfaceWalkingMode.generated.h"

struct FProposedMove;
struct FFloorCheckResult;
struct FRelativeBaseInfo;
struct FMovementRecord;
struct FOptionalFloorCheckResult;

UCLASS(Blueprintable, BlueprintType)
class MOONSHOTMOVER_API UMoonshotMoverSurfaceWalkingMode : public UBaseMovementMode
{
	GENERATED_UCLASS_BODY()


public:
	UFUNCTION(BlueprintCallable, Category=Mover)
	virtual void OnGenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;

	UFUNCTION(BlueprintCallable, Category=Mover)
	virtual void OnSimulationTick(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;
	
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

    virtual bool AttemptJump(float JumpSpeed, FMoverTickEndData& OutputState);
	virtual bool AttemptTeleport(USceneComponent* UpdatedComponent, const FVector& TeleportPos, const FRotator& TeleportRot, const FVector& PriorVelocity, FMoverTickEndData& Output);

	void CaptureFinalState(USceneComponent* UpdatedComponent, bool bDidAttemptMovement, const FFloorCheckResult& FloorResult, const FMovementRecord& Record, FMoverDefaultSyncState& OutputSyncState) const;

    FRelativeBaseInfo UpdateFloorAndBaseInfo(const FFloorCheckResult& FloorResult) const;

	TObjectPtr<const UMoonshotMoverCommonMovementSettings> CommonMovementSettings;
};

// Input parameters for controlled ZeroG movement function
USTRUCT(BlueprintType)
struct MOONSHOTMOVER_API FSurfaceWalkingModeParams
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

    UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
    float Friction = 0.0f;
};

/**
 * SurfaceWalkingModeUtils: a collection of stateless static BP-accessible functions for a variety of ZeroG movement-related operations
 */
UCLASS()
class MOONSHOTMOVER_API USurfaceWalkingModeUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
    static constexpr double SMALL_MOVE_DISTANCE = 1e-3;

    /** Used to change a movement to be along a ramp's surface, typically to prevent slow movement when running up/down a ramp */
    static FVector ComputeDeflectedMoveOntoRamp(const FVector& OrigMoveDelta, const FHitResult& RampHitResult, float MaxWalkSlopeCosine, const bool bHitFromLineTrace, USceneComponent* UpdatedComponent);

    UFUNCTION(BlueprintCallable, Category = Mover)
	static bool CanStepUpOnHitSurface(const FHitResult& Hit);

    // TODO: Refactor this API for fewer parameters
	/** Move up steps or slope. Does nothing and returns false if hit surface is invalid for step-up use */
	static bool TryMoveToStepUp(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, UMoverComponent* MoverComponent, const FVector& GravDir, float MaxStepHeight, float MaxWalkSlopeCosine, float FloorSweepDistance, const FVector& MoveDelta, const FHitResult& Hit, const FFloorCheckResult& CurrentFloor, bool bIsFalling, FOptionalFloorCheckResult* OutStepDownResult, FMovementRecord& MoveRecord);

    /** Attempts to move a component along a surface in the walking mode. Returns the percent of time applied, with 0.0 meaning no movement occurred.
     *  Note: This modifies the normal and calls UMovementUtils::TryMoveToSlideAlongSurface
     */
    UFUNCTION(BlueprintCallable, Category=Mover)
    static float TryWalkToSlideAlongSurface(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, UMoverComponent* MoverComponent, const FVector& Delta, float PctOfDeltaToMove, const FQuat Rotation, const FVector& Normal, FHitResult& Hit, bool bHandleImpact, FMovementRecord& MoveRecord, float MaxWalkSlopeCosine, float MaxStepHeight);

    /** Moves vertically to stay within range of the walkable floor. Does nothing and returns false if floor is unwalkable or if already in range. */ 
	static bool TryMoveToAdjustHeightAboveFloor(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, FFloorCheckResult& CurrentFloor, float MaxWalkSlopeCosine, FMovementRecord& MoveRecord);
};

/** Jump Impulse: introduces an instantaneous upwards change in velocity. This overrides the existing 'up' component of the actor's current velocity */
USTRUCT(BlueprintType)
struct MOONSHOTMOVER_API FLayeredMove_SurfaceWalkingModeJumpImpulse : public FLayeredMoveBase
{
	GENERATED_USTRUCT_BODY()

    FLayeredMove_SurfaceWalkingModeJumpImpulse();

	virtual ~FLayeredMove_SurfaceWalkingModeJumpImpulse() {}

	// Units per second, in whatever direction the target actor considers 'up'
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	float UpwardsSpeed;

	// Generate a movement 
	virtual bool GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;

	virtual FLayeredMoveBase* Clone() const override;

	virtual void NetSerialize(FArchive& Ar) override;

	virtual UScriptStruct* GetScriptStruct() const override;

	virtual FString ToSimpleString() const override;

	virtual void AddReferencedObjects(class FReferenceCollector& Collector) override;
};

template<>
struct TStructOpsTypeTraits< FLayeredMove_SurfaceWalkingModeJumpImpulse > : public TStructOpsTypeTraitsBase2< FLayeredMove_SurfaceWalkingModeJumpImpulse >
{
	enum
	{
		//WithNetSerializer = true,
		WithCopy = true
	};
};