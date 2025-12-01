// Fill out your copyright notice in the Description page of Project Settings.


#include "Pipes/PipeSpline.h"

#include "Components/SplineComponent.h"
#include "Pipes/PipeSplineDataAsset.h"

// Sets default values
APipeSpline::APipeSpline()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	PipeSplineComp = CreateDefaultSubobject<USplineComponent>("PipeSpline");
	SetRootComponent(PipeSplineComp);


}
void APipeSpline::OnConstruction(const FTransform& Transform)
{
	if (SplineDataAsset)
	{
		LoadSplineFromDataAsset(SplineDataAsset);
	}
	
	Super::OnConstruction(Transform);
	
}
// Called when the game starts or when spawned
void APipeSpline::BeginPlay()
{
	Super::BeginPlay();


}

void APipeSpline::SaveSplineToDataAsset(UPipeSplineDataAsset* DataAsset)
{
	if (!DataAsset || !PipeSplineComp) return;

	DataAsset->SplinePoints.Empty();
	DataAsset->ArriveTangents.Empty();
	DataAsset->LeaveTangents.Empty();

	int32 NumPoints = PipeSplineComp->GetNumberOfSplinePoints();
    
	for (int32 i = 0; i < NumPoints; i++)
	{
		DataAsset->SplinePoints.Add(PipeSplineComp->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local));
		DataAsset->ArriveTangents.Add(PipeSplineComp->GetArriveTangentAtSplinePoint(i, ESplineCoordinateSpace::Local));
		DataAsset->LeaveTangents.Add(PipeSplineComp->GetLeaveTangentAtSplinePoint(i, ESplineCoordinateSpace::Local));
	}
}

void APipeSpline::LoadSplineFromDataAsset(UPipeSplineDataAsset* DataAsset)
{
	if (!DataAsset || !PipeSplineComp) return;

	PipeSplineComp->ClearSplinePoints();

	for (int32 i = 0; i < DataAsset->SplinePoints.Num(); i++)
	{
		PipeSplineComp->AddSplinePoint(DataAsset->SplinePoints[i], ESplineCoordinateSpace::Local);
        
		if (i < DataAsset->ArriveTangents.Num())
			PipeSplineComp->SetTangentsAtSplinePoint(i, DataAsset->ArriveTangents[i], DataAsset->LeaveTangents[i], ESplineCoordinateSpace::Local);
	}

	PipeSplineComp->UpdateSpline();
}

// Called every frame
void APipeSpline::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

