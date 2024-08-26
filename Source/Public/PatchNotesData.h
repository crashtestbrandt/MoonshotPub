#pragma once

#include "CoreMinimal.h"
#include "PatchNotesData.generated.h"


USTRUCT(BlueprintType)
struct FPatchEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PatchNotes")
    FString Version;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PatchNotes")
    FString PrereleaseType;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PatchNotes")
    FString ReleaseDate;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PatchNotes")
    FString Commit;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PatchNotes")
    TArray<FString> Changes;
};

USTRUCT(BlueprintType)
struct FPatchList
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PatchNotes")
    TArray<FPatchEntry> PatchHistory;
};