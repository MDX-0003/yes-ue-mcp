// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/Write/SpawnActorTool.h"
#include "Utils/McpAssetModifier.h"
#include "YesUeMcpEditor.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMeshActor.h"
#include "Camera/CameraActor.h"
#include "Engine/TriggerBox.h"
#include "Engine/TriggerSphere.h"
#include "ScopedTransaction.h"

FString USpawnActorTool::GetToolDescription() const
{
	return TEXT("Spawn an actor in the currently open level. Supports native classes and Blueprint actors.");
}

TMap<FString, FMcpSchemaProperty> USpawnActorTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty ActorClass;
	ActorClass.Type = TEXT("string");
	ActorClass.Description = TEXT("Actor class name (e.g., 'PointLight', 'StaticMeshActor') or Blueprint path (e.g., '/Game/BP_Enemy')");
	ActorClass.bRequired = true;
	Schema.Add(TEXT("actor_class"), ActorClass);

	FMcpSchemaProperty Location;
	Location.Type = TEXT("array");
	Location.Description = TEXT("Spawn location as [x, y, z]");
	Location.bRequired = false;
	Schema.Add(TEXT("location"), Location);

	FMcpSchemaProperty Rotation;
	Rotation.Type = TEXT("array");
	Rotation.Description = TEXT("Spawn rotation as [pitch, yaw, roll]");
	Rotation.bRequired = false;
	Schema.Add(TEXT("rotation"), Rotation);

	FMcpSchemaProperty Label;
	Label.Type = TEXT("string");
	Label.Description = TEXT("Actor label in the World Outliner");
	Label.bRequired = false;
	Schema.Add(TEXT("label"), Label);

	return Schema;
}

TArray<FString> USpawnActorTool::GetRequiredParams() const
{
	return { TEXT("actor_class") };
}

FMcpToolResult USpawnActorTool::Execute(
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	FString ActorClass = GetStringArgOrDefault(Arguments, TEXT("actor_class"));
	FString Label = GetStringArgOrDefault(Arguments, TEXT("label"));

	// Get location
	FVector Location(0, 0, 0);
	const TArray<TSharedPtr<FJsonValue>>* LocationArray;
	if (Arguments->TryGetArrayField(TEXT("location"), LocationArray) && LocationArray->Num() >= 3)
	{
		Location.X = (*LocationArray)[0]->AsNumber();
		Location.Y = (*LocationArray)[1]->AsNumber();
		Location.Z = (*LocationArray)[2]->AsNumber();
	}

	// Get rotation
	FRotator Rotation(0, 0, 0);
	const TArray<TSharedPtr<FJsonValue>>* RotationArray;
	if (Arguments->TryGetArrayField(TEXT("rotation"), RotationArray) && RotationArray->Num() >= 3)
	{
		Rotation.Pitch = (*RotationArray)[0]->AsNumber();
		Rotation.Yaw = (*RotationArray)[1]->AsNumber();
		Rotation.Roll = (*RotationArray)[2]->AsNumber();
	}

	if (ActorClass.IsEmpty())
	{
		return FMcpToolResult::Error(TEXT("actor_class is required"));
	}

	UE_LOG(LogYesUeMcp, Log, TEXT("spawn-actor: %s at (%f, %f, %f)"),
		*ActorClass, Location.X, Location.Y, Location.Z);

	// Get the editor world
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return FMcpToolResult::Error(TEXT("No world available. Open a level first."));
	}

	// Begin transaction
	TSharedPtr<FScopedTransaction> Transaction = FMcpAssetModifier::BeginTransaction(
		FText::Format(NSLOCTEXT("MCP", "SpawnActor", "Spawn {0}"), FText::FromString(ActorClass)));

	UClass* SpawnClass = nullptr;

	// Check if it's a Blueprint path
	if (ActorClass.StartsWith(TEXT("/")))
	{
		FString LoadError;
		UBlueprint* Blueprint = FMcpAssetModifier::LoadAssetByPath<UBlueprint>(ActorClass, LoadError);
		if (Blueprint && Blueprint->GeneratedClass)
		{
			SpawnClass = Blueprint->GeneratedClass;
		}
		else
		{
			return FMcpToolResult::Error(FString::Printf(TEXT("Blueprint not found or invalid: %s"), *ActorClass));
		}
	}
	else
	{
		// Try to find native class
		if (ActorClass.Equals(TEXT("PointLight"), ESearchCase::IgnoreCase))
		{
			SpawnClass = APointLight::StaticClass();
		}
		else if (ActorClass.Equals(TEXT("SpotLight"), ESearchCase::IgnoreCase))
		{
			SpawnClass = ASpotLight::StaticClass();
		}
		else if (ActorClass.Equals(TEXT("DirectionalLight"), ESearchCase::IgnoreCase))
		{
			SpawnClass = ADirectionalLight::StaticClass();
		}
		else if (ActorClass.Equals(TEXT("StaticMeshActor"), ESearchCase::IgnoreCase))
		{
			SpawnClass = AStaticMeshActor::StaticClass();
		}
		else if (ActorClass.Equals(TEXT("CameraActor"), ESearchCase::IgnoreCase))
		{
			SpawnClass = ACameraActor::StaticClass();
		}
		else if (ActorClass.Equals(TEXT("TriggerBox"), ESearchCase::IgnoreCase))
		{
			SpawnClass = ATriggerBox::StaticClass();
		}
		else if (ActorClass.Equals(TEXT("TriggerSphere"), ESearchCase::IgnoreCase))
		{
			SpawnClass = ATriggerSphere::StaticClass();
		}
		else
		{
			// Try to find by name
			SpawnClass = FindFirstObject<UClass>(*ActorClass, EFindFirstObjectOptions::ExactClass);
			if (!SpawnClass)
			{
				SpawnClass = FindFirstObject<UClass>(*(TEXT("A") + ActorClass), EFindFirstObjectOptions::ExactClass);
			}
		}
	}

	if (!SpawnClass)
	{
		return FMcpToolResult::Error(FString::Printf(TEXT("Actor class not found: %s"), *ActorClass));
	}

	// Spawn the actor
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* SpawnedActor = World->SpawnActor<AActor>(SpawnClass, Location, Rotation, SpawnParams);

	if (!SpawnedActor)
	{
		return FMcpToolResult::Error(TEXT("Failed to spawn actor"));
	}

	// Set label if provided
	if (!Label.IsEmpty())
	{
		SpawnedActor->SetActorLabel(Label);
	}

	// Mark level as dirty
	World->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("actor_name"), SpawnedActor->GetName());
	Result->SetStringField(TEXT("actor_label"), SpawnedActor->GetActorLabel());
	Result->SetStringField(TEXT("actor_class"), SpawnClass->GetName());

	TArray<TSharedPtr<FJsonValue>> LocationJson;
	LocationJson.Add(MakeShared<FJsonValueNumber>(Location.X));
	LocationJson.Add(MakeShared<FJsonValueNumber>(Location.Y));
	LocationJson.Add(MakeShared<FJsonValueNumber>(Location.Z));
	Result->SetArrayField(TEXT("location"), LocationJson);

	Result->SetBoolField(TEXT("needs_save"), true);

	UE_LOG(LogYesUeMcp, Log, TEXT("spawn-actor: Spawned %s"), *SpawnedActor->GetName());

	return FMcpToolResult::Json(Result);
}
