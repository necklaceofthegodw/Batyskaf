// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PipeSpline.generated.h"

class UPipeSplineDataAsset;
class USplineComponent;

UCLASS()
class BATYSKAF_API APipeSpline : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	APipeSpline();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BLueprintReadWrite)
	USplineComponent * PipeSplineComp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline", meta = (ExposeOnSpawn="true"))
	UPipeSplineDataAsset * SplineDataAsset = nullptr;

public:

	UFUNCTION(BlueprintCallable, Category = "Spline")
	void SaveSplineToDataAsset(UPipeSplineDataAsset* DataAsset);

	UFUNCTION(BlueprintCallable, Category = "Spline")
	void LoadSplineFromDataAsset(UPipeSplineDataAsset* DataAsset);
	
	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	virtual void OnConstruction(const FTransform& Transform) override;
};


