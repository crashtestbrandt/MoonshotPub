// Fill out your copyright notice in the Description page of Project Settings.

#include "MoonshotMoverSurfaceWalkingMode.h"
#include "MoonshotMoverDataModelTypes.h"
#include "MoonshotMoverUtils.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Mover/Public/DefaultMovementSet/LayeredMoves/BasicLayeredMoves.h"
#include "Mover/Public/MoverComponent.h"
#include "Mover/Public/MoveLibrary/FloorQueryUtils.h"
#include "Mover/Public/MoveLibrary/MovementUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MoonshotMoverSurfaceWalkingMode)


UMoonshotMoverSurfaceWalkingMode::UMoonshotMoverSurfaceWalkingMode(const FObjectInitializer& ObjectInitializer)
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

void UMoonshotMoverSurfaceWalkingMode::OnGenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const
{
	const UMoverComponent* MoverComp = GetMoverComponent();
    const FMoonshotMoverCharacterInputs* CharacterInputs = StartState.InputCmd.InputCollection.FindDataByType<FMoonshotMoverCharacterInputs>();
	const FMoverDefaultSyncState* StartingSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	check(StartingSyncState);

    const float DeltaSeconds = TimeStep.StepMs * 0.001f;
	FFloorCheckResult LastFloorResult;
	FVector MovementNormal;

	UMoverBlackboard* SimBlackboard = GetBlackboard_Mutable();

	// Try to use the floor as the basis for the intended move direction (i.e. try to walk along slopes, rather than into them)
	if (SimBlackboard && SimBlackboard->TryGet(CommonBlackboard::LastFloorResult, LastFloorResult) && LastFloorResult.IsWalkableFloor())
	{
		MovementNormal = LastFloorResult.HitResult.ImpactNormal;
	}
	else
	{
		MovementNormal = MoverComp->GetOwner()->GetActorUpVector();
	}

	FSurfaceWalkingModeParams Params;
	if (CharacterInputs)
	{
		Params.MoveInputType = CharacterInputs->GetMoveInputType();
		//Params.MoveInput = CharacterInputs->GetMoveInput();
        Params.MoveInput = CharacterInputs->GetMoveInput_WorldSpace();
        Params.ControlRotation = CharacterInputs->ControlRotation;
        Params.GravityAcceleration = CharacterInputs->GravityAcceleration;
        MovementNormal = -CharacterInputs->GravityAcceleration.GetSafeNormal();
	}
	else
	{
		Params.MoveInputType = EMoveInputType::Invalid;
		Params.MoveInput = FVector::ZeroVector;
	}
	Params.PriorVelocity = FVector::VectorPlaneProject(StartingSyncState->GetVelocity_WorldSpace(), MovementNormal);
	Params.PriorOrientation = StartingSyncState->GetOrientation_WorldSpace();
	Params.TurningRate = CommonMovementSettings->TurningRate;
	Params.TurningBoost = CommonMovementSettings->TurningBoost;
	Params.MaxSpeed = CommonMovementSettings->MaxSpeed;
	Params.Acceleration = CommonMovementSettings->Acceleration;
    Params.Deceleration = CommonMovementSettings->Deceleration;
	Params.DeltaSeconds = DeltaSeconds;

    if (Params.MoveInput.SizeSquared() > 0.f && !UMovementUtils::IsExceedingMaxSpeed(Params.PriorVelocity, CommonMovementSettings->MaxSpeed))
	{
		Params.Friction = CommonMovementSettings->GroundFriction;
	}
	else
	{
		Params.Friction = CommonMovementSettings->bUseSeparateBrakingFriction ? CommonMovementSettings->BrakingFriction : CommonMovementSettings->GroundFriction;
		Params.Friction *= CommonMovementSettings->BrakingFrictionFactor;
	}

    FRotator IntendedOrientation_WorldSpace;
    if (!CharacterInputs || CharacterInputs->OrientationIntent.IsNearlyZero())
    {
        //Params.OrientationIntent = Params.PriorOrientation.ToOrientationRotator();
        IntendedOrientation_WorldSpace = StartingSyncState->GetOrientation_WorldSpace();

    } else
    {
		FVector InOrient = CharacterInputs->GetOrientationIntentDir_WorldSpace();
        IntendedOrientation_WorldSpace = CharacterInputs->GetOrientationIntentDir_WorldSpace().ToOrientationRotator();
    }

    Params.OrientationIntent = IntendedOrientation_WorldSpace;
	
    const FPlane MovementPlane(MoverComp->GetOwner()->GetActorLocation(), MovementNormal);

    //UKismetSystemLibrary::DrawDebugPlane(GetWorld(), MovementPlane, MoverComp->GetOwner()->GetActorLocation(), 100.0f, FColor::Yellow, false);

    // Just in case
	OutProposedMove.DirectionIntent = FVector::VectorPlaneProject(Params.MoveInput, MovementNormal);

    //DrawDebugLine(GetWorld(), MoverComp->GetOwner()->GetActorLocation(), MoverComp->GetOwner()->GetActorLocation() + OutProposedMove.DirectionIntent * 200.f, FColor::Blue, false, 0.1f, 0, 1.0f);

    OutProposedMove.bHasDirIntent = !OutProposedMove.DirectionIntent.IsNearlyZero();

    FComputeVelocityParams ComputeVelocityParams;
	ComputeVelocityParams.DeltaSeconds = Params.DeltaSeconds;
	ComputeVelocityParams.InitialVelocity = Params.PriorVelocity;
	ComputeVelocityParams.MoveDirectionIntent = Params.MoveInput;
	ComputeVelocityParams.MaxSpeed = Params.MaxSpeed;
	ComputeVelocityParams.TurningBoost = Params.TurningBoost;
	ComputeVelocityParams.Deceleration = Params.Deceleration;
	ComputeVelocityParams.Acceleration = Params.Acceleration;
	ComputeVelocityParams.Friction = Params.Friction;

    OutProposedMove.LinearVelocity = UMovementUtils::ComputeVelocity(ComputeVelocityParams);



    //DrawDebugLine(GetWorld(), GetMoverComponent()->GetOwner()->GetActorLocation(), GetMoverComponent()->GetOwner()->GetActorLocation() + Params.OrientationIntent.Vector() * 200.f, FColor::Blue, false, 0.1f, 0, 1.0f);
    //DrawDebugLine(GetWorld(), GetMoverComponent()->GetOwner()->GetActorLocation(), GetMoverComponent()->GetOwner()->GetActorLocation() + Params.PriorOrientation.Vector() * 200.f, FColor::Blue, false, 0.1f, 0, 0.2f);
    
    // Calculate angular velocity for this move
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

void UMoonshotMoverSurfaceWalkingMode::OnSimulationTick(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
    UMoverComponent* MoverComp = GetMoverComponent();
	const FMoverTickStartData& StartState = Params.StartState;
	USceneComponent* UpdatedComponent = Params.UpdatedComponent;
	UPrimitiveComponent* UpdatedPrimitive = Params.UpdatedPrimitive;
	FProposedMove ProposedMove = Params.ProposedMove;

    if (!UpdatedComponent || !UpdatedPrimitive)
	{
		return;
	}

    const FMoonshotMoverCharacterInputs* CharacterInputs = StartState.InputCmd.InputCollection.FindDataByType<FMoonshotMoverCharacterInputs>();
	const FMoverDefaultSyncState* StartingSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	check(StartingSyncState);

	FMoverDefaultSyncState& OutputSyncState = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();

	const float DeltaSeconds = Params.TimeStep.StepMs * 0.001f;
    float PctTimeApplied = 0.f;

	// Instantaneous movement changes that are executed and we exit before consuming any time
	if ( (ProposedMove.bHasTargetLocation &&
        AttemptTeleport(UpdatedComponent, ProposedMove.TargetLocation, UpdatedComponent->GetComponentRotation(), StartingSyncState->GetVelocity_WorldSpace(), OutputState)) ||	// Teleport
		    (CharacterInputs && CharacterInputs->bIsJumpJustPressed &&
                AttemptJump(CommonMovementSettings->JumpUpwardsSpeed, OutputState)) )	// Jump
	{
		UpdatedComponent->ComponentVelocity = StartingSyncState->GetVelocity_WorldSpace();
		OutputState.MovementEndState.RemainingMs = Params.TimeStep.StepMs; 	// Give back all the time
		return;
	}

	TObjectPtr<AActor> OwnerActor = UpdatedComponent->GetOwner();
	check(OwnerActor);

    /** Move Record Starts Here **/
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

    // Apply orientation changes (if any)
    FQuat OrientQuat = StartingOrient.Quaternion();
	if (!ProposedMove.AngularVelocity.IsZero())
	{
        FQuat AngularVelocityQuat = (ProposedMove.AngularVelocity * DeltaSeconds).Quaternion();
        OrientQuat = OrientQuat * AngularVelocityQuat;
	}

    // Get gravity-relative up direction for adjusting orientation
    FVector CurrentUp = OwnerActor->GetActorUpVector();
    FVector GravityUp = CurrentUp;
    if (CurrentFloor.HitResult.IsValidBlockingHit())
    {
        GravityUp = CurrentFloor.HitResult.ImpactNormal;
    }
    else
    {
		// If we can't find a floor normal, we should fall (which will probably result in ZeroG)
        OutputState.MovementEndState.NextModeName = CommonMovementSettings->AirMovementModeName;
        return;
    }

    FQuat GravityQuat = FQuat::FindBetweenNormals(CurrentUp, GravityUp);
    OrientQuat = GravityQuat * OrientQuat;


	/// TODO: Remove debug slerp
	OrientQuat = FQuat::Slerp(StartingOrient.Quaternion(), OrientQuat, 1.0f);

    OrientQuat.Normalize();
	
    const FVector OrigMoveDelta = ProposedMove.LinearVelocity * DeltaSeconds;


	FHitResult MoveHitResult(1.f);
    FVector CurMoveDelta = OrigMoveDelta;

    FOptionalFloorCheckResult StepUpFloorResult;	// passed to sub-operations, so we can use their final floor results if they did a test

	bool bDidAttemptMovement = false;

	float PercentTimeAppliedSoFar = MoveHitResult.Time;
	bool bWasFirstMoveBlocked = false;

    // Actual move attempt starts here
	if (!CurMoveDelta.IsNearlyZero() || !ProposedMove.AngularVelocity.IsNearlyZero())
	{
        // Attempt to move the full amount first
		bDidAttemptMovement = true;
		bool bMoved = UMovementUtils::TrySafeMoveUpdatedComponent(UpdatedComponent, UpdatedPrimitive, CurMoveDelta, OrientQuat, true, MoveHitResult, ETeleportType::None, MoveRecord);

        float LastMoveSeconds = DeltaSeconds;

        if (MoveHitResult.bStartPenetrating)
		{
			UE_LOG(LogTemp, Display, TEXT("MoonshotMoverSurfaceWalkingMode: Attempted move and got stuck in geometry with MoveHitResult.bBlockingHit=%s, MoveHitResult.bStartPenetrating=%s, CurrentFloor.bWalkableFloor=%s and CurrentFloor.HitResult.bStartPenetrating=%s"), MoveHitResult.bBlockingHit ? TEXT("true") : TEXT("false"), MoveHitResult.bStartPenetrating ? TEXT("true") : TEXT("false"), CurrentFloor.bWalkableFloor ? TEXT("true") : TEXT("false"), CurrentFloor.HitResult.bStartPenetrating ? TEXT("true") : TEXT("false"));	
			// We started by being stuck in geometry and need to resolve it first
			bWasFirstMoveBlocked = true;
		}
        else if (MoveHitResult.IsValidBlockingHit())
		{
            bWasFirstMoveBlocked = true;
			PercentTimeAppliedSoFar = MoveHitResult.Time;

            if ((MoveHitResult.Time > 0.f) &&
                UMoonshotMoverUtils::IsHitSurfaceWalkable(MoveHitResult, CommonMovementSettings->MaxWalkSlopeCosine, UpdatedComponent))
			{
                const float PercentTimeRemaining = 1.f - PercentTimeAppliedSoFar;
                /// TODO: Verify this is correctly accounting for arbitrary gravity
                CurMoveDelta = USurfaceWalkingModeUtils::ComputeDeflectedMoveOntoRamp(CurMoveDelta * PercentTimeRemaining, MoveHitResult, CommonMovementSettings->MaxWalkSlopeCosine, CurrentFloor.bLineTrace, UpdatedComponent);

                UMovementUtils::TrySafeMoveUpdatedComponent(UpdatedComponent, UpdatedPrimitive, CurMoveDelta, OrientQuat, true, MoveHitResult, ETeleportType::None, MoveRecord);

                LastMoveSeconds = PercentTimeRemaining * LastMoveSeconds;

				const float SecondHitPercent = MoveHitResult.Time * PercentTimeRemaining;
				PercentTimeAppliedSoFar = FMath::Clamp(PercentTimeAppliedSoFar + SecondHitPercent, 0.f, 1.f);
            }

            if (MoveHitResult.IsValidBlockingHit())
			{
                // If still blocked, try to step up onto the blocking object OR slide along it
                ///  TODO: Take movement bases into account (Omfg)
                /// TODO: Verify this is correctly accounting for arbitrary gravity
                if (USurfaceWalkingModeUtils::CanStepUpOnHitSurface(MoveHitResult)) // || (CharacterOwner->GetMovementBase() != nullptr && Hit.HitObjectHandle == CharacterOwner->GetMovementBase()->GetOwner()))
				{
                    // We hit a barrier or unwalkable surface; try to step up and onto it
                    const FVector PreStepUpLocation = UpdatedComponent->GetComponentLocation();
					//const FVector DownwardDir = -MoverComp->GetOwner()->GetActorUpVector();
                    FVector DownwardDir = -MoveHitResult.ImpactNormal;
                    /// TODO: Override this to account for arbitrary gravity
                    if (!USurfaceWalkingModeUtils::TryMoveToStepUp(UpdatedComponent, UpdatedPrimitive, MoverComp, DownwardDir, CommonMovementSettings->MaxStepHeight, CommonMovementSettings->MaxWalkSlopeCosine, CommonMovementSettings->FloorSweepDistance, OrigMoveDelta * (1.f - PercentTimeAppliedSoFar), MoveHitResult, CurrentFloor, false, &StepUpFloorResult, MoveRecord))
					{
                        FMoverOnImpactParams ImpactParams(DefaultModeNames::Walking, MoveHitResult, OrigMoveDelta);
						MoverComp->HandleImpact(ImpactParams);
						float PercentAvailableToSlide = 1.f - PercentTimeAppliedSoFar;
						//UE_LOG(LogTemp, Display, TEXT("MagneticWalkingMode: Before TryWalkToSlideAlongSurface() with MoveHitResult.bBlockingHit=%s, MoveHitResult.bStartPenetrating=%s, CurrentFloor.bWalkableFloor=%s, CurrentFloor.HitResult.bStartPenetrating=%s"), MoveHitResult.bBlockingHit ? TEXT("true") : TEXT("false"), MoveHitResult.bStartPenetrating ? TEXT("true") : TEXT("false"), CurrentFloor.bWalkableFloor ? TEXT("true") : TEXT("false"), CurrentFloor.HitResult.bStartPenetrating ? TEXT("true") : TEXT("false"));	
                        /// TODO: Override this to account for arbitrary gravity
						float SlideAmount = USurfaceWalkingModeUtils::TryWalkToSlideAlongSurface(UpdatedComponent, UpdatedPrimitive, MoverComp, OrigMoveDelta, PercentAvailableToSlide, OrientQuat, MoveHitResult.Normal, MoveHitResult, true, MoveRecord, CommonMovementSettings->MaxWalkSlopeCosine, CommonMovementSettings->MaxStepHeight);
						//UE_LOG(LogTemp, Display, TEXT("MagneticWalkingMode: After TryWalkToSlideAlongSurface()=%f with MoveHitResult.bBlockingHit=%s, MoveHitResult.HitResult.bStartPenetrating=%s, CurrentFloor.bWalkableFloor=%s, CurrentFloor.HitResult.bStartPenetrating=%s"), SlideAmount, MoveHitResult.bBlockingHit ? TEXT("true") : TEXT("false"), MoveHitResult.bStartPenetrating ? TEXT("true") : TEXT("false"), CurrentFloor.bWalkableFloor ? TEXT("true") : TEXT("false"), CurrentFloor.HitResult.bStartPenetrating ? TEXT("true") : TEXT("false"));	
						PercentTimeAppliedSoFar += PercentAvailableToSlide * SlideAmount;
                    }
                }
                else if (MoveHitResult.Component.IsValid() && !MoveHitResult.Component.Get()->CanCharacterStepUp(Cast<APawn>(MoveHitResult.GetActor())))
				{
					//UE_LOG(LogTemp, Display, TEXT("MagneticWalkingMode: After CanStepUpOnHitSurface()=false, MoveHitResult.bBlockingHit=%s, MoveHitResult.Component.IsValid(), CanCharacterStepUp()=false with MoveHitResult.bStartPenetrating=%s, CurrentFloor.bWalkableFloor=%s, CurrentFloor.HitResult.bStartPenetrating=%s"), MoveHitResult.bBlockingHit ? TEXT("true") : TEXT("false"), MoveHitResult.bStartPenetrating ? TEXT("true") : TEXT("false"), CurrentFloor.bWalkableFloor ? TEXT("true") : TEXT("false"), CurrentFloor.HitResult.bStartPenetrating ? TEXT("true") : TEXT("false"));	
					FMoverOnImpactParams ImpactParams(DefaultModeNames::Walking, MoveHitResult, OrigMoveDelta);
					MoverComp->HandleImpact(ImpactParams);
					float PercentAvailableToSlide = 1.f - PercentTimeAppliedSoFar;
					//UE_LOG(LogTemp, Display, TEXT("MagneticWalkingMode: Before TryWalkToSlideAlongSurface() with MoveHitResult.bBlockingHit=%s, MoveHitResult.bStartPenetrating=%s, CurrentFloor.bWalkableFloor=%s, CurrentFloor.HitResult.bStartPenetrating=%s"), MoveHitResult.bBlockingHit ? TEXT("true") : TEXT("false"), MoveHitResult.bStartPenetrating ? TEXT("true") : TEXT("false"), CurrentFloor.bWalkableFloor ? TEXT("true") : TEXT("false"), CurrentFloor.HitResult.bStartPenetrating ? TEXT("true") : TEXT("false"));	
                    /// TODO: Override this to account for arbitrary gravity
					float SlideAmount = USurfaceWalkingModeUtils::TryWalkToSlideAlongSurface(UpdatedComponent, UpdatedPrimitive, MoverComp, OrigMoveDelta, 1.f - PercentTimeAppliedSoFar, OrientQuat, MoveHitResult.Normal, MoveHitResult, true, MoveRecord, CommonMovementSettings->MaxWalkSlopeCosine, CommonMovementSettings->MaxStepHeight);
					//UE_LOG(LogTemp, Display, TEXT("MagneticWalkingMode: After TryWalkToSlideAlongSurface()=%f with MoveHitResult.bBlockingHit=%s, MoveHitResult.HitResult.bStartPenetrating=%s, CurrentFloor.bWalkableFloor=%s, CurrentFloor.HitResult.bStartPenetrating=%s"), SlideAmount, MoveHitResult.bBlockingHit ? TEXT("true") : TEXT("false"), MoveHitResult.bStartPenetrating ? TEXT("true") : TEXT("false"), CurrentFloor.bWalkableFloor ? TEXT("true") : TEXT("false"), CurrentFloor.HitResult.bStartPenetrating ? TEXT("true") : TEXT("false"));	
					PercentTimeAppliedSoFar += PercentAvailableToSlide * SlideAmount;
				}
            }
        }

        // Search for the floor we've ended up on
		UMoonshotMoverUtils::FindFloor(UpdatedComponent, UpdatedPrimitive,
			CommonMovementSettings->FloorSweepDistance, CommonMovementSettings->MaxWalkSlopeCosine,
			UpdatedPrimitive->GetComponentLocation(), CurrentFloor);
        
        if (CurrentFloor.IsWalkableFloor())
		{
            //UE_LOG(LogTemp, Warning, TEXT("MagneticWalkingMode: Got a walkable floor!"));
            /// TODO: Verify this is correctly accounting for arbitrary gravity
			USurfaceWalkingModeUtils::TryMoveToAdjustHeightAboveFloor(UpdatedComponent, UpdatedPrimitive, CurrentFloor, CommonMovementSettings->MaxWalkSlopeCosine, MoveRecord);
		}

        if (!CurrentFloor.IsWalkableFloor() && !CurrentFloor.HitResult.bStartPenetrating)
		{
            //UE_LOG(LogTemp, Warning, TEXT("MagneticWalkingMode (MoveDelta > 0): Starting AirMovementMode because bWalkableFloor=%s and bStartPenetrating=%s"), CurrentFloor.bWalkableFloor ? TEXT("true") : TEXT("false"), CurrentFloor.HitResult.bStartPenetrating ? TEXT("true") : TEXT("false"));
			// No floor or not walkable, so let's let the airborne movement mode deal with it
			OutputState.MovementEndState.NextModeName = CommonMovementSettings->AirMovementModeName;
			OutputState.MovementEndState.RemainingMs = Params.TimeStep.StepMs - (Params.TimeStep.StepMs * PercentTimeAppliedSoFar);
			MoveRecord.SetDeltaSeconds((Params.TimeStep.StepMs - OutputState.MovementEndState.RemainingMs) * 0.001f);
			CaptureFinalState(UpdatedComponent, bDidAttemptMovement, CurrentFloor, MoveRecord, OutputSyncState);
			return;
		}
	}
    else
    {
        // If the actor isn't moving we still need to check if they have a valid floor
		UMoonshotMoverUtils::FindFloor(UpdatedComponent, UpdatedPrimitive,
			CommonMovementSettings->FloorSweepDistance, CommonMovementSettings->MaxWalkSlopeCosine,
			UpdatedPrimitive->GetComponentLocation(), CurrentFloor);
        
        FHitResult Hit(CurrentFloor.HitResult);
        if (Hit.bStartPenetrating)
		{
			// The floor check failed because it started in penetration
			// We do not want to try to move downward because the downward sweep failed, rather we'd like to try to pop out of the floor.
			Hit.TraceEnd = Hit.TraceStart + FVector(0.f, 0.f, 2.4f);
			FVector RequestedAdjustment = UMovementUtils::ComputePenetrationAdjustment(Hit);
			
			const EMoveComponentFlags IncludeBlockingOverlapsWithoutEvents = (MOVECOMP_NeverIgnoreBlockingOverlaps | MOVECOMP_DisableBlockingOverlapDispatch);
			EMoveComponentFlags MoveComponentFlags = MOVECOMP_NoFlags;
			MoveComponentFlags = (MoveComponentFlags | IncludeBlockingOverlapsWithoutEvents);
			UMovementUtils::TryMoveToResolvePenetration(UpdatedComponent, UpdatedPrimitive, MoveComponentFlags, RequestedAdjustment, Hit, UpdatedComponent->GetComponentQuat(), MoveRecord);
		}
    }

    CaptureFinalState(UpdatedComponent, bDidAttemptMovement, CurrentFloor, MoveRecord, OutputSyncState);
}

bool UMoonshotMoverSurfaceWalkingMode::AttemptJump(float JumpSpeed, FMoverTickEndData& OutputState)
{
    // TODO: This should check if a jump is even allowed
	TSharedPtr<FLayeredMove_SurfaceWalkingModeJumpImpulse> JumpMove = MakeShared<FLayeredMove_SurfaceWalkingModeJumpImpulse>();
	JumpMove->UpwardsSpeed = JumpSpeed;
	OutputState.SyncState.LayeredMoves.QueueLayeredMove(JumpMove);
    //UE_LOG(LogTemp, Warning, TEXT("MagneticWalkingMode: Starting AirMovementMode because AttemptJump()"));
	OutputState.MovementEndState.NextModeName = CommonMovementSettings->AirMovementModeName;
	return true;
}

bool UMoonshotMoverSurfaceWalkingMode::AttemptTeleport(USceneComponent* UpdatedComponent, const FVector& TeleportPos, const FRotator& TeleportRot, const FVector& PriorVelocity, FMoverTickEndData& Output)
{
	if (UpdatedComponent->GetOwner()->TeleportTo(TeleportPos, TeleportRot))
	{
		FMoverDefaultSyncState& OutputSyncState = Output.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();

		OutputSyncState.SetTransforms_WorldSpace( UpdatedComponent->GetComponentLocation(),
												  UpdatedComponent->GetComponentRotation(),
												  PriorVelocity,
												  nullptr ); // no movement base
		
		// TODO: instead of invalidating it, consider checking for a floor. Possibly a dynamic base?
		if (UMoverBlackboard* SimBlackboard = GetBlackboard_Mutable())
		{
			SimBlackboard->Invalidate(CommonBlackboard::LastFloorResult);
			SimBlackboard->Invalidate(CommonBlackboard::LastFoundDynamicMovementBase);
		}

		return true;
	}

	return false;
}


// TODO: replace this function with simply looking at/collapsing the MovementRecord
void UMoonshotMoverSurfaceWalkingMode::CaptureFinalState(USceneComponent* UpdatedComponent, bool bDidAttemptMovement, const FFloorCheckResult& FloorResult, const FMovementRecord& Record, FMoverDefaultSyncState& OutputSyncState) const
{
	//UE_LOG(LogTemp, Warning, TEXT("MagneticWalkingMode: Entering CaptureFinalState with bWalkableFloor=%s and bStartPenetrating=%s"), FloorResult.bWalkableFloor ? TEXT("true") : TEXT("false"), FloorResult.HitResult.bStartPenetrating ? TEXT("true") : TEXT("false"));
	FRelativeBaseInfo PriorBaseInfo;

	UMoverBlackboard* SimBlackboard = GetBlackboard_Mutable();

	const bool bHasPriorBaseInfo = SimBlackboard->TryGet(CommonBlackboard::LastFoundDynamicMovementBase, PriorBaseInfo);

	FRelativeBaseInfo CurrentBaseInfo = UpdateFloorAndBaseInfo(FloorResult);

	// If we're on a dynamic base and we're not trying to move, keep using the same relative actor location. This prevents slow relative 
	//  drifting that can occur from repeated floor sampling as the base moves through the world.
	if (CurrentBaseInfo.HasRelativeInfo() 
		&& bHasPriorBaseInfo && !bDidAttemptMovement 
		&& PriorBaseInfo.UsesSameBase(CurrentBaseInfo))
	{
		CurrentBaseInfo.ContactLocalPosition = PriorBaseInfo.ContactLocalPosition;
	}

	// TODO: Update Main/large movement record with substeps from our local record
	
	if (CurrentBaseInfo.HasRelativeInfo())
	{
		SimBlackboard->Set(CommonBlackboard::LastFoundDynamicMovementBase, CurrentBaseInfo);

		OutputSyncState.SetTransforms_WorldSpace( UpdatedComponent->GetComponentLocation(),
												  UpdatedComponent->GetComponentRotation(),
												  Record.GetRelevantVelocity(),
												  CurrentBaseInfo.MovementBase.Get(), CurrentBaseInfo.BoneName);
	}
	else
	{
		SimBlackboard->Invalidate(CommonBlackboard::LastFoundDynamicMovementBase);

		OutputSyncState.SetTransforms_WorldSpace( UpdatedComponent->GetComponentLocation(),
												  UpdatedComponent->GetComponentRotation(),
												  Record.GetRelevantVelocity(),
												  nullptr);	// no movement base
	}

	UpdatedComponent->ComponentVelocity = OutputSyncState.GetVelocity_WorldSpace();
}

FRelativeBaseInfo UMoonshotMoverSurfaceWalkingMode::UpdateFloorAndBaseInfo(const FFloorCheckResult& FloorResult) const
{
	FRelativeBaseInfo ReturnBaseInfo;

	UMoverBlackboard* SimBlackboard = GetBlackboard_Mutable();

	SimBlackboard->Set(CommonBlackboard::LastFloorResult, FloorResult);

	if (FloorResult.IsWalkableFloor() && UBasedMovementUtils::IsADynamicBase(FloorResult.HitResult.GetComponent()))
	{
		ReturnBaseInfo.SetFromFloorResult(FloorResult);
	}

	return ReturnBaseInfo;
}

void UMoonshotMoverSurfaceWalkingMode::OnRegistered(const FName ModeName)
{
	Super::OnRegistered(ModeName);

	CommonMovementSettings = GetMoverComponent()->FindSharedSettings<UMoonshotMoverCommonMovementSettings>();
	ensureMsgf(CommonMovementSettings, TEXT("Failed to find instance of MoonshotMoverCommonMovementSettings on %s. Movement may not function properly."), *GetPathNameSafe(this));
}

void UMoonshotMoverSurfaceWalkingMode::OnUnregistered()
{
	CommonMovementSettings = nullptr;

	Super::OnUnregistered();
}

FVector USurfaceWalkingModeUtils::ComputeDeflectedMoveOntoRamp(const FVector& OrigMoveDelta, const FHitResult& RampHitResult, float MaxWalkSlopeCosine, const bool bHitFromLineTrace, USceneComponent* UpdatedComponent)
{
	const FVector FloorNormal = RampHitResult.ImpactNormal;
	const FVector ContactNormal = RampHitResult.Normal;

	// JAH TODO: Change these Z tests to take movement plane into account, rather than assuming we're using the Z=0 plane
	//if (FloorNormal.Z < (1.f - UE_KINDA_SMALL_NUMBER) && FloorNormal.Z > UE_KINDA_SMALL_NUMBER && 
	//	ContactNormal.Z > UE_KINDA_SMALL_NUMBER && 
	//	UMoonshotMoverUtils::IsHitSurfaceWalkable(RampHitResult, MaxWalkSlopeCosine) && !bHitFromLineTrace)
    if (UMoonshotMoverUtils::IsHitSurfaceWalkable(RampHitResult, MaxWalkSlopeCosine, UpdatedComponent) && !bHitFromLineTrace)
	{
		// Compute a vector that moves parallel to the surface, by projecting the horizontal movement direction onto the ramp.
		const FPlane RampSurfacePlane(FVector::ZeroVector, FloorNormal);
		return UMovementUtils::ConstrainToPlane(OrigMoveDelta, RampSurfacePlane, true);
	}

	return OrigMoveDelta;
}

bool USurfaceWalkingModeUtils::CanStepUpOnHitSurface(const FHitResult& Hit)
{
	if (!Hit.IsValidBlockingHit())
	{
		return false;
	}

	// No component for "fake" hits when we are on a known good base.
	const UPrimitiveComponent* HitComponent = Hit.Component.Get();
	if (!HitComponent)
	{
		return true;
	}

	APawn* PawnOwner = Cast<APawn>(Hit.GetActor());

	if (!HitComponent->CanCharacterStepUp(PawnOwner))
	{
		return false;
	}

	// No actor for "fake" hits when we are on a known good base.

	if (!Hit.HitObjectHandle.IsValid())
	{
		return true;
	}

	const AActor* HitActor = Hit.HitObjectHandle.GetManagingActor();
	if (!HitActor->CanBeBaseForCharacter(PawnOwner))
	{
		return false;
	}

	return true;
}

static const FName StepUpSubstepName = "StepUp";
static const FName StepFwdSubstepName = "StepFwd";
static const FName StepDownSubstepName = "StepDown";
static const FName SlideSubstepName = "SlideFromStep";

bool USurfaceWalkingModeUtils::TryMoveToStepUp(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, UMoverComponent* MoverComponent, const FVector& GravDir, float MaxStepHeight, float MaxWalkSlopeCosine, float FloorSweepDistance, const FVector& MoveDelta, const FHitResult& MoveHitResult, const FFloorCheckResult& CurrentFloor, bool bIsFalling, FOptionalFloorCheckResult* OutFloorTestResult, FMovementRecord& MoveRecord)
{
	FVector UpDir = GravDir;

	UCapsuleComponent* CapsuleComponent = Cast<UCapsuleComponent>(UpdatedPrimitive);

	if (CapsuleComponent == nullptr || !CanStepUpOnHitSurface(MoveHitResult) || MaxStepHeight <= 0.f)
	{
		return false;
	}

	TArray<FMovementSubstep> QueuedSubsteps;	// keeping track of substeps before committing, because some moves can be backed out


	const FVector OldLocation = UpdatedPrimitive->GetComponentLocation();
	FVector LastComponentLocation = OldLocation;

	float PawnRadius, PawnHalfHeight;

	CapsuleComponent->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);

	// Don't bother stepping up if top of capsule is hitting something.
	//const float InitialImpactZ = MoveHitResult.ImpactPoint.Z;
	const float InitialImpactHeight = FVector::DotProduct(MoveHitResult.ImpactPoint - OldLocation, UpDir);
	//if (InitialImpactZ > OldLocation.Z + (PawnHalfHeight - PawnRadius))
	if (InitialImpactHeight > PawnHalfHeight - PawnRadius)
	{
		UE_LOG(LogMover, VeryVerbose, TEXT("Not stepping up due to top of capsule hitting something"));
		return false;
	}

	// TODO: We should rely on movement plane normal, rather than gravity direction
	//if (GravDir.IsZero())
	//{
	//	UE_LOG(LogMover, VeryVerbose, TEXT("Not stepping up because there's no gravity"));
	//	return false;
	//}

	// Gravity should be a normalized direction
	ensure(GravDir.IsNormalized());

	float StepTravelUpHeight = MaxStepHeight;
	float StepTravelDownHeight = StepTravelUpHeight;
	const float StepSideZ = -1.f * FVector::DotProduct(MoveHitResult.ImpactNormal, GravDir);
	//float PawnInitialFloorBaseZ = OldLocation.Z - PawnHalfHeight;
	//float PawnFloorPointZ = PawnInitialFloorBaseZ;
	float PawnInitialFloorBaseHeight = FVector::DotProduct(OldLocation, UpDir) - PawnHalfHeight;
    float PawnFloorPointHeight = PawnInitialFloorBaseHeight;

	//if (IsMovingOnGround() && CurrentFloor.IsWalkableFloor())
	if (CurrentFloor.IsWalkableFloor())
	{
		// Since we float a variable amount off the floor, we need to enforce max step height off the actual point of impact with the floor.
		const float FloorDist = FMath::Max(0.f, CurrentFloor.GetDistanceToFloor());
		//PawnInitialFloorBaseZ -= FloorDist;
		PawnInitialFloorBaseHeight -= FloorDist;
		StepTravelUpHeight = FMath::Max(StepTravelUpHeight - FloorDist, 0.f);
		StepTravelDownHeight = (MaxStepHeight + UMoonshotMoverUtils::MAX_FLOOR_DIST * 2.f);

		const bool bHitVerticalFace = !UMoonshotMoverUtils::IsWithinEdgeTolerance(MoveHitResult.Location, MoveHitResult.ImpactPoint, PawnRadius);
		if (!CurrentFloor.bLineTrace && !bHitVerticalFace)
		{
			//PawnFloorPointZ = CurrentFloor.HitResult.ImpactPoint.Z;
			PawnFloorPointHeight = FVector::DotProduct(CurrentFloor.HitResult.ImpactPoint, UpDir);
		}
		else
		{
			// Base floor point is the base of the capsule moved down by how far we are hovering over the surface we are hitting.
			//PawnFloorPointZ -= CurrentFloor.FloorDist;
			PawnFloorPointHeight -= CurrentFloor.FloorDist;
		}
	}

	// Don't step up if the impact is below us, accounting for distance from floor.
	//if (InitialImpactZ <= PawnInitialFloorBaseZ)
	if (InitialImpactHeight <= PawnInitialFloorBaseHeight)
	{
		UE_LOG(LogMover, VeryVerbose, TEXT("Not stepping up because the impact is below us"));
		return false;
	}

	// Scope our movement updates, and do not apply them until all intermediate moves are completed.
	FScopedMovementUpdate ScopedStepUpMovement(UpdatedComponent, EScopedUpdate::DeferredUpdates);

	// step up - treat as vertical wall
	FHitResult SweepUpHit(1.f);
	const FQuat PawnRotation = UpdatedComponent->GetComponentQuat();

	const FVector UpAdjustment = -GravDir * StepTravelUpHeight;
	const bool bDidStepUp = UMovementUtils::TryMoveUpdatedComponent_Internal(UpdatedComponent, UpAdjustment, PawnRotation, true, MOVECOMP_NoFlags, &SweepUpHit, ETeleportType::None);

	UE_LOG(LogMover, VeryVerbose, TEXT("TryMoveToStepUp Up: %s (role %i) UpAdjustment=%s DidMove=%i"),
		*GetNameSafe(UpdatedComponent->GetOwner()), UpdatedComponent->GetOwnerRole(), *UpAdjustment.ToCompactString(), bDidStepUp);

	if (SweepUpHit.bStartPenetrating)
	{
		// Undo movement
		UE_LOG(LogMover, VeryVerbose, TEXT("Reverting step-up attempt because we started in a penetrating state"));
		ScopedStepUpMovement.RevertMove();
		return false;
	}

	// Cache upwards substep
	QueuedSubsteps.Add(FMovementSubstep(StepUpSubstepName, UpdatedPrimitive->GetComponentLocation()-LastComponentLocation, false));
	LastComponentLocation = UpdatedPrimitive->GetComponentLocation();

	// step fwd
	FHitResult StepFwdHit(1.f);
	const bool bDidStepFwd = UMovementUtils::TryMoveUpdatedComponent_Internal(UpdatedComponent, MoveDelta, PawnRotation, true, MOVECOMP_NoFlags, &StepFwdHit, ETeleportType::None);

	UE_LOG(LogMover, VeryVerbose, TEXT("TryMoveToStepUp Fwd: %s (role %i) MoveDelta=%s DidMove=%i"),
		*GetNameSafe(UpdatedComponent->GetOwner()), UpdatedComponent->GetOwnerRole(), *MoveDelta.ToCompactString(), bDidStepFwd);

	// Check result of forward movement
	if (StepFwdHit.bBlockingHit)
	{
		if (StepFwdHit.bStartPenetrating)
		{
			// Undo movement
			UE_LOG(LogMover, VeryVerbose, TEXT("Reverting step-fwd attempt during step-up, because we started in a penetrating state"));
			ScopedStepUpMovement.RevertMove();
			return false;
		}

		// If we hit something above us and also something ahead of us, we should notify about the upward hit as well.
		// The forward hit will be handled later (in the bSteppedOver case below).
		// In the case of hitting something above but not forward, we are not blocked from moving so we don't need the notification.
		if (MoverComponent && SweepUpHit.bBlockingHit && StepFwdHit.bBlockingHit)
		{
			FMoverOnImpactParams ImpactParams(NAME_None, SweepUpHit, MoveDelta);
			MoverComponent->HandleImpact(ImpactParams);
		}

		// pawn ran into a wall
		if (MoverComponent)
		{
			FMoverOnImpactParams ImpactParams(NAME_None, StepFwdHit, MoveDelta);
			MoverComponent->HandleImpact(ImpactParams);
		}
		
		if (bIsFalling)
		{
			QueuedSubsteps.Add( FMovementSubstep(StepFwdSubstepName, UpdatedComponent->GetComponentLocation()-LastComponentLocation, true) );

			// Commit queued substeps to movement record
			for (FMovementSubstep Substep : QueuedSubsteps)
			{
				MoveRecord.Append(Substep);
			}

			return true;
		}

		// Cache forwards substep before the slide attempt
		QueuedSubsteps.Add(FMovementSubstep(StepFwdSubstepName, UpdatedPrimitive->GetComponentLocation() - LastComponentLocation, true));
		LastComponentLocation = UpdatedPrimitive->GetComponentLocation();

		// adjust and try again
		const float ForwardHitTime = StepFwdHit.Time;

		// locking relevancy so velocity isn't added until it is needed to (adding it to the QueuedSubsteps so it can get added later)
		MoveRecord.LockRelevancy(false);
		const float ForwardSlideAmount = TryWalkToSlideAlongSurface(UpdatedComponent, UpdatedPrimitive, MoverComponent, MoveDelta, 1.f - StepFwdHit.Time, PawnRotation, StepFwdHit.Normal, StepFwdHit, true, MoveRecord, MaxWalkSlopeCosine, MaxStepHeight);
		QueuedSubsteps.Add( FMovementSubstep(SlideSubstepName, UpdatedComponent->GetComponentLocation()-LastComponentLocation, true) );
		LastComponentLocation = UpdatedPrimitive->GetComponentLocation();
		MoveRecord.UnlockRelevancy();

		if (bIsFalling)
		{
			UE_LOG(LogMover, VeryVerbose, TEXT("Reverting step-fwd attempt during step-up, because we could not adjust without falling"));
			ScopedStepUpMovement.RevertMove();
			return false;
		}

		// If both the forward hit and the deflection got us nowhere, there is no point in this step up.
		if (ForwardHitTime == 0.f && ForwardSlideAmount == 0.f)
		{
			UE_LOG(LogMover, VeryVerbose, TEXT("Reverting step-fwd attempt during step-up, because no movement differences occurred"));
			ScopedStepUpMovement.RevertMove();
			return false;
		}
	}
	else
	{
		// Our forward move attempt was unobstructed - cache it
		QueuedSubsteps.Add(FMovementSubstep(StepFwdSubstepName, UpdatedPrimitive->GetComponentLocation() - LastComponentLocation, true));
		LastComponentLocation = UpdatedPrimitive->GetComponentLocation();
	}


	// Step down
	const FVector StepDownAdjustment = GravDir * StepTravelDownHeight;
	const bool bDidStepDown = UMovementUtils::TryMoveUpdatedComponent_Internal(UpdatedComponent, StepDownAdjustment, UpdatedComponent->GetComponentQuat(), true, MOVECOMP_NoFlags, &StepFwdHit, ETeleportType::None);

	UE_LOG(LogMover, VeryVerbose, TEXT("TryMoveToStepUp Down: %s (role %i) StepDownAdjustment=%s DidMove=%i"),
		*GetNameSafe(UpdatedComponent->GetOwner()), UpdatedComponent->GetOwnerRole(), *StepDownAdjustment.ToCompactString(), bDidStepDown);


	// If step down was initially penetrating abort the step up
	if (StepFwdHit.bStartPenetrating)
	{
		UE_LOG(LogMover, VeryVerbose, TEXT("Reverting step-down attempt during step-up/step-fwd, because we started in a penetrating state"));
		ScopedStepUpMovement.RevertMove();
		return false;
	}

	FOptionalFloorCheckResult StepDownResult;
	if (StepFwdHit.IsValidBlockingHit())
	{
		// See if this step sequence would have allowed us to travel higher than our max step height allows.
		//const float DeltaZ = StepFwdHit.ImpactPoint.Z - PawnFloorPointZ;
		const float DeltaHeight = FVector::DotProduct(StepFwdHit.ImpactPoint - (PawnFloorPointHeight * UpDir), UpDir);
		//if (DeltaZ > MaxStepHeight)
		if (DeltaHeight > MaxStepHeight)
		{
			UE_LOG(LogMover, VeryVerbose, TEXT("Reject step-down attempt during step-up/step-fwd, because it made us travel too high (too high Height %.3f) up from floor base %f to %f"), DeltaHeight, PawnInitialFloorBaseHeight, FVector::DotProduct(StepFwdHit.ImpactPoint, UpDir));
			ScopedStepUpMovement.RevertMove();
			return false;
		}

		// Reject unwalkable surface normals here.
		if (!UMoonshotMoverUtils::IsHitSurfaceWalkable(StepFwdHit, MaxWalkSlopeCosine, UpdatedComponent))
		{
			// Reject if normal opposes movement direction
			const bool bNormalTowardsMe = (MoveDelta | StepFwdHit.ImpactNormal) < 0.f;
			if (bNormalTowardsMe)
			{
				UE_LOG(LogMover, VeryVerbose, TEXT("Reject step-down attempt during step-up/step-fwd, due to unwalkable normal %s opposed to movement"), *StepFwdHit.ImpactNormal.ToString());
				ScopedStepUpMovement.RevertMove();
				return false;
			}

			// Also reject if we would end up being higher than our starting location by stepping down.
			// It's fine to step down onto an unwalkable normal below us, we will just slide off. Rejecting those moves would prevent us from being able to walk off the edge.
			//if (StepFwdHit.Location.Z > OldLocation.Z)
			if (FVector::DotProduct(StepFwdHit.Location, UpDir) > FVector::DotProduct(OldLocation, UpDir))
			{
				UE_LOG(LogMover, VeryVerbose, TEXT("Reject step-down attempt during step-up/step-fwd, due to unwalkable normal %s above old position)"), *StepFwdHit.ImpactNormal.ToString());
				ScopedStepUpMovement.RevertMove();
				return false;
			}
		}

		// Reject moves where the downward sweep hit something very close to the edge of the capsule. This maintains consistency with FindFloor as well.
		if (!UMoonshotMoverUtils::IsWithinEdgeTolerance(StepFwdHit.Location, StepFwdHit.ImpactPoint, PawnRadius))
		{
			UE_LOG(LogMover, VeryVerbose, TEXT("Reject step-down attempt during step-up/step-fwd, due to being outside edge tolerance"));
			ScopedStepUpMovement.RevertMove();
			return false;
		}

		// Don't step up onto invalid surfaces if traveling higher.
		//if (DeltaZ > 0.f && !CanStepUpOnHitSurface(StepFwdHit))
		if (DeltaHeight > 0.f && !CanStepUpOnHitSurface(StepFwdHit))
		{
			UE_LOG(LogMover, VeryVerbose, TEXT("Reject step-down attempt during step-up/step-fwd, due to being up onto surface with !CanStepUpOnHitSurface")); 
			ScopedStepUpMovement.RevertMove();
			return false;
		}

		// See if we can validate the floor as a result of this step down. In almost all cases this should succeed, and we can avoid computing the floor outside this method.
		if (OutFloorTestResult != NULL)
		{

			UMoonshotMoverUtils::FindFloor(UpdatedComponent, UpdatedPrimitive,
				FloorSweepDistance, MaxWalkSlopeCosine,
				UpdatedComponent->GetComponentLocation(), StepDownResult.FloorTestResult);

			// Reject unwalkable normals if we end up higher than our initial height.
			// It's fine to walk down onto an unwalkable surface, don't reject those moves.
			//if (StepFwdHit.Location.Z > OldLocation.Z)
			if (FVector::DotProduct(StepFwdHit.Location, UpDir) > FVector::DotProduct(OldLocation, UpDir))
			{
				// We should reject the floor result if we are trying to step up an actual step where we are not able to perch (this is rare).
				// In those cases we should instead abort the step up and try to slide along the stair.
				const float MAX_STEP_SIDE_Z = 0.08f; // TODO: Move magic numbers elsewhere
				if (!StepDownResult.FloorTestResult.bBlockingHit && StepSideZ < MAX_STEP_SIDE_Z)
				{
					UE_LOG(LogMover, VeryVerbose, TEXT("Reject step-down attempt during step-up/step-fwd, due to it being an unperchable step")); 
					ScopedStepUpMovement.RevertMove();
					return false;
				}
			}

			StepDownResult.bHasFloorResult = true;
		}
	}

	// Cache downwards substep
	QueuedSubsteps.Add(FMovementSubstep(StepDownSubstepName, UpdatedPrimitive->GetComponentLocation() - LastComponentLocation, false));
	LastComponentLocation = UpdatedPrimitive->GetComponentLocation();

	// Copy step down result.
	if (OutFloorTestResult != NULL)
	{
		*OutFloorTestResult = StepDownResult;
	}

	// Don't recalculate velocity based on this height adjustment, if considering vertical adjustments.
	//bJustTeleported |= !bMaintainHorizontalGroundVelocity;

	// Commit queued substeps to movement record
	for (FMovementSubstep Substep : QueuedSubsteps)
	{
		MoveRecord.Append(Substep);
	}

	return true;

}

