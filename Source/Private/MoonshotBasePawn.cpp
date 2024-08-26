// Copyright 2024 Frazimuth, LLC.


#include "MoonshotBasePawn.h"
#include "MoonshotMover/Public/MoonshotMoverDataModelTypes.h"
#include "MoonshotBasePlayerController.h"
#include "Components/InputComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/SpringArmComponent.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "MoveLibrary/BasedMovementUtils.h"
#include "MoverExamples/Public/CharacterVariants/AbilityInputs.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/PhysicsVolume.h"
#include "EnhancedInputComponent.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "InputAction.h"

static const FName Name_CharacterMotionComponent(TEXT("MoverComponent"));

AMoonshotBasePawn::AMoonshotBasePawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CharacterMotionComponent = CreateDefaultSubobject<UCharacterMoverComponent>(Name_CharacterMotionComponent);
	ensure(CharacterMotionComponent);

	PrimaryActorTick.bCanEverTick = true;

	SetReplicatingMovement(false);	// disable Actor-level movement replication, since our Mover component will handle it

	auto IsImplementedInBlueprint = [](const UFunction* Func) -> bool
	{
		return Func && ensure(Func->GetOuter())
			&& Func->GetOuter()->IsA(UBlueprintGeneratedClass::StaticClass());
	};

	static FName ProduceInputBPFuncName = FName(TEXT("OnProduceInputInBlueprint"));
	UFunction* ProduceInputFunction = GetClass()->FindFunctionByName(ProduceInputBPFuncName);
	bHasProduceInputinBpFunc = IsImplementedInBlueprint(ProduceInputFunction);

}

void AMoonshotBasePawn::ResetCounter()
{
	UE_LOG(LogTemp, Warning, TEXT("MoonshotBasePawn::ResetCounter() Last Second Count: %d"), DebugTimerCount);
	DebugTimerCount = 0;

}

void AMoonshotBasePawn::IncrementCallCounter()
{
	DebugTimerCount++;
}


// Called every frame
void AMoonshotBasePawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	/// TODO: Replace this with a proper floor trace
    FHitResult HitResult;
    bCanAttachToSurface = GetWorld()->LineTraceSingleByChannel(
        HitResult,
        GetActorLocation(),
        GetActorLocation() - GetActorUpVector() * MaxAttachDistance,
        ECollisionChannel::ECC_Visibility,
        GetTraceIgnoreParams()
        );
    AttachSurfaceNormal = HitResult.Normal;

	if (!IsFlyingActive())
	{
		if (!bCanAttachToSurface)
		{
			bShouldToggleFlying = true;
		}
	}

	UpdatePerspective(DeltaTime);

	FVector forward, right, up;
	GetGravitySystemAxes(forward, right, up);
}

void AMoonshotBasePawn::BeginPlay()
{
	Super::BeginPlay();

	CameraBoom = GetComponentByClass<USpringArmComponent>();
	ensure(CameraBoom);

	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		Controller->SetControlRotation(GetActorRotation());
	}

	if (CameraBoom)
	{
		CameraBoom->bUsePawnControlRotation = true;
		CameraBoom->bInheritRoll = true;
		CameraBoom->bInheritYaw= true;
		CameraBoom->bInheritPitch = true;
	}

	LookScaleToUse = LookInputScale;

	/// TODO: Remove debug timer
	//GetWorld()->GetTimerManager().SetTimer(TimerHandle_Debug, this, &AMoonshotBasePawn::ResetCounter, 1.0f, true);
}

