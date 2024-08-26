// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MoonshotMoverCommonMovementSettings.h"
#include "CoreMinimal.h"
#include "Mover/Public/MovementMode.h"
#include "Mover/Public/MoverDataModelTypes.h"
#include "MoonshotMoverZeroGMode.generated.h"

struct FProposedMove;

UCLASS(Blueprintable, BlueprintType)
class MOONSHOTMOVER_API UMoonshotMoverZeroGMode : public UBaseMovementMode
{
	GENERATED_UCLASS_BODY()


public:
	UFUNCTION(BlueprintCallable, Category=Mover)
	virtual void OnGenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;

	UFUNCTION(BlueprintCallable, Category=Mover)
	virtual void OnSimulationTick(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;

protected:
	virtual void OnRegistered(const FName ModeName) override;
	virtual void OnUnregistered() override;

	virtual bool AttemptTeleport(USceneComponent* UpdatedComponent, const FVector& TeleportPos, const FRotator& TeleportRot, const FMoverDefaultSyncState& StartingSyncState, FMoverTickEndData& Output);

	void CaptureFinalState(USceneComponent* UpdatedComponent, FMovementRecord& Record, const FMoverDefaultSyncState& StartSyncState, FMoverDefaultSyncState& OutputSyncState, const float DeltaSeconds) const;

	TObjectPtr<const UMoonshotMoverCommonMovementSettings> CommonMovementSettings;
};

UCLASS(BlueprintType)
class MOONSHOTMOVER_API UZeroGModeSettings : public UObject, public IMovementSettingsInterface
{
	GENERATED_BODY()

	virtual FString GetDisplayName() const override { return GetName(); }

public:
	UPROPERTY(Category="General", EditAnywhere, BlueprintReadWrite)
	FName ZeroGMovementModeName = TEXT("ZeroG");

