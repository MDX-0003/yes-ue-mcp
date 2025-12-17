// Copyright softdaddy-o 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/McpToolBase.h"
#include "BlueprintVariablesTool.generated.h"

/**
 * Tool for listing Blueprint variables with metadata
 */
UCLASS()
class YESUEMCPEDITOR_API UBlueprintVariablesTool : public UMcpToolBase
{
	GENERATED_BODY()

public:
	virtual FString GetToolName() const override { return TEXT("get-blueprint-variables"); }
	virtual FString GetToolDescription() const override
	{
		return TEXT("Get all variables defined in a Blueprint with their types, default values, categories, and property flags.");
	}

	virtual TMap<FString, FMcpSchemaProperty> GetInputSchema() const override;
	virtual TArray<FString> GetRequiredParams() const override { return { TEXT("asset_path") }; }

	virtual FMcpToolResult Execute(
		const TSharedPtr<FJsonObject>& Arguments,
		const FMcpToolContext& Context) override;
};