// Called to bind functionality to input
void AMoonshotBasePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Setup some bindings - we are currently using Enhanced Input and just using some input actions assigned in editor for simplicity
	Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (Input)
	{	
		Input->BindAction(MoveInputAction, ETriggerEvent::Triggered, this, &AMoonshotBasePawn::OnMoveTriggered);
		Input->BindAction(MoveInputAction, ETriggerEvent::Completed, this, &AMoonshotBasePawn::OnMoveCompleted);
        Input->BindAction(MoveUpInputAction, ETriggerEvent::Triggered, this, &AMoonshotBasePawn::OnMoveUpTriggered);
        Input->BindAction(MoveUpInputAction, ETriggerEvent::Completed, this, &AMoonshotBasePawn::OnMoveUpCompleted);
		Input->BindAction(LookInputAction, ETriggerEvent::Triggered, this, &AMoonshotBasePawn::OnLookTriggered);
		Input->BindAction(LookInputAction, ETriggerEvent::Completed, this, &AMoonshotBasePawn::OnLookCompleted);
        Input->BindAction(LookGamepadInputAction, ETriggerEvent::Started, this, &AMoonshotBasePawn::OnGamepadLookStarted);
        Input->BindAction(LookGamepadInputAction, ETriggerEvent::Triggered, this, &AMoonshotBasePawn::OnGamepadLookTriggered);
        Input->BindAction(LookGamepadInputAction, ETriggerEvent::Completed, this, &AMoonshotBasePawn::OnGamepadLookCompleted);
        Input->BindAction(RollInputAction, ETriggerEvent::Triggered, this, &AMoonshotBasePawn::OnRollTriggered);
        Input->BindAction(RollInputAction, ETriggerEvent::Completed, this, &AMoonshotBasePawn::OnRollCompleted);
		Input->BindAction(JumpInputAction, ETriggerEvent::Started, this, &AMoonshotBasePawn::OnJumpStarted);
		Input->BindAction(JumpInputAction, ETriggerEvent::Completed, this, &AMoonshotBasePawn::OnJumpReleased);
		Input->BindAction(FlyInputAction, ETriggerEvent::Triggered, this, &AMoonshotBasePawn::OnFlyTriggered);
        Input->BindAction(ModifyControlInputAction, ETriggerEvent::Triggered, this, &AMoonshotBasePawn::OnModifyControlTriggered);
        Input->BindAction(ModifyControlInputAction, ETriggerEvent::Completed, this, &AMoonshotBasePawn::OnModifyControlCompleted);
		Input->BindAction(ModifySelectInputAction, ETriggerEvent::Triggered, this, &AMoonshotBasePawn::OnModifySelectTriggered);
        Input->BindAction(ModifySelectInputAction, ETriggerEvent::Completed, this, &AMoonshotBasePawn::OnModifySelectCompleted);
	}
}

bool AMoonshotBasePawn::FindSurfaceGravity(FVector& OutUnitGravity) const
{
	if (bCanAttachToSurface)
	{
		OutUnitGravity = -AttachSurfaceNormal;
		return true;
	}

	return false;
}

bool AMoonshotBasePawn::GetGravitySystemAxes(FVector& OutForward, FVector& OutRight, FVector& OutUp) const
{
	FMatrix GravMatrix = FRotationMatrix::Make(GravityQuat);

	OutForward 	= GravMatrix.GetUnitAxis(EAxis::X);
	OutRight 	= GravMatrix.GetUnitAxis(EAxis::Y);
	OutUp 		= GravMatrix.GetUnitAxis(EAxis::Z);

	return bCanAttachToSurface;
}

bool AMoonshotBasePawn::IsFlyingActive() const
{
	return GetMoverComponent()->GetMovementModeName() == "ZeroG";
}

void AMoonshotBasePawn::ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult)
{
	OnProduceInput((float)SimTimeMs, InputCmdResult);

	if (bHasProduceInputinBpFunc)
	{
		InputCmdResult = OnProduceInputInBlueprint((float)SimTimeMs, InputCmdResult);
	}
}

