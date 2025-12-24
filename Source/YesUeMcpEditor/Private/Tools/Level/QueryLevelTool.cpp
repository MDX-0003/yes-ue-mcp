// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/Level/QueryLevelTool.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Tools/McpToolResult.h"
#include "YesUeMcpEditor.h"

FString UQueryLevelTool::GetToolDescription() const
{
	return TEXT("List actors in the currently open level with filtering options. "\
		"Returns actor names, classes, transforms, and optionally components.");
}

TMap<FString, FMcpSchemaProperty> UQueryLevelTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty ClassFilter;
	ClassFilter.Type = TEXT("string");
	ClassFilter.Description = TEXT("Filter by actor class (wildcards supported, e.g., '*Light*', 'StaticMeshActor')");
	ClassFilter.bRequired = false;
	Schema.Add(TEXT("class_filter"), ClassFilter);

	FMcpSchemaProperty FolderFilter;
	FolderFilter.Type = TEXT("string");
	FolderFilter.Description = TEXT("Filter by World Outliner folder path");
	FolderFilter.bRequired = false;
	Schema.Add(TEXT("folder_filter"), FolderFilter);

	FMcpSchemaProperty TagFilter;
	TagFilter.Type = TEXT("string");
	TagFilter.Description = TEXT("Filter by actor tag");
	TagFilter.bRequired = false;
	Schema.Add(TEXT("tag_filter"), TagFilter);

	FMcpSchemaProperty IncludeHidden;
	IncludeHidden.Type = TEXT("boolean");
	IncludeHidden.Description = TEXT("Include hidden actors in results (default: false)");
	IncludeHidden.bRequired = false;
	Schema.Add(TEXT("include_hidden"), IncludeHidden);

	FMcpSchemaProperty IncludeComponents;
	IncludeComponents.Type = TEXT("boolean");
	IncludeComponents.Description = TEXT("Include component list for each actor (default: false)");
	IncludeComponents.bRequired = false;
	Schema.Add(TEXT("include_components"), IncludeComponents);

	FMcpSchemaProperty IncludeTransform;
	IncludeTransform.Type = TEXT("boolean");
	IncludeTransform.Description = TEXT("Include actor transforms (default: true)");
	IncludeTransform.bRequired = false;
	Schema.Add(TEXT("include_transform"), IncludeTransform);

	FMcpSchemaProperty Limit;
	Limit.Type = TEXT("integer");
	Limit.Description = TEXT("Maximum number of results to return (default: 100)");
	Limit.bRequired = false;
	Schema.Add(TEXT("limit"), Limit);

	return Schema;
}

TArray<FString> UQueryLevelTool::GetRequiredParams() const
{
	return {}; // No required params
}

FMcpToolResult UQueryLevelTool::Execute(
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	// Get editor world
	if (!GEditor)
	{
		return FMcpToolResult::Error(TEXT("Editor not available"));
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return FMcpToolResult::Error(TEXT("No world loaded in editor"));
	}

	ULevel* Level = World->PersistentLevel;
	if (!Level)
	{
		return FMcpToolResult::Error(TEXT("No persistent level found"));
	}

	// Get optional parameters
	FString ClassFilter = GetStringArgOrDefault(Arguments, TEXT("class_filter"), TEXT(""));
	FString FolderFilter = GetStringArgOrDefault(Arguments, TEXT("folder_filter"), TEXT(""));
	FString TagFilter = GetStringArgOrDefault(Arguments, TEXT("tag_filter"), TEXT(""));
	bool bIncludeHidden = GetBoolArgOrDefault(Arguments, TEXT("include_hidden"), false);
	bool bIncludeComponents = GetBoolArgOrDefault(Arguments, TEXT("include_components"), false);
	bool bIncludeTransform = GetBoolArgOrDefault(Arguments, TEXT("include_transform"), true);
	int32 Limit = GetIntArgOrDefault(Arguments, TEXT("limit"), 100);

	UE_LOG(LogYesUeMcp, Log, TEXT("query-level: class='%s', folder='%s', tag='%s', limit=%d"),
		*ClassFilter, *FolderFilter, *TagFilter, Limit);

	// Build result
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetStringField(TEXT("level_name"), Level->GetName());
	Result->SetStringField(TEXT("world_name"), World->GetName());

	// Iterate actors
	TArray<TSharedPtr<FJsonValue>> ActorsArray;
	int32 ProcessedCount = 0;

	for (AActor* Actor : Level->Actors)
	{
		if (!Actor)
		{
			continue;
		}

		// Check limit
		if (ActorsArray.Num() >= Limit)
		{
			break;
		}

		ProcessedCount++;

		// Apply hidden filter
		if (!bIncludeHidden && Actor->IsHidden())
		{
			continue;
		}

		// Apply class filter
		if (!ClassFilter.IsEmpty() && !MatchesClassFilter(Actor, ClassFilter))
		{
			continue;
		}

		// Apply folder filter
		if (!FolderFilter.IsEmpty() && !MatchesFolderFilter(Actor, FolderFilter))
		{
			continue;
		}

		// Apply tag filter
		if (!TagFilter.IsEmpty() && !MatchesTagFilter(Actor, TagFilter))
		{
			continue;
		}

		// Add actor to results
		TSharedPtr<FJsonObject> ActorJson = ActorToJson(Actor, bIncludeComponents, bIncludeTransform);
		if (ActorJson.IsValid())
		{
			ActorsArray.Add(MakeShareable(new FJsonValueObject(ActorJson)));
		}
	}

	Result->SetArrayField(TEXT("actors"), ActorsArray);
	Result->SetNumberField(TEXT("actor_count"), ActorsArray.Num());
	Result->SetNumberField(TEXT("total_actors_in_level"), ProcessedCount);
	Result->SetBoolField(TEXT("limit_reached"), ActorsArray.Num() >= Limit);

	return FMcpToolResult::Json(Result);
}

TSharedPtr<FJsonObject> UQueryLevelTool::ActorToJson(AActor* Actor, bool bIncludeComponents, bool bIncludeTransform) const
{
	if (!Actor)
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> ActorJson = MakeShareable(new FJsonObject);

	// Basic info
	ActorJson->SetStringField(TEXT("name"), Actor->GetName());
	ActorJson->SetStringField(TEXT("label"), Actor->GetActorLabel());
	ActorJson->SetStringField(TEXT("class"), Actor->GetClass()->GetName());

	// Actor state
	ActorJson->SetBoolField(TEXT("is_hidden"), Actor->IsHidden());
	ActorJson->SetBoolField(TEXT("is_selected"), Actor->IsSelected());

	// Folder
	FName FolderPath = Actor->GetFolderPath();
	if (FolderPath != NAME_None)
	{
		ActorJson->SetStringField(TEXT("folder"), FolderPath.ToString());
	}

	// Tags
	if (Actor->Tags.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> TagsArray;
		for (const FName& Tag : Actor->Tags)
		{
			TagsArray.Add(MakeShareable(new FJsonValueString(Tag.ToString())));
		}
		ActorJson->SetArrayField(TEXT("tags"), TagsArray);
	}

	// Transform
	if (bIncludeTransform)
	{
		ActorJson->SetObjectField(TEXT("transform"), TransformToJson(Actor->GetActorTransform()));
	}

	// Components
	if (bIncludeComponents)
	{
		TArray<TSharedPtr<FJsonValue>> ComponentsArray;
		TArray<UActorComponent*> Components;
		Actor->GetComponents(Components);

		for (UActorComponent* Component : Components)
		{
			if (!Component)
			{
				continue;
			}

			TSharedPtr<FJsonObject> ComponentJson = MakeShareable(new FJsonObject);
			ComponentJson->SetStringField(TEXT("name"), Component->GetName());
			ComponentJson->SetStringField(TEXT("class"), Component->GetClass()->GetName());
			ComponentJson->SetBoolField(TEXT("is_active"), Component->IsActive());

			ComponentsArray.Add(MakeShareable(new FJsonValueObject(ComponentJson)));
		}

		ActorJson->SetArrayField(TEXT("components"), ComponentsArray);
		ActorJson->SetNumberField(TEXT("component_count"), ComponentsArray.Num());
	}

	return ActorJson;
}

TSharedPtr<FJsonObject> UQueryLevelTool::TransformToJson(const FTransform& Transform) const
{
	TSharedPtr<FJsonObject> TransformJson = MakeShareable(new FJsonObject);

	// Location
	TSharedPtr<FJsonObject> LocationJson = MakeShareable(new FJsonObject);
	LocationJson->SetNumberField(TEXT("x"), Transform.GetLocation().X);
	LocationJson->SetNumberField(TEXT("y"), Transform.GetLocation().Y);
	LocationJson->SetNumberField(TEXT("z"), Transform.GetLocation().Z);
	TransformJson->SetObjectField(TEXT("location"), LocationJson);

	// Rotation
	TSharedPtr<FJsonObject> RotationJson = MakeShareable(new FJsonObject);
	FRotator Rotator = Transform.Rotator();
	RotationJson->SetNumberField(TEXT("pitch"), Rotator.Pitch);
	RotationJson->SetNumberField(TEXT("yaw"), Rotator.Yaw);
	RotationJson->SetNumberField(TEXT("roll"), Rotator.Roll);
	TransformJson->SetObjectField(TEXT("rotation"), RotationJson);

	// Scale
	TSharedPtr<FJsonObject> ScaleJson = MakeShareable(new FJsonObject);
	ScaleJson->SetNumberField(TEXT("x"), Transform.GetScale3D().X);
	ScaleJson->SetNumberField(TEXT("y"), Transform.GetScale3D().Y);
	ScaleJson->SetNumberField(TEXT("z"), Transform.GetScale3D().Z);
	TransformJson->SetObjectField(TEXT("scale"), ScaleJson);

	return TransformJson;
}

bool UQueryLevelTool::MatchesClassFilter(AActor* Actor, const FString& Filter) const
{
	if (!Actor || Filter.IsEmpty())
	{
		return true;
	}

	FString ClassName = Actor->GetClass()->GetName();
	return MatchesWildcard(ClassName, Filter);
}

bool UQueryLevelTool::MatchesFolderFilter(AActor* Actor, const FString& Filter) const
{
	if (!Actor || Filter.IsEmpty())
	{
		return true;
	}

	FName FolderPath = Actor->GetFolderPath();
	if (FolderPath == NAME_None)
	{
		return false;
	}

	FString FolderString = FolderPath.ToString();
	return MatchesWildcard(FolderString, Filter);
}

bool UQueryLevelTool::MatchesTagFilter(AActor* Actor, const FString& Filter) const
{
	if (!Actor || Filter.IsEmpty())
	{
		return true;
	}

	for (const FName& Tag : Actor->Tags)
	{
		if (MatchesWildcard(Tag.ToString(), Filter))
		{
			return true;
		}
	}

	return false;
}

bool UQueryLevelTool::MatchesWildcard(const FString& Name, const FString& Pattern) const
{
	if (Pattern.IsEmpty())
	{
		return true;
	}

	// Simple wildcard matching
	if (Pattern.Contains(TEXT("*")))
	{
		if (Pattern.StartsWith(TEXT("*")) && Pattern.EndsWith(TEXT("*")))
		{
			// *substring*
			FString Substring = Pattern.Mid(1, Pattern.Len() - 2);
			return Name.Contains(Substring);
		}
		else if (Pattern.StartsWith(TEXT("*")))
		{
			// *suffix
			FString Suffix = Pattern.Mid(1);
			return Name.EndsWith(Suffix);
		}
		else if (Pattern.EndsWith(TEXT("*")))
		{
			// prefix*
			FString Prefix = Pattern.Left(Pattern.Len() - 1);
			return Name.StartsWith(Prefix);
		}
	}

	// Exact match
	return Name.Equals(Pattern, ESearchCase::IgnoreCase);
}
