// Copyright softdaddy-o 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/McpToolBase.h"
#include "DataAssetTool.generated.h"

/**
 * Tool for inspecting DataTable and DataAsset contents
 */
UCLASS()
class YESUEMCPEDITOR_API UDataAssetTool : public UMcpToolBase
{
	GENERATED_BODY()

public:
	virtual FString GetToolName() const override { return TEXT("inspect-data-asset"); }
	virtual FString GetToolDescription() const override;
	virtual TMap<FString, FMcpSchemaProperty> GetInputSchema() const override;
	virtual TArray<FString> GetRequiredParams() const override;

	virtual FMcpToolResult Execute(
		const TSharedPtr<FJsonObject>& Arguments,
		const FMcpToolContext& Context) override;

private:
	/** Inspect a DataTable */
	TSharedPtr<FJsonObject> InspectDataTable(class UDataTable* DataTable, const FString& RowFilter) const;

	/** Inspect a DataAsset */
	TSharedPtr<FJsonObject> InspectDataAsset(class UDataAsset* DataAsset) const;

	/** Convert struct property to JSON */
	TSharedPtr<FJsonObject> StructToJson(const UScriptStruct* Struct, const void* StructData) const;

	/** Convert property to JSON value */
	TSharedPtr<FJsonValue> PropertyToJsonValue(FProperty* Property, const void* ValuePtr) const;

	/** Check if row name matches filter (wildcard support) */
	bool MatchesRowFilter(const FName& RowName, const FString& Filter) const;
};
