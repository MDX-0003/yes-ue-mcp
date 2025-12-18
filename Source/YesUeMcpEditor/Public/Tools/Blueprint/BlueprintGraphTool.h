// Copyright softdaddy-o 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/McpToolBase.h"
#include "BlueprintGraphTool.generated.h"

/**
 * Tool for reading complete Blueprint graph structure including nodes, connections, and pins
 */
UCLASS()
class YESUEMCPEDITOR_API UBlueprintGraphTool : public UMcpToolBase
{
	GENERATED_BODY()

public:
	virtual FString GetToolName() const override { return TEXT("get-blueprint-graph"); }
	virtual FString GetToolDescription() const override;
	virtual TMap<FString, FMcpSchemaProperty> GetInputSchema() const override;
	virtual TArray<FString> GetRequiredParams() const override;

	virtual FMcpToolResult Execute(
		const TSharedPtr<FJsonObject>& Arguments,
		const FMcpToolContext& Context) override;

private:
	/** Convert a graph to JSON */
	TSharedPtr<FJsonObject> GraphToJson(class UEdGraph* Graph, const FString& GraphType, bool bIncludePositions) const;

	/** Convert a node to JSON */
	TSharedPtr<FJsonObject> NodeToJson(class UEdGraphNode* Node, bool bIncludePositions) const;

	/** Convert a pin to JSON */
	TSharedPtr<FJsonObject> PinToJson(class UEdGraphPin* Pin) const;

	/** Get pin type category as string */
	FString GetPinCategoryString(FName Category) const;

	/** Get pin direction as string */
	FString GetPinDirectionString(EEdGraphPinDirection Direction) const;
};
