// Copyright softdaddy-o 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/McpToolBase.h"
#include "ProjectInfoTool.generated.h"

/**
 * Tool for retrieving basic project and plugin information.
 * Returns project name, path, and plugin version.
 */
UCLASS()
class YESUEMCPEDITOR_API UProjectInfoTool : public UMcpToolBase
{
	GENERATED_BODY()

public:
	virtual FString GetToolName() const override { return TEXT("get-project-info"); }
	virtual FString GetToolDescription() const override;
	virtual TMap<FString, FMcpSchemaProperty> GetInputSchema() const override;
	virtual TArray<FString> GetRequiredParams() const override;

	virtual FMcpToolResult Execute(
		const TSharedPtr<FJsonObject>& Arguments,
		const FMcpToolContext& Context) override;

private:
	/** Get the plugin version from YesUeMcp.uplugin */
	FString GetPluginVersion() const;
};
