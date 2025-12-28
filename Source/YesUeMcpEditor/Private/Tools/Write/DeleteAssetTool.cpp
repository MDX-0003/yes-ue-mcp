// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/Write/DeleteAssetTool.h"
#include "Utils/McpAssetModifier.h"
#include "YesUeMcpEditor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ObjectTools.h"
#include "FileHelpers.h"

FString UDeleteAssetTool::GetToolDescription() const
{
	return TEXT("Delete an asset from the project.");
}

TMap<FString, FMcpSchemaProperty> UDeleteAssetTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty AssetPath;
	AssetPath.Type = TEXT("string");
	AssetPath.Description = TEXT("Asset path to delete (e.g., '/Game/Blueprints/BP_OldActor')");
	AssetPath.bRequired = true;
	Schema.Add(TEXT("asset_path"), AssetPath);

	return Schema;
}

TArray<FString> UDeleteAssetTool::GetRequiredParams() const
{
	return { TEXT("asset_path") };
}

FMcpToolResult UDeleteAssetTool::Execute(
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	FString AssetPath = GetStringArgOrDefault(Arguments, TEXT("asset_path"));

	if (AssetPath.IsEmpty())
	{
		return FMcpToolResult::Error(TEXT("asset_path is required"));
	}

	UE_LOG(LogYesUeMcp, Log, TEXT("delete-asset: %s"), *AssetPath);

	// Check if asset exists
	if (!FMcpAssetModifier::AssetExists(AssetPath))
	{
		return FMcpToolResult::Error(FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
	}

	// Load the asset to get the object
	FString LoadError;
	UObject* Object = FMcpAssetModifier::LoadAssetByPath(AssetPath, LoadError);
	if (!Object)
	{
		return FMcpToolResult::Error(LoadError);
	}

	// Get the package
	UPackage* Package = Object->GetOutermost();

	// Delete the object
	TArray<UObject*> ObjectsToDelete;
	ObjectsToDelete.Add(Object);

	int32 DeletedCount = ObjectTools::DeleteObjects(ObjectsToDelete, false);

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetStringField(TEXT("asset_path"), AssetPath);

	if (DeletedCount > 0)
	{
		Result->SetBoolField(TEXT("success"), true);
		Result->SetStringField(TEXT("status"), TEXT("deleted"));

		UE_LOG(LogYesUeMcp, Log, TEXT("delete-asset: Successfully deleted %s"), *AssetPath);
	}
	else
	{
		Result->SetBoolField(TEXT("success"), false);
		Result->SetStringField(TEXT("error"), TEXT("Failed to delete asset. It may be in use."));

		UE_LOG(LogYesUeMcp, Warning, TEXT("delete-asset: Failed to delete %s"), *AssetPath);
	}

	return FMcpToolResult::Json(Result);
}
