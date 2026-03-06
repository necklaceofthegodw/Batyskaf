// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ChoiceSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class BATYSKAF_API UChoiceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:

	UFUNCTION(BlueprintCallable)
	void ShowCurrentValues();

	UFUNCTION(BlueprintCallable)
	void SaveCSV();
	
	UFUNCTION(BlueprintCallable)
	void AddRow(float Time, int32 Value);

private:

	TArray<FString> Rows;
};
