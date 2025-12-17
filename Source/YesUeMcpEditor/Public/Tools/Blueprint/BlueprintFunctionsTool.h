// Copyright softdaddy-o 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/McpToolBase.h"
#include "BlueprintFunctionsTool.generated.h"

/**
 * Tool for listing Blueprint functions with detailed signatures
 */
UCLASS()
class YESUEMCPEDITOR_API UBlueprintFunctionsTool : public UMcpToolBase
{
	GENERATED_BODY()

public:
	virtual FString GetToolName() const override { return TEXT("get-blueprint-functions"); }
	virtual FString GetToolDescription() const override
	{
		return TEXT("Get all functions defined in a Blueprint with their signatures, parameters, return types, and metadata.");
	}

	virtual TMap<FString, FMcpSchemaProperty> GetInputSchema() const override;
	virtual TArray<FString> GetRequiredParams() const override { return { TEXT("asset_path") }; }

	virtual FMcpToolResult Execute(
		const TSharedPtr<FJsonObject>& Arguments,
		const FMcpToolContext& Context) override;
};