/** Generate user commands to be fed into the Mover simulation this tick. 
 * - Does not run on server or non-controlling clients.
 * - Is not rerun during reconcile/resimulates.
*/
void AMoonshotBasePawn::OnProduceInput(float DeltaMs, FMoverInputCmdContext& OutInputCmd)
{
    FMoonshotMoverCharacterInputs& CharacterInputs = OutInputCmd.InputCollection.FindOrAddMutableDataByType<FMoonshotMoverCharacterInputs>();

	if (Controller == nullptr)
	{
		if (GetLocalRole() == ENetRole::ROLE_Authority && GetRemoteRole() == ENetRole::ROLE_SimulatedProxy)
		{
            static const FMoonshotMoverCharacterInputs DoNothingInput;
			CharacterInputs = DoNothingInput;
		}

		// Simulated proxies will just use previous input when extrapolating.
		return;
	}

	USpringArmComponent* Boom = GetCameraBoom();
	APlayerController* PC = Cast<APlayerController>(Controller);
	const UCharacterMoverComponent* MoverComp = GetMoverComponent();

	if (!Boom || !PC || !MoverComp) return;

	CharacterInputs.ControlRotation = PC->GetControlRotation();

	//FQuat ControlRotationQuat = PC->GetControlRotation().Quaternion();
	FQuat ControlRotationQuat = Boom->GetRelativeRotation().Quaternion();

	FVector FinalDirectionalIntent = ControlRotationQuat.RotateVector(CachedMoveInputIntent);
	if (bCanAttachToSurface && (MoverComp->IsFalling() || MoverComp->IsOnGround()))
	{
		FinalDirectionalIntent = FVector::VectorPlaneProject(FinalDirectionalIntent, AttachSurfaceNormal).GetSafeNormal();
	}

	CharacterInputs.SetMoveInput(EMoveInputType::DirectionalIntent, FinalDirectionalIntent);

	static float RotationMagMin(1e-3);

	const bool bHasAffirmativeMoveInput = (CharacterInputs.GetMoveInput().Size() >= RotationMagMin);
	
	// Figure out intended orientation
	CharacterInputs.OrientationIntent = FVector::ZeroVector;
	CharacterInputs.AngularVelocity.Pitch = 0.0f;
	CharacterInputs.AngularVelocity.Yaw = 0.0f;
	float DeltaSeconds = 0.001f * DeltaMs;

	//FVector ControlForward = PC->GetControlRotation().Vector();
	FVector ControlForward = Boom->GetRelativeRotation().Vector().GetSafeNormal();

	//** ZeroG Orientation */
    if (IsFlyingActive())
    {
		/// TODO: We're also applying dampening in the ZeroG movement mode, aren't we?
		/// TODO: Dampen should have a dedicated input, probably
		ZeroGCachedAngularVelocity += CachedTurnInput;

		ZeroGCachedAngularVelocity.Roll = FMath::Clamp(
				ZeroGCachedAngularVelocity.Roll,
				-ZeroGMaxAngularVelocity.Roll,
				ZeroGMaxAngularVelocity.Roll
				);
		
        ZeroGCachedAngularVelocity.Roll     *= (1.0f - ZeroGAngularVelocityDecay.Roll - (bIsJumpPressed ? ZeroGAngularDampeningScale.Roll : 0.0f));
        ZeroGCachedAngularVelocity.Pitch    *= (1.0f - ZeroGAngularVelocityDecay.Pitch - (bIsJumpPressed ? ZeroGAngularDampeningScale.Pitch : 0.0f));
        ZeroGCachedAngularVelocity.Yaw      *= (1.0f - ZeroGAngularVelocityDecay.Yaw - (bIsJumpPressed ? ZeroGAngularDampeningScale.Yaw : 0.0f));

        if (bCanAttachToSurface && GEngine)
        {
			/// TODO: Make this a widget
			GEngine->AddOnScreenDebugMessage(1, 0.f, FColor::Green, TEXT("Can attach to surface!"));
        }

//		if (bHasAffirmativeMoveInput)
		{
			CharacterInputs.AngularVelocity.Roll = ZeroGCachedAngularVelocity.Roll;
			//FQuat DeltaQuat = GetActorTransform().GetRotation().Inverse() * PC->GetControlRotation().Quaternion();
			FQuat DeltaQuat = GetActorTransform().GetRotation().Inverse() * Boom->GetRelativeRotation().Quaternion();;

			FQuat TargetQuat = FQuat::Slerp(FQuat::Identity, DeltaQuat, 0.5f * DeltaSeconds);
			TargetQuat.Normalize();

			CharacterInputs.AngularVelocity.Pitch = TargetQuat.Rotator().Pitch;
			CharacterInputs.AngularVelocity.Yaw = TargetQuat.Rotator().Yaw;
			CharacterInputs.AngularVelocity.Roll = ZeroGCachedAngularVelocity.Roll;

			FRotator BoomRot = Boom->GetRelativeRotation();
			BoomRot.Roll += ZeroGCachedAngularVelocity.Roll;
			//Boom->SetRelativeRotation(BoomRot);

		}
	//	else
		{

		}
    }
    else if (bCanAttachToSurface)
    {
		//** Surface Orientation - We have a valid surface and we're not in ZeroG*/
		ControlForward = FVector::VectorPlaneProject(ControlForward, AttachSurfaceNormal).GetSafeNormal();
		
		if (!bOrientRotationToMovement)
		{
			// Orient pawn with aim direction
			//CharacterInputs.OrientationIntent = FMath::Lerp(GetActorForwardVector(), ControlForward, 1.0f);
			FQuat DeltaQuat = FQuat(FQuat::FindBetweenNormals(GetActorForwardVector(), ControlForward));
			DeltaQuat.Normalize();
			CharacterInputs.OrientationIntent = DeltaQuat.RotateVector(GetActorForwardVector());

			/// We keep trying ... 
			//SetActorRotation(DeltaQuat * GetActorTransform().GetRotation());
		}
		else if (bHasAffirmativeMoveInput)
		{
			// Orient pawn with move direction
			//ControlForward = PC->GetControlRotation().Quaternion().RotateVector(CachedMoveInputIntent);
			ControlForward = Boom->GetRelativeRotation().Quaternion().RotateVector(CachedMoveInputIntent);
			CharacterInputs.OrientationIntent = FMath::Lerp(GetActorForwardVector(), FVector::VectorPlaneProject(ControlForward, AttachSurfaceNormal).GetSafeNormal(), 1.0f);
		}
	}
	
	CharacterInputs.bIsJumpPressed = bIsJumpPressed;
	CharacterInputs.bIsJumpJustPressed = bIsJumpJustPressed;

	if (bShouldToggleFlying)
	{
		if (!bIsFlyingActive)
		{
            CharacterInputs.SuggestedMovementMode = FName("ZeroG");
            CachedMoveInputIntent = FVector::ZeroVector;
			ZeroGCachedAngularVelocity = FRotator::ZeroRotator;
		}
		else
		{
            if (bCanAttachToSurface)
            {
			    CharacterInputs.SuggestedMovementMode = DefaultModeNames::Falling;
            }
		}

		bIsFlyingActive = !bIsFlyingActive;
	}
	else
	{
		CharacterInputs.SuggestedMovementMode = NAME_None;
	}

	// Convert inputs to be relative to the current movement base (depending on options and state)
	CharacterInputs.bUsingMovementBase = false;

	if (bUseBaseRelativeMovement)
	{
		if (UPrimitiveComponent* MovementBase = MoverComp->GetMovementBase())
		{
			FName MovementBaseBoneName = MoverComp->GetMovementBaseBoneName();

			FVector RelativeMoveInput, RelativeOrientDir;

			UBasedMovementUtils::TransformWorldDirectionToBased(MovementBase, MovementBaseBoneName, CharacterInputs.GetMoveInput(), RelativeMoveInput);
			UBasedMovementUtils::TransformWorldDirectionToBased(MovementBase, MovementBaseBoneName, CharacterInputs.OrientationIntent, RelativeOrientDir);

			CharacterInputs.SetMoveInput(CharacterInputs.GetMoveInputType(), RelativeMoveInput);
			CharacterInputs.OrientationIntent = RelativeOrientDir;

			CharacterInputs.bUsingMovementBase = true;
			CharacterInputs.MovementBase = MovementBase;
			CharacterInputs.MovementBaseBoneName = MovementBaseBoneName;
		}
	}

	// Clear/consume temporal movement inputs. We are not consuming others in the event that the game world is ticking at a lower rate than the Mover simulation. 
	// In that case, we want most input to carry over between simulation frames.
	{

		bIsJumpJustPressed = false;
		bShouldToggleFlying = false;
	}
}

