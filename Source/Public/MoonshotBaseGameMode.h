// Copyright 2024 Frazimuth, LLC.

#pragma once

#include "CoreMinimal.h"

#include "GameFramework/GameMode.h"
#include "Engine/Engine.h"
#include "MoonshotBaseGameMode.generated.h"

class APlayerStart;

UCLASS(BlueprintType)
class AMoonshotBaseGameMode : public AGameMode
{
	GENERATED_BODY()

	AMoonshotBaseGameMode(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool AllowCheats(APlayerController* P) { return true; }

	/** select best spawn point for player */
	//virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

private:
	//bool CanPlayerPawnFit(const APlayerStart* SpawnPoint, const AController* Player) const;
};