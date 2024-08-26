// Fill out your copyright notice in the Description page of Project Settings.

#include "MoonshotMoverUtils.h"
#include "Components/CapsuleComponent.h"
#include "Mover/Public/MoveLibrary/FloorQueryUtils.h"
#include "Mover/Public/MoveLibrary/MovementUtils.h"
#include "Mover/Public/DefaultMovementSet/CharacterMoverComponent.h"
#include "Engine/World.h"


bool UMoonshotMoverUtils::IsHitSurfaceWalkable(const FHitResult& Hit, float MaxWalkSlopeCosine, const USceneComponent* UpdatedComponent)
{
	if (!Hit.IsValidBlockingHit())
	{
		// No hit, or starting in penetration
		//UE_LOG(LogTemp, Display, TEXT("MoonshotMoverUtils: IsHitSurfaceWalkable() failed because: bBlockingHit=%s, bStartPenetrating=%s"),
		//	Hit.bBlockingHit ? TEXT("true") : TEXT("false"),
		//	Hit.bStartPenetrating ? TEXT("true") : TEXT("false")
		//	);
		return false;
	}

	// Never walk up vertical surfaces.
//	if (Hit.ImpactNormal.Z < KINDA_SMALL_NUMBER)
//	{
//		return false;
//	}

	float TestWalkableZ = MaxWalkSlopeCosine;

	// See if this component overrides the walkable floor z.
	const UPrimitiveComponent* HitComponent = Hit.Component.Get();
	if (HitComponent)
	{
		const FWalkableSlopeOverride& SlopeOverride = HitComponent->GetWalkableSlopeOverride();
		TestWalkableZ = SlopeOverride.ModifyWalkableFloorZ(TestWalkableZ);
	}

    FVector ComponentUp = UpdatedComponent->GetUpVector();
	// Can't walk on this surface if it is too steep.
	//if (Hit.ImpactNormal.Z < TestWalkableZ)
    if (FVector::DotProduct(Hit.ImpactNormal, ComponentUp) < TestWalkableZ)
	{   
        //(LogTemp, Display, TEXT("MoonshotMoverUtils: IsHitSurfaceWalkable() failed cosine test (bBlockingHit=%s, bStartPenetrating=%s)"), Hit.bBlockingHit ? TEXT("true") : TEXT("false"), Hit.bStartPenetrating ? TEXT("true") : TEXT("false"));
		return false;
	}

	return true;
}

