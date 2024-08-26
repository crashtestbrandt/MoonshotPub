// Copyright 2024 Frazimuth, LLC.

#include "PatchNotesReader.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "JsonObjectConverter.h"

bool UPatchNotesReader::LoadPatchHistory(const FString& FilePath, FPatchList& OutPatchHistory)
{
    UE_LOG(LogTemp, Warning, TEXT("Loading patch notes from %s"), *FilePath);
    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load %s"), *FilePath);
        return false;
    }

    TArray<FPatchEntry> PatchEntries;
    if (!FJsonObjectConverter::JsonArrayStringToUStruct(JsonString, &PatchEntries, 0, 0))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to convert JSON to struct: %s"), *FilePath);
        return false;
    }

    OutPatchHistory.PatchHistory = PatchEntries;
    return true;
}