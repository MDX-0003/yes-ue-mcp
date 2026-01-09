// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/PIE/StopPieTool.h"
#include "YesUeMcpEditor.h"
#include "Editor.h"

FString UStopPieTool::GetToolDescription() const
{
	return TEXT("Stop the current Play-In-Editor (PIE) session.");
}

TMap<FString, FMcpSchemaProperty> UStopPieTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty SaveWorld;
	SaveWorld.Type = TEXT("boolean");
	SaveWorld.Description = TEXT("Save any level changes made during PIE before stopping (default: false)");
	SaveWorld.bRequired = false;
	Schema.Add(TEXT("save_world"), SaveWorld);

	return Schema;
}

FMcpToolResult UStopPieTool::Execute(
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	bool bSaveWorld = GetBoolArgOrDefault(Arguments, TEXT("save_world"), false);

	UE_LOG(LogYesUeMcp, Log, TEXT("stop-pie: Stopping PIE session (save=%d)"), bSaveWorld);

	// Check if PIE is running
	if (!GEditor->IsPlaySessionInProgress())
	{
		TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
		Result->SetBoolField(TEXT("success"), true);
		Result->SetStringField(TEXT("state"), TEXT("not_running"));
		Result->SetStringField(TEXT("message"), TEXT("No PIE session was running"));
		return FMcpToolResult::Json(Result);
	}

	// Calculate session duration
	// Note: We don't have exact start time, so we report what we can
	const double StopTime = FPlatformTime::Seconds();

	// Request end of play session
	GEditor->RequestEndPlayMap();

	// Wait briefly for PIE to actually stop
	const double WaitStart = FPlatformTime::Seconds();
	while (GEditor->IsPlaySessionInProgress() && (FPlatformTime::Seconds() - WaitStart) < 5.0)
	{
		FPlatformProcess::Sleep(0.05f);
	}

	// Check if PIE actually stopped
	if (GEditor->IsPlaySessionInProgress())
	{
		return FMcpToolResult::Error(TEXT("Failed to stop PIE session within timeout"));
	}

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("state"), TEXT("stopped"));
	Result->SetStringField(TEXT("message"), TEXT("PIE session stopped"));

	UE_LOG(LogYesUeMcp, Log, TEXT("stop-pie: PIE session stopped"));

	return FMcpToolResult::Json(Result);
}
