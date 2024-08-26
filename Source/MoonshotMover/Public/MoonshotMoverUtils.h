// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Components/PrimitiveComponent.h"
#include "Mover/Public/MoveLibrary/FloorQueryUtils.h"
#include "MoonshotMoverUtils.generated.h"

UCLASS()
class MOONSHOTMOVER_API UMoonshotMoverUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static void FindFloor(const USceneComponent* UpdatedComponent, const UPrimitiveComponent* UpdatedPrimitive, float FloorSweepDistance, float MaxWalkSlopeCosine, const FVector& Location, FFloorCheckResult& OutFloorResult);

	static void ComputeFloorDist(const USceneComponent* UpdatedComponent, const UPrimitiveComponent* UpdatedPrimitive, float LineTraceDistance, float FloorSweepDistance, float MaxWalkSlopeCosine, const FVector& Location, FFloorCheckResult& OutFloorResult);

	static bool FloorSweepTest(const UPrimitiveComponent* UpdatedPrimitive, FHitResult& OutHit, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const struct FCollisionShape& CollisionShape, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParam);

    UFUNCTION(BlueprintCallable, Category=Mover)
	static bool IsHitSurfaceWalkable(const FHitResult& Hit, float MaxWalkSlopeCosine, const USceneComponent* UpdatedComponent);

	/**
	 * Return true if the 2D distance to the impact point is inside the edge tolerance (CapsuleRadius minus a small rejection threshold).
	 * Useful for rejecting adjacent hits when finding a floor or landing spot.
	 */
	static bool IsWithinEdgeTolerance(const FVector& CapsuleLocation, const FVector& TestImpactPoint, float CapsuleRadius);

    static constexpr float MIN_FLOOR_DIST = 1.9f;
	static constexpr float MAX_FLOOR_DIST = 2.4f;
	static constexpr float SWEEP_EDGE_REJECT_DISTANCE = 0.15f;
};
