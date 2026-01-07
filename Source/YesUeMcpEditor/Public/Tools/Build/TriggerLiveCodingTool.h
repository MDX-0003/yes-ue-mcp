// Copyright softdaddy-o 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/McpToolBase.h"
#include "TriggerLiveCodingTool.generated.h"

// Forward declarations
#if PLATFORM_WINDOWS
class ILiveCodingModule;
#endif

/**
 * Trigger Live Coding compilation for C++ code changes.
 * Uses UE's Live Coding system (Ctrl+Alt+F11 equivalent).
 * Supports both async and sync modes with compilation result tracking.
 * Windows only. Requires Live Coding to be enabled in Editor Preferences.
 */
UCLASS()
class YESUEMCPEDITOR_API UTriggerLiveCodingTool : public UMcpToolBase
{
	GENERATED_BODY()

public:
	virtual FString GetToolName() const override { return TEXT("trigger-live-coding"); }
	virtual FString GetToolDescription() const override;
	virtual TMap<FString, FMcpSchemaProperty> GetInputSchema() const override;
	virtual TArray<FString> GetRequiredParams() const override { return {}; }
	virtual FMcpToolResult Execute(
		const TSharedPtr<FJsonObject>& Arguments,
		const FMcpToolContext& Context) override;
	virtual bool RequiresGameThread() const override { return true; }

private:
#if PLATFORM_WINDOWS
	// Execute synchronous compilation with delegate-based waiting
	FMcpToolResult ExecuteSynchronous(ILiveCodingModule* LiveCodingModule, int32 TimeoutSeconds);

	// Execute asynchronous compilation (fire and forget)
	FMcpToolResult ExecuteAsynchronous(ILiveCodingModule* LiveCodingModule);
#endif
};
