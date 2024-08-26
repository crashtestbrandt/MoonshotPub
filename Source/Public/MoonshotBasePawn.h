// Copyright 2024 Frazimuth, LLC.

#pragma once

#include "CoreMinimal.h"
#include "Mover/Public/MoverSimulationTypes.h"
#include "GameFramework/Pawn.h"
#include "Engine/EngineTypes.h"
#include "EnhancedInput/Public/EnhancedInputComponent.h"
#include "MoonshotBasePawn.generated.h"

class UInputAction;
class UCharacterMoverComponent;
class USpringArmComponent;
struct FInputActionValue;

UCLASS()
class MOONSHOT_API AMoonshotBasePawn : public APawn, public IMoverInputProducerInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	AMoonshotBasePawn(const FObjectInitializer& ObjectInitializer);


public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual void BeginPlay() override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// Accessor for the actor's movement component
	UFUNCTION(BlueprintPure, Category = Mover)
	UCharacterMoverComponent* GetMoverComponent() const { return CharacterMotionComponent; }

	UFUNCTION(BlueprintPure, Category=Camera)
	USpringArmComponent* GetCameraBoom() const { return CameraBoom; };

	// Request the character starts moving with an intended directional magnitude. A length of 1 indicates maximum acceleration.
	UFUNCTION(BlueprintCallable, Category=Movement)
	virtual void RequestMoveByIntent(const FVector& DesiredIntent) { CachedMoveInputIntent = DesiredIntent; }

	// Request the character starts moving with a desired velocity. This will be used in lieu of any other input.
	UFUNCTION(BlueprintCallable, Category=Movement)
	virtual void RequestMoveByVelocity(const FVector& DesiredVelocity) { CachedMoveInputVelocity=DesiredVelocity; }

	UFUNCTION(BlueprintPure, Category=Input)
	bool UseModifiedControls() const { return bModifyControl; }

	UFUNCTION(BlueprintPure, Category=Input)
	bool UseModifiedSelect() const { return bModifySelect; }

	UFUNCTION(BlueprintCallable, Category=Movement)
	bool FindSurfaceGravity(FVector& OutUnitGravity) const;

	UFUNCTION(BlueprintCallable, Category=Gravity)
	bool GetGravitySystemAxes(FVector& OutForward, FVector& OutRight, FVector& OutUp) const;

	UFUNCTION(BlueprintCallable, Category=Gravity)
	FQuat GetGravityQuat() const { return GravityQuat; }



	UFUNCTION(BlueprintCallable, Category=Movement)
	bool IsFlyingActive() const;

	//UFUNCTION(BlueprintCallable, Category=Collision)
	FCollisionQueryParams GetTraceIgnoreParams() const;

	/// TODO: Remove debug timer
	FTimerHandle TimerHandle_Debug;
	int32 DebugTimerCount = 0;

	void ResetCounter();
	void IncrementCallCounter();

	/// TODO: Remove debug shit
	FVector CumulativeDebugIntent = FVector(0.0f, 1.0f, 0.0f);

protected:
	// Entry point for input production. Do not override. To extend in derived character types, override OnProduceInput for native types or implement "Produce Input" blueprint event
	virtual void ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult) override;

	// Override this function in native class to author input for the next simulation frame. Consider also calling Super method.
	virtual void OnProduceInput(float DeltaMs, FMoverInputCmdContext& InputCmdResult);

	// Implement this event in Blueprints to author input for the next simulation frame. Consider also calling Parent event.
	UFUNCTION(BlueprintImplementableEvent, DisplayName="On Produce Input", meta = (ScriptName = "OnProduceInput"))
	FMoverInputCmdContext OnProduceInputInBlueprint(float DeltaMs, FMoverInputCmdContext InputCmd);

	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input)
	UInputAction* MoveInputAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input)
	UInputAction* MoveUpInputAction;
   
	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input)
	UInputAction* LookInputAction;

	/** Look Input Action (Required for ignoring modified input)*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input)
	UInputAction* LookGamepadInputAction;

	/** Roll Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input)
	UInputAction* RollInputAction;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input)
	UInputAction* JumpInputAction;

	/** Fly Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input)
	UInputAction* FlyInputAction;

	/** Alt Movement Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input)
	UInputAction* ModifyControlInputAction;

	/** Alt Selection Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input)
	UInputAction* ModifySelectInputAction;

	/** Default gravity magnitude to use when attached to surfaces */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Movement)
	float DefaultGravityMagnitude = 980.0f;