float USurfaceWalkingModeUtils::TryWalkToSlideAlongSurface(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, UMoverComponent* MovementComponent, const FVector& Delta, float PctOfDeltaToMove, const FQuat Rotation, const FVector& Normal, FHitResult& Hit, bool bHandleImpact, FMovementRecord& MoveRecord, float MaxWalkSlopeCosine, float MaxStepHeight)
{
	FVector UpDir = UpdatedComponent->GetOwner()->GetActorUpVector();

	if (!Hit.bBlockingHit)
	{
		return 0.f;
	}
	
	FVector SafeWalkNormal(Normal);

	// We don't want to be pushed up an unwalkable surface.
	if (FVector::DotProduct(SafeWalkNormal, UpDir) > 0.f && !UMoonshotMoverUtils::IsHitSurfaceWalkable(Hit, MaxWalkSlopeCosine, UpdatedComponent))
	{
		SafeWalkNormal = FVector::VectorPlaneProject(SafeWalkNormal, UpDir);
	}

	float PctOfTimeUsed = 0.f;
	const FVector OldSafeHitNormal = SafeWalkNormal;

	FVector SlideDelta = UMovementUtils::ComputeSlideDelta(Delta, PctOfDeltaToMove, SafeWalkNormal, Hit);

	if (FVector::DotProduct(SlideDelta, Delta) > 0.f)
	{
		UMovementUtils::TrySafeMoveUpdatedComponent(UpdatedComponent, UpdatedPrimitive, SlideDelta, Rotation, true, Hit, ETeleportType::None, MoveRecord);

		PctOfTimeUsed = Hit.Time;

		if (Hit.IsValidBlockingHit())
		{
			// Notify first impact
			if (MovementComponent && bHandleImpact)
			{
				FMoverOnImpactParams ImpactParams(NAME_None, Hit, SlideDelta);
				MovementComponent->HandleImpact(ImpactParams);
			}

			// Compute new slide normal when hitting multiple surfaces.
			SlideDelta = UMovementUtils::ComputeTwoWallAdjustedDelta(SlideDelta, Hit, OldSafeHitNormal);
			//if (SlideDelta.Z > 0.f && UMoonshotMoverUtils::IsHitSurfaceWalkable(Hit, MaxWalkSlopeCosine, UpdatedComponent) && Hit.Normal.Z > UE_KINDA_SMALL_NUMBER)
			if (FVector::DotProduct(SlideDelta, UpDir) > 0.f && UMoonshotMoverUtils::IsHitSurfaceWalkable(Hit, MaxWalkSlopeCosine, UpdatedComponent) && FVector::DotProduct(Hit.Normal, UpDir) > UE_KINDA_SMALL_NUMBER)
			{
				// Maintain horizontal velocity
				const float Time = (1.f - Hit.Time);
				const FVector ScaledDelta = SlideDelta.GetSafeNormal() * SlideDelta.Size();
				SlideDelta = FVector::VectorPlaneProject(ScaledDelta, UpDir) * Time;
				//SlideDelta = FVector(SlideDelta.X, SlideDelta.Y, ScaledDelta.Z / Hit.Normal.Z) * Time;
				// Should never exceed MaxStepHeight in vertical component, so rescale if necessary.
				// This should be rare (Hit.Normal.Z above would have been very small) but we'd rather lose horizontal velocity than go too high.
				//if (SlideDelta.Z > MaxStepHeight)
				if (FVector::DotProduct(SlideDelta, UpDir) > MaxStepHeight)
				{
					//const float Rescale = MaxStepHeight / SlideDelta.Z;
					const float Rescale = MaxStepHeight / FVector::DotProduct(SlideDelta, UpDir);
					SlideDelta *= Rescale;
				}
			}
			else
			{
				//SlideDelta.Z = 0.f;
				SlideDelta = FVector::VectorPlaneProject(SlideDelta, UpDir);
			}
			
			// Only proceed if the new direction is of significant length and not in reverse of original attempted move.
			if (!SlideDelta.IsNearlyZero(SMALL_MOVE_DISTANCE) && (SlideDelta | Delta) > 0.f)
			{
				// Perform second move
				UMovementUtils::TrySafeMoveUpdatedComponent(UpdatedComponent, UpdatedPrimitive, SlideDelta, Rotation, true, Hit, ETeleportType::None, MoveRecord);
				PctOfTimeUsed += (Hit.Time * (1.f - PctOfTimeUsed));

				// Notify second impact
				if (MovementComponent && bHandleImpact && Hit.bBlockingHit)
				{
					FMoverOnImpactParams ImpactParams(NAME_None, Hit, SlideDelta);
					MovementComponent->HandleImpact(ImpactParams);
				}
			}
		}

		return FMath::Clamp(PctOfTimeUsed, 0.f, 1.f);
	}

	return 0.f;
}

