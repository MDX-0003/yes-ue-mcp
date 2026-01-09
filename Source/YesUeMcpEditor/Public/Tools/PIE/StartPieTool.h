// Copyright softdaddy-o 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/McpToolBase.h"
#include "StartPieTool.generated.h"

/**
 * Start a Play-In-Editor (PIE) session for gameplay testing.
 * Returns session information including world name and player start location.
 */
UCLASS()
class YESUEMCPEDITOR_API UStartPieTool : public UMcpToolBase
{
	GENERATED_BODY()

public:
	virtual FString GetToolName() const override { return TEXT("start-pie"); }
	virtual FString GetToolDescription() const override;
	virtual TMap<FString, FMcpSchemaProperty> GetInputSchema() const override;
	virtual TArray<FString> GetRequiredParams() const override { return {}; }
	virtual FMcpToolResult Execute(
		const TSharedPtr<FJsonObject>& Arguments,
		const FMcpToolContext& Context) override;
	virtual bool RequiresGameThread() const override { return true; }

private:
	FString GenerateSessionId() const;
	UWorld* GetPIEWorld() const;
	bool WaitForPIEReady(float TimeoutSeconds) const;
};