public:
	// Whether or not we author our movement inputs relative to whatever base we're standing on, or leave them in world space
	UPROPERTY(BlueprintReadWrite, Category=Movement)
	bool bUseBaseRelativeMovement = true;
	
	/**
	 * If true, rotate the Character toward the direction the actor is moving
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Movement)
	bool bOrientRotationToMovement = true;
	
	/**
	 * If true, the actor will remain vertical despite any rotation applied to the actor
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Movement)
	bool bShouldRemainVertical = false;

	/**
	 * If true, the actor will continue orienting towards the last intended orientation (from input) even after movement intent input has ceased.
	 * This makes the character finish orienting after a quick stick flick from the player.  If false, character will not turn without input.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Movement)
	bool bMaintainLastInputOrientation = true;

	/**
	 * Maximum angle between control rotation and control forward before character orientation
	 * adjusts to reduce it (ZeroG).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Movement)
	float ZeroGDeadZoneHalfAngle = 20.0f;

	/**
	 * Maximum angle between control rotation and control forward before character orientation
	 * adjusts to reduce it (Surface).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Movement)
	float SurfaceDeadZoneHalfAngle = 85.0f;



	/**
	 * This is used to adjust the sensitivity of the look input.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Input)
	FRotator LookInputScale = FRotator(0.5f, 0.5f, 0.5f);

	/**
	 * This is used to scale the sensitivity of the look input when aiming.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Input)
	FRotator AimInputScale = FRotator(0.2f, 0.2f, 0.2f);

	/**
	 * This is the look input scale currently in use.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Input)
	FRotator LookScaleToUse = FRotator::ZeroRotator;

	/** Max deg/s for look input. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Movement)
	float LookRateMaxDpS = 150.0f;

	/** Decay rate for ZeroG rotation when brake is pressed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Movement)
	FRotator ZeroGAngularDampeningScale = FRotator(0.025f, 0.0f, 0.0f);

protected:
	UPROPERTY(Category = Movement, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCharacterMoverComponent> CharacterMotionComponent;

	UPROPERTY(Category = Input, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	UEnhancedInputComponent* Input;

	UPROPERTY(Category = Camera, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    USpringArmComponent* CameraBoom;

private:
	/** Checks for changes in gravity reference frame and applies them
	 * to the camera and control rotation.
	*/
	void UpdatePerspective(float DeltaSeconds);

	FQuat GravityQuat = FQuat::Identity;
	FVector LastGravityUp = FVector::UpVector;

	FVector LastAffirmativeMoveInput = FVector::ZeroVector;	// Movement input (intent or velocity) the last time we had one that wasn't zero

	FVector CachedMoveInputIntent = FVector::ZeroVector;
	FVector CachedMoveInputVelocity = FVector::ZeroVector;

	FRotator CachedTurnInput = FRotator::ZeroRotator;
	FRotator CachedLookInput = FRotator::ZeroRotator;

	float MaxAttachDistance = 3000.0f; /// TODO: Populate this from settings
	bool bCanAttachToSurface = false;
	FVector AttachSurfaceNormal = FVector::ZeroVector;

	bool bIsJumpJustPressed = false;
	bool bIsJumpPressed = false;
	bool bIsFlyingActive = false;
	bool bShouldToggleFlying = false;
	bool bModifyControl = false;
	bool bModifySelect = false;

	// ZeroG
	FVector ZeroGMoveInputScale = FVector(1.0f, 1.0f, 1.0f);
	FRotator ZeroGCachedAngularVelocity = FRotator::ZeroRotator;
	FRotator ZeroGMaxAngularVelocity = FRotator(180.0f, 180.0f, 180.0f);
	FRotator ZeroGAngularVelocityScale = FRotator(1.0f, 1.0f, 0.2f); 
	FRotator ZeroGAngularVelocityDecay = FRotator(0.0f, 0.0f, 0.0f);

	void OnMoveTriggered(const FInputActionValue& Value);
	void OnMoveCompleted(const FInputActionValue& Value);
	void OnMoveUpTriggered(const FInputActionValue& Value);
	void OnMoveUpCompleted(const FInputActionValue& Value);
	void OnLookTriggered(const FInputActionValue& Value);
	void OnLookCompleted(const FInputActionValue& Value);
	void OnRollTriggered(const FInputActionValue& Value);
	void OnRollCompleted(const FInputActionValue& Value);
	void OnGamepadLookStarted(const FInputActionValue& Value);
	void OnGamepadLookTriggered(const FInputActionValue& Value);
	void OnGamepadLookCompleted(const FInputActionValue& Value);
	void OnJumpStarted(const FInputActionValue& Value);
	void OnJumpReleased(const FInputActionValue& Value);
	void OnFlyTriggered(const FInputActionValue& Value);
	void OnModifyControlCompleted(const FInputActionValue& Value);
	void OnModifyControlTriggered(const FInputActionValue& Value);
	void OnModifySelectCompleted(const FInputActionValue& Value);
	void OnModifySelectTriggered(const FInputActionValue& Value);

	uint8 bHasProduceInputinBpFunc : 1;
};
