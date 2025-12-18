// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/Level/ActorDetailsTool.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Tools/McpToolResult.h"

FString UActorDetailsTool::GetToolDescription() const
{
	return TEXT("Get detailed information about a specific actor in the level. "\
		"Returns actor properties, components with details, and transform information.");
}

TMap<FString, FMcpSchemaProperty> UActorDetailsTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty ActorName;
	ActorName.Type = TEXT("string");
	ActorName.Description = TEXT("Actor name to inspect (e.g., 'PlayerStart', 'DirectionalLight_0')");
	ActorName.bRequired = true;
	Schema.Add(TEXT("actor_name"), ActorName);

	FMcpSchemaProperty IncludeProperties;
	IncludeProperties.Type = TEXT("boolean");
	IncludeProperties.Description = TEXT("Include all actor properties (default: true)");
	IncludeProperties.bRequired = false;
	Schema.Add(TEXT("include_properties"), IncludeProperties);

	FMcpSchemaProperty IncludeComponents;
	IncludeComponents.Type = TEXT("boolean");
	IncludeComponents.Description = TEXT("Include detailed component information (default: true)");
	IncludeComponents.bRequired = false;
	Schema.Add(TEXT("include_components"), IncludeComponents);

	return Schema;
}

TArray<FString> UActorDetailsTool::GetRequiredParams() const
{
	return { TEXT("actor_name") };
}

FMcpToolResult UActorDetailsTool::Execute(
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	// Get actor name
	FString ActorName;
	if (!GetStringArg(Arguments, TEXT("actor_name"), ActorName))
	{
		return FMcpToolResult::Error(TEXT("Missing required parameter: actor_name"));
	}

	// Get optional parameters
	bool bIncludeProperties = GetBoolArgOrDefault(Arguments, TEXT("include_properties"), true);
	bool bIncludeComponents = GetBoolArgOrDefault(Arguments, TEXT("include_components"), true);

	// Get editor world
	if (!GEditor)
	{
		return FMcpToolResult::Error(TEXT("Editor not available"));
	}

	// Find actor
	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
	{
		return FMcpToolResult::Error(FString::Printf(
			TEXT("Actor '%s' not found in current level"), *ActorName));
	}

	// Build detailed result
	TSharedPtr<FJsonObject> Result = ActorToDetailedJson(Actor, bIncludeProperties, bIncludeComponents);

	return FMcpToolResult::Json(Result);
}

AActor* UActorDetailsTool::FindActorByName(const FString& ActorName) const
{
	if (!GEditor)
	{
		return nullptr;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return nullptr;
	}

	ULevel* Level = World->PersistentLevel;
	if (!Level)
	{
		return nullptr;
	}

	// Search for actor by name or label
	for (AActor* Actor : Level->Actors)
	{
		if (!Actor)
		{
			continue;
		}

		if (Actor->GetName().Equals(ActorName, ESearchCase::IgnoreCase) ||
			Actor->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase))
		{
			return Actor;
		}
	}

	return nullptr;
}

TSharedPtr<FJsonObject> UActorDetailsTool::ActorToDetailedJson(AActor* Actor, bool bIncludeProperties, bool bIncludeComponents) const
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

	// Parent class hierarchy
	TArray<TSharedPtr<FJsonValue>> ParentClassesArray;
	UClass* CurrentClass = Actor->GetClass()->GetSuperClass();
	while (CurrentClass && CurrentClass != UObject::StaticClass())
	{
		ParentClassesArray.Add(MakeShareable(new FJsonValueString(CurrentClass->GetName())));
		CurrentClass = CurrentClass->GetSuperClass();
	}
	if (ParentClassesArray.Num() > 0)
	{
		ActorJson->SetArrayField(TEXT("parent_classes"), ParentClassesArray);
	}

	// Actor state
	ActorJson->SetBoolField(TEXT("is_hidden"), Actor->IsHidden());
	ActorJson->SetBoolField(TEXT("is_selected"), Actor->IsSelected());
	ActorJson->SetBoolField(TEXT("is_editor_only"), Actor->IsEditorOnly());

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
	ActorJson->SetObjectField(TEXT("transform"), TransformToJson(Actor->GetActorTransform()));

	// Properties
	if (bIncludeProperties)
	{
		TArray<TSharedPtr<FJsonValue>> PropertiesArray;

		for (TFieldIterator<FProperty> PropIt(Actor->GetClass(), EFieldIteratorFlags::ExcludeSuper); PropIt; ++PropIt)
		{
			FProperty* Property = *PropIt;
			if (!Property)
			{
				continue;
			}

			// Skip some internal properties
			FString PropertyName = Property->GetName();
			if (PropertyName.StartsWith(TEXT("b")) && PropertyName.Contains(TEXT("Internal")))
			{
				continue;
			}

			void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Actor);
			if (!ValuePtr)
			{
				continue;
			}

			TSharedPtr<FJsonObject> PropertyJson = PropertyToJson(Property, ValuePtr, Actor);
			if (PropertyJson.IsValid())
			{
				PropertiesArray.Add(MakeShareable(new FJsonValueObject(PropertyJson)));
			}
		}

		ActorJson->SetArrayField(TEXT("properties"), PropertiesArray);
		ActorJson->SetNumberField(TEXT("property_count"), PropertiesArray.Num());
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

			TSharedPtr<FJsonObject> ComponentJson = ComponentToJson(Component, bIncludeProperties);
			if (ComponentJson.IsValid())
			{
				ComponentsArray.Add(MakeShareable(new FJsonValueObject(ComponentJson)));
			}
		}

		ActorJson->SetArrayField(TEXT("components"), ComponentsArray);
		ActorJson->SetNumberField(TEXT("component_count"), ComponentsArray.Num());
	}

	return ActorJson;
}

