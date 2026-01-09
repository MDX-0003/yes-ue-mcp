// Copyright softdaddy-o 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/McpToolBase.h"
#include "PieSessionTool.generated.h"

/**
 * Consolidated PIE session control tool.
 * Actions: start, stop, pause, resume, get-state
 */
UCLASS()
class YESUEMCPEDITOR_API UPieSessionTool : public UMcpToolBase
{
	GENERATED_BODY()

public:
	virtual FString GetToolName() const override { return TEXT("pie-session"); }
	virtual FString GetToolDescription() const override;
	virtual TMap<FString, FMcpSchemaProperty> GetInputSchema() const override;
	virtual TArray<FString> GetRequiredParams() const override { return {TEXT("action")}; }
	virtual FMcpToolResult Execute(
		const TSharedPtr<FJsonObject>& Arguments,
		const FMcpToolContext& Context) override;
	virtual bool RequiresGameThread() const override { return true; }

private:
	FMcpToolResult ExecuteStart(const TSharedPtr<FJsonObject>& Arguments);
	FMcpToolResult ExecuteStop(const TSharedPtr<FJsonObject>& Arguments);
	FMcpToolResult ExecutePause(const TSharedPtr<FJsonObject>& Arguments);
	FMcpToolResult ExecuteResume(const TSharedPtr<FJsonObject>& Arguments);
	FMcpToolResult ExecuteGetState(const TSharedPtr<FJsonObject>& Arguments);

	FString GenerateSessionId() const;
	UWorld* GetPIEWorld() const;
	bool WaitForPIEReady(float TimeoutSeconds) const;
	TSharedPtr<FJsonObject> GetWorldInfo(UWorld* PIEWorld) const;
	TSharedPtr<FJsonArray> GetPlayersInfo(UWorld* PIEWorld) const;
};
