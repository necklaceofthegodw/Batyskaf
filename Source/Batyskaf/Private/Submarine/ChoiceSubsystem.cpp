// Fill out your copyright notice in the Description page of Project Settings.


#include "Submarine/ChoiceSubsystem.h"

void UChoiceSubsystem::ShowCurrentValues()
{
	for (auto row: Rows)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s"), *row);
	}
}

void UChoiceSubsystem::AddRow(float Time, int32 Value)
{
	Rows.Add(FString::Printf(TEXT("%f,%d"), Time, Value));
}

void UChoiceSubsystem::SaveCSV()
{
	FString FilePath = FPaths::ProjectSavedDir() + TEXT("GameData.csv");
	FString Content = FString::Join(Rows, TEXT("\n"));

	FFileHelper::SaveStringToFile(Content, *FilePath);
}


