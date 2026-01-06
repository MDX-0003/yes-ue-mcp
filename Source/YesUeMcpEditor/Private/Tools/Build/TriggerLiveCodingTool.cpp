// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/Build/TriggerLiveCodingTool.h"
#include "YesUeMcpEditor.h"
#include "Engine/Engine.h"

FString UTriggerLiveCodingTool::GetToolDescription() const
{
	return TEXT("Trigger Live Coding compilation for C++ code changes. Equivalent to pressing Ctrl+Alt+F11. Windows only.");
}

TMap<FString, FMcpSchemaProperty> UTriggerLiveCodingTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty WaitForCompletion;
	WaitForCompletion.Type = TEXT("boolean");
	WaitForCompletion.Description = TEXT("Wait for compilation to complete before returning (default: false, async mode)");
	WaitForCompletion.bRequired = false;
	Schema.Add(TEXT("wait_for_completion"), WaitForCompletion);

	return Schema;
}

FMcpToolResult UTriggerLiveCodingTool::Execute(
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	bool bWaitForCompletion = GetBoolArgOrDefault(Arguments, TEXT("wait_for_completion"), false);

	UE_LOG(LogYesUeMcp, Log, TEXT("trigger-live-coding: Triggering Live Coding compilation..."));

	// Check if GEngine is available
	if (!GEngine)
	{
		return FMcpToolResult::Error(TEXT("GEngine not available"));
	}

	// Execute LiveCoding.Compile console command
	// This is equivalent to pressing Ctrl+Alt+F11
	bool bExecuted = GEngine->Exec(nullptr, TEXT("LiveCoding.Compile"));

	if (!bExecuted)
	{
		UE_LOG(LogYesUeMcp, Warning, TEXT("trigger-live-coding: Failed to execute LiveCoding.Compile command"));
		return FMcpToolResult::Error(TEXT("Failed to execute LiveCoding.Compile. Live Coding may not be available or enabled. Enable it in Editor Preferences > General > Live Coding."));
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("status"), bWaitForCompletion ? TEXT("triggered_sync") : TEXT("triggered_async"));
	Result->SetStringField(TEXT("message"), TEXT("Live Coding compilation initiated. Check Output Log for compilation results."));
	Result->SetStringField(TEXT("shortcut"), TEXT("Ctrl+Alt+F11"));

	UE_LOG(LogYesUeMcp, Log, TEXT("trigger-live-coding: Live Coding compilation triggered successfully"));

	// Note: The current implementation triggers the compile asynchronously
	// The wait_for_completion parameter is accepted but not yet implemented
	// as monitoring compilation status requires deeper integration with Live Coding module
	if (bWaitForCompletion)
	{
		Result->SetStringField(TEXT("note"), TEXT("wait_for_completion is not yet implemented. Compilation is triggered asynchronously."));
		UE_LOG(LogYesUeMcp, Warning, TEXT("trigger-live-coding: wait_for_completion parameter not yet implemented"));
	}

	return FMcpToolResult::Json(Result);
}
