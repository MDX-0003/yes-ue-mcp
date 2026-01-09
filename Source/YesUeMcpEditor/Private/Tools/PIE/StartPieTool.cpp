// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/PIE/StartPieTool.h"
#include "YesUeMcpEditor.h"
#include "Editor.h"
#include "LevelEditor.h"
#include "LevelEditorSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Guid.h"

FString UStartPieTool::GetToolDescription() const
{
	return TEXT("Start a Play-In-Editor (PIE) session for gameplay testing. Returns session ID, world info, and player start location.");
}

TMap<FString, FMcpSchemaProperty> UStartPieTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty Mode;
	Mode.Type = TEXT("string");
	Mode.Description = TEXT("PIE launch mode: 'viewport' (in current viewport), 'new_window' (separate window), or 'standalone' (separate process, default)");
	Mode.bRequired = false;
	Schema.Add(TEXT("mode"), Mode);

	FMcpSchemaProperty MapPath;
	MapPath.Type = TEXT("string");
	MapPath.Description = TEXT("Optional map to load (e.g., '/Game/Maps/TestLevel'). Uses current map if not specified.");
	MapPath.bRequired = false;
	Schema.Add(TEXT("map"), MapPath);

	FMcpSchemaProperty TimeoutSeconds;
	TimeoutSeconds.Type = TEXT("number");
	TimeoutSeconds.Description = TEXT("Timeout in seconds to wait for PIE to be ready (default: 30)");
	TimeoutSeconds.bRequired = false;
	Schema.Add(TEXT("timeout"), TimeoutSeconds);

	return Schema;
}

FMcpToolResult UStartPieTool::Execute(
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	FString Mode = GetStringArgOrDefault(Arguments, TEXT("mode"), TEXT("viewport"));
	FString MapPath = GetStringArgOrDefault(Arguments, TEXT("map"));
	float Timeout = GetFloatArgOrDefault(Arguments, TEXT("timeout"), 30.0f);

	UE_LOG(LogYesUeMcp, Log, TEXT("start-pie: Starting PIE session (mode=%s, map=%s)"),
		*Mode, MapPath.IsEmpty() ? TEXT("current") : *MapPath);

	// Check if PIE is already running
	if (GEditor->IsPlaySessionInProgress())
	{
		// Already in PIE - return current session info
		UWorld* PIEWorld = GetPIEWorld();
		if (PIEWorld)
		{
			TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
			Result->SetBoolField(TEXT("success"), true);
			Result->SetStringField(TEXT("session_id"), GenerateSessionId());
			Result->SetStringField(TEXT("world_name"), PIEWorld->GetName());
			Result->SetStringField(TEXT("state"), TEXT("already_running"));
			Result->SetStringField(TEXT("message"), TEXT("PIE session was already running"));
			return FMcpToolResult::Json(Result);
		}
	}

	// Load specific map if requested
	if (!MapPath.IsEmpty())
	{
		ULevelEditorSubsystem* LevelEditorSubsystem = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();
		if (LevelEditorSubsystem)
		{
			if (!LevelEditorSubsystem->LoadLevel(MapPath))
			{
				return FMcpToolResult::Error(FString::Printf(TEXT("Failed to load map: %s"), *MapPath));
			}
			UE_LOG(LogYesUeMcp, Log, TEXT("start-pie: Loaded map %s"), *MapPath);
		}
	}

	// Configure PIE settings based on mode
	FRequestPlaySessionParams Params;
	Params.WorldType = EPlaySessionWorldType::PlayInEditor;

	if (Mode.Equals(TEXT("standalone"), ESearchCase::IgnoreCase))
	{
		Params.DestinationSlateViewport = nullptr;
		// Standalone process mode - not directly supported via RequestPlaySession
		// Fall back to new window for now
		UE_LOG(LogYesUeMcp, Log, TEXT("start-pie: Using new_window mode (standalone not directly supported)"));
	}
	else if (Mode.Equals(TEXT("new_window"), ESearchCase::IgnoreCase))
	{
		Params.DestinationSlateViewport = nullptr;
	}
	else // viewport (default)
	{
		// Use active viewport
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		TSharedPtr<IAssetViewport> ActiveViewport = LevelEditorModule.GetFirstActiveViewport();
		if (ActiveViewport.IsValid())
		{
			Params.DestinationSlateViewport = ActiveViewport;
		}
	}

	// Request PIE session
	GEditor->RequestPlaySession(Params);

	// Wait for PIE to be ready
	if (!WaitForPIEReady(Timeout))
	{
		return FMcpToolResult::Error(FString::Printf(TEXT("PIE did not start within %.0f seconds timeout"), Timeout));
	}

	// Get the PIE world
	UWorld* PIEWorld = GetPIEWorld();
	if (!PIEWorld)
	{
		return FMcpToolResult::Error(TEXT("PIE started but could not find PIE world"));
	}

	// Get player start location
	TArray<double> PlayerStartLocation = {0, 0, 0};
	TArray<AActor*> PlayerStarts;
	UGameplayStatics::GetAllActorsOfClass(PIEWorld, APlayerStart::StaticClass(), PlayerStarts);
	if (PlayerStarts.Num() > 0)
	{
		FVector Loc = PlayerStarts[0]->GetActorLocation();
		PlayerStartLocation = {Loc.X, Loc.Y, Loc.Z};
	}

	// Get player controller info
	APlayerController* PC = PIEWorld->GetFirstPlayerController();
	FString PlayerPawnName = TEXT("None");
	if (PC && PC->GetPawn())
	{
		PlayerPawnName = PC->GetPawn()->GetName();
	}

	// Build result
	FString SessionId = GenerateSessionId();

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("session_id"), SessionId);
	Result->SetStringField(TEXT("world_name"), PIEWorld->GetName());
	Result->SetStringField(TEXT("state"), TEXT("running"));

	// Player start as array
	TArray<TSharedPtr<FJsonValue>> StartLocArray;
	for (double Val : PlayerStartLocation)
	{
		StartLocArray.Add(MakeShareable(new FJsonValueNumber(Val)));
	}
	Result->SetArrayField(TEXT("player_start"), StartLocArray);

	Result->SetStringField(TEXT("player_pawn"), PlayerPawnName);
	Result->SetStringField(TEXT("mode"), Mode);

	UE_LOG(LogYesUeMcp, Log, TEXT("start-pie: PIE session started (world=%s, pawn=%s)"),
		*PIEWorld->GetName(), *PlayerPawnName);

	return FMcpToolResult::Json(Result);
}

FString UStartPieTool::GenerateSessionId() const
{
	return FString::Printf(TEXT("pie_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Short));
}

UWorld* UStartPieTool::GetPIEWorld() const
{
	// Find the PIE world
	for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
	{
		if (WorldContext.WorldType == EWorldType::PIE && WorldContext.World())
		{
			return WorldContext.World();
		}
	}
	return nullptr;
}

bool UStartPieTool::WaitForPIEReady(float TimeoutSeconds) const
{
	const double StartTime = FPlatformTime::Seconds();
	const double EndTime = StartTime + TimeoutSeconds;

	while (FPlatformTime::Seconds() < EndTime)
	{
		// Process engine loop to allow PIE to initialize
		FPlatformProcess::Sleep(0.1f);

		// Check if PIE is ready
		if (GEditor->IsPlaySessionInProgress())
		{
			UWorld* PIEWorld = GetPIEWorld();
			if (PIEWorld && PIEWorld->GetFirstPlayerController())
			{
				// PIE is ready with a player controller
				return true;
			}
		}
	}

	return false;
}
