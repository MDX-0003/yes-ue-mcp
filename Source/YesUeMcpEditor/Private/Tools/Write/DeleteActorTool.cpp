// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/Write/DeleteActorTool.h"
#include "Utils/McpAssetModifier.h"
#include "YesUeMcpEditor.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"
#include "ScopedTransaction.h"

FString UDeleteActorTool::GetToolDescription() const
{
	return TEXT("Delete an actor from the editor level or PIE world. Use 'world' param: 'editor' (default) or 'pie'.");
}

TMap<FString, FMcpSchemaProperty> UDeleteActorTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty ActorName;
	ActorName.Type = TEXT("string");
	ActorName.Description = TEXT("Actor name or label to delete");
	ActorName.bRequired = true;
	Schema.Add(TEXT("actor_name"), ActorName);

	FMcpSchemaProperty WorldParam;
	WorldParam.Type = TEXT("string");
	WorldParam.Description = TEXT("Target world: 'editor' (default) or 'pie' for Play-In-Editor world");
	WorldParam.bRequired = false;
	Schema.Add(TEXT("world"), WorldParam);

	return Schema;
}

TArray<FString> UDeleteActorTool::GetRequiredParams() const
{
	return { TEXT("actor_name") };
}

FMcpToolResult UDeleteActorTool::Execute(
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	FString ActorName = GetStringArgOrDefault(Arguments, TEXT("actor_name"));
	FString WorldParam = GetStringArgOrDefault(Arguments, TEXT("world"), TEXT("editor"));

	if (ActorName.IsEmpty())
	{
		return FMcpToolResult::Error(TEXT("actor_name is required"));
	}

	const bool bUsePIE = WorldParam.Equals(TEXT("pie"), ESearchCase::IgnoreCase);

	UE_LOG(LogYesUeMcp, Log, TEXT("delete-actor: %s in %s world"), *ActorName, bUsePIE ? TEXT("PIE") : TEXT("editor"));

	// Get the target world
	UWorld* World = nullptr;
	if (bUsePIE)
	{
		for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
		{
			if (WorldContext.WorldType == EWorldType::PIE && WorldContext.World())
			{
				World = WorldContext.World();
				break;
			}
		}
		if (!World)
		{
			return FMcpToolResult::Error(TEXT("No PIE session running. Use pie-session action:start first."));
		}
	}
	else
	{
		World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (!World)
		{
			return FMcpToolResult::Error(TEXT("No editor world available. Open a level first."));
		}
	}

	// Find the actor
	AActor* FoundActor = nullptr;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor)
		{
			// Check by name or label
			if (Actor->GetName().Equals(ActorName, ESearchCase::IgnoreCase) ||
				Actor->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase))
			{
				FoundActor = Actor;
				break;
			}
		}
	}

	if (!FoundActor)
	{
		return FMcpToolResult::Error(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	// Begin transaction
	TSharedPtr<FScopedTransaction> Transaction = FMcpAssetModifier::BeginTransaction(
		FText::Format(NSLOCTEXT("MCP", "DeleteActor", "Delete {0}"), FText::FromString(ActorName)));

	FString DeletedName = FoundActor->GetName();

	// Destroy the actor
	bool bDestroyed = World->DestroyActor(FoundActor);

	// Mark level as dirty (only for editor world)
	if (!bUsePIE)
	{
		World->MarkPackageDirty();
	}

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetStringField(TEXT("actor_name"), DeletedName);
	Result->SetStringField(TEXT("world"), bUsePIE ? TEXT("pie") : TEXT("editor"));

	if (bDestroyed)
	{
		Result->SetBoolField(TEXT("success"), true);
		Result->SetStringField(TEXT("status"), TEXT("deleted"));
		if (!bUsePIE)
		{
			Result->SetBoolField(TEXT("needs_save"), true);
		}

		UE_LOG(LogYesUeMcp, Log, TEXT("delete-actor: Deleted %s in %s world"), *DeletedName, bUsePIE ? TEXT("PIE") : TEXT("editor"));
	}
	else
	{
		Result->SetBoolField(TEXT("success"), false);
		Result->SetStringField(TEXT("error"), TEXT("Failed to destroy actor"));
	}

	return FMcpToolResult::Json(Result);
}
