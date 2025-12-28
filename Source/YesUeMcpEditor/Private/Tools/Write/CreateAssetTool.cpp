// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/Write/CreateAssetTool.h"
#include "Utils/McpAssetModifier.h"
#include "YesUeMcpEditor.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/BlueprintFactory.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/DataTableFactory.h"
#include "Factories/WorldFactory.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "Materials/Material.h"
#include "UObject/SavePackage.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "ScopedTransaction.h"

FString UCreateAssetTool::GetToolDescription() const
{
	return TEXT("Create a new asset. Supports Blueprint, Material, DataTable, Level, and more.");
}

TMap<FString, FMcpSchemaProperty> UCreateAssetTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty AssetPath;
	AssetPath.Type = TEXT("string");
	AssetPath.Description = TEXT("Full asset path including name (e.g., '/Game/Blueprints/BP_NewActor')");
	AssetPath.bRequired = true;
	Schema.Add(TEXT("asset_path"), AssetPath);

	FMcpSchemaProperty AssetClass;
	AssetClass.Type = TEXT("string");
	AssetClass.Description = TEXT("Asset type: 'Blueprint', 'Material', 'DataTable', 'Level', 'WidgetBlueprint', 'AnimBlueprint'");
	AssetClass.bRequired = true;
	Schema.Add(TEXT("asset_class"), AssetClass);

	FMcpSchemaProperty ParentClass;
	ParentClass.Type = TEXT("string");
	ParentClass.Description = TEXT("Parent class for Blueprints (e.g., 'Actor', 'Character', 'Pawn'). Default: 'Actor'");
	ParentClass.bRequired = false;
	Schema.Add(TEXT("parent_class"), ParentClass);

	FMcpSchemaProperty RowStruct;
	RowStruct.Type = TEXT("string");
	RowStruct.Description = TEXT("Row struct path for DataTables (e.g., '/Script/Engine.DataTableRowHandle')");
	RowStruct.bRequired = false;
	Schema.Add(TEXT("row_struct"), RowStruct);

	return Schema;
}

TArray<FString> UCreateAssetTool::GetRequiredParams() const
{
	return { TEXT("asset_path"), TEXT("asset_class") };
}

FMcpToolResult UCreateAssetTool::Execute(
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	FString AssetPath = GetStringArgOrDefault(Arguments, TEXT("asset_path"));
	FString AssetClass = GetStringArgOrDefault(Arguments, TEXT("asset_class"));
	FString ParentClass = GetStringArgOrDefault(Arguments, TEXT("parent_class"), TEXT("Actor"));
	FString RowStruct = GetStringArgOrDefault(Arguments, TEXT("row_struct"));

	if (AssetPath.IsEmpty() || AssetClass.IsEmpty())
	{
		return FMcpToolResult::Error(TEXT("asset_path and asset_class are required"));
	}

	// Validate path format
	FString ValidateError;
	if (!FMcpAssetModifier::ValidateAssetPath(AssetPath, ValidateError))
	{
		return FMcpToolResult::Error(ValidateError);
	}

	// Check if asset already exists
	if (FMcpAssetModifier::AssetExists(AssetPath))
	{
		return FMcpToolResult::Error(FString::Printf(TEXT("Asset already exists: %s"), *AssetPath));
	}

	UE_LOG(LogYesUeMcp, Log, TEXT("create-asset: %s of type %s"), *AssetPath, *AssetClass);

	// Extract package path and asset name
	FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	FString AssetName = FPackageName::GetShortName(AssetPath);

	// Begin transaction
	TSharedPtr<FScopedTransaction> Transaction = FMcpAssetModifier::BeginTransaction(
		FText::Format(NSLOCTEXT("MCP", "CreateAsset", "Create {0}"), FText::FromString(AssetPath)));

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	Result->SetStringField(TEXT("asset_class"), AssetClass);

	UObject* CreatedAsset = nullptr;

	// Create Blueprint
	if (AssetClass.Equals(TEXT("Blueprint"), ESearchCase::IgnoreCase))
	{
		// Find parent class
		UClass* ParentUClass = nullptr;

		if (ParentClass.Equals(TEXT("Actor"), ESearchCase::IgnoreCase))
		{
			ParentUClass = AActor::StaticClass();
		}
		else if (ParentClass.Equals(TEXT("Character"), ESearchCase::IgnoreCase))
		{
			ParentUClass = ACharacter::StaticClass();
		}
		else if (ParentClass.Equals(TEXT("Pawn"), ESearchCase::IgnoreCase))
		{
			ParentUClass = APawn::StaticClass();
		}
		else if (ParentClass.Equals(TEXT("PlayerController"), ESearchCase::IgnoreCase))
		{
			ParentUClass = APlayerController::StaticClass();
		}
		else if (ParentClass.Equals(TEXT("GameModeBase"), ESearchCase::IgnoreCase))
		{
			ParentUClass = AGameModeBase::StaticClass();
		}
		else
		{
			// Try to find the class
			ParentUClass = FindObject<UClass>(ANY_PACKAGE, *ParentClass);
			if (!ParentUClass)
			{
				ParentUClass = AActor::StaticClass();
			}
		}

		UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
		Factory->ParentClass = ParentUClass;

		CreatedAsset = AssetTools.CreateAsset(AssetName, PackagePath, UBlueprint::StaticClass(), Factory);

		if (CreatedAsset)
		{
			Result->SetStringField(TEXT("parent_class"), ParentUClass->GetName());
		}
	}
	// Create Material
	else if (AssetClass.Equals(TEXT("Material"), ESearchCase::IgnoreCase))
	{
		UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
		CreatedAsset = AssetTools.CreateAsset(AssetName, PackagePath, UMaterial::StaticClass(), Factory);
	}
	// Create DataTable
	else if (AssetClass.Equals(TEXT("DataTable"), ESearchCase::IgnoreCase))
	{
		UDataTableFactory* Factory = NewObject<UDataTableFactory>();

		// Find row struct
		if (!RowStruct.IsEmpty())
		{
			UScriptStruct* Struct = FindObject<UScriptStruct>(ANY_PACKAGE, *RowStruct);
			if (Struct)
			{
				Factory->Struct = Struct;
			}
		}

		CreatedAsset = AssetTools.CreateAsset(AssetName, PackagePath, UDataTable::StaticClass(), Factory);
	}
	// Create Level
	else if (AssetClass.Equals(TEXT("Level"), ESearchCase::IgnoreCase) ||
	         AssetClass.Equals(TEXT("Map"), ESearchCase::IgnoreCase))
	{
		UWorldFactory* Factory = NewObject<UWorldFactory>();
		Factory->WorldType = EWorldType::Editor;
		CreatedAsset = AssetTools.CreateAsset(AssetName, PackagePath, UWorld::StaticClass(), Factory);
	}
	else
	{
		return FMcpToolResult::Error(FString::Printf(TEXT("Unsupported asset class: %s. Supported: Blueprint, Material, DataTable, Level"), *AssetClass));
	}

	if (!CreatedAsset)
	{
		return FMcpToolResult::Error(TEXT("Failed to create asset"));
	}

	// Mark dirty and register
	FMcpAssetModifier::MarkPackageDirty(CreatedAsset);
	FAssetRegistryModule::AssetCreated(CreatedAsset);

	Result->SetBoolField(TEXT("success"), true);
	Result->SetBoolField(TEXT("needs_save"), true);

	UE_LOG(LogYesUeMcp, Log, TEXT("create-asset: Successfully created %s"), *AssetPath);

	return FMcpToolResult::Json(Result);
}