/** Checks for changes in gravity reference frame and applies them
 * to the camera and control rotation.
*/
void AMoonshotBasePawn::UpdatePerspective(float DeltaSeconds)
{
	USpringArmComponent* Boom = GetCameraBoom();
	UMoverComponent* Mover = GetMoverComponent();
	APlayerController* PC = Cast<APlayerController>(Controller);
	if (!Boom || !Mover || !PC) return;

	GravityQuat = GetActorTransform().GetRotation();
	
	/// TODO: This was a clever, stupid idea
	// Get the new gravity reference frame
//	FVector Forward, Right, Up;
//    if (IsFlyingActive() || !bCanAttachToSurface)
//    {
//		Forward = PC->GetControlRotation().Vector().GetSafeNormal();
//		Right = FVector::CrossProduct(GetActorUpVector(), Forward).GetSafeNormal();
//		Up = FVector::CrossProduct(Forward, Right).GetSafeNormal();
//	}
//	else
//	{
//		Forward = FVector::VectorPlaneProject(PC->GetControlRotation().Vector(), AttachSurfaceNormal).GetSafeNormal();
//		Right = FVector::CrossProduct(AttachSurfaceNormal, Forward).GetSafeNormal();
//		Up = FVector::CrossProduct(Forward, Right).GetSafeNormal();
//    }
//	FQuat PawnQuat = FQuat(FMatrix(Forward, Right, Up, FVector::ZeroVector));
//	PawnQuat.Normalize();
//
//
//	SetActorRotation(
//		FQuat::Slerp(GetActorTransform().GetrRotation(), PawnQuat, 0.5f)
//		);
//
//	DrawDebugLine(GetWorld(), GetActorLocation(), GetActorLocation() + Forward * 200, FColor::Red, false, 0.1f);
//	DrawDebugLine(GetWorld(), GetActorLocation(), GetActorLocation() + Right * 200, FColor::Green, false, 0.1f);
//	DrawDebugLine(GetWorld(), GetActorLocation(), GetActorLocation() + Up * 200, FColor::Blue, false, 0.1f);
}

