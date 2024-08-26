// Fill out your copyright notice in the Description page of Project Settings.

#include "MoonshotMoverZeroGMode.h"
#include "MoonshotMoverDataModelTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Mover/Public/MoverComponent.h"
#include "Mover/Public/MoveLibrary/FloorQueryUtils.h"
#include "Mover/Public/MoveLibrary/MovementUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MoonshotMoverZeroGMode)


UMoonshotMoverZeroGMode::UMoonshotMoverZeroGMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMoonshotMoverZeroGMode::OnGenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const
{
	//const FCharacterDefaultInputs* CharacterInputs = StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
    const FMoonshotMoverCharacterInputs* CharacterInputs = StartState.InputCmd.InputCollection.FindDataByType<FMoonshotMoverCharacterInputs>();
	const FMoverDefaultSyncState* StartingSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	check(StartingSyncState);

	const float DeltaSeconds = TimeStep.StepMs * 0.001f;

	FZeroGModeParams Params;
	if (CharacterInputs)
	{
		Params.MoveInputType = CharacterInputs->GetMoveInputType();
		Params.MoveInput = CharacterInputs->GetMoveInput();
        //Params.ControlRotation = CharacterInputs->ControlRotation;
        Params.AngularVelocity = CharacterInputs->AngularVelocity;
        Params.Deceleration = CharacterInputs->bIsJumpPressed ? CommonMovementSettings->LinearBrakingScale : CommonMovementSettings->ZeroGDeceleration;
        if (FMath::Abs(Params.AngularVelocity.Roll) > 180.0f)
        {
            Params.AngularVelocity.Roll -= 360.0f;
        }
		if (FMath::Abs(Params.AngularVelocity.Pitch) > 180.0f)
        {
            Params.AngularVelocity.Pitch -= 360.0f;
        }
		if (FMath::Abs(Params.AngularVelocity.Yaw) > 180.0f)
        {
            Params.AngularVelocity.Yaw -= 360.0f;
        }

        Params.AngularVelocity = FRotator(
            Params.AngularVelocity.Pitch,	//CharacterInputs->OrientationIntent.Y,   // Pitch
            Params.AngularVelocity.Yaw,								//CharacterInputs->OrientationIntent.Z,   // Yaw
            Params.AngularVelocity.Roll //CharacterInputs->OrientationIntent.X //+ Params.PriorAngularVelocity.Roll     // Roll
        );

		//Params.OrientationIntent = CharacterInputs->AngularVelocity;

        //UE_LOG(LogTemp, Display, TEXT("PrevAngVel: %f"), Params.AngularVelocity.Roll);
	}
	else
	{
		Params.MoveInputType = EMoveInputType::Invalid;
		Params.MoveInput = FVector::ZeroVector;
        Params.Deceleration = CommonMovementSettings->Deceleration;
	}

	FRotator IntendedOrientation_WorldSpace;
	/// If there's no intent from input to change orientation, use the current orientation
    /// TODO: Unless there is non-zero angular velocity to apply. Where do we capture that?
    /// TODO: Extend FFreeMoveParams? No. That's not persistent.
    /// TODO: It'll have to be FMoverDefaultSyncState or its base struct
	
    /// TODO: Yep. All of the above. Motherfucker.

    //UE_LOG(LogTemp, Display, TEXT("Orientation Intent (Transformed): %s"), *Params.OrientationIntent.ToString());

	Params.PriorVelocity = StartingSyncState->GetVelocity_WorldSpace();
	Params.PriorOrientation = StartingSyncState->GetOrientation_WorldSpace();
	Params.TurningRate = CommonMovementSettings->ZeroGTurningRate;
	Params.TurningBoost = CommonMovementSettings->TurningBoost;
	Params.MaxSpeed = CommonMovementSettings->ZeroGMaxSpeed;
	Params.Acceleration = CommonMovementSettings->ZeroGLinearAcceleration;
	Params.DeltaSeconds = DeltaSeconds;
	
	OutProposedMove = UZeroGModeUtils::ComputeControlledFreeMove(Params, GetMoverComponent()->GetOwner()->GetActorTransform(), GetWorld());
}

