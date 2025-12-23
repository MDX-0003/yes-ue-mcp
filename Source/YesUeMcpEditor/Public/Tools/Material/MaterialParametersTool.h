// Copyright softdaddy-o 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/McpToolBase.h"
#include "MaterialParametersTool.generated.h"

/**
 * Tool for reading material parameters from Materials and MaterialInstances
 */
UCLASS()
class YESUEMCPEDITOR_API UMaterialParametersTool : public UMcpToolBase
{
	GENERATED_BODY()

public:
	virtual FString GetToolName() const override { return TEXT("get-material-parameters"); }
	virtual FString GetToolDescription() const override;
	virtual TMap<FString, FMcpSchemaProperty> GetInputSchema() const override;
	virtual TArray<FString> GetRequiredParams() const override;

	virtual FMcpToolResult Execute(
		const TSharedPtr<FJsonObject>& Arguments,
		const FMcpToolContext& Context) override;

private:
	/** Get parameters from a base Material */
	void GetMaterialParameters(class UMaterial* Material,
		TArray<TSharedPtr<FJsonValue>>& OutScalarParams,
		TArray<TSharedPtr<FJsonValue>>& OutVectorParams,
		TArray<TSharedPtr<FJsonValue>>& OutTextureParams,
		TArray<TSharedPtr<FJsonValue>>& OutSwitchParams,
		const FString& NameFilter) const;

	/** Get parameters from a MaterialInstance */
	void GetMaterialInstanceParameters(class UMaterialInstance* MatInstance,
		TArray<TSharedPtr<FJsonValue>>& OutScalarParams,
		TArray<TSharedPtr<FJsonValue>>& OutVectorParams,
		TArray<TSharedPtr<FJsonValue>>& OutTextureParams,
		TArray<TSharedPtr<FJsonValue>>& OutSwitchParams,
		bool bIncludeDefaults,
		const FString& NameFilter) const;

	/** Check if parameter name matches filter */
	bool MatchesFilter(const FString& ParamName, const FString& Filter) const;
};
