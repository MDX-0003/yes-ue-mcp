// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/Write/CompileBlueprintTool.h"
#include "Utils/McpAssetModifier.h"
#include "YesUeMcpEditor.h"
#include "Engine/Blueprint.h"

FString UCompileBlueprintTool::GetToolDescription() const
{
	return TEXT("Compile a Blueprint or AnimBlueprint. Call after making modifications to apply changes.");
}

TMap<FString, FMcpSchemaProperty> UCompileBlueprintTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty AssetPath;
	AssetPath.Type = TEXT("string");
	AssetPath.Description = TEXT("Asset path to the Blueprint (e.g., '/Game/Blueprints/BP_Player')");
	AssetPath.bRequired = true;
	Schema.Add(TEXT("asset_path"), AssetPath);

	return Schema;
}

TArray<FString> UCompileBlueprintTool::GetRequiredParams() const
{
	return { TEXT("asset_path") };
}

FMcpToolResult UCompileBlueprintTool::Execute(
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	FString AssetPath = GetStringArgOrDefault(Arguments, TEXT("asset_path"));

	if (AssetPath.IsEmpty())
	{
		return FMcpToolResult::Error(TEXT("asset_path is required"));
	}

	UE_LOG(LogYesUeMcp, Log, TEXT("compile-blueprint: %s"), *AssetPath);

	// Load the Blueprint
	FString LoadError;
	UBlueprint* Blueprint = FMcpAssetModifier::LoadAssetByPath<UBlueprint>(AssetPath, LoadError);
	if (!Blueprint)
	{
		return FMcpToolResult::Error(LoadError);
	}

	// Refresh nodes first
	FMcpAssetModifier::RefreshBlueprintNodes(Blueprint);

	// Compile
	FString CompileError;
	bool bSuccess = FMcpAssetModifier::CompileBlueprint(Blueprint, CompileError);

	// Build result
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), bSuccess);
	Result->SetStringField(TEXT("asset"), AssetPath);

	if (bSuccess)
	{
		Result->SetStringField(TEXT("status"), TEXT("compiled"));
		UE_LOG(LogYesUeMcp, Log, TEXT("compile-blueprint: Successfully compiled %s"), *AssetPath);
	}
	else
	{
		Result->SetStringField(TEXT("status"), TEXT("error"));
		Result->SetStringField(TEXT("error"), CompileError);
		UE_LOG(LogYesUeMcp, Warning, TEXT("compile-blueprint: Failed to compile %s: %s"), *AssetPath, *CompileError);
	}

	// Check for warnings
	if (Blueprint->Status == BS_UpToDateWithWarnings)
	{
		Result->SetStringField(TEXT("status"), TEXT("compiled_with_warnings"));
	}

	return FMcpToolResult::Json(Result);
}