void UMoonshotMoverUtils::ComputeFloorDist(const USceneComponent* UpdatedComponent, const UPrimitiveComponent* UpdatedPrimitive, float LineTraceDistance, float FloorSweepDistance, float MaxWalkSlopeCosine, const FVector& Location, FFloorCheckResult& OutFloorResult)
{
	OutFloorResult.Clear();

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ComputeFloorDist), false, UpdatedPrimitive->GetOwner());
	FCollisionResponseParams ResponseParam;
	UMovementUtils::InitCollisionParams(UpdatedPrimitive, QueryParams, ResponseParam);
	const ECollisionChannel CollisionChannel = UpdatedComponent->GetCollisionObjectType();

	// TODO: pluggable shapes
	float PawnRadius = 0.0f;
	float PawnHalfHeight = 0.0f;
	UpdatedPrimitive->CalcBoundingCylinder(PawnRadius, PawnHalfHeight);

	bool bBlockingHit = false;
	
	// Sweep test
	if (FloorSweepDistance > 0.f)
	{
		// Use a shorter height to avoid sweeps giving weird results if we start on a surface.
		// This also allows us to adjust out of penetrations.
		const float ShrinkScale = 0.9f;
		const float ShrinkScaleOverlap = 0.1f;
		float ShrinkHeight = (PawnHalfHeight - PawnRadius) * (1.f - ShrinkScale);
		float TraceDist = FloorSweepDistance + ShrinkHeight;
		FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(PawnRadius, PawnHalfHeight - ShrinkHeight); 

		/// TODO: Get rid of these casts
		const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(UpdatedPrimitive);
		if (Capsule)
		{
			PawnHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
			PawnRadius = Capsule->GetScaledCapsuleRadius();
			CapsuleShape = FCollisionShape::MakeCapsule(PawnRadius, PawnHalfHeight - ShrinkHeight);

		}
		/// ^ Debug Stuff

		FHitResult Hit(1.f);
		// TODO: arbitrary direction
		//FVector SweepDirection = FVector(0.f, 0.f, -TraceDist);
        FVector SweepDirection = UpdatedComponent->GetUpVector() * -TraceDist;

		bBlockingHit = UMoonshotMoverUtils::FloorSweepTest(UpdatedPrimitive, Hit, Location, Location + SweepDirection, CollisionChannel, CapsuleShape, QueryParams, ResponseParam);
		//UE_LOG(LogTemp, Display, TEXT("MoonshotMoverUtils: ComputeFloorDist(): Called first FloorSweepTest(Location=%s, SweepDirection=%s) and got (bBlockingHit=%s, HitResult.bStartPenetrating=%s)"), *Location.ToString(), *SweepDirection.ToString(), bBlockingHit ? TEXT("true") : TEXT("false"), Hit.bStartPenetrating ? TEXT("true") : TEXT("false"));
		if (bBlockingHit)
		{
			//UE_LOG(LogTemp, Display, TEXT("MoonshotMoverUtils: Got a blocking hit ..."));
			// Reject hits adjacent to us, we only care about hits on the bottom portion of our capsule.
			// Check 2D distance to impact point, reject if within a tolerance from radius.
			if (Hit.bStartPenetrating || !UMoonshotMoverUtils::IsWithinEdgeTolerance(Location, Hit.ImpactPoint, CapsuleShape.Capsule.Radius))
			{
				//UE_LOG(LogTemp, Warning, TEXT("MoonshotMoverUtils: ComputeFloorDist(): Penetrating HitResult: %s"), *Hit.ToString());
				//DrawDebugCapsule(UpdatedPrimitive->GetWorld(), Location + SweepDirection, PawnHalfHeight, PawnRadius, UpdatedPrimitive->GetComponentQuat(), FColor::Green, false, 0.5f);
				//DrawDebugPoint(UpdatedPrimitive->GetWorld(), Hit.ImpactPoint, 10.f, FColor::Red, false, 0.5f);
				// Use a capsule with a slightly smaller radius and shorter height to avoid the adjacent object.
				// Capsule must not be nearly zero or the trace will fall back to a line trace from the start point and have the wrong length.
				CapsuleShape.Capsule.Radius = FMath::Max(0.f, CapsuleShape.Capsule.Radius - SWEEP_EDGE_REJECT_DISTANCE - KINDA_SMALL_NUMBER);
				if (!CapsuleShape.IsNearlyZero())
				{
					//UE_LOG(LogTemp, Display, TEXT("AntidacneFloorQueryUtils: CapsuleShape is not nearly zero, setting CapsuleShape.Capsule.Radius to %f"), CapsuleShape.Capsule.Radius);
					ShrinkHeight = (PawnHalfHeight - PawnRadius) * (1.f - ShrinkScaleOverlap);
					TraceDist = FloorSweepDistance + ShrinkHeight;
					//SweepDirection = FVector(0.f, 0.f, -TraceDist);
                    SweepDirection = UpdatedComponent->GetUpVector() * -TraceDist;
					CapsuleShape.Capsule.HalfHeight = FMath::Max(PawnHalfHeight - ShrinkHeight, CapsuleShape.Capsule.Radius);
					Hit.Reset(1.f, false);

					bBlockingHit = UMoonshotMoverUtils::FloorSweepTest(UpdatedPrimitive, Hit, Location, Location + SweepDirection, CollisionChannel, CapsuleShape, QueryParams, ResponseParam);
					//UE_LOG(LogTemp, Display, TEXT("MoonshotMoverUtils: ComputeFloorDist(): Called second FloorSweepTest(Location=%s, SweepDirection=%s) and got (bBlockingHit=%s, HitResult.bStartPenetrating=%s)"), *Location.ToString(), *SweepDirection.ToString(), bBlockingHit ? TEXT("true") : TEXT("false"), Hit.bStartPenetrating ? TEXT("true") : TEXT("false"));
				}
			}

			// Reduce hit distance by ShrinkHeight because we shrank the capsule for the trace.
			// We allow negative distances here, because this allows us to pull out of penetrations.
			// JAH TODO: move magic numbers to a common location
			const float MaxPenetrationAdjust = FMath::Max(MAX_FLOOR_DIST, PawnRadius);
			const float SweepResult = FMath::Max(-MaxPenetrationAdjust, Hit.Time * TraceDist - ShrinkHeight);

			OutFloorResult.SetFromSweep(Hit, SweepResult, false);
			//UE_LOG(LogTemp, Display, TEXT("MoonshotMoverUtils: bBlockingHit=%s; bStartPenetrating=%s, IsValidBlockingHit()=%s, IsHitSurfaceWalkable()=%s"),
			//	bBlockingHit ? TEXT("true") : TEXT("false"),
			//	Hit.bStartPenetrating ? TEXT("true") : TEXT("false"),
			//	Hit.IsValidBlockingHit() ? TEXT("true") : TEXT("false"),
			//	UMoonshotMoverUtils::IsHitSurfaceWalkable(Hit, MaxWalkSlopeCosine, UpdatedComponent) ? TEXT("true") : TEXT("false")
			//	);
			if (Hit.IsValidBlockingHit() && UMoonshotMoverUtils::IsHitSurfaceWalkable(Hit, MaxWalkSlopeCosine, UpdatedComponent))
            {
				if (SweepResult <= FloorSweepDistance)
				{
					//UE_LOG(LogTemp, Display, TEXT("MoonshotMoverUtils: Setting bWalkableFloor to TRUE with SweepResult=%f <= FloorSweepDistance:%f"), SweepResult, FloorSweepDistance);
					OutFloorResult.bWalkableFloor = true;
					return;
				}
                //UE_LOG(LogTemp, Display, TEXT("MoonshotMoverUtils: bWalkable floor remains %s with SweepResult=%f > FloorSweepDistance:%f"),
                //    OutFloorResult.bWalkableFloor ? TEXT("true") : TEXT("false"),
                //    SweepResult, FloorSweepDistance
                //    );
            }
			
		}
	}

	// Since we require a longer sweep than line trace, we don't want to run the line trace if the sweep missed everything.
	// We do however want to try a line trace if the sweep was stuck in penetration.
	if (!OutFloorResult.bBlockingHit && !OutFloorResult.HitResult.bStartPenetrating)
	{
		//UE_LOG(LogTemp, Display, TEXT("MoonshotMoverUtils: ComputeFloorDist(): Setting FloorDist = FloorSweepDist (bBlockingHit=%s, bWalkableFloor=%s, HitResult.bStartPenetrating=%s)"), OutFloorResult.bBlockingHit ? TEXT("true") : TEXT("false"), OutFloorResult.bWalkableFloor ? TEXT("true") : TEXT("false"), OutFloorResult.HitResult.bStartPenetrating ? TEXT("true") : TEXT("false"));
		OutFloorResult.FloorDist = FloorSweepDistance;

		return;
	}

	// Line trace
	if (LineTraceDistance > 0.f)
	{
		const float ShrinkHeight = PawnHalfHeight;
		const FVector LineTraceStart = Location;	
		const float TraceDist = LineTraceDistance + ShrinkHeight;
		const FVector Down = FVector(0.f, 0.f, -TraceDist);
		QueryParams.TraceTag = SCENE_QUERY_STAT_NAME_ONLY(FloorLineTrace);

		FHitResult Hit(1.f);
		//UE_LOG(LogTemp, Display, TEXT("MoonshotMoverUtils: ComputeFloorDist(): Before line trace: bBlockingHit=%s, bWalkableFloor=%s, HitResult.bStartPenetrating=%s)"), OutFloorResult.bBlockingHit ? TEXT("true") : TEXT("false"), OutFloorResult.bWalkableFloor ? TEXT("true") : TEXT("false"), OutFloorResult.HitResult.bStartPenetrating ? TEXT("true") : TEXT("false"));
		bBlockingHit = UpdatedComponent->GetWorld()->LineTraceSingleByChannel(Hit, LineTraceStart, LineTraceStart + Down, CollisionChannel, QueryParams, ResponseParam);
		
		if (bBlockingHit && Hit.Time > 0.f)
		{
			// Reduce hit distance by ShrinkHeight because we started the trace higher than the base.
			// We allow negative distances here, because this allows us to pull out of penetrations.
			const float MaxPenetrationAdjust = FMath::Max(MAX_FLOOR_DIST, PawnRadius);
			const float LineResult = FMath::Max(-MaxPenetrationAdjust, Hit.Time * TraceDist - ShrinkHeight);
			
			OutFloorResult.bBlockingHit = true;
			if (LineResult <= LineTraceDistance && UMoonshotMoverUtils::IsHitSurfaceWalkable(Hit, MaxWalkSlopeCosine, UpdatedComponent))
			{
				OutFloorResult.SetFromLineTrace(Hit, OutFloorResult.FloorDist, LineResult, true);
				return;
			}
		}
		//UE_LOG(LogTemp, Display, TEXT("MoonshotMoverUtils: ComputeFloorDist(): After line trace: bBlockingHit=%s, bWalkableFloor=%s, HitResult.bStartPenetrating=%s)"), OutFloorResult.bBlockingHit ? TEXT("true") : TEXT("false"), OutFloorResult.bWalkableFloor ? TEXT("true") : TEXT("false"), OutFloorResult.HitResult.bStartPenetrating ? TEXT("true") : TEXT("false"));
	}

	// No hits were acceptable.
    //UE_LOG(LogTemp, Display, TEXT("MoonshotMoverUtils: ComputeFloorDist(): No hits were acceptable (bBlockingHit=%s, bWalkableFloor=%s, HitResult.bStartPenetrating=%s)"), OutFloorResult.bBlockingHit ? TEXT("true") : TEXT("false"), OutFloorResult.bWalkableFloor ? TEXT("true") : TEXT("false"), OutFloorResult.HitResult.bStartPenetrating ? TEXT("true") : TEXT("false"));
	OutFloorResult.bWalkableFloor = false;
}

