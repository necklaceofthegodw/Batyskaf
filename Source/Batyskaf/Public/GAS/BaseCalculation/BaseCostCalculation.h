// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayModMagnitudeCalculation.h"
#include "BaseCostCalculation.generated.h"

/**
 * 
 */
UCLASS()
class BATYSKAF_API UBaseCostCalculation : public UGameplayModMagnitudeCalculation
{
public:
	virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;

private:
	GENERATED_BODY()
};
