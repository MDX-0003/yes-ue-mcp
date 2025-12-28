// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/Write/RemoveDataTableRowTool.h"
#include "Utils/McpAssetModifier.h"
#include "YesUeMcpEditor.h"
#include "Engine/DataTable.h"
#include "ScopedTransaction.h"

FString URemoveDataTableRowTool::GetToolDescription() const
{
	return TEXT("Remove a row from a DataTable.");
}

TMap<FString, FMcpSchemaProperty> URemoveDataTableRowTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty AssetPath;
	AssetPath.Type = TEXT("string");
	AssetPath.Description = TEXT("DataTable asset path");
	AssetPath.bRequired = true;
	Schema.Add(TEXT("asset_path"), AssetPath);

	FMcpSchemaProperty RowName;
	RowName.Type = TEXT("string");
	RowName.Description = TEXT("Name of the row to remove");
	RowName.bRequired = true;
	Schema.Add(TEXT("row_name"), RowName);

	return Schema;
}

TArray<FString> URemoveDataTableRowTool::GetRequiredParams() const
{
	return { TEXT("asset_path"), TEXT("row_name") };
}

FMcpToolResult URemoveDataTableRowTool::Execute(
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	FString AssetPath = GetStringArgOrDefault(Arguments, TEXT("asset_path"));
	FString RowName = GetStringArgOrDefault(Arguments, TEXT("row_name"));

	if (AssetPath.IsEmpty() || RowName.IsEmpty())
	{
		return FMcpToolResult::Error(TEXT("asset_path and row_name are required"));
	}

	UE_LOG(LogYesUeMcp, Log, TEXT("remove-datatable-row: %s from %s"), *RowName, *AssetPath);

	// Load the DataTable
	FString LoadError;
	UDataTable* DataTable = FMcpAssetModifier::LoadAssetByPath<UDataTable>(AssetPath, LoadError);
	if (!DataTable)
	{
		return FMcpToolResult::Error(LoadError);
	}

	// Check if row exists
	if (!DataTable->FindRowUnchecked(FName(*RowName)))
	{
		return FMcpToolResult::Error(FString::Printf(TEXT("Row not found: %s"), *RowName));
	}

	// Begin transaction
	TSharedPtr<FScopedTransaction> Transaction = FMcpAssetModifier::BeginTransaction(
		FText::Format(NSLOCTEXT("MCP", "RemoveRow", "Remove row {0} from {1}"),
			FText::FromString(RowName), FText::FromString(AssetPath)));

	FMcpAssetModifier::MarkModified(DataTable);

	// Remove the row
	DataTable->RemoveRow(FName(*RowName));

	FMcpAssetModifier::MarkPackageDirty(DataTable);

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("asset"), AssetPath);
	Result->SetStringField(TEXT("row_name"), RowName);
	Result->SetBoolField(TEXT("needs_save"), true);

	UE_LOG(LogYesUeMcp, Log, TEXT("remove-datatable-row: Removed row %s"), *RowName);

	return FMcpToolResult::Json(Result);
}
