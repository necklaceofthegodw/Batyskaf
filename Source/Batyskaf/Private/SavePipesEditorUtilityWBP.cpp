// Fill out your copyright notice in the Description page of Project Settings.


#include "SavePipesEditorUtilityWBP.h"
#include "AssetToolsModule.h"
#include "FileHelpers.h"
#include "ObjectTools.h"
#include "Engine/DataTable.h"

void USavePipesEditorUtilityWBP::SavePipesTable()
{
	UDataTable* MyDataTable = NewObject<UDataTable>();
	MyDataTable->RowStruct = FMyStruct::StaticStruct();
    
	// Add your rows...
    
	// Use Asset Tools to create proper asset
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
    
	FString PackageName = TEXT("/Game/MyFolder/MyDataTable");
	FString AssetName = TEXT("MyDataTable");
    
	UDataTable* NewAsset = Cast<UDataTable>(AssetToolsModule.Get().CreateAsset(
		AssetName,
		TEXT("/Game/MyFolder"),
		UDataTable::StaticClass(),
		nullptr
	));
    
	if (NewAsset)
	{
		NewAsset->RowStruct = FMyStruct::StaticStruct();
		// Copy your data to NewAsset
		NewAsset->MarkPackageDirty();
        
		// Force save
		TArray<UPackage*> PackagesToSave;
		PackagesToSave.Add(NewAsset->GetPackage());
		FEditorFileUtils::PromptForCheckoutAndSave(
			PackagesToSave, 
			false, 
			false
		);
	}
}
