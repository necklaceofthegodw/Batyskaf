// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SplineHelperLibrary.generated.h"

/**
 * 
 */
class USplineMeshComponent;
class USplineComponent;

USTRUCT(BlueprintType)
struct FCircleData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FVector Center;

	UPROPERTY(BlueprintReadOnly)
	FVector Tangent;
};

UCLASS()
class BATYSKAF_API USplineHelperLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	
	UFUNCTION(BlueprintCallable, Category = "Spline Utilities", 
			  meta = (DisplayName = "Get Cylinder Wall Positions"))
	static TArray<FCircleData> GetSplineMeshCenterPositions(
		USplineMeshComponent* SplineMesh,
		int32 SamplesPerSlice = 10
	);
};