void AMoonshotBasePawn::OnMoveTriggered(const FInputActionValue& Value)
{
	const FVector MovementVector = Value.Get<FVector>();
	CachedMoveInputIntent.X = FMath::Clamp(ZeroGMoveInputScale.X * MovementVector.X, -1.0f, 1.0f);
	CachedMoveInputIntent.Y = FMath::Clamp(ZeroGMoveInputScale.Y * MovementVector.Y, -1.0f, 1.0f);
}

void AMoonshotBasePawn::OnMoveCompleted(const FInputActionValue& Value)
{
	CachedMoveInputIntent.X = 0.0f;
    CachedMoveInputIntent.Y = 0.0f;
}

void AMoonshotBasePawn::OnMoveUpTriggered(const FInputActionValue& Value)
{
    const float MoveUpValue = Value.Get<float>();
    CachedMoveInputIntent.Z = FMath::Clamp(ZeroGMoveInputScale.Z * MoveUpValue, -1.0f, 1.0f);
}

void AMoonshotBasePawn::OnMoveUpCompleted(const FInputActionValue& Value)
{
    CachedMoveInputIntent.Z = 0.0f;
}

void AMoonshotBasePawn::OnLookTriggered(const FInputActionValue& Value)
{
	USpringArmComponent* Boom = Cast<USpringArmComponent>(CameraBoom);
	APlayerController* PC = Cast<APlayerController>(Controller);

	if (!Boom || !PC) return;

	const FVector2D LookVector = Value.Get<FVector2D>();

	CachedLookInput.Yaw = 	FMath::Clamp(LookVector.X, -1.0f, 1.0f);
	CachedLookInput.Pitch = FMath::Clamp(LookVector.Y, -1.0f, 1.0f);
 
	PC->AddYawInput(CachedLookInput.Yaw * LookScaleToUse.Yaw * LookRateMaxDpS * GetWorld()->GetDeltaSeconds());
	PC->AddPitchInput(-CachedLookInput.Pitch * LookScaleToUse.Pitch * LookRateMaxDpS * GetWorld()->GetDeltaSeconds());

	FRotator BoomRot = PC->GetControlRotation();
	Boom->SetRelativeRotation(BoomRot);
	
}