void UMoonshotMoverZeroGMode::OnSimulationTick(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
	const FMoverTickStartData& StartState = Params.StartState;
	USceneComponent* UpdatedComponent = Params.UpdatedComponent;
	UPrimitiveComponent* UpdatedPrimitive = Params.UpdatedPrimitive;
	FProposedMove ProposedMove = Params.ProposedMove;

	//const FCharacterDefaultInputs* CharacterInputs = StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
    const FMoonshotMoverCharacterInputs* CharacterInputs = StartState.InputCmd.InputCollection.FindDataByType<FMoonshotMoverCharacterInputs>();
	const FMoverDefaultSyncState* StartingSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	check(StartingSyncState);

	FMoverDefaultSyncState& OutputSyncState = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();

	const float DeltaSeconds = Params.TimeStep.StepMs * 0.001f;

	// Instantaneous movement changes that are executed and we exit before consuming any time
	if (ProposedMove.bHasTargetLocation && AttemptTeleport(UpdatedComponent, ProposedMove.TargetLocation, UpdatedComponent->GetComponentRotation(), *StartingSyncState, OutputState))
	{
		OutputState.MovementEndState.RemainingMs = Params.TimeStep.StepMs; 	// Give back all the time
		return;
	}

	FMovementRecord MoveRecord;
	MoveRecord.SetDeltaSeconds(DeltaSeconds);

	UMoverBlackboard* SimBlackboard = GetBlackboard_Mutable();

	SimBlackboard->Invalidate(CommonBlackboard::LastFloorResult);	// flying = no valid floor
	SimBlackboard->Invalidate(CommonBlackboard::LastFoundDynamicMovementBase);

	OutputSyncState.MoveDirectionIntent = (ProposedMove.bHasDirIntent ? ProposedMove.DirectionIntent : FVector::ZeroVector);

	// Use the orientation intent directly. If no intent is provided, use last frame's orientation. Note that we are assuming rotation changes can't fail. 
	const FRotator StartingOrient = StartingSyncState->GetOrientation_WorldSpace();
	//FRotator TargetOrient = StartingOrient;

	//bool bIsOrientationChanging = false;

    //FQuat AngularQuat = FQuat::MakeFromEuler((ProposedMove.AngularVelocity * DeltaSeconds).Euler());

	// Apply orientation changes (if any)
	FQuat OrientQuat = StartingOrient.Quaternion();
	if (!ProposedMove.AngularVelocity.IsZero())
	{
		FQuat AngularQuat = (ProposedMove.AngularVelocity * DeltaSeconds).Quaternion();
		OrientQuat = OrientQuat * AngularQuat;
	}
	
	FVector MoveDelta = ProposedMove.LinearVelocity * DeltaSeconds;
	//FQuat OrientQuat = TargetOrient.Quaternion() * AngularQuat;
    OrientQuat.Normalize();

	FHitResult Hit(1.f);

	if (!MoveDelta.IsNearlyZero() || !ProposedMove.AngularVelocity.IsNearlyZero())
	{
		UMovementUtils::TrySafeMoveUpdatedComponent(UpdatedComponent, UpdatedPrimitive, MoveDelta, OrientQuat, true, Hit, ETeleportType::None, MoveRecord);
	}

	if (Hit.IsValidBlockingHit())
	{
		UMoverComponent* MoverComponent = GetMoverComponent();
		FMoverOnImpactParams ImpactParams(DefaultModeNames::Flying, Hit, MoveDelta);
		MoverComponent->HandleImpact(ImpactParams);
		// Try to slide the remaining distance along the surface.
		UMovementUtils::TryMoveToSlideAlongSurface(UpdatedComponent, UpdatedPrimitive, MoverComponent, MoveDelta, 1.f - Hit.Time, OrientQuat, Hit.Normal, Hit, true, MoveRecord);
	}

	CaptureFinalState(UpdatedComponent, MoveRecord, *StartingSyncState, OutputSyncState, DeltaSeconds);
}


bool UMoonshotMoverZeroGMode::AttemptTeleport(USceneComponent* UpdatedComponent, const FVector& TeleportPos, const FRotator& TeleportRot, const FMoverDefaultSyncState& StartingSyncState, FMoverTickEndData& Output)
{
	if (UpdatedComponent->GetOwner()->TeleportTo(TeleportPos, TeleportRot))
	{
		FMoverDefaultSyncState& OutputSyncState = Output.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();

		OutputSyncState.SetTransforms_WorldSpace( UpdatedComponent->GetComponentLocation(),
												  UpdatedComponent->GetComponentRotation(),
												  StartingSyncState.GetVelocity_WorldSpace(),
												  nullptr); // no movement base

		UpdatedComponent->ComponentVelocity = StartingSyncState.GetVelocity_WorldSpace();

		GetBlackboard_Mutable()->Invalidate(CommonBlackboard::LastFloorResult);

		return true;
	}

	return false;
}


