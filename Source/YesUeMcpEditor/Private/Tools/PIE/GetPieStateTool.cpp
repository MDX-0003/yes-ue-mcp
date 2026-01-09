// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/PIE/GetPieStateTool.h"
#include "YesUeMcpEditor.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "Engine/Engine.h"
#include "Stats/Stats.h"

FString UGetPieStateTool::GetToolDescription() const
{
	return TEXT("Get the current state of the PIE session including world info, player states, and optionally performance stats.");
}

TMap<FString, FMcpSchemaProperty> UGetPieStateTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty Include;
	Include.Type = TEXT("array");
	Include.Description = TEXT("What to include in response: 'world', 'players', 'stats'. Default: all.");
	Include.bRequired = false;
	Schema.Add(TEXT("include"), Include);

	return Schema;
}

FMcpToolResult UGetPieStateTool::Execute(
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	// Parse include options
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

	// Default to include all
	bool bIncludeWorld = IncludeSet.Num() == 0 || IncludeSet.Contains(TEXT("world"));
	bool bIncludePlayers = IncludeSet.Num() == 0 || IncludeSet.Contains(TEXT("players"));
	bool bIncludeStats = IncludeSet.Num() == 0 || IncludeSet.Contains(TEXT("stats"));

	// Check if PIE is running
	bool bRunning = GEditor->IsPlaySessionInProgress();

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("running"), bRunning);

	if (!bRunning)
	{
		Result->SetStringField(TEXT("state"), TEXT("not_running"));
		return FMcpToolResult::Json(Result);
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
		Result->SetStringField(TEXT("state"), TEXT("initializing"));
		return FMcpToolResult::Json(Result);
	}

	Result->SetStringField(TEXT("state"), TEXT("running"));

	// Add world info
	if (bIncludeWorld)
	{
		Result->SetObjectField(TEXT("world"), GetWorldInfo(PIEWorld));
	}

	// Add players info
	if (bIncludePlayers)
	{
		Result->SetArrayField(TEXT("players"), GetPlayersInfo(PIEWorld));
	}

	// Add stats
	if (bIncludeStats)
	{
		Result->SetObjectField(TEXT("stats"), GetStatsInfo());
	}

	return FMcpToolResult::Json(Result);
}

TSharedPtr<FJsonObject> UGetPieStateTool::GetWorldInfo(UWorld* PIEWorld) const
{
	TSharedPtr<FJsonObject> WorldInfo = MakeShareable(new FJsonObject);

	WorldInfo->SetStringField(TEXT("name"), PIEWorld->GetName());
	WorldInfo->SetStringField(TEXT("map_name"), PIEWorld->GetMapName());

	// Get game time
	float GameTime = PIEWorld->GetTimeSeconds();
	WorldInfo->SetNumberField(TEXT("time_seconds"), GameTime);

	// Count actors
	int32 ActorCount = 0;
	for (TActorIterator<AActor> It(PIEWorld); It; ++It)
	{
		ActorCount++;
	}
	WorldInfo->SetNumberField(TEXT("actor_count"), ActorCount);

	// Is game paused?
	WorldInfo->SetBoolField(TEXT("paused"), PIEWorld->IsPaused());

	return WorldInfo;
}

TSharedPtr<FJsonArray> UGetPieStateTool::GetPlayersInfo(UWorld* PIEWorld) const
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

		APawn* Pawn = PC->GetPawn();
		if (Pawn)
		{
			PlayerInfo->SetStringField(TEXT("pawn_name"), Pawn->GetName());
			PlayerInfo->SetStringField(TEXT("pawn_class"), Pawn->GetClass()->GetName());

			// Location
			FVector Loc = Pawn->GetActorLocation();
			TArray<TSharedPtr<FJsonValue>> LocArray;
			LocArray.Add(MakeShareable(new FJsonValueNumber(Loc.X)));
			LocArray.Add(MakeShareable(new FJsonValueNumber(Loc.Y)));
			LocArray.Add(MakeShareable(new FJsonValueNumber(Loc.Z)));
			PlayerInfo->SetArrayField(TEXT("location"), LocArray);

			// Rotation
			FRotator Rot = Pawn->GetActorRotation();
			TArray<TSharedPtr<FJsonValue>> RotArray;
			RotArray.Add(MakeShareable(new FJsonValueNumber(Rot.Pitch)));
			RotArray.Add(MakeShareable(new FJsonValueNumber(Rot.Yaw)));
			RotArray.Add(MakeShareable(new FJsonValueNumber(Rot.Roll)));
			PlayerInfo->SetArrayField(TEXT("rotation"), RotArray);

			// Velocity (if moving)
			FVector Velocity = Pawn->GetVelocity();
			PlayerInfo->SetNumberField(TEXT("speed"), Velocity.Size());

			// Health if character
			ACharacter* Character = Cast<ACharacter>(Pawn);
			if (Character)
			{
				PlayerInfo->SetBoolField(TEXT("is_character"), true);
			}
		}
		else
		{
			PlayerInfo->SetStringField(TEXT("pawn_name"), TEXT("None"));
		}

		PlayersArray->Add(MakeShareable(new FJsonValueObject(PlayerInfo)));
		PlayerIndex++;
	}

	return PlayersArray;
}

TSharedPtr<FJsonObject> UGetPieStateTool::GetStatsInfo() const
{
	TSharedPtr<FJsonObject> Stats = MakeShareable(new FJsonObject);

	// Get FPS info
	extern ENGINE_API float GAverageFPS;
	extern ENGINE_API float GAverageMS;

	Stats->SetNumberField(TEXT("fps"), GAverageFPS);
	Stats->SetNumberField(TEXT("frame_time_ms"), GAverageMS);

	return Stats;
}
