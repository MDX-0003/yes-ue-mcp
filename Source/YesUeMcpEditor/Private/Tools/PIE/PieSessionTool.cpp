// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/PIE/PieSessionTool.h"
#include "YesUeMcpEditor.h"
#include "Editor.h"
#include "LevelEditor.h"
#include "LevelEditorSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Guid.h"

FString UPieSessionTool::GetToolDescription() const
{
	return TEXT("Control PIE (Play-In-Editor) sessions. Actions: 'start' (begin PIE), 'stop' (end PIE), 'pause', 'resume', 'get-state' (query session info).");
}

TMap<FString, FMcpSchemaProperty> UPieSessionTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty Action;
	Action.Type = TEXT("string");
	Action.Description = TEXT("Action to perform: 'start', 'stop', 'pause', 'resume', or 'get-state'");
	Action.bRequired = true;
	Schema.Add(TEXT("action"), Action);

	FMcpSchemaProperty Mode;
	Mode.Type = TEXT("string");
	Mode.Description = TEXT("[start] PIE launch mode: 'viewport' (default), 'new_window', or 'standalone'");
	Mode.bRequired = false;
	Schema.Add(TEXT("mode"), Mode);

	FMcpSchemaProperty MapPath;
	MapPath.Type = TEXT("string");
	MapPath.Description = TEXT("[start] Optional map to load (e.g., '/Game/Maps/TestLevel')");
	MapPath.bRequired = false;
	Schema.Add(TEXT("map"), MapPath);

	FMcpSchemaProperty TimeoutSeconds;
	TimeoutSeconds.Type = TEXT("number");
	TimeoutSeconds.Description = TEXT("[start] Timeout in seconds to wait for PIE ready (default: 30)");
	TimeoutSeconds.bRequired = false;
	Schema.Add(TEXT("timeout"), TimeoutSeconds);

	FMcpSchemaProperty Include;
	Include.Type = TEXT("array");
	Include.Description = TEXT("[get-state] What to include: 'world', 'players'. Default: all.");
	Include.bRequired = false;
	Schema.Add(TEXT("include"), Include);

	return Schema;
}

FMcpToolResult UPieSessionTool::Execute(
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	FString Action = GetStringArg(Arguments, TEXT("action")).ToLower();

	if (Action == TEXT("start"))
	{
		return ExecuteStart(Arguments);
	}
	else if (Action == TEXT("stop"))
	{
		return ExecuteStop(Arguments);
	}
	else if (Action == TEXT("pause"))
	{
		return ExecutePause(Arguments);
	}
	else if (Action == TEXT("resume"))
	{
		return ExecuteResume(Arguments);
	}
	else if (Action == TEXT("get-state") || Action == TEXT("state") || Action == TEXT("status"))
	{
		return ExecuteGetState(Arguments);
	}
	else
	{
		return FMcpToolResult::Error(FString::Printf(
			TEXT("Unknown action: '%s'. Valid actions: start, stop, pause, resume, get-state"), *Action));
	}
}

FMcpToolResult UPieSessionTool::ExecuteStart(const TSharedPtr<FJsonObject>& Arguments)
{
	FString Mode = GetStringArgOrDefault(Arguments, TEXT("mode"), TEXT("viewport"));
	FString MapPath = GetStringArgOrDefault(Arguments, TEXT("map"));
	float Timeout = GetFloatArgOrDefault(Arguments, TEXT("timeout"), 30.0f);

	UE_LOG(LogYesUeMcp, Log, TEXT("pie-session: Starting PIE (mode=%s, map=%s)"),
		*Mode, MapPath.IsEmpty() ? TEXT("current") : *MapPath);

	// Check if PIE is already running
	if (GEditor->IsPlaySessionInProgress())
	{
		UWorld* PIEWorld = GetPIEWorld();
		if (PIEWorld)
		{
			TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
			Result->SetBoolField(TEXT("success"), true);
			Result->SetStringField(TEXT("session_id"), GenerateSessionId());
			Result->SetStringField(TEXT("world_name"), PIEWorld->GetName());
			Result->SetStringField(TEXT("state"), TEXT("already_running"));
			return FMcpToolResult::Json(Result);
		}
	}

	// Load specific map if requested
	if (!MapPath.IsEmpty())
	{
		ULevelEditorSubsystem* LevelEditorSubsystem = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();
		if (LevelEditorSubsystem && !LevelEditorSubsystem->LoadLevel(MapPath))
		{
			return FMcpToolResult::Error(FString::Printf(TEXT("Failed to load map: %s"), *MapPath));
		}
	}

	// Configure PIE settings
	FRequestPlaySessionParams Params;
	Params.WorldType = EPlaySessionWorldType::PlayInEditor;

	if (Mode.Equals(TEXT("new_window"), ESearchCase::IgnoreCase) ||
		Mode.Equals(TEXT("standalone"), ESearchCase::IgnoreCase))
	{
		Params.DestinationSlateViewport = nullptr;
	}
	else // viewport (default)
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		TSharedPtr<IAssetViewport> ActiveViewport = LevelEditorModule.GetFirstActiveViewport();
		if (ActiveViewport.IsValid())
		{
			Params.DestinationSlateViewport = ActiveViewport;
		}
	}

	GEditor->RequestPlaySession(Params);

	if (!WaitForPIEReady(Timeout))
	{
		return FMcpToolResult::Error(FString::Printf(TEXT("PIE did not start within %.0f seconds"), Timeout));
	}

	UWorld* PIEWorld = GetPIEWorld();
	if (!PIEWorld)
	{
		return FMcpToolResult::Error(TEXT("PIE started but could not find PIE world"));
	}

	// Get player info
	TArray<double> PlayerStartLocation = {0, 0, 0};
	TArray<AActor*> PlayerStarts;
	UGameplayStatics::GetAllActorsOfClass(PIEWorld, APlayerStart::StaticClass(), PlayerStarts);
	if (PlayerStarts.Num() > 0)
	{
		FVector Loc = PlayerStarts[0]->GetActorLocation();
		PlayerStartLocation = {Loc.X, Loc.Y, Loc.Z};
	}

	APlayerController* PC = PIEWorld->GetFirstPlayerController();
	FString PlayerPawnName = PC && PC->GetPawn() ? PC->GetPawn()->GetName() : TEXT("None");

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("session_id"), GenerateSessionId());
	Result->SetStringField(TEXT("world_name"), PIEWorld->GetName());
	Result->SetStringField(TEXT("state"), TEXT("running"));
	Result->SetStringField(TEXT("player_pawn"), PlayerPawnName);

	TArray<TSharedPtr<FJsonValue>> StartLocArray;
	for (double Val : PlayerStartLocation)
	{
		StartLocArray.Add(MakeShareable(new FJsonValueNumber(Val)));
	}
	Result->SetArrayField(TEXT("player_start"), StartLocArray);

	UE_LOG(LogYesUeMcp, Log, TEXT("pie-session: Started (world=%s)"), *PIEWorld->GetName());
	return FMcpToolResult::Json(Result);
}