	/** Walkable slope angle, represented as cosine(max slope angle) for performance reasons. Ex: for max slope angle of 30 degrees, value is cosine(30 deg) = 0.866 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Ground Movement")
	float MaxWalkSlopeCosine = 0.71f;

	/** Max distance to scan for floor surfaces under a Mover actor */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Ground Movement", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm"))
	float FloorSweepDistance = 40.0f;

	/** Mover actors will be able to step up onto or over obstacles shorter than this */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Ground Movement", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm"))
	float MaxStepHeight = 40.0f;

	/** Maximum speed in the movement plane */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="General", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s"))
	float ZeroGMaxSpeed = 6400.f;

	/**
	 * Setting that affects movement control. Higher values allow faster changes in direction. This can be used to simulate slippery
	 * surfaces such as ice or oil by lowering the value (possibly based on the material the actor is standing on).
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="General|Friction", meta = (ClampMin = "0", UIMin = "0"))
	float GroundFriction = 8.0f;

	/**
	  * If true, BrakingFriction will be used to slow the character to a stop (when there is no Acceleration).
	  * If false, braking uses the same friction passed to CalcVelocity() (ie GroundFriction when walking), multiplied by BrakingFrictionFactor.
	  * This setting applies to all movement modes; if only desired in certain modes, consider toggling it when movement modes change.
	  * @see BrakingFriction
	  */
	UPROPERTY(Category="General|Friction", EditDefaultsOnly, BlueprintReadWrite)
	uint8 bUseSeparateBrakingFriction:1;

	/**
	 * Friction (drag) coefficient applied when braking (whenever Acceleration = 0, or if character is exceeding max speed); actual value used is this multiplied by BrakingFrictionFactor.
	 * When braking, this property allows you to control how much friction is applied when moving across the ground, applying an opposing force that scales with current velocity.
	 * Braking is composed of friction (velocity-dependent drag) and constant deceleration.
	 * This is the current value, used in all movement modes; if this is not desired, override it or bUseSeparateBrakingFriction when movement mode changes.
	 * @note Only used if bUseSeparateBrakingFriction setting is true, otherwise current friction such as GroundFriction is used.
	 * @see bUseSeparateBrakingFriction, BrakingFrictionFactor, GroundFriction, BrakingDecelerationWalking
	 */
	UPROPERTY(Category="General|Friction", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", EditCondition="bUseSeparateBrakingFriction"))
	float BrakingFriction = 8.0f;

	/**
	 * Factor used to multiply actual value of friction used when braking.
	 * This applies to any friction value that is currently used, which may depend on bUseSeparateBrakingFriction.
	 * @note This is 2 by default for historical reasons, a value of 1 gives the true drag equation.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="General|Friction", meta=(ClampMin="0", UIMin="0"))
	float BrakingFrictionFactor = 2.0f;
	
	/** Default max linear rate of deceleration when there is no controlled input */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="General", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s^2"))
	float ZeroGDeceleration = 0.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="General", meta = (ClampMin = "0", UIMin = "0"))
	float LinearBrakingScale = 0.05f;


	/** Default max linear rate of acceleration for controlled input. May be scaled based on magnitude of input. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="General", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s^2"))
	float ZeroGLinearAcceleration = 800.f;

	/** Maximum rate of turning rotation (degrees per second). Negative numbers indicate instant rotation and should cause rotation to snap instantly to desired direction. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="General", meta = (ClampMin = "-1", UIMin = "0", ForceUnits = "degrees/s"))
	float ZeroGTurningRate = 500.f;

	/** Speeds velocity direction changes while turning, to reduce sliding */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="General", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "Multiplier"))
	float TurningBoost = 8.f;

 	/** Whether the actor ignores changes in rotation of the base it is standing on when using based movement.
  	 * If true, the actor maintains its current world rotation.
  	 * If false, the actor rotates with the moving base.
  	 */
 	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General")
	bool bIgnoreBaseRotation = false;

	/** Instantaneous speed induced in an actor upon jumping */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Jumping", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s"))
	float JumpUpwardsSpeed = 500.0f;
	
	/** Depth at which the pawn starts swimming */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Swimming", meta = (Units = "cm"))
	float SwimmingStartImmersionDepth = 64.5f;

	/** Depth at which the pawn will float when in water */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Swimming", meta = (Units = "cm"))
	float SwimmingIdealImmersionDepth = 51.66f;

	/** Depth at which the pawn stops swimming */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Swimming", meta = (Units = "cm"))
	float SwimmingStopImmersionDepth = 39.9f;
	
};

// Input parameters for controlled ZeroG movement function
USTRUCT(BlueprintType)
struct MOONSHOTMOVER_API FZeroGModeParams
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
 * ZeroGModeUtils: a collection of stateless static BP-accessible functions for a variety of ZeroG movement-related operations
 */
UCLASS()
class MOONSHOTMOVER_API UZeroGModeUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
    static constexpr double SMALL_MOVE_DISTANCE = 1e-3;

	/** Generate a new movement based on move/orientation intents and the prior state, unconstrained like when flying */
	UFUNCTION(BlueprintCallable, Category = Mover)
	static FProposedMove ComputeControlledFreeMove(const FZeroGModeParams& InParams,  FTransform OwnerTransform, UWorld* World);
	
    // Checks if a hit result represents a walkable location that an actor can land on
    UFUNCTION(BlueprintCallable, Category=Mover)
    static bool IsValidLandingSpot(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, const FVector& Location, const FHitResult& Hit, float FloorSweepDistance, float MaxWalkSlopeCosine, FFloorCheckResult& OutFloorResult);
    
    /** Attempts to move a component along a surface, while checking for landing on a walkable surface. Intended for use while falling. Returns the percent of time applied, with 0.0 meaning no movement occurred. */
    UFUNCTION(BlueprintCallable, Category=Mover)
    static float TryMoveToFallAlongSurface(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, UMoverComponent* MoverComponent, const FVector& Delta, float PctOfDeltaToMove, const FQuat Rotation, const FVector& Normal, FHitResult& Hit, bool bHandleImpact, float FloorSweepDistance, float MaxWalkSlopeCosine, FFloorCheckResult& OutFloorResult, FMovementRecord& MoveRecord);

};