TSharedPtr<FJsonObject> UActorDetailsTool::ComponentToJson(UActorComponent* Component, bool bIncludeProperties) const
{
	if (!Component)
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> ComponentJson = MakeShareable(new FJsonObject);

	// Basic info
	ComponentJson->SetStringField(TEXT("name"), Component->GetName());
	ComponentJson->SetStringField(TEXT("class"), Component->GetClass()->GetName());
	ComponentJson->SetBoolField(TEXT("is_active"), Component->IsActive());
	ComponentJson->SetBoolField(TEXT("is_editor_only"), Component->IsEditorOnly());

	// Scene component specific
	if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
	{
		ComponentJson->SetBoolField(TEXT("is_scene_component"), true);

		// Relative transform
		ComponentJson->SetObjectField(TEXT("relative_transform"), TransformToJson(SceneComponent->GetRelativeTransform()));

		// World transform
		ComponentJson->SetObjectField(TEXT("world_transform"), TransformToJson(SceneComponent->GetComponentTransform()));

		// Mobility
		FString Mobility;
		switch (SceneComponent->Mobility)
		{
		case EComponentMobility::Static:
			Mobility = TEXT("Static");
			break;
		case EComponentMobility::Stationary:
			Mobility = TEXT("Stationary");
			break;
		case EComponentMobility::Movable:
			Mobility = TEXT("Movable");
			break;
		default:
			Mobility = TEXT("Unknown");
		}
		ComponentJson->SetStringField(TEXT("mobility"), Mobility);

		// Parent component
		if (SceneComponent->GetAttachParent())
		{
			ComponentJson->SetStringField(TEXT("parent_component"), SceneComponent->GetAttachParent()->GetName());
		}

		// Child components
		TArray<USceneComponent*> ChildComponents = SceneComponent->GetAttachChildren();
		if (ChildComponents.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> ChildrenArray;
			for (USceneComponent* Child : ChildComponents)
			{
				if (Child)
				{
					ChildrenArray.Add(MakeShareable(new FJsonValueString(Child->GetName())));
				}
			}
			ComponentJson->SetArrayField(TEXT("child_components"), ChildrenArray);
		}
	}
	else
	{
		ComponentJson->SetBoolField(TEXT("is_scene_component"), false);
	}

	// Properties
	if (bIncludeProperties)
	{
		TArray<TSharedPtr<FJsonValue>> PropertiesArray;

		for (TFieldIterator<FProperty> PropIt(Component->GetClass(), EFieldIteratorFlags::ExcludeSuper); PropIt; ++PropIt)
		{
			FProperty* Property = *PropIt;
			if (!Property)
			{
				continue;
			}

			void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Component);
			if (!ValuePtr)
			{
				continue;
			}

			TSharedPtr<FJsonObject> PropertyJson = PropertyToJson(Property, ValuePtr, Component);
			if (PropertyJson.IsValid())
			{
				PropertiesArray.Add(MakeShareable(new FJsonValueObject(PropertyJson)));
			}
		}

		ComponentJson->SetArrayField(TEXT("properties"), PropertiesArray);
		ComponentJson->SetNumberField(TEXT("property_count"), PropertiesArray.Num());
	}

	return ComponentJson;
}

TSharedPtr<FJsonObject> UActorDetailsTool::PropertyToJson(FProperty* Property, void* ValuePtr, UObject* Owner) const
{
	if (!Property || !ValuePtr)
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> PropertyJson = MakeShareable(new FJsonObject);

	// Basic info
	PropertyJson->SetStringField(TEXT("name"), Property->GetName());
	PropertyJson->SetStringField(TEXT("type"), GetPropertyTypeString(Property));

	// Category
	FString Category = Property->GetMetaData(TEXT("Category"));
	if (!Category.IsEmpty())
	{
		PropertyJson->SetStringField(TEXT("category"), Category);
	}

	// Export value as string
	FString Value;
	Property->ExportText_Direct(Value, ValuePtr, ValuePtr, Owner, PPF_None);
	PropertyJson->SetStringField(TEXT("value"), Value);

	return PropertyJson;
}

FString UActorDetailsTool::GetPropertyTypeString(FProperty* Property) const
{
	if (!Property)
	{
		return TEXT("unknown");
	}

	// Check for specific property types
	if (Property->IsA<FBoolProperty>())
	{
		return TEXT("bool");
	}
	else if (Property->IsA<FIntProperty>())
	{
		return TEXT("int32");
	}
	else if (Property->IsA<FFloatProperty>())
	{
		return TEXT("float");
	}
	else if (Property->IsA<FNameProperty>())
	{
		return TEXT("FName");
	}
	else if (Property->IsA<FStrProperty>())
	{
		return TEXT("FString");
	}
	else if (Property->IsA<FTextProperty>())
	{
		return TEXT("FText");
	}
	else if (FObjectProperty* ObjectProp = CastField<FObjectProperty>(Property))
	{
		if (ObjectProp->PropertyClass)
		{
			return FString::Printf(TEXT("TObjectPtr<%s>"), *ObjectProp->PropertyClass->GetName());
		}
		return TEXT("TObjectPtr<UObject>");
	}
	else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		if (StructProp->Struct)
		{
			return StructProp->Struct->GetName();
		}
		return TEXT("struct");
	}
	else if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
	{
		FString InnerType = GetPropertyTypeString(ArrayProp->Inner);
		return FString::Printf(TEXT("TArray<%s>"), *InnerType);
	}

	// Fallback
	return Property->GetClass()->GetName();
}

TSharedPtr<FJsonObject> UActorDetailsTool::TransformToJson(const FTransform& Transform) const
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
