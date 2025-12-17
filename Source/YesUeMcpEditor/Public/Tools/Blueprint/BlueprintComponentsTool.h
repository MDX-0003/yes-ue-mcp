// Copyright softdaddy-o 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/McpToolBase.h"
#include "BlueprintComponentsTool.generated.h"

/**
 * Tool for listing Blueprint components hierarchy
 */
UCLASS()
class YESUEMCPEDITOR_API UBlueprintComponentsTool : public UMcpToolBase
{
	GENERATED_BODY()

public:
	virtual FString GetToolName() const override { return TEXT("get-blueprint-components"); }
	virtual FString GetToolDescription() const override
	{
		return TEXT("Get the component hierarchy of a Blueprint, including component types, parent relationships, and default properties.");
	}

	virtual TMap<FString, FMcpSchemaProperty> GetInputSchema() const override;
	virtual TArray<FString> GetRequiredParams() const override { return { TEXT("asset_path") }; }

	virtual FMcpToolResult Execute(
		const TSharedPtr<FJsonObject>& Arguments,
		const FMcpToolContext& Context) override;

private:
	/** Build component tree recursively */
	TSharedPtr<FJsonObject> BuildComponentNode(class USCS_Node* Node, class USimpleConstructionScript* SCS);
};
