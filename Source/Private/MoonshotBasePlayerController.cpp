// Copyright 2024 Frazimuth, LLC.

#include "MoonshotBasePlayerController.h"
#include "MoonshotBasePawn.h"
#include "GameFramework/Pawn.h"
#include "Mover/Public/DefaultMovementSet/CharacterMoverComponent.h"
#include "DrawDebugHelpers.h"

void AMoonshotBasePlayerController::BeginPlay()
{
	Super::BeginPlay();

	UpdateRotation(GetWorld()->GetDeltaSeconds());
}

void AMoonshotBasePlayerController::UpdateRotation(float DeltaTime)
{	
	FVector GravityDirection = FVector::DownVector;
	FName MovementModeName = NAME_None;
	AMoonshotBasePawn* PlayerPawn = Cast<AMoonshotBasePawn>(GetPawn());
	if (PlayerPawn)    // TODO: Get rid of this cast?
	{
		if (UCharacterMoverComponent* MoveComp = PlayerPawn->GetMoverComponent())
		{
			MovementModeName = MoveComp->GetMovementModeName();

			/** If the character is in ZeroG, rotate gravity reference frame with the character.
			 * If not, try to find the surface gravity and set it as the gravity reference frame.
			 * If no surface gravity is found, use the character's up vector as the gravity
			 * reference frame (TODO: The character should be changing movement modes if no surface
			 * gravity is found).
			 * (TODO: We should really be slerping the gravity direction from one frame to the next?)
			 */
			if (MovementModeName == "ZeroG" || !PlayerPawn->FindSurfaceGravity(GravityDirection))
			{
				GravityDirection = -PlayerPawn->GetActorUpVector();
			}
		}
	}

	// Get the current control rotation in world space
	FRotator ViewRotation = GetControlRotation();
	
	// This is necessary for the camera to rotate with the character along changing surfaces.
	if (!LastFrameGravity.Equals(FVector::ZeroVector) && MovementModeName != "ZeroG")
	{
		const FQuat DeltaGravityRotation = FQuat::FindBetweenNormals(LastFrameGravity, GravityDirection);
		const FQuat WarpedCameraRotation = DeltaGravityRotation * FQuat(ViewRotation);
	
		ViewRotation = WarpedCameraRotation.Rotator();
	}
	
	LastFrameGravity = GravityDirection;

	// Convert the view rotation from world space to gravity relative space.
	// Now we can work with the rotation as if no custom gravity was affecting it.
	//ViewRotation = GetGravityRelativeRotation(ViewRotation, GravityDirection);
	if (!GravityDirection.Equals(FVector::DownVector))
	{
		ViewRotation = (FQuat::FindBetweenNormals(GravityDirection, FVector::DownVector) * ViewRotation.Quaternion()).Rotator();
	}

	// Calculate Delta to be applied on ViewRotation
	FRotator DeltaRot(RotationInput);
 
	if (PlayerCameraManager)
	{
		PlayerCameraManager->ProcessViewRotation(DeltaTime, ViewRotation, DeltaRot);

		// Zero the roll of the camera as we always want it horizontal in relation to the gravity.
		ViewRotation.Roll = 0;

		// Convert the rotation back to world space, and set it as the current control rotation.
		//FRotator NewControlRotation = GetGravityWorldRotation(ViewRotation, GravityDirection);
		FRotator NewControlRotation = ViewRotation;
		if (!GravityDirection.Equals(FVector::DownVector))
		{
			NewControlRotation = (FQuat::FindBetweenNormals(FVector::DownVector, GravityDirection) * ViewRotation.Quaternion()).Rotator();
		}
	
		SetControlRotation(NewControlRotation);
	}

	APawn* const P = GetPawnOrSpectator();
	if (P)
	{
		P->FaceRotation(ViewRotation, DeltaTime);
	}
	
}


FRotator AMoonshotBasePlayerController::GetGravityRelativeRotation(FRotator Rotation, FVector GravityDirection)
{
	if (!GravityDirection.Equals(FVector::DownVector))
	{
		FQuat GravityRotation = FQuat::FindBetweenNormals(GravityDirection, FVector::DownVector);
		return (GravityRotation * Rotation.Quaternion()).Rotator();
	}

	return Rotation;
}

FRotator AMoonshotBasePlayerController::GetGravityWorldRotation(FRotator Rotation, FVector GravityDirection)
{
	if (!GravityDirection.Equals(FVector::DownVector))
	{
		FQuat GravityRotation = FQuat::FindBetweenNormals(FVector::DownVector, GravityDirection);
		return (GravityRotation * Rotation.Quaternion()).Rotator();
	}

	return Rotation;
}