// Fill out your copyright notice in the Description page of Project Settings.

#include "MoonshotMoverAttachingMode.h"
#include "MoonshotMoverDataModelTypes.h"
#include "MoonshotMoverUtils.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Mover/Public/MoverComponent.h"
#include "Mover/Public/MoveLibrary/FloorQueryUtils.h"
#include "Mover/Public/MoveLibrary/MovementUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MoonshotMoverAttachingMode)


UMoonshotMoverAttachingMode::UMoonshotMoverAttachingMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer),
	  AirControlPercentage(0.4f),
	  FallingDeceleration(200.0f),
	  OverTerminalSpeedFallingDeceleration(800.0f),
	  TerminalMovementPlaneSpeed(1500.0f),
	  bShouldClampTerminalVerticalSpeed(true),
	  VerticalFallingDeceleration(4000.0f),
	  TerminalVerticalSpeed(2000.0f)
{
}

void UMoonshotMoverAttachingMode::OnGenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const
{
	//const FCharacterDefaultInputs* CharacterInputs = StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
    const FMoonshotMoverCharacterInputs* CharacterInputs = StartState.InputCmd.InputCollection.FindDataByType<FMoonshotMoverCharacterInputs>();
	const FMoverDefaultSyncState* StartingSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	check(StartingSyncState);

	const float DeltaSeconds = TimeStep.StepMs * 0.001f;

	FAttachingModeParams Params;
	if (CharacterInputs)
	{
		Params.MoveInputType = CharacterInputs->GetMoveInputType();
		Params.MoveInput = CharacterInputs->GetMoveInput();
        Params.ControlRotation = CharacterInputs->ControlRotation;
        Params.GravityAcceleration = CharacterInputs->GravityAcceleration;
	}
	else
	{
		Params.MoveInputType = EMoveInputType::Invalid;
		Params.MoveInput = FVector::ZeroVector;
	}

	/// If there's no intent from input to change orientation, use the current orientation
    /// TODO: Unless there is non-zero angular velocity to apply. Where do we capture that?
    /// TODO: Extend FFreeMoveParams? No. That's not persistent.
    /// TODO: It'll have to be FMoverDefaultSyncState or its base struct
	
    /// TODO: Yep. All of the above. Motherfucker.

	Params.PriorVelocity = StartingSyncState->GetVelocity_WorldSpace();
	Params.PriorOrientation = StartingSyncState->GetOrientation_WorldSpace();
	Params.TurningRate = CommonMovementSettings->ZeroGTurningRate;
	Params.TurningBoost = CommonMovementSettings->TurningBoost;
	Params.MaxSpeed = CommonMovementSettings->ZeroGMaxSpeed;
	Params.Acceleration = CommonMovementSettings->ZeroGLinearAcceleration;
    Params.Deceleration = CommonMovementSettings->AttachBrakingScale;
	Params.DeltaSeconds = DeltaSeconds;

    FRotator IntendedOrientation_WorldSpace;
    if (!CharacterInputs || CharacterInputs->OrientationIntent.IsNearlyZero())
    {
        //Params.OrientationIntent = Params.PriorOrientation.ToOrientationRotator();
        IntendedOrientation_WorldSpace = StartingSyncState->GetOrientation_WorldSpace();

    } else
    {
        IntendedOrientation_WorldSpace = CharacterInputs->GetOrientationIntentDir_WorldSpace().ToOrientationRotator();
    }

    Params.OrientationIntent = IntendedOrientation_WorldSpace;

    if (!CharacterInputs || Params.GravityAcceleration.IsNearlyZero())
    {
        Params.GravityAcceleration = -980.0f * GetMoverComponent()->GetOwner()->GetActorUpVector();
    }
	
	OutProposedMove.DirectionIntent = Params.MoveInput.GetSafeNormal();
	OutProposedMove.bHasDirIntent = !OutProposedMove.DirectionIntent.IsNearlyZero();

    FVector Acceleration = AirControlPercentage * Params.Acceleration * Params.MoveInput;

    OutProposedMove.LinearVelocity = Params.PriorVelocity.GetClampedToMaxSize(Params.MaxSpeed) + (Acceleration + Params.GravityAcceleration) * Params.DeltaSeconds;

    //DrawDebugLine(GetWorld(), GetMoverComponent()->GetOwner()->GetActorLocation(), GetMoverComponent()->GetOwner()->GetActorLocation() + Params.OrientationIntent.Vector() * 200.f, FColor::Blue, false, 0.1f, 0, 1.0f);
    //DrawDebugLine(GetWorld(), GetMoverComponent()->GetOwner()->GetActorLocation(), GetMoverComponent()->GetOwner()->GetActorLocation() + Params.PriorOrientation.Vector() * 200.f, FColor::Blue, false, 0.1f, 0, 0.2f);
    
    FRotator AngularVelocityDpS(FRotator::ZeroRotator);


    if (Params.DeltaSeconds > 0.0f)
    {
        Params.OrientationIntent = Params.OrientationIntent.Vector().ToOrientationRotator();
        Params.PriorOrientation = Params.PriorOrientation.Vector().ToOrientationRotator();

        Params.OrientationIntent = GetMoverComponent()->GetOwner()->GetActorTransform().InverseTransformRotation(Params.OrientationIntent.Quaternion()).Rotator();
        Params.PriorOrientation = GetMoverComponent()->GetOwner()->GetActorTransform().InverseTransformRotation(Params.PriorOrientation.Quaternion()).Rotator();

        FQuat DeltaQuat = FQuat::FindBetweenNormals(Params.PriorOrientation.Vector(), Params.OrientationIntent.Vector());
        FRotator AngularDelta = DeltaQuat.Rotator();

        //UE_LOG(LogTemp, Display, TEXT("Delta: %s"), *AngularDelta.ToString());

        FRotator Winding, Remainder;

        AngularDelta.GetWindingAndRemainder(Winding, Remainder);

        AngularVelocityDpS = Remainder * (1.0f / Params.DeltaSeconds);

        if (Params.TurningRate >= 0.0f)
        {
            AngularVelocityDpS.Yaw = FMath::Clamp(AngularVelocityDpS.Yaw, -Params.TurningRate, Params.TurningRate);
            AngularVelocityDpS.Pitch = FMath::Clamp(AngularVelocityDpS.Pitch, -Params.TurningRate, Params.TurningRate);
            AngularVelocityDpS.Roll = FMath::Clamp(AngularVelocityDpS.Roll, -Params.TurningRate, Params.TurningRate);
        }

    }

    //DrawDebugLine(GetWorld(), GetMoverComponent()->GetOwner()->GetActorLocation(), GetMoverComponent()->GetOwner()->GetActorLocation() + AngularVelocityDpS.Vector() * 200.f, FColor::Purple, false, 0.1f, 0, 1);

    OutProposedMove.AngularVelocity = AngularVelocityDpS;

    //UE_LOG(LogTemp, Display, TEXT("GravityAccel: %s, Velocity: %s"), *Params.GravityAcceleration.ToString(), *OutProposedMove.LinearVelocity.ToString());
}

void UMoonshotMoverAttachingMode::OnSimulationTick(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
	const FMoverTickStartData& StartState = Params.StartState;
	USceneComponent* UpdatedComponent = Params.UpdatedComponent;
	UPrimitiveComponent* UpdatedPrimitive = Params.UpdatedPrimitive;
	FProposedMove ProposedMove = Params.ProposedMove;

    const FMoonshotMoverCharacterInputs* CharacterInputs = StartState.InputCmd.InputCollection.FindDataByType<FMoonshotMoverCharacterInputs>();
	const FMoverDefaultSyncState* StartingSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	check(StartingSyncState);

	FMoverDefaultSyncState& OutputSyncState = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();

	const float DeltaSeconds = Params.TimeStep.StepMs * 0.001f;
    float PctTimeApplied = 0.f;

	// Instantaneous movement changes that are executed and we exit before consuming any time
	if (ProposedMove.bHasTargetLocation && AttemptTeleport(UpdatedComponent, ProposedMove.TargetLocation, UpdatedComponent->GetComponentRotation(), *StartingSyncState, OutputState))
	{
		OutputState.MovementEndState.RemainingMs = Params.TimeStep.StepMs; 	// Give back all the time
		return;
	}

	FMovementRecord MoveRecord;
	MoveRecord.SetDeltaSeconds(DeltaSeconds);

	FFloorCheckResult CurrentFloor;
	UMoverBlackboard* SimBlackboard = GetBlackboard_Mutable();

	// If we don't have cached floor information, we need to search for it again
	if (!SimBlackboard->TryGet(CommonBlackboard::LastFloorResult, CurrentFloor))
	{
		UMoonshotMoverUtils::FindFloor(UpdatedComponent, UpdatedPrimitive,
			CommonMovementSettings->FloorSweepDistance, CommonMovementSettings->MaxWalkSlopeCosine,
			UpdatedPrimitive->GetComponentLocation(), CurrentFloor);
	}
 
	OutputSyncState.MoveDirectionIntent = (ProposedMove.bHasDirIntent ? ProposedMove.DirectionIntent : FVector::ZeroVector);

	// Use the orientation intent directly. If no intent is provided, use last frame's orientation. Note that we are assuming rotation changes can't fail. 
	const FRotator StartingOrient = StartingSyncState->GetOrientation_WorldSpace();
	//FRotator TargetOrient = StartingOrient;

    // Apply orientation changes (if any)
    FQuat OrientQuat = StartingOrient.Quaternion();
	if (!ProposedMove.AngularVelocity.IsZero())
	{
		//FRotator TargetOrient = StartingOrient + ProposedMove.AngularVelocity * DeltaSeconds;
        //OrientQuat = TargetOrient.Quaternion();
        FQuat AngularVelocityQuat = (ProposedMove.AngularVelocity * DeltaSeconds).Quaternion();
        OrientQuat = OrientQuat * AngularVelocityQuat;
	}

    FVector CurrentUp = GetMoverComponent()->GetOwner()->GetActorUpVector();
    FVector GravityUp = CurrentUp;

	if (CurrentFloor.HitResult.IsValidBlockingHit())
    {
        GravityUp = CurrentFloor.HitResult.ImpactNormal;
    }
	else
	{
		//if (GetMoverComponent()->GetOwner())->FindSurfaceGravity(GravityUp))
        FHitResult Hit(1.f);
        FCollisionQueryParams IgnoreParams;
        TArray<AActor*> PawnChildren;
        GetMoverComponent()->GetOwner()->GetAllChildActors(PawnChildren);
        IgnoreParams.AddIgnoredActors(PawnChildren);
        IgnoreParams.AddIgnoredActor(GetMoverComponent()->GetOwner());

        if (GetWorld()->LineTraceSingleByChannel(
                    Hit,
                    GetMoverComponent()->GetOwner()->GetActorLocation(),
                    GetMoverComponent()->GetOwner()->GetActorLocation() - GetMoverComponent()->GetOwner()->GetActorUpVector() * CommonMovementSettings->MaxAttachDistance,
                    ECollisionChannel::ECC_Visibility,
                    IgnoreParams
                )
            )
		{

			GravityUp = Hit.ImpactNormal;
		}
		else
		{
			OutputState.MovementEndState.NextModeName = CommonMovementSettings->ZeroGMovementModeName;
			return;
		}
	}

    FQuat GravityQuat = FQuat::FindBetweenNormals(CurrentUp, GravityUp);
    OrientQuat = GravityQuat * OrientQuat;


    OrientQuat.Normalize();
	
	FVector MoveDelta = ProposedMove.LinearVelocity * DeltaSeconds;


	FHitResult Hit(1.f);

	if (!MoveDelta.IsNearlyZero() || !ProposedMove.AngularVelocity.IsNearlyZero())
	{
		UMovementUtils::TrySafeMoveUpdatedComponent(UpdatedComponent, UpdatedPrimitive, MoveDelta, OrientQuat, true, Hit, ETeleportType::None, MoveRecord);
	}

    FFloorCheckResult LandingFloor;

	if (Hit.IsValidBlockingHit() && UpdatedPrimitive)
	{   
        float LastMoveTimeSlice = DeltaSeconds;
		float SubTimeTickRemaining = LastMoveTimeSlice * (1.f - Hit.Time);

        PctTimeApplied += Hit.Time * (1.f - PctTimeApplied);

        if (UAttachingModeUtils::IsValidLandingSpot(UpdatedComponent, UpdatedPrimitive, UpdatedPrimitive->GetComponentLocation(),
            Hit, CommonMovementSettings->FloorSweepDistance, CommonMovementSettings->MaxWalkSlopeCosine, OUT LandingFloor))
        {
            //UE_LOG(LogTemp, Warning, TEXT("WE got a valid landing spot!"));
            CaptureFinalState(UpdatedComponent, *StartingSyncState, LandingFloor, DeltaSeconds, DeltaSeconds * PctTimeApplied, OutputSyncState, OutputState, MoveRecord);
            return;
        }

        LandingFloor.HitResult = Hit;
		SimBlackboard->Set(CommonBlackboard::LastFloorResult, LandingFloor);


		UMoverComponent* MoverComponent = GetMoverComponent();
		//FMoverOnImpactParams ImpactParams(DefaultModeNames::Flying, Hit, MoveDelta);
		FMoverOnImpactParams ImpactParams(FName("ZeroG"), Hit, MoveDelta);
		MoverComponent->HandleImpact(ImpactParams);
		// Try to slide the remaining distance along the surface.
        //UE_LOG(LogTemp, Display, TEXT("Not a valid landing spot, trying to slide."));
		UMovementUtils::TryMoveToSlideAlongSurface(UpdatedComponent, UpdatedPrimitive, MoverComponent, MoveDelta, 1.f - Hit.Time, OrientQuat, Hit.Normal, Hit, true, MoveRecord);

        PctTimeApplied += Hit.Time * (1.f - PctTimeApplied);

        if (LandingFloor.IsWalkableFloor())
        {
            CaptureFinalState(UpdatedComponent, *StartingSyncState, LandingFloor, DeltaSeconds, DeltaSeconds * PctTimeApplied, OutputSyncState, OutputState, MoveRecord);
            return;
        }
	}
    else
    {
        // This indicates an unimpeded full move
		PctTimeApplied = 1.f;
    }

	CaptureFinalState(UpdatedComponent, *StartingSyncState, LandingFloor, DeltaSeconds, DeltaSeconds* PctTimeApplied, OutputSyncState, OutputState, MoveRecord);
}


bool UMoonshotMoverAttachingMode::AttemptTeleport(USceneComponent* UpdatedComponent, const FVector& TeleportPos, const FRotator& TeleportRot, const FMoverDefaultSyncState& StartingSyncState, FMoverTickEndData& Output)
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

void UMoonshotMoverAttachingMode::ProcessLanded(const FFloorCheckResult& FloorResult, FVector& Velocity, FRelativeBaseInfo& BaseInfo, FMoverTickEndData& TickEndData) const
{
    UMoverBlackboard* SimBlackboard = GetBlackboard_Mutable();
	FName NextMovementMode = NAME_None; 

	//UE_LOG(LogTemp, Warning, TEXT("ProcessLanded: FloorResult.IsWalkableFloor? %s"), FloorResult.IsWalkableFloor() ? TEXT("true") : TEXT("false"));

    if (FloorResult.IsWalkableFloor())
	{
		//UE_LOG(LogTemp, Display, TEXT("MoonshotMoverAttachingMode: Found a walkable floor."));
		// Transfer to LandingMovementMode (usually walking), and cache any floor / movement base info
		//Velocity.Z = 0.0;
        Velocity = FVector::ZeroVector;
		NextMovementMode = CommonMovementSettings->GroundMovementModeName;

		SimBlackboard->Set(CommonBlackboard::LastFloorResult, FloorResult);

		if (UBasedMovementUtils::IsADynamicBase(FloorResult.HitResult.GetComponent()))
		{
			BaseInfo.SetFromFloorResult(FloorResult);
		}
	}

    TickEndData.MovementEndState.NextModeName = NextMovementMode;
	OnAttach.Broadcast(NextMovementMode, FloorResult.HitResult);
}


// TODO: replace this function with simply looking at/collapsing the MovementRecord
void UMoonshotMoverAttachingMode::CaptureFinalState(USceneComponent* UpdatedComponent, const FMoverDefaultSyncState& StartSyncState, const FFloorCheckResult& FloorResult, float DeltaSeconds, float DeltaSecondsUsed, FMoverDefaultSyncState& OutputSyncState, FMoverTickEndData& TickEndData, FMovementRecord& Record) const
{
	UMoverBlackboard* SimBlackboard = GetBlackboard_Mutable();

	const FVector FinalLocation = UpdatedComponent->GetComponentLocation();

	// Check for time refunds
	constexpr float MinRemainingSecondsToRefund = 0.0001f;	// If we have this amount of time (or more) remaining, give it to the next simulation step.

	if ((DeltaSeconds - DeltaSecondsUsed) >= MinRemainingSecondsToRefund)
	{
		const float PctOfTimeRemaining = (1.0f - (DeltaSecondsUsed / DeltaSeconds));
		TickEndData.MovementEndState.RemainingMs = PctOfTimeRemaining * DeltaSeconds * 1000.f;
	}
	else
	{
		TickEndData.MovementEndState.RemainingMs = 0.f;
	}
	
	Record.SetDeltaSeconds( DeltaSecondsUsed );
	
	FVector EffectiveVelocity = Record.GetRelevantVelocity();
	// TODO: Update Main/large movement record with substeps from our local record

	FRelativeBaseInfo MovementBaseInfo;

    //UE_LOG(LogTemp, Warning, TEXT("About to process landed with FloorResult.IsWalkableFloor? %s"), FloorResult.IsWalkableFloor() ? TEXT("true") : TEXT("false"));
	ProcessLanded(FloorResult, EffectiveVelocity, MovementBaseInfo, TickEndData);

	if (MovementBaseInfo.HasRelativeInfo())
	{
		SimBlackboard->Set(CommonBlackboard::LastFoundDynamicMovementBase, MovementBaseInfo);

		OutputSyncState.SetTransforms_WorldSpace( FinalLocation,
												  UpdatedComponent->GetComponentRotation(),
												  EffectiveVelocity,
												  MovementBaseInfo.MovementBase.Get(), MovementBaseInfo.BoneName);
	}
	else
	{
		OutputSyncState.SetTransforms_WorldSpace( FinalLocation,
												  UpdatedComponent->GetComponentRotation(),
												  EffectiveVelocity,
												  nullptr); // no movement base
	}

	UpdatedComponent->ComponentVelocity = EffectiveVelocity;
}

