// Fill out your copyright notice in the Description page of Project Settings.

#include "MoonshotMoverDataModelTypes.h"
#include "Mover/Public/MoverTypes.h"
#include "Mover/Public/MoverDataModelTypes.h"
#include "Components/PrimitiveComponent.h"
#include "Mover/Public/MoveLibrary/BasedMovementUtils.h"
#include "Mover/Public/MoverLog.h"

FMoverDataStructBase* FMoonshotMoverCharacterInputs::Clone() const
{
	// TODO: ensure that this memory allocation jives with deletion method
	FMoonshotMoverCharacterInputs* CopyPtr = new FMoonshotMoverCharacterInputs(*this);
	return CopyPtr;
}

bool FMoonshotMoverCharacterInputs::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Super::NetSerialize(Ar, Map, bOutSuccess);

    SerializePackedVector<100, 30>(GravityAcceleration, Ar);
    AngularVelocity.SerializeCompressedShort(Ar);

    bOutSuccess = true;
    return bOutSuccess;
}

void FMoonshotMoverCharacterInputs::ToString(FAnsiStringBuilderBase& Out) const
{
	Super::ToString(Out);

    Out.Appendf("GravityAcceleration: X=%.2f Y=%.2f Z=%.2f\n", GravityAcceleration.X, GravityAcceleration.Y, GravityAcceleration.Z);
    Out.Appendf("AngularVelocity: P=%.2f Y=%.2f R=%.2f\n", AngularVelocity.Pitch, AngularVelocity.Yaw, AngularVelocity.Roll);
}