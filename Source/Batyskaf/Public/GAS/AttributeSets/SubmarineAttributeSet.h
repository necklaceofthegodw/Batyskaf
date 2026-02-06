// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "SubmarineAttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)
/**
 * 
 */
UCLASS()
class BATYSKAF_API USubmarineAttributeSet : public UAttributeSet
{
	GENERATED_BODY()
public:
	USubmarineAttributeSet();
	
	UPROPERTY()
	FGameplayAttributeData MaxSpeed = 100.0f;
	ATTRIBUTE_ACCESSORS(USubmarineAttributeSet, MaxSpeed)

	UPROPERTY()
	FGameplayAttributeData MaxAcceleration = 100.0f;
	ATTRIBUTE_ACCESSORS(USubmarineAttributeSet, MaxAcceleration)

	UPROPERTY()
	FGameplayAttributeData Aerodynamics = 100.0f;
	ATTRIBUTE_ACCESSORS(USubmarineAttributeSet, Aerodynamics)

	UPROPERTY()
	FGameplayAttributeData BatteryCapacity = 100.0f;
	ATTRIBUTE_ACCESSORS(USubmarineAttributeSet, BatteryCapacity)

	UPROPERTY()
	FGameplayAttributeData MaxSonarRange = 100.0f;
	ATTRIBUTE_ACCESSORS(USubmarineAttributeSet, MaxSonarRange)

	UPROPERTY()
	FGameplayAttributeData MaxCapacity = 100.0f;
	ATTRIBUTE_ACCESSORS(USubmarineAttributeSet, MaxCapacity)


};
