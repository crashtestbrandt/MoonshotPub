// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Mover/Public/MoverTypes.h"
#include "Mover/Public/LayeredMove.h"
#include "Mover/Public/MoverDataModelTypes.h"
#include "MoonshotMoverDataModelTypes.generated.h"

// Data block containing all inputs that need to be authored and consumed for the default Mover character simulation
USTRUCT(BlueprintType)
struct MOONSHOTMOVER_API FMoonshotMoverCharacterInputs : public FCharacterDefaultInputs
{
	GENERATED_USTRUCT_BODY()

protected:

public:
    // For maintaining angular momentum in ZeroG
	UPROPERTY(BlueprintReadWrite, Category = Mover)
	FRotator AngularVelocity = FRotator::ZeroRotator;

    // For synchronizing gravity
    UPROPERTY(BlueprintReadWrite, Category = Mover)
    FVector GravityAcceleration = FVector::ZeroVector;

	FMoonshotMoverCharacterInputs() : FCharacterDefaultInputs()
	{
	}

	virtual ~FMoonshotMoverCharacterInputs() {}

	// @return newly allocated copy of this FMoonshotMoverCharacterInputs. Must be overridden by child classes
	virtual FMoverDataStructBase* Clone() const override;

	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;

	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }

	virtual void ToString(FAnsiStringBuilderBase& Out) const override;

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override { Super::AddReferencedObjects(Collector); }

};

template<>
struct TStructOpsTypeTraits< FMoonshotMoverCharacterInputs > : public TStructOpsTypeTraitsBase2< FMoonshotMoverCharacterInputs >
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};