void AMoonshotBasePawn::OnLookCompleted(const FInputActionValue& Value)
{
	CachedLookInput.Pitch = CachedLookInput.Pitch = 0.0f;
}

void AMoonshotBasePawn::OnRollTriggered(const FInputActionValue& Value)
{
    const float RollValue = Value.Get<float>();
    CachedTurnInput.Roll = ZeroGAngularVelocityScale.Roll * FMath::Clamp(RollValue, -1.0f, 1.0f);
}

void AMoonshotBasePawn::OnRollCompleted(const FInputActionValue& Value)
{
    CachedTurnInput.Roll = 0.0f;
}

void AMoonshotBasePawn::OnGamepadLookStarted(const FInputActionValue& Value)
{
}

void AMoonshotBasePawn::OnGamepadLookTriggered(const FInputActionValue& Value)
{
    USpringArmComponent* Boom = Cast<USpringArmComponent>(CameraBoom);
	APlayerController* PC = Cast<APlayerController>(Controller);

	if (!Boom || !PC) return;

	const FVector2D LookVector = Value.Get<FVector2D>();

	CachedLookInput.Yaw = 	FMath::Clamp(LookVector.X, -1.0f, 1.0f);
	CachedLookInput.Pitch = FMath::Clamp(LookVector.Y, -1.0f, 1.0f);
 
	PC->AddYawInput(CachedLookInput.Yaw * LookScaleToUse.Yaw * LookRateMaxDpS * GetWorld()->GetDeltaSeconds());
	PC->AddPitchInput(-CachedLookInput.Pitch * LookScaleToUse.Pitch * LookRateMaxDpS * GetWorld()->GetDeltaSeconds());

	FRotator BoomRot = PC->GetControlRotation();
	Boom->SetRelativeRotation(BoomRot);
}

void AMoonshotBasePawn::OnGamepadLookCompleted(const FInputActionValue& Value)
{
	CachedLookInput.Pitch = CachedLookInput.Pitch = 0.0f;
}

void AMoonshotBasePawn::OnJumpStarted(const FInputActionValue& Value)
{
	if (GetMoverComponent()->IsFalling())
	{
		bShouldToggleFlying = true;
		return;
	}

	bIsJumpJustPressed = !bIsJumpPressed;
	bIsJumpPressed = true;
}

void AMoonshotBasePawn::OnJumpReleased(const FInputActionValue& Value)
{
	bIsJumpPressed = false;
	bIsJumpJustPressed = false;
}

void AMoonshotBasePawn::OnFlyTriggered(const FInputActionValue& Value)
{
	bShouldToggleFlying = true;
}

void AMoonshotBasePawn::OnModifyControlTriggered(const FInputActionValue& Value)
{
    bModifyControl = true;
}

void AMoonshotBasePawn::OnModifyControlCompleted(const FInputActionValue& Value)
{
    bModifyControl = false;
}

void AMoonshotBasePawn::OnModifySelectTriggered(const FInputActionValue& Value)
{
	UE_LOG(LogTemp, Display, TEXT("Modify select triggered"));
    bModifySelect = true;
}

void AMoonshotBasePawn::OnModifySelectCompleted(const FInputActionValue& Value)
{
	UE_LOG(LogTemp, Display, TEXT("Modify select complete"));
    bModifySelect = false;
}

FCollisionQueryParams AMoonshotBasePawn::GetTraceIgnoreParams() const
{
    FCollisionQueryParams Params;

	TArray<AActor*> PawnChildren;
	GetAllChildActors(PawnChildren);
	Params.AddIgnoredActors(PawnChildren);
	Params.AddIgnoredActor(this);

	return Params;
}