// Copyright softdaddy-o 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/McpToolBase.h"
#include "BlueprintDefaultsTool.generated.h"

/**
 * Tool for reading CDO (Class Default Object) property values from Blueprint
 */
UCLASS()
class YESUEMCPEDITOR_API UBlueprintDefaultsTool : public UMcpToolBase
{
	GENERATED_BODY()

public:
	virtual FString GetToolName() const override { return TEXT("get-blueprint-defaults"); }
	virtual FString GetToolDescription() const override;
	virtual TMap<FString, FMcpSchemaProperty> GetInputSchema() const override;
	virtual TArray<FString> GetRequiredParams() const override;

	virtual FMcpToolResult Execute(
		const TSharedPtr<FJsonObject>& Arguments,
		const FMcpToolContext& Context) override;

private:
	/** Convert a property to JSON with its default value */
	TSharedPtr<FJsonObject> PropertyToJson(class FProperty* Property, void* ValuePtr, UObject* CDO) const;

	/** Get property type as string */
	FString GetPropertyTypeString(class FProperty* Property) const;

	/** Get property flags as string array */
	TArray<FString> GetPropertyFlags(class FProperty* Property) const;

	/** Check if property matches wildcard filter */
	bool MatchesFilter(const FString& Name, const FString& Filter) const;
};
