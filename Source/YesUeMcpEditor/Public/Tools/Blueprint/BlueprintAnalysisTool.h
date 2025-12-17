// Copyright softdaddy-o 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/McpToolBase.h"
#include "BlueprintAnalysisTool.generated.h"

/**
 * Tool for comprehensive Blueprint analysis
 * Returns complete structure including functions, variables, components, and graphs
 */
UCLASS()
class YESUEMCPEDITOR_API UBlueprintAnalysisTool : public UMcpToolBase
{
	GENERATED_BODY()

public:
	//~ UMcpToolBase interface
	virtual FString GetToolName() const override { return TEXT("analyze-blueprint"); }
	virtual FString GetToolDescription() const override
	{
		return TEXT("Analyze a Blueprint asset and return its complete structure including "
					"parent class, functions, variables, components, and event graph information.");
	}

	virtual TMap<FString, FMcpSchemaProperty> GetInputSchema() const override;
	virtual TArray<FString> GetRequiredParams() const override { return { TEXT("asset_path") }; }

	virtual FMcpToolResult Execute(
		const TSharedPtr<FJsonObject>& Arguments,
		const FMcpToolContext& Context) override;

private:
	/** Internal implementation */
	FMcpToolResult AnalyzeBlueprintInternal(const FString& AssetPath, bool bIncludeGraph);

	/** Extract function information */
	TSharedPtr<FJsonObject> ExtractFunctions(class UBlueprint* Blueprint);

	/** Extract variable information */
	TSharedPtr<FJsonObject> ExtractVariables(class UBlueprint* Blueprint);

	/** Extract component information */
	TSharedPtr<FJsonObject> ExtractComponents(class UBlueprint* Blueprint);

	/** Extract event graph summary */
	TSharedPtr<FJsonObject> ExtractEventGraphSummary(class UBlueprint* Blueprint);
};
