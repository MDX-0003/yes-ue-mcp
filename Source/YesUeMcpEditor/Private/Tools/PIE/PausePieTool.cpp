// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/PIE/PausePieTool.h"
#include "YesUeMcpEditor.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

FString UPausePieTool::GetToolDescription() const
{
	return TEXT("Pause or resume the PIE session. If paused, resumes. If running, pauses.");
}

TMap<FString, FMcpSchemaProperty> UPausePieTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty Paused;
	Paused.Type = TEXT("boolean");
	Paused.Description = TEXT("Set to true to pause, false to resume. If not provided, toggles current state.");
	Paused.bRequired = false;
	Schema.Add(TEXT("paused"), Paused);

	return Schema;
}

FMcpToolResult UPausePieTool::Execute(
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	// Check if PIE is running
	if (!GEditor->IsPlaySessionInProgress())
	{
		return FMcpToolResult::Error(TEXT("No PIE session is running. Start PIE first with start-pie."));
	}

	// Find the PIE world
	UWorld* PIEWorld = nullptr;
	for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
	{
		if (WorldContext.WorldType == EWorldType::PIE && WorldContext.World())
		{
			PIEWorld = WorldContext.World();
			break;
		}
	}

	if (!PIEWorld)
	{
		return FMcpToolResult::Error(TEXT("PIE world not found"));
	}

	// Determine target pause state
	bool bCurrentlyPaused = PIEWorld->IsPaused();
	bool bTargetPaused;

	if (Arguments->HasField(TEXT("paused")))
	{
		bTargetPaused = GetBoolArgOrDefault(Arguments, TEXT("paused"), true);
	}
	else
	{
		// Toggle
		bTargetPaused = !bCurrentlyPaused;
	}

	if (bTargetPaused == bCurrentlyPaused)
	{
		// Already in desired state
		TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
		Result->SetBoolField(TEXT("success"), true);
		Result->SetBoolField(TEXT("paused"), bCurrentlyPaused);
		Result->SetStringField(TEXT("message"), bCurrentlyPaused ? TEXT("Already paused") : TEXT("Already running"));
		return FMcpToolResult::Json(Result);
	}

	// Set pause state
	// Use the world settings pause method
	AWorldSettings* WorldSettings = PIEWorld->GetWorldSettings();
	if (WorldSettings)
	{
		WorldSettings->SetPauserPlayerState(bTargetPaused ? PIEWorld->GetFirstPlayerController()->PlayerState : nullptr);
	}

	// Alternative: Use console command
	if (bTargetPaused)
	{
		GEditor->PlayWorld->bDebugPauseExecution = true;
	}
	else
	{
		GEditor->PlayWorld->bDebugPauseExecution = false;
	}

	UE_LOG(LogYesUeMcp, Log, TEXT("pause-pie: PIE %s"), bTargetPaused ? TEXT("paused") : TEXT("resumed"));

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetBoolField(TEXT("paused"), bTargetPaused);
	Result->SetStringField(TEXT("message"), bTargetPaused ? TEXT("PIE paused") : TEXT("PIE resumed"));

	return FMcpToolResult::Json(Result);
}
