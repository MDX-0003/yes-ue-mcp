// Copyright softdaddy-o 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/McpToolBase.h"
#include "BlueprintCallableListTool.generated.h"

/**
 * Tool for listing all callable entry points (events, functions, macros) in a Blueprint
 * Returns lightweight metadata without full graph details
 */
UCLASS()
class YESUEMCPEDITOR_API UBlueprintCallableListTool : public UMcpToolBase
{
	GENERATED_BODY()

public:
	virtual FString GetToolName() const override { return TEXT("list-blueprint-callables"); }
	virtual FString GetToolDescription() const override;
	virtual TMap<FString, FMcpSchemaProperty> GetInputSchema() const override;
	virtual TArray<FString> GetRequiredParams() const override;

	virtual FMcpToolResult Execute(
		const TSharedPtr<FJsonObject>& Arguments,
		const FMcpToolContext& Context) override;

private:
	/** Extract event information from UbergraphPages */
	void ExtractEvents(class UBlueprint* Blueprint, TArray<TSharedPtr<FJsonValue>>& OutEventsArray) const;

	/** Extract function information from FunctionGraphs */
	void ExtractFunctions(class UBlueprint* Blueprint, TArray<TSharedPtr<FJsonValue>>& OutFunctionsArray) const;

	/** Extract macro information from MacroGraphs */
	void ExtractMacros(class UBlueprint* Blueprint, TArray<TSharedPtr<FJsonValue>>& OutMacrosArray) const;

	/** Extract parameter info from pins */
	void ExtractParameterInfo(const TArray<class UEdGraphPin*>& Pins, EEdGraphPinDirection Direction, TArray<TSharedPtr<FJsonValue>>& OutParams) const;
};
