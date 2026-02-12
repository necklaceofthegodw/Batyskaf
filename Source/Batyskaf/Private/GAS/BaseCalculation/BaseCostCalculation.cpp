// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/BaseCalculation/BaseCostCalculation.h"
#include "GAS/GameplayAbilities/CostAbility.h"

float UBaseCostCalculation::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	const UCostAbility* Ability = Cast<UCostAbility>(Spec.GetContext().GetAbility());

	if (Ability == nullptr)
	{
		return 0.f;
	}
	
	return Ability->Cost;
}
