// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/PIE/PieSpawnActorTool.h"
#include "YesUeMcpEditor.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/Blueprint.h"
#include "UObject/UObjectGlobals.h"
#include "AssetRegistry/AssetRegistryModule.h"

FString UPieSpawnActorTool::GetToolDescription() const
{
	return TEXT("Spawn an actor in the PIE world. Supports both native C++ classes and Blueprint classes.");
}

TMap<FString, FMcpSchemaProperty> UPieSpawnActorTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty ActorClass;
	ActorClass.Type = TEXT("string");
	ActorClass.Description = TEXT("Class to spawn (e.g., 'Character', 'BP_Enemy', or '/Game/Blueprints/BP_Enemy')");
	ActorClass.bRequired = true;
	Schema.Add(TEXT("actor_class"), ActorClass);

	FMcpSchemaProperty Location;
	Location.Type = TEXT("array");
	Location.Description = TEXT("Spawn location as [X, Y, Z] (default: [0, 0, 0])");
	Location.bRequired = false;
	Schema.Add(TEXT("location"), Location);

	FMcpSchemaProperty Rotation;
	Rotation.Type = TEXT("array");
	Rotation.Description = TEXT("Spawn rotation as [Pitch, Yaw, Roll] (default: [0, 0, 0])");
	Rotation.bRequired = false;
	Schema.Add(TEXT("rotation"), Rotation);

	FMcpSchemaProperty Label;
	Label.Type = TEXT("string");
	Label.Description = TEXT("Optional label/name for the spawned actor");
	Label.bRequired = false;
	Schema.Add(TEXT("label"), Label);

	return Schema;
}

FMcpToolResult UPieSpawnActorTool::Execute(
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	FString ClassName = GetStringArg(Arguments, TEXT("actor_class"));
	FString Label = GetStringArgOrDefault(Arguments, TEXT("label"));

	// Parse location
	FVector Location = FVector::ZeroVector;
	if (Arguments->HasField(TEXT("location")))
	{
		const TArray<TSharedPtr<FJsonValue>>* LocArray;
		if (Arguments->TryGetArrayField(TEXT("location"), LocArray) && LocArray->Num() >= 3)
		{
			Location.X = (*LocArray)[0]->AsNumber();
			Location.Y = (*LocArray)[1]->AsNumber();
			Location.Z = (*LocArray)[2]->AsNumber();
		}
	}

	// Parse rotation
	FRotator Rotation = FRotator::ZeroRotator;
	if (Arguments->HasField(TEXT("rotation")))
	{
		const TArray<TSharedPtr<FJsonValue>>* RotArray;
		if (Arguments->TryGetArrayField(TEXT("rotation"), RotArray) && RotArray->Num() >= 3)
		{
			Rotation.Pitch = (*RotArray)[0]->AsNumber();
			Rotation.Yaw = (*RotArray)[1]->AsNumber();
			Rotation.Roll = (*RotArray)[2]->AsNumber();
		}
	}

	UE_LOG(LogYesUeMcp, Log, TEXT("pie-spawn-actor: Spawning %s at [%.0f, %.0f, %.0f]"),
		*ClassName, Location.X, Location.Y, Location.Z);

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

	// Find the actor class
	UClass* ActorClass = FindActorClass(ClassName);
	if (!ActorClass)
	{
		return FMcpToolResult::Error(FString::Printf(TEXT("Actor class not found: %s"), *ClassName));
	}

	// Validate it's an Actor subclass
	if (!ActorClass->IsChildOf(AActor::StaticClass()))
	{
		return FMcpToolResult::Error(FString::Printf(TEXT("Class is not an Actor: %s"), *ClassName));
	}

	// Spawn the actor
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	if (!Label.IsEmpty())
	{
		SpawnParams.Name = FName(*Label);
	}

	AActor* SpawnedActor = PIEWorld->SpawnActor<AActor>(ActorClass, Location, Rotation, SpawnParams);
	if (!SpawnedActor)
	{
		return FMcpToolResult::Error(FString::Printf(TEXT("Failed to spawn actor of class: %s"), *ClassName));
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("actor_name"), SpawnedActor->GetName());
	Result->SetStringField(TEXT("class"), ActorClass->GetName());

	// Return actual location (may differ due to collision adjustment)
	FVector ActualLoc = SpawnedActor->GetActorLocation();
	TArray<TSharedPtr<FJsonValue>> LocArray;
	LocArray.Add(MakeShareable(new FJsonValueNumber(ActualLoc.X)));
	LocArray.Add(MakeShareable(new FJsonValueNumber(ActualLoc.Y)));
	LocArray.Add(MakeShareable(new FJsonValueNumber(ActualLoc.Z)));
	Result->SetArrayField(TEXT("location"), LocArray);

	UE_LOG(LogYesUeMcp, Log, TEXT("pie-spawn-actor: Spawned %s as %s"),
		*ClassName, *SpawnedActor->GetName());

	return FMcpToolResult::Json(Result);
}

UClass* UPieSpawnActorTool::FindActorClass(const FString& ClassName) const
{
	// First, try to find as a native class
	UClass* FoundClass = FindObject<UClass>(nullptr, *ClassName);
	if (FoundClass)
	{
		return FoundClass;
	}

	// Try with common prefixes
	TArray<FString> PrefixesToTry = {
		FString::Printf(TEXT("/Script/Engine.%s"), *ClassName),
		FString::Printf(TEXT("/Script/CoreUObject.%s"), *ClassName),
	};

	for (const FString& Path : PrefixesToTry)
	{
		FoundClass = FindObject<UClass>(nullptr, *Path);
		if (FoundClass)
		{
			return FoundClass;
		}
	}

	// Try as a Blueprint path
	FString BlueprintPath = ClassName;

	// Handle short names by searching
	if (!BlueprintPath.StartsWith(TEXT("/")))
	{
		// Search asset registry for Blueprint with this name
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		TArray<FAssetData> Assets;
		FARFilter Filter;
		Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
		Filter.bRecursiveClasses = true;

		AssetRegistry.GetAssets(Filter, Assets);

		for (const FAssetData& Asset : Assets)
		{
			if (Asset.AssetName.ToString().Equals(ClassName, ESearchCase::IgnoreCase))
			{
				BlueprintPath = Asset.GetSoftObjectPath().ToString();
				break;
			}
		}
	}

	// Ensure path ends with _C for generated class
	if (!BlueprintPath.EndsWith(TEXT("_C")))
	{
		if (BlueprintPath.EndsWith(TEXT(".")))
		{
			BlueprintPath.RemoveFromEnd(TEXT("."));
		}

		// Check if it's a package path that needs the asset name appended
		int32 LastSlash;
		if (BlueprintPath.FindLastChar('/', LastSlash))
		{
			FString AssetName = BlueprintPath.Mid(LastSlash + 1);
			if (!AssetName.Contains(TEXT(".")))
			{
				BlueprintPath = FString::Printf(TEXT("%s.%s_C"), *BlueprintPath, *AssetName);
			}
			else
			{
				BlueprintPath += TEXT("_C");
			}
		}
		else
		{
			BlueprintPath += TEXT("_C");
		}
	}

	// Load the Blueprint generated class
	FoundClass = LoadClass<AActor>(nullptr, *BlueprintPath);
	if (FoundClass)
	{
		return FoundClass;
	}

	return nullptr;
}
