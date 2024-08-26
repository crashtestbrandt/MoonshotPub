// Copyright 2024 Frazimuth, LLC.#pragma once

#include "CoreMinimal.h"
#include "PatchNotesData.h"
#include "PatchNotesReader.generated.h"

#pragma once

UCLASS()
class MOONSHOT_API UPatchNotesReader : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "PatchNotes")
    static bool LoadPatchHistory(const FString& FilePath, FPatchList& OutPatchHistory);
};