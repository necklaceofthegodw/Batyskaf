// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "SavePipesEditorUtilityWBP.generated.h"

/**
 * 
 */
UCLASS()
class BATYSKAF_API USavePipesEditorUtilityWBP : public UEditorUtilityWidgetBlueprint
{
	GENERATED_BODY()
	
	UFUNCTION(BlueprintCallable)
	void SavePipesTable();
};
