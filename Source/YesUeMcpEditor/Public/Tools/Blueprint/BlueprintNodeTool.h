// Copyright softdaddy-o 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/McpToolBase.h"
#include "BlueprintNodeTool.generated.h"

/**
 * Tool for reading detailed information about a specific Blueprint node by GUID
 */
UCLASS()
class YESUEMCPEDITOR_API UBlueprintNodeTool : public UMcpToolBase
{
	GENERATED_BODY()

public:
	virtual FString GetToolName() const override { return TEXT("get-blueprint-node"); }
	virtual FString GetToolDescription() const override;
	virtual TMap<FString, FMcpSchemaProperty> GetInputSchema() const override;
	virtual TArray<FString> GetRequiredParams() const override;

	virtual FMcpToolResult Execute(
		const TSharedPtr<FJsonObject>& Arguments,
		const FMcpToolContext& Context) override;

private:
	/** Find node by GUID in all graphs of a Blueprint */
	class UEdGraphNode* FindNodeByGuid(class UBlueprint* Blueprint, const FGuid& NodeGuid) const;

	/** Convert a node to JSON with full details */
	TSharedPtr<FJsonObject> NodeToJson(class UEdGraphNode* Node) const;

	/** Convert a pin to JSON */
	TSharedPtr<FJsonObject> PinToJson(class UEdGraphPin* Pin) const;

	/** Get pin type category as string */
	FString GetPinCategoryString(FName Category) const;

	/** Get pin direction as string */
	FString GetPinDirectionString(EEdGraphPinDirection Direction) const;
};
