// Copyright softdaddy-o 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/McpToolBase.h"
#include "ProjectSettingsTool.generated.h"

/**
 * Tool for querying project configuration settings
 */
UCLASS()
class YESUEMCPEDITOR_API UProjectSettingsTool : public UMcpToolBase
{
	GENERATED_BODY()

public:
	virtual FString GetToolName() const override { return TEXT("get-project-settings"); }
	virtual FString GetToolDescription() const override;
	virtual TMap<FString, FMcpSchemaProperty> GetInputSchema() const override;
	virtual TArray<FString> GetRequiredParams() const override;

	virtual FMcpToolResult Execute(
		const TSharedPtr<FJsonObject>& Arguments,
		const FMcpToolContext& Context) override;

private:
	/** Get input mappings (actions and axes) */
	TSharedPtr<FJsonObject> GetInputMappings() const;

	/** Get collision settings */
	TSharedPtr<FJsonObject> GetCollisionSettings() const;

	/** Get gameplay tags */
	TSharedPtr<FJsonObject> GetGameplayTags() const;

	/** Get default maps and game modes */
	TSharedPtr<FJsonObject> GetDefaultMapsAndModes() const;
};
