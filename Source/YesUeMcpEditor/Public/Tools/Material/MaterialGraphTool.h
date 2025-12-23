// Copyright softdaddy-o 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/McpToolBase.h"
#include "MaterialGraphTool.generated.h"

/**
 * Tool for reading complete Material expression graph structure
 */
UCLASS()
class YESUEMCPEDITOR_API UMaterialGraphTool : public UMcpToolBase
{
	GENERATED_BODY()

public:
	virtual FString GetToolName() const override { return TEXT("get-material-graph"); }
	virtual FString GetToolDescription() const override;
	virtual TMap<FString, FMcpSchemaProperty> GetInputSchema() const override;
	virtual TArray<FString> GetRequiredParams() const override;

	virtual FMcpToolResult Execute(
		const TSharedPtr<FJsonObject>& Arguments,
		const FMcpToolContext& Context) override;

private:
	/** Convert a material expression to JSON */
	TSharedPtr<FJsonObject> ExpressionToJson(class UMaterialExpression* Expression, bool bIncludePositions) const;

	/** Convert expression input to JSON */
	TSharedPtr<FJsonObject> InputToJson(struct FExpressionInput* Input, const FString& InputName, class UMaterial* Material) const;

	/** Convert expression output to JSON */
	TSharedPtr<FJsonObject> OutputToJson(const struct FExpressionOutput& Output, int32 Index) const;

	/** Get material output connection info */
	TSharedPtr<FJsonObject> MaterialOutputToJson(struct FExpressionInput* Input, const FString& OutputName, class UMaterial* Material) const;

	/** Generate stable ID for an expression */
	FString GetExpressionId(class UMaterialExpression* Expression) const;

	/** Get material domain as string */
	FString GetMaterialDomainString(EMaterialDomain Domain) const;

	/** Get blend mode as string */
	FString GetBlendModeString(EBlendMode BlendMode) const;

	/** Get shading model as string */
	FString GetShadingModelString(EMaterialShadingModel ShadingModel) const;

	/** Get expression type-specific properties */
	void AddExpressionSpecificProperties(class UMaterialExpression* Expression, TSharedPtr<FJsonObject>& OutJson) const;
};