bool USurfaceWalkingModeUtils::TryMoveToAdjustHeightAboveFloor(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, FFloorCheckResult& CurrentFloor, float MaxWalkSlopeCosine, FMovementRecord& MoveRecord)
{
	// If we have a floor check that hasn't hit anything, don't adjust height.
	if (!CurrentFloor.IsWalkableFloor())
	{
        //UE_LOG(LogTemp, Display, TEXT("TryMoveToAdjustHeightAboveFloor: Returning false - No walkable floor."));
		return false;
	}

	float OldFloorDist = CurrentFloor.FloorDist;
	if (CurrentFloor.bLineTrace)
	{
		if (OldFloorDist < UMoonshotMoverUtils::MIN_FLOOR_DIST && CurrentFloor.LineDist >= UMoonshotMoverUtils::MIN_FLOOR_DIST)
		{
            //UE_LOG(LogTemp, Display, TEXT("TryMoveToAdjustHeightAboveFloor: Returning false - OldFloorDist < MIN_FLOOR_DIST"));
			// This would cause us to scale unwalkable walls
			return false;
		}
		// Falling back to a line trace means the sweep was unwalkable (or in penetration). Use the line distance for the vertical adjustment.
		OldFloorDist = CurrentFloor.LineDist;
	}

	// Move up or down to maintain floor height.
	if (OldFloorDist < UMoonshotMoverUtils::MIN_FLOOR_DIST || OldFloorDist > UMoonshotMoverUtils::MAX_FLOOR_DIST)
	{
        //UE_LOG(LogTemp, Display, TEXT("TryMoveToAdjustHeightAboveFloor: Moving to maintain floor height."));
		FHitResult AdjustHit(1.f);
		//const float InitialZ = UpdatedComponent->GetComponentLocation().Z;
        const float InitialZ = FVector::DotProduct(UpdatedPrimitive->GetComponentLocation(), CurrentFloor.HitResult.ImpactNormal);
		const float AvgFloorDist = (UMoonshotMoverUtils::MIN_FLOOR_DIST + UMoonshotMoverUtils::MAX_FLOOR_DIST) * 0.5f;
		const float MoveDist = AvgFloorDist - OldFloorDist;

		MoveRecord.LockRelevancy(false);

		//UMovementUtils::TrySafeMoveUpdatedComponent(UpdatedComponent, UpdatedPrimitive,
		//	FVector(0.f, 0.f, MoveDist), UpdatedComponent->GetComponentQuat(), 
		//	true, AdjustHit, ETeleportType::None, MoveRecord);
        /// TODO: Verify this moves pawn out of floor correctly

        UMovementUtils::TrySafeMoveUpdatedComponent(UpdatedComponent, UpdatedPrimitive,
			MoveDist * CurrentFloor.HitResult.ImpactNormal,
			FQuat::FindBetweenNormals(UpdatedComponent->GetOwner()->GetActorUpVector(), CurrentFloor.HitResult.ImpactNormal) * UpdatedComponent->GetComponentQuat(),
			true, AdjustHit, ETeleportType::None, MoveRecord);

		MoveRecord.UnlockRelevancy();

		if (!AdjustHit.IsValidBlockingHit())
		{
			CurrentFloor.FloorDist += MoveDist;
		}
		else if (MoveDist > 0.f)
		{
			//const float CurrentZ = UpdatedComponent->GetComponentLocation().Z;
            const float CurrentZ = FVector::DotProduct(UpdatedPrimitive->GetComponentLocation(), CurrentFloor.HitResult.ImpactNormal);
			CurrentFloor.FloorDist += CurrentZ - InitialZ;
		}
		else
		{
			checkSlow(MoveDist < 0.f);
			//const float CurrentZ = UpdatedComponent->GetComponentLocation().Z;
            const float CurrentZ = FVector::DotProduct(UpdatedPrimitive->GetComponentLocation(), CurrentFloor.HitResult.ImpactNormal);
			//CurrentFloor.FloorDist = CurrentZ - AdjustHit.Location.Z;
            CurrentFloor.FloorDist = CurrentZ - FVector::DotProduct(AdjustHit.Location, CurrentFloor.HitResult.ImpactNormal);
			if (UMoonshotMoverUtils::IsHitSurfaceWalkable(AdjustHit, MaxWalkSlopeCosine, UpdatedComponent))
			{
				CurrentFloor.SetFromSweep(AdjustHit, CurrentFloor.FloorDist, true);
			}
		}

		return true;
	}
    //UE_LOG(LogTemp, Display, TEXT("TryMoveToAdjustHeightAboveFloor: Returning false - Fallthrough."));
	return false;
}

