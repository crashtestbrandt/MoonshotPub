// Copyright 2024 Frazimuth, LLC.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "MoonshotBaseGameInstance.generated.h"

/**
 * 
 */
UCLASS(config=Game)
class MOONSHOT_API UMoonshotBaseGameInstance : public UGameInstance
{
	GENERATED_BODY()

	UPROPERTY(Config)
	FString PatchNotesFilePath;

public:
	UFUNCTION(BlueprintCallable, Category = "PatchNotes")
	FString GetPatchNotesFilePath();
};