void UMoonshotMoverAttachingMode::OnRegistered(const FName ModeName)
{
	Super::OnRegistered(ModeName);

	CommonMovementSettings = GetMoverComponent()->FindSharedSettings<UMoonshotMoverCommonMovementSettings>();
	ensureMsgf(CommonMovementSettings, TEXT("Failed to find instance of MoonshotMoverCommonMovementSettings on %s. Movement may not function properly."), *GetPathNameSafe(this));
}

void UMoonshotMoverAttachingMode::OnUnregistered()
{
	CommonMovementSettings = nullptr;

	Super::OnUnregistered();
}

bool UAttachingModeUtils::IsValidLandingSpot(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, const FVector& Location, const FHitResult& Hit, float FloorSweepDistance, float WalkableFloorZ, FFloorCheckResult& OutFloorResult)
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
	if (!UMoonshotMoverUtils::IsHitSurfaceWalkable(Hit, WalkableFloorZ, UpdatedComponent))
	{
		return false;
	}

	// Make sure floor test passes here.
	UMoonshotMoverUtils::FindFloor(UpdatedComponent, UpdatedPrimitive, 
		FloorSweepDistance, WalkableFloorZ,
		Location, OutFloorResult);

	if (!OutFloorResult.IsWalkableFloor())
	{
		return false;
	}

	return true;
}

float UAttachingModeUtils::TryMoveToFallAlongSurface(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, UMoverComponent* MoverComponent, const FVector& Delta, float PctOfDeltaToMove, const FQuat Rotation, const FVector& Normal, FHitResult& Hit, bool bHandleImpact, float FloorSweepDistance, float MaxWalkSlopeCosine, FFloorCheckResult& OutFloorResult, FMovementRecord& MoveRecord)
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
			if (!UAttachingModeUtils::IsValidLandingSpot(UpdatedComponent, UpdatedPrimitive, UpdatedPrimitive->GetComponentLocation(),
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
					UAttachingModeUtils::IsValidLandingSpot(UpdatedComponent, UpdatedPrimitive, UpdatedPrimitive->GetComponentLocation(),
						Hit, FloorSweepDistance, MaxWalkSlopeCosine, OutFloorResult);
				}
			}
		}

		return FMath::Clamp(PctOfTimeUsed, 0.f, 1.f);
	}

	return 0.f;
}