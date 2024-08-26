// Copyright 2024 Frazimuth, LLC.


#include "MoonshotBaseGameInstance.h"

FString UMoonshotBaseGameInstance::GetPatchNotesFilePath()
{
    //return FPaths::ProjectContentDir() + "Moonshot/Data/PatchNotes.json";

    GConfig->GetString(
    TEXT("Script/Moonshot.MoonshotBaseGameInstance"),
    TEXT("PatchNotesFilePath"),
    PatchNotesFilePath,
    GGameIni
    );
    return FPaths::ProjectContentDir() + PatchNotesFilePath;
}