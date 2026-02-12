// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "CostAbility.generated.h"

/**
 * 
 */
UCLASS()
class BATYSKAF_API UCostAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	
	UPROPERTY(EditDefaultsOnly)
	float Cost;
	
	UPROPERTY(EditDefaultsOnly)
	float Cooldown;
};