FLayeredMove_SurfaceWalkingModeJumpImpulse::FLayeredMove_SurfaceWalkingModeJumpImpulse() 
	: UpwardsSpeed(0.f)
{
	DurationMs = 0.f;
	MixMode = EMoveMixMode::OverrideVelocity;
}

bool FLayeredMove_SurfaceWalkingModeJumpImpulse::GenerateMove(const FMoverTickStartData& SimState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove)
{	
	const FMoverDefaultSyncState* SyncState = SimState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	check(SyncState);

	const FVector UpDir = MoverComp->GetOwner()->GetActorUpVector();

	const FVector ImpulseVelocity = UpDir * UpwardsSpeed;

	// Jump impulse overrides vertical velocity while maintaining the rest
	if (MixMode == EMoveMixMode::OverrideVelocity)
	{
		const FVector PriorVelocityWS = SyncState->GetVelocity_WorldSpace();
		const FVector StartingNonUpwardsVelocity = PriorVelocityWS - PriorVelocityWS.ProjectOnToNormal(UpDir);

		OutProposedMove.LinearVelocity = StartingNonUpwardsVelocity + ImpulseVelocity;
	}
	else
	{
		ensureMsgf(false, TEXT("JumpImpulse layered move only supports Override Velocity mix mode and was queued with a different mix mode. Layered move will do nothing."));
		return false;
	}

	return true;
}

FLayeredMoveBase* FLayeredMove_SurfaceWalkingModeJumpImpulse::Clone() const
{
	FLayeredMove_SurfaceWalkingModeJumpImpulse* CopyPtr = new FLayeredMove_SurfaceWalkingModeJumpImpulse(*this);
	return CopyPtr;
}

void FLayeredMove_SurfaceWalkingModeJumpImpulse::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);

	Ar << UpwardsSpeed;
}

UScriptStruct* FLayeredMove_SurfaceWalkingModeJumpImpulse::GetScriptStruct() const
{
	return FLayeredMove_SurfaceWalkingModeJumpImpulse::StaticStruct();
}

FString FLayeredMove_SurfaceWalkingModeJumpImpulse::ToSimpleString() const
{
	return FString::Printf(TEXT("SurfaceWalkingModeJumpImpulse"));
}

void FLayeredMove_SurfaceWalkingModeJumpImpulse::AddReferencedObjects(class FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
}