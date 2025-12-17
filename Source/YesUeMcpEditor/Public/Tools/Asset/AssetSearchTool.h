// Copyright softdaddy-o 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/McpToolBase.h"
#include "AssetSearchTool.generated.h"

/**
 * Tool for searching assets in the project
 */
UCLASS()
class YESUEMCPEDITOR_API UAssetSearchTool : public UMcpToolBase
{
	GENERATED_BODY()

public:
	virtual FString GetToolName() const override { return TEXT("search-assets"); }
	virtual FString GetToolDescription() const override
	{
		return TEXT("Search for assets in the project by name pattern, class type, or path. "
					"Supports wildcards (* and ?) in the search query.");
	}

	virtual TMap<FString, FMcpSchemaProperty> GetInputSchema() const override;
	virtual TArray<FString> GetRequiredParams() const override { return {}; }

	virtual FMcpToolResult Execute(
		const TSharedPtr<FJsonObject>& Arguments,
		const FMcpToolContext& Context) override;
};
