// Fill out your copyright notice in the Description page of Project Settings.


#include "Batyskaf/Public/Submarine/SubmarinePawn.h"

// Sets default values
ASubmarinePawn::ASubmarinePawn()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ASubmarinePawn::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ASubmarinePawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void ASubmarinePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