void UMoonshotMoverUtils::FindFloor(const USceneComponent* UpdatedComponent, const UPrimitiveComponent* UpdatedPrimitive, float FloorSweepDistance, float MaxWalkSlopeCosine, const FVector& Location, FFloorCheckResult& OutFloorResult)
{
	if (!UpdatedComponent || !UpdatedComponent->IsQueryCollisionEnabled())
	{
		OutFloorResult.Clear();
		return;
	}

	// Sweep for the floor
	// TODO: Might need to plug in a different value for LineTraceDistance - using the same value as FloorSweepDistance for now - function takes both so we can plug in different values if needed
	UMoonshotMoverUtils::ComputeFloorDist(UpdatedComponent, UpdatedPrimitive, FloorSweepDistance, FloorSweepDistance, MaxWalkSlopeCosine, Location, OutFloorResult);
	//UE_LOG(LogTemp, Display, TEXT("MoonshotMoverUtils: FindFloor() returning with bBlockingHit=%s, bWalkableFloor=%s, HitResult.bStartPenetrating=%s"), OutFloorResult.bBlockingHit ? TEXT("true") : TEXT("false"), OutFloorResult.bWalkableFloor ? TEXT("true") : TEXT("false"), OutFloorResult.HitResult.bStartPenetrating ? TEXT("true") : TEXT("false"));
}