// TODO: replace this function with simply looking at/collapsing the MovementRecord
void UMoonshotMoverZeroGMode::CaptureFinalState(USceneComponent* UpdatedComponent, FMovementRecord& Record, const FMoverDefaultSyncState& StartSyncState, FMoverDefaultSyncState& OutputSyncState, const float DeltaSeconds) const
{
	const FVector FinalLocation = UpdatedComponent->GetComponentLocation();
	const FVector FinalVelocity = Record.GetRelevantVelocity();
	
	// TODO: Update Main/large movement record with substeps from our local record

	OutputSyncState.SetTransforms_WorldSpace(FinalLocation,
											  UpdatedComponent->GetComponentRotation(),
											  FinalVelocity,
											  nullptr); // no movement base

	UpdatedComponent->ComponentVelocity = FinalVelocity;
}

void UMoonshotMoverZeroGMode::OnRegistered(const FName ModeName)
{
	Super::OnRegistered(ModeName);

	CommonMovementSettings = GetMoverComponent()->FindSharedSettings<UMoonshotMoverCommonMovementSettings>();
	ensureMsgf(CommonMovementSettings, TEXT("Failed to find instance of MoonshotMoverCommonMovementSettings on %s. Movement may not function properly."), *GetPathNameSafe(this));
}


void UMoonshotMoverZeroGMode::OnUnregistered()
{
	CommonMovementSettings = nullptr;

	Super::OnUnregistered();
}

FProposedMove UZeroGModeUtils::ComputeControlledFreeMove(const FZeroGModeParams& InParams, FTransform OwnerTransform, UWorld* World)
{
	FProposedMove OutMove;

	//const FPlane MovementPlane(FVector::ZeroVector, FVector::UpVector);
    const FPlane MovementPlane(FVector::ZeroVector, InParams.OrientationIntent.Quaternion().GetUpVector());

	OutMove.DirectionIntent = InParams.MoveInput.GetSafeNormal();
	OutMove.bHasDirIntent = !OutMove.DirectionIntent.IsNearlyZero();
/*
	FComputeVelocityParams ComputeVelocityParams;
	ComputeVelocityParams.DeltaSeconds = InParams.DeltaSeconds;
	ComputeVelocityParams.InitialVelocity = InParams.PriorVelocity;
	ComputeVelocityParams.MoveDirectionIntent = InParams.MoveInput;
	ComputeVelocityParams.MaxSpeed = InParams.MaxSpeed;
	ComputeVelocityParams.TurningBoost = InParams.TurningBoost;
	ComputeVelocityParams.Deceleration = InParams.Deceleration;
	ComputeVelocityParams.Acceleration = InParams.Acceleration;
	
	OutMove.LinearVelocity = UMovementUtils::ComputeVelocity(ComputeVelocityParams);
*/

    FVector Acceleration = InParams.Acceleration * InParams.MoveInput;
    FVector Velocity = InParams.PriorVelocity;

    bool bTooFast = Velocity.SizeSquared() >= FMath::Square(InParams.MaxSpeed);
    if (Acceleration.SizeSquared() > 0.0f && !bTooFast)
    {
        Velocity += Acceleration * InParams.DeltaSeconds;

        if (Velocity.SizeSquared() > FMath::Square(InParams.MaxSpeed))
        {
            Velocity = Velocity.GetSafeNormal() * InParams.MaxSpeed;
        }
    }

    OutMove.LinearVelocity = (1.0f - InParams.Deceleration) * Velocity.GetClampedToMaxSize(InParams.MaxSpeed);

    if (InParams.DeltaSeconds > 0.0f)
	{
        OutMove.AngularVelocity = InParams.AngularVelocity * (1.0f / InParams.DeltaSeconds);

        if (InParams.TurningRate >= 0.0f)
		{
			OutMove.AngularVelocity.Yaw   = 10.0f * FMath::Clamp(OutMove.AngularVelocity.Yaw,   -InParams.TurningRate, InParams.TurningRate);
			OutMove.AngularVelocity.Pitch = 10.0f * FMath::Clamp(OutMove.AngularVelocity.Pitch, -InParams.TurningRate, InParams.TurningRate);
			OutMove.AngularVelocity.Roll  = FMath::Clamp(OutMove.AngularVelocity.Roll,  -InParams.TurningRate, InParams.TurningRate);
		}

	}

	return OutMove;
}

