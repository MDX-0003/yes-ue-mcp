// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/Write/SaveAssetTool.h"
#include "Utils/McpAssetModifier.h"
#include "YesUeMcpEditor.h"

FString USaveAssetTool::GetToolDescription() const
{
	return TEXT("Save a modified asset to disk. Call after making changes to persist them.");
}

TMap<FString, FMcpSchemaProperty> USaveAssetTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty AssetPath;
	AssetPath.Type = TEXT("string");
	AssetPath.Description = TEXT("Asset path to save (e.g., '/Game/Blueprints/BP_Player')");
	AssetPath.bRequired = true;
	Schema.Add(TEXT("asset_path"), AssetPath);

	return Schema;
}

TArray<FString> USaveAssetTool::GetRequiredParams() const
{
	return { TEXT("asset_path") };
}

FMcpToolResult USaveAssetTool::Execute(
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	FString AssetPath = GetStringArgOrDefault(Arguments, TEXT("asset_path"));

	if (AssetPath.IsEmpty())
	{
		return FMcpToolResult::Error(TEXT("asset_path is required"));
	}

	UE_LOG(LogYesUeMcp, Log, TEXT("save-asset: %s"), *AssetPath);

	// Load the asset
	FString LoadError;
	UObject* Object = FMcpAssetModifier::LoadAssetByPath(AssetPath, LoadError);
	if (!Object)
	{
		return FMcpToolResult::Error(LoadError);
	}

	// Save the asset
	FString SaveError;
	bool bSuccess = FMcpAssetModifier::SaveAsset(Object, false, SaveError);

	// Build result
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), bSuccess);
	Result->SetStringField(TEXT("asset"), AssetPath);

	if (bSuccess)
	{
		Result->SetStringField(TEXT("status"), TEXT("saved"));
		UE_LOG(LogYesUeMcp, Log, TEXT("save-asset: Successfully saved %s"), *AssetPath);
	}
	else
	{
		Result->SetStringField(TEXT("status"), TEXT("error"));
		Result->SetStringField(TEXT("error"), SaveError);
		UE_LOG(LogYesUeMcp, Warning, TEXT("save-asset: Failed to save %s: %s"), *AssetPath, *SaveError);
	}

	return FMcpToolResult::Json(Result);
}
