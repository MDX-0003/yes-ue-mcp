// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/Write/SetPropertyTool.h"
#include "Utils/McpAssetModifier.h"
#include "YesUeMcpEditor.h"
#include "Engine/Blueprint.h"
#include "ScopedTransaction.h"

FString USetPropertyTool::GetToolDescription() const
{
	return TEXT("Set any property on any asset using UE reflection. Supports nested paths (e.g., 'Stats.MaxHealth'), "
		"array indices (e.g., 'Items[0]'), TArray, TMap (as JSON object), TSet (as JSON array), "
		"object references (as asset paths), structs, and vectors/rotators/colors.");
}

TMap<FString, FMcpSchemaProperty> USetPropertyTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty AssetPath;
	AssetPath.Type = TEXT("string");
	AssetPath.Description = TEXT("Asset path (e.g., '/Game/Blueprints/BP_Player')");
	AssetPath.bRequired = true;
	Schema.Add(TEXT("asset_path"), AssetPath);

	FMcpSchemaProperty PropertyPath;
	PropertyPath.Type = TEXT("string");
	PropertyPath.Description = TEXT("Dot-separated property path (e.g., 'Health', 'Stats.MaxHealth', 'Items[0].Value')");
	PropertyPath.bRequired = true;
	Schema.Add(TEXT("property_path"), PropertyPath);

	FMcpSchemaProperty Value;
	Value.Type = TEXT("any");
	Value.Description = TEXT("Value to set (number, string, boolean, array, or object depending on property type)");
	Value.bRequired = true;
	Schema.Add(TEXT("value"), Value);

	return Schema;
}

TArray<FString> USetPropertyTool::GetRequiredParams() const
{
	return { TEXT("asset_path"), TEXT("property_path"), TEXT("value") };
}

FMcpToolResult USetPropertyTool::Execute(
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	// Get parameters
	FString AssetPath = GetStringArgOrDefault(Arguments, TEXT("asset_path"));
	FString PropertyPath = GetStringArgOrDefault(Arguments, TEXT("property_path"));

	if (AssetPath.IsEmpty())
	{
		return FMcpToolResult::Error(TEXT("asset_path is required"));
	}

	if (PropertyPath.IsEmpty())
	{
		return FMcpToolResult::Error(TEXT("property_path is required"));
	}

	// Get the value (can be any JSON type)
	TSharedPtr<FJsonValue> Value = Arguments->TryGetField(TEXT("value"));
	if (!Value.IsValid())
	{
		return FMcpToolResult::Error(TEXT("value is required"));
	}

	UE_LOG(LogYesUeMcp, Log, TEXT("set-property: %s.%s"), *AssetPath, *PropertyPath);

	// Load the asset
	FString LoadError;
	UObject* Object = FMcpAssetModifier::LoadAssetByPath(AssetPath, LoadError);
	if (!Object)
	{
		return FMcpToolResult::Error(LoadError);
	}

	// For Blueprints, we need to modify the CDO (Class Default Object)
	UBlueprint* Blueprint = Cast<UBlueprint>(Object);
	UObject* TargetObject = Object;

	if (Blueprint)
	{
		// Get the generated class's CDO
		if (Blueprint->GeneratedClass)
		{
			TargetObject = Blueprint->GeneratedClass->GetDefaultObject();
			if (!TargetObject)
			{
				return FMcpToolResult::Error(TEXT("Failed to get Blueprint CDO"));
			}
		}
		else
		{
			return FMcpToolResult::Error(TEXT("Blueprint has no generated class - compile it first"));
		}
	}

	// Find the property
	FProperty* Property = nullptr;
	void* Container = nullptr;
	FString FindError;

	if (!FMcpAssetModifier::FindPropertyByPath(TargetObject, PropertyPath, Property, Container, FindError))
	{
		return FMcpToolResult::Error(FindError);
	}

	// Begin transaction for undo support
	TSharedPtr<FScopedTransaction> Transaction = FMcpAssetModifier::BeginTransaction(
		FText::Format(NSLOCTEXT("MCP", "SetProperty", "Set {0}.{1}"),
			FText::FromString(AssetPath),
			FText::FromString(PropertyPath)));

	// Mark object as modified
	FMcpAssetModifier::MarkModified(TargetObject);
	if (Blueprint)
	{
		FMcpAssetModifier::MarkModified(Blueprint);
	}

	// Set the property value
	FString SetError;
	if (!FMcpAssetModifier::SetPropertyFromJson(Property, Container, Value, SetError))
	{
		return FMcpToolResult::Error(SetError);
	}

	// Mark package as dirty
	FMcpAssetModifier::MarkPackageDirty(Object);

	// Build result
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("asset"), AssetPath);
	Result->SetStringField(TEXT("property"), PropertyPath);
	Result->SetBoolField(TEXT("needs_save"), true);

	if (Blueprint)
	{
		Result->SetBoolField(TEXT("needs_compile"), true);
	}

	UE_LOG(LogYesUeMcp, Log, TEXT("set-property: Successfully set %s.%s"), *AssetPath, *PropertyPath);

	return FMcpToolResult::Json(Result);
}
