// Fill out your copyright notice in the Description page of Project Settings.


#include "Batyskaf/Public/GAS/AttributeSets/SubmarineAttributeSet.h"
#include "GameplayEffectExtension.h"   // FGameplayEffectModCallbackData
#include "AbilitySystemComponent.h"    // UAbilitySystemComponent

USubmarineAttributeSet::USubmarineAttributeSet()
{
}

namespace
{
	// Pomocnik: skaluje Current, gdy zmienia si� Max (z clampem do nowego Max).
	static void AdjustForMaxChange(
		USubmarineAttributeSet* Self,
		FGameplayAttributeData& AffectedAttr,       // CurrentX
		const FGameplayAttributeData& MaxAttr,      // MaxX
		float NewMaxValue,
		const FGameplayAttribute& AffectedAttrProperty) // GetCurrentXAttribute()
	{
		const float OldMax = MaxAttr.GetCurrentValue();
		if (OldMax <= 0.f || FMath::IsNearlyEqual(OldMax, NewMaxValue))
		{
			return;
		}

		const float Current = AffectedAttr.GetCurrentValue();
		const float NewCurrent = FMath::Clamp(Current * (NewMaxValue / OldMax), 0.f, NewMaxValue);

		if (UAbilitySystemComponent* ASC = Self ? Self->GetOwningAbilitySystemComponent() : nullptr)
		{
			const float Delta = NewCurrent - Current;
			ASC->ApplyModToAttributeUnsafe(AffectedAttrProperty, EGameplayModOp::Additive, Delta);
		}
		else
		{
			AffectedAttr.SetBaseValue(NewCurrent);
			AffectedAttr.SetCurrentValue(NewCurrent);
		}
	}

	// Pomocnik: clampuje Base+Current jednocze�nie.
	static void ClampBaseAndCurrent(FGameplayAttributeData& Attr, float Min, float Max)
	{
		const float Clamped = FMath::Clamp(Attr.GetCurrentValue(), Min, Max);
		Attr.SetBaseValue(Clamped);
		Attr.SetCurrentValue(Clamped);
	}
}


void USubmarineAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	
	const FGameplayAttribute& Attr = Data.EvaluatedData.Attribute;

	if (Attr == GetCurrentBatteryCapacityAttribute())
	{
		ClampBaseAndCurrent(CurrentBatteryCapacity, 0.f, MaximumBaterryCapacity.GetCurrentValue());
	}
}

void USubmarineAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	
	if (Attribute == GetCurrentBatteryCapacityAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, MaximumBaterryCapacity.GetCurrentValue());
	}
	// --- Zmiana Max*: skaluje odpowiadajace Current* proporcjonalnie ---
	else if (Attribute == GetMaximumBaterryCapacityAttribute())
	{
		AdjustForMaxChange(this, CurrentBatteryCapacity, MaximumBaterryCapacity, NewValue, GetCurrentBatteryCapacityAttribute());
	}
}

void USubmarineAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);
	
	if (Attribute == GetCurrentBatteryCapacityAttribute())
	{
		ClampBaseAndCurrent(CurrentBatteryCapacity, 0.f, MaximumBaterryCapacity.GetCurrentValue());
	}
}