bool UMoonshotMoverUtils::FloorSweepTest(const UPrimitiveComponent* UpdatedPrimitive, FHitResult& OutHit, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const struct FCollisionShape& CollisionShape, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParam)
{
	bool bBlockingHit = false;

	if (UpdatedPrimitive)
	{
		//DrawDebugCapsule(UpdatedPrimitive->GetWorld(), End, CollisionShape.GetCapsuleHalfHeight(), CollisionShape.GetCapsuleRadius(), UpdatedPrimitive->GetComponentQuat(), FColor::Green, false, 0.5f);
		bBlockingHit = UpdatedPrimitive->GetWorld()->SweepSingleByChannel(OutHit, Start, End, UpdatedPrimitive->GetComponentQuat(), TraceChannel, CollisionShape, Params, ResponseParam);
	}
	//UE_LOG(LogTemp, Display, TEXT("MoonshotMoverUtils: FloorSweepTest() returning with bBlockingHit=%s, OutHit.bStartPenetrating=%s"), bBlockingHit ? TEXT("true") : TEXT("false"), OutHit.bStartPenetrating ? TEXT("true") : TEXT("false"));
	return bBlockingHit;
}

bool UMoonshotMoverUtils::IsWithinEdgeTolerance(const FVector& CapsuleLocation, const FVector& TestImpactPoint, float CapsuleRadius)
{
	const float DistFromCenterSq = (TestImpactPoint - CapsuleLocation).SizeSquared2D();
	const float ReducedRadiusSq = FMath::Square(FMath::Max(SWEEP_EDGE_REJECT_DISTANCE + UE_KINDA_SMALL_NUMBER, CapsuleRadius - SWEEP_EDGE_REJECT_DISTANCE));
	return DistFromCenterSq < ReducedRadiusSq;
}