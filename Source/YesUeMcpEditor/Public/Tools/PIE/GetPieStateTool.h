// Copyright softdaddy-o 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/McpToolBase.h"
#include "GetPieStateTool.generated.h"

/**
 * Get the current state of the Play-In-Editor (PIE) session.
 * Returns world info, player states, and optionally performance stats.
 */
UCLASS()
class YESUEMCPEDITOR_API UGetPieStateTool : public UMcpToolBase
{
	GENERATED_BODY()

public:
	virtual FString GetToolName() const override { return TEXT("get-pie-state"); }
	virtual FString GetToolDescription() const override;
	virtual TMap<FString, FMcpSchemaProperty> GetInputSchema() const override;
	virtual TArray<FString> GetRequiredParams() const override { return {}; }
	virtual FMcpToolResult Execute(
		const TSharedPtr<FJsonObject>& Arguments,
		const FMcpToolContext& Context) override;
	virtual bool RequiresGameThread() const override { return true; }

private:
	TSharedPtr<FJsonObject> GetWorldInfo(UWorld* PIEWorld) const;
	TSharedPtr<FJsonArray> GetPlayersInfo(UWorld* PIEWorld) const;
	TSharedPtr<FJsonObject> GetStatsInfo() const;
};
