// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/Write/RemoveComponentTool.h"
#include "Utils/McpAssetModifier.h"
#include "YesUeMcpEditor.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"
#include "ScopedTransaction.h"

FString URemoveComponentTool::GetToolDescription() const
{
	return TEXT("Remove a component from an actor in the level.");
}

TMap<FString, FMcpSchemaProperty> URemoveComponentTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty ActorName;
	ActorName.Type = TEXT("string");
	ActorName.Description = TEXT("Actor name or label");
	ActorName.bRequired = true;
	Schema.Add(TEXT("actor_name"), ActorName);

	FMcpSchemaProperty ComponentName;
	ComponentName.Type = TEXT("string");
	ComponentName.Description = TEXT("Component name to remove");
	ComponentName.bRequired = true;
	Schema.Add(TEXT("component_name"), ComponentName);

	return Schema;
}

TArray<FString> URemoveComponentTool::GetRequiredParams() const
{
	return { TEXT("actor_name"), TEXT("component_name") };
}

FMcpToolResult URemoveComponentTool::Execute(
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	FString ActorName = GetStringArgOrDefault(Arguments, TEXT("actor_name"));
	FString ComponentName = GetStringArgOrDefault(Arguments, TEXT("component_name"));

	if (ActorName.IsEmpty() || ComponentName.IsEmpty())
	{
		return FMcpToolResult::Error(TEXT("actor_name and component_name are required"));
	}

	UE_LOG(LogYesUeMcp, Log, TEXT("remove-component: %s from %s"), *ComponentName, *ActorName);

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

	// Find the component
	UActorComponent* FoundComponent = nullptr;
	TArray<UActorComponent*> Components;
	FoundActor->GetComponents(Components);

	for (UActorComponent* Comp : Components)
	{
		if (Comp && Comp->GetName().Equals(ComponentName, ESearchCase::IgnoreCase))
		{
			FoundComponent = Comp;
			break;
		}
	}

	if (!FoundComponent)
	{
		return FMcpToolResult::Error(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
	}

	// Begin transaction
	TSharedPtr<FScopedTransaction> Transaction = FMcpAssetModifier::BeginTransaction(
		FText::Format(NSLOCTEXT("MCP", "RemoveComponent", "Remove {0} from {1}"),
			FText::FromString(ComponentName), FText::FromString(ActorName)));

	FoundActor->Modify();

	// Destroy the component
	FoundComponent->DestroyComponent();

	// Mark level as dirty
	World->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("actor_name"), FoundActor->GetName());
	Result->SetStringField(TEXT("component_name"), ComponentName);
	Result->SetBoolField(TEXT("needs_save"), true);

	UE_LOG(LogYesUeMcp, Log, TEXT("remove-component: Removed %s from %s"), *ComponentName, *FoundActor->GetName());

	return FMcpToolResult::Json(Result);
}