FMcpToolResult UPieSessionTool::ExecuteStop(const TSharedPtr<FJsonObject>& Arguments)
{
	if (!GEditor->IsPlaySessionInProgress())
	{
		TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
		Result->SetBoolField(TEXT("success"), true);
		Result->SetStringField(TEXT("state"), TEXT("not_running"));
		return FMcpToolResult::Json(Result);
	}

	GEditor->RequestEndPlayMap();

	const double WaitStart = FPlatformTime::Seconds();
	while (GEditor->IsPlaySessionInProgress() && (FPlatformTime::Seconds() - WaitStart) < 5.0)
	{
		FPlatformProcess::Sleep(0.05f);
	}

	if (GEditor->IsPlaySessionInProgress())
	{
		return FMcpToolResult::Error(TEXT("Failed to stop PIE within timeout"));
	}

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("state"), TEXT("stopped"));

	UE_LOG(LogYesUeMcp, Log, TEXT("pie-session: Stopped"));
	return FMcpToolResult::Json(Result);
}

FMcpToolResult UPieSessionTool::ExecutePause(const TSharedPtr<FJsonObject>& Arguments)
{
	if (!GEditor->IsPlaySessionInProgress())
	{
		return FMcpToolResult::Error(TEXT("No PIE session running"));
	}

	UWorld* PIEWorld = GetPIEWorld();
	if (!PIEWorld)
	{
		return FMcpToolResult::Error(TEXT("PIE world not found"));
	}

	if (PIEWorld->IsPaused())
	{
		TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
		Result->SetBoolField(TEXT("success"), true);
		Result->SetBoolField(TEXT("paused"), true);
		Result->SetStringField(TEXT("message"), TEXT("Already paused"));
		return FMcpToolResult::Json(Result);
	}

	if (GEditor->PlayWorld)
	{
		GEditor->PlayWorld->bDebugPauseExecution = true;
	}

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetBoolField(TEXT("paused"), true);

	UE_LOG(LogYesUeMcp, Log, TEXT("pie-session: Paused"));
	return FMcpToolResult::Json(Result);
}

FMcpToolResult UPieSessionTool::ExecuteResume(const TSharedPtr<FJsonObject>& Arguments)
{
	if (!GEditor->IsPlaySessionInProgress())
	{
		return FMcpToolResult::Error(TEXT("No PIE session running"));
	}

	UWorld* PIEWorld = GetPIEWorld();
	if (!PIEWorld)
	{
		return FMcpToolResult::Error(TEXT("PIE world not found"));
	}

	if (!PIEWorld->IsPaused())
	{
		TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
		Result->SetBoolField(TEXT("success"), true);
		Result->SetBoolField(TEXT("paused"), false);
		Result->SetStringField(TEXT("message"), TEXT("Already running"));
		return FMcpToolResult::Json(Result);
	}

	if (GEditor->PlayWorld)
	{
		GEditor->PlayWorld->bDebugPauseExecution = false;
	}

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetBoolField(TEXT("paused"), false);

	UE_LOG(LogYesUeMcp, Log, TEXT("pie-session: Resumed"));
	return FMcpToolResult::Json(Result);
}