bool UZeroGModeUtils::IsValidLandingSpot(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, const FVector& Location, const FHitResult& Hit, float FloorSweepDistance, float WalkableFloorZ, FFloorCheckResult& OutFloorResult)
{
	OutFloorResult.Clear();

	if (!Hit.bBlockingHit)
	{
		return false;
	}

	if (Hit.bStartPenetrating)
	{
		return false;
	}

	// Reject unwalkable floor normals.
	if (!UFloorQueryUtils::IsHitSurfaceWalkable(Hit, WalkableFloorZ))
	{
		return false;
	}

	// Make sure floor test passes here.
	UFloorQueryUtils::FindFloor(UpdatedComponent, UpdatedPrimitive, 
		FloorSweepDistance, WalkableFloorZ,
		Location, OutFloorResult);

	if (!OutFloorResult.IsWalkableFloor())
	{
		return false;
	}

	return true;
}

float UZeroGModeUtils::TryMoveToFallAlongSurface(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, UMoverComponent* MoverComponent, const FVector& Delta, float PctOfDeltaToMove, const FQuat Rotation, const FVector& Normal, FHitResult& Hit, bool bHandleImpact, float FloorSweepDistance, float MaxWalkSlopeCosine, FFloorCheckResult& OutFloorResult, FMovementRecord& MoveRecord)
{
	OutFloorResult.Clear();

	if (!Hit.bBlockingHit)
	{
		return 0.f;
	}

	float PctOfTimeUsed = 0.f;
	const FVector OldHitNormal = Normal;

	FVector SlideDelta = UMovementUtils::ComputeSlideDelta(Delta, PctOfDeltaToMove, Normal, Hit);

	if ((SlideDelta | Delta) > 0.f)
	{
		// First sliding attempt along surface
		UMovementUtils::TrySafeMoveUpdatedComponent(UpdatedComponent, UpdatedPrimitive, SlideDelta, Rotation, true, Hit, ETeleportType::None, MoveRecord);

		PctOfTimeUsed = Hit.Time;
		if (Hit.IsValidBlockingHit())
		{
			// Notify first impact
			if (MoverComponent && bHandleImpact)
			{
				FMoverOnImpactParams ImpactParams(NAME_None, Hit, SlideDelta);
				MoverComponent->HandleImpact(ImpactParams);
			}

			// Check if we landed
			if (!IsValidLandingSpot(UpdatedComponent, UpdatedPrimitive, UpdatedPrimitive->GetComponentLocation(),
				Hit, FloorSweepDistance, MaxWalkSlopeCosine, OutFloorResult))
			{
				// We've hit another surface during our first move, so let's try to slide along both of them together

				// Compute new slide normal when hitting multiple surfaces.
				SlideDelta = UMovementUtils::ComputeTwoWallAdjustedDelta(SlideDelta, Hit, OldHitNormal);

				// Only proceed if the new direction is of significant length and not in reverse of original attempted move.
				if (!SlideDelta.IsNearlyZero(SMALL_MOVE_DISTANCE) && (SlideDelta | Delta) > 0.f)
				{
					// Perform second move, taking 2 walls into account
					UMovementUtils::TrySafeMoveUpdatedComponent(UpdatedComponent, UpdatedPrimitive, SlideDelta, Rotation, true, Hit, ETeleportType::None, MoveRecord);
					PctOfTimeUsed += (Hit.Time * (1.f - PctOfTimeUsed));

					// Notify second impact
					if (MoverComponent && bHandleImpact && Hit.bBlockingHit)
					{
						FMoverOnImpactParams ImpactParams(NAME_None, Hit, SlideDelta);
						MoverComponent->HandleImpact(ImpactParams);
					}

					// Check if we've landed, to acquire floor result
					IsValidLandingSpot(UpdatedComponent, UpdatedPrimitive, UpdatedPrimitive->GetComponentLocation(),
						Hit, FloorSweepDistance, MaxWalkSlopeCosine, OutFloorResult);
				}
			}
		}

		return FMath::Clamp(PctOfTimeUsed, 0.f, 1.f);
	}

	return 0.f;
}