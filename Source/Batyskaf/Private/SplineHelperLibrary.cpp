// Fill out your copyright notice in the Description page of Project Settings.


#include "SplineHelperLibrary.h"
#include "StaticMeshResources.h"
#include "Components/SplineMeshComponent.h"


TArray<FCircleData> USplineHelperLibrary::GetSplineMeshCenterPositions(USplineMeshComponent* SplineMesh,
	int32 SamplesPerSlice)
{
    TArray<FCircleData> Results;

    if (!IsValid(SplineMesh))
    {
        UE_LOG(LogTemp, Warning, TEXT("Invalid spline mesh component"));
        return Results;
    }

    const FVector StartPos = SplineMesh->GetStartPosition();
    const FVector EndPos = SplineMesh->GetEndPosition();
    const FVector StartTangent = SplineMesh->GetStartTangent();
    const FVector EndTangent = SplineMesh->GetEndTangent();

    for (int32 i = 0; i < SamplesPerSlice; i++)
    {
        const float Alpha = i / float(FMath::Max(1, SamplesPerSlice - 1));
        
        FVector Position = FMath::CubicInterp(StartPos, StartTangent, EndPos, EndTangent, Alpha);
        FVector Tangent = FMath::CubicInterpDerivative(StartPos, StartTangent, EndPos, EndTangent, Alpha).GetSafeNormal();
        
        // Transform to world space
        Position = SplineMesh->GetComponentTransform().TransformPosition(Position);
        Tangent = SplineMesh->GetComponentTransform().TransformVector(Tangent);
        
        FCircleData Data;
        Data.Center = Position;
        Data.Tangent = Tangent;
        Results.Add(Data);
    }

    return Results;
}