FMcpToolResult UPieSessionTool::ExecuteGetState(const TSharedPtr<FJsonObject>& Arguments)
{
	TSet<FString> IncludeSet;
	if (Arguments->HasField(TEXT("include")))
	{
		const TArray<TSharedPtr<FJsonValue>>* IncludeArray;
		if (Arguments->TryGetArrayField(TEXT("include"), IncludeArray))
		{
			for (const TSharedPtr<FJsonValue>& Val : *IncludeArray)
			{
				IncludeSet.Add(Val->AsString().ToLower());
			}
		}
	}

	bool bIncludeWorld = IncludeSet.Num() == 0 || IncludeSet.Contains(TEXT("world"));
	bool bIncludePlayers = IncludeSet.Num() == 0 || IncludeSet.Contains(TEXT("players"));

	bool bRunning = GEditor->IsPlaySessionInProgress();

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("running"), bRunning);

	if (!bRunning)
	{
		Result->SetStringField(TEXT("state"), TEXT("not_running"));
		return FMcpToolResult::Json(Result);
	}

	UWorld* PIEWorld = GetPIEWorld();
	if (!PIEWorld)
	{
		Result->SetStringField(TEXT("state"), TEXT("initializing"));
		return FMcpToolResult::Json(Result);
	}

	Result->SetStringField(TEXT("state"), TEXT("running"));
	Result->SetBoolField(TEXT("paused"), PIEWorld->IsPaused());

	if (bIncludeWorld)
	{
		Result->SetObjectField(TEXT("world"), GetWorldInfo(PIEWorld));
	}

	if (bIncludePlayers)
	{
		Result->SetArrayField(TEXT("players"), GetPlayersInfo(PIEWorld));
	}

	return FMcpToolResult::Json(Result);
}

FString UPieSessionTool::GenerateSessionId() const
{
	return FString::Printf(TEXT("pie_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Short));
}

UWorld* UPieSessionTool::GetPIEWorld() const
{
	for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
	{
		if (WorldContext.WorldType == EWorldType::PIE && WorldContext.World())
		{
			return WorldContext.World();
		}
	}
	return nullptr;
}

bool UPieSessionTool::WaitForPIEReady(float TimeoutSeconds) const
{
	const double EndTime = FPlatformTime::Seconds() + TimeoutSeconds;

	while (FPlatformTime::Seconds() < EndTime)
	{
		FPlatformProcess::Sleep(0.1f);

		if (GEditor->IsPlaySessionInProgress())
		{
			UWorld* PIEWorld = GetPIEWorld();
			if (PIEWorld && PIEWorld->GetFirstPlayerController())
			{
				return true;
			}
		}
	}
	return false;
}

TSharedPtr<FJsonObject> UPieSessionTool::GetWorldInfo(UWorld* PIEWorld) const
{
	TSharedPtr<FJsonObject> WorldInfo = MakeShareable(new FJsonObject);
	WorldInfo->SetStringField(TEXT("name"), PIEWorld->GetName());
	WorldInfo->SetStringField(TEXT("map_name"), PIEWorld->GetMapName());
	WorldInfo->SetNumberField(TEXT("time_seconds"), PIEWorld->GetTimeSeconds());

	int32 ActorCount = 0;
	for (TActorIterator<AActor> It(PIEWorld); It; ++It) { ActorCount++; }
	WorldInfo->SetNumberField(TEXT("actor_count"), ActorCount);

	return WorldInfo;
}

TSharedPtr<FJsonArray> UPieSessionTool::GetPlayersInfo(UWorld* PIEWorld) const
{
	TSharedPtr<FJsonArray> PlayersArray = MakeShareable(new FJsonArray);

	int32 PlayerIndex = 0;
	for (FConstPlayerControllerIterator It = PIEWorld->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) continue;

		TSharedPtr<FJsonObject> PlayerInfo = MakeShareable(new FJsonObject);
		PlayerInfo->SetNumberField(TEXT("player_index"), PlayerIndex);
		PlayerInfo->SetStringField(TEXT("controller_name"), PC->GetName());

		if (APawn* Pawn = PC->GetPawn())
		{
			PlayerInfo->SetStringField(TEXT("pawn_name"), Pawn->GetName());
			PlayerInfo->SetStringField(TEXT("pawn_class"), Pawn->GetClass()->GetName());

			FVector Loc = Pawn->GetActorLocation();
			TArray<TSharedPtr<FJsonValue>> LocArray;
			LocArray.Add(MakeShareable(new FJsonValueNumber(Loc.X)));
			LocArray.Add(MakeShareable(new FJsonValueNumber(Loc.Y)));
			LocArray.Add(MakeShareable(new FJsonValueNumber(Loc.Z)));
			PlayerInfo->SetArrayField(TEXT("location"), LocArray);

			PlayerInfo->SetNumberField(TEXT("speed"), Pawn->GetVelocity().Size());
		}

		PlayersArray->Add(MakeShareable(new FJsonValueObject(PlayerInfo)));
		PlayerIndex++;
	}

	return PlayersArray;
}
