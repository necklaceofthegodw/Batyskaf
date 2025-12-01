// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PipeSplineDataAsset.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class BATYSKAF_API UPipeSplineDataAsset : public UDataAsset
{
	GENERATED_BODY()
	
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline")
    TArray<FVector> SplinePoints;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline")
    TArray<FVector> ArriveTangents;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline")
    TArray<FVector> LeaveTangents;
};
