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
	return TEXT("Delete an actor from the currently open level.");
}

TMap<FString, FMcpSchemaProperty> UDeleteActorTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty ActorName;
	ActorName.Type = TEXT("string");
	ActorName.Description = TEXT("Actor name or label to delete");
	ActorName.bRequired = true;
	Schema.Add(TEXT("actor_name"), ActorName);

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

	if (ActorName.IsEmpty())
	{
		return FMcpToolResult::Error(TEXT("actor_name is required"));
	}

	UE_LOG(LogYesUeMcp, Log, TEXT("delete-actor: %s"), *ActorName);

	// Get the editor world
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return FMcpToolResult::Error(TEXT("No world available. Open a level first."));
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

	// Mark level as dirty
	World->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetStringField(TEXT("actor_name"), DeletedName);

	if (bDestroyed)
	{
		Result->SetBoolField(TEXT("success"), true);
		Result->SetStringField(TEXT("status"), TEXT("deleted"));
		Result->SetBoolField(TEXT("needs_save"), true);

		UE_LOG(LogYesUeMcp, Log, TEXT("delete-actor: Deleted %s"), *DeletedName);
	}
	else
	{
		Result->SetBoolField(TEXT("success"), false);
		Result->SetStringField(TEXT("error"), TEXT("Failed to destroy actor"));
	}

	return FMcpToolResult::Json(Result);
}
