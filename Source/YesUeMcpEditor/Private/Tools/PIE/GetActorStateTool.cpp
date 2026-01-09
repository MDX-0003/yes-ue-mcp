// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/PIE/GetActorStateTool.h"
#include "YesUeMcpEditor.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "UObject/PropertyIterator.h"

FString UGetActorStateTool::GetToolDescription() const
{
	return TEXT("Get the state of an actor in the PIE world including location, rotation, and specified properties.");
}

TMap<FString, FMcpSchemaProperty> UGetActorStateTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty ActorName;
	ActorName.Type = TEXT("string");
	ActorName.Description = TEXT("Name of the actor to query (e.g., 'BP_Enemy_C_0')");
	ActorName.bRequired = true;
	Schema.Add(TEXT("actor_name"), ActorName);

	FMcpSchemaProperty Properties;
	Properties.Type = TEXT("array");
	Properties.Description = TEXT("List of property names to retrieve (e.g., ['Health', 'bIsDead']). Empty array returns basic info only.");
	Properties.bRequired = false;
	Schema.Add(TEXT("properties"), Properties);

	return Schema;
}

FMcpToolResult UGetActorStateTool::Execute(
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	FString ActorName = GetStringArg(Arguments, TEXT("actor_name"));

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

	// Find the actor
	AActor* Actor = FindActorByName(PIEWorld, ActorName);
	if (!Actor)
	{
		return FMcpToolResult::Error(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("actor_name"), Actor->GetName());
	Result->SetStringField(TEXT("class"), Actor->GetClass()->GetName());

	// Location
	FVector Loc = Actor->GetActorLocation();
	TArray<TSharedPtr<FJsonValue>> LocArray;
	LocArray.Add(MakeShareable(new FJsonValueNumber(Loc.X)));
	LocArray.Add(MakeShareable(new FJsonValueNumber(Loc.Y)));
	LocArray.Add(MakeShareable(new FJsonValueNumber(Loc.Z)));
	Result->SetArrayField(TEXT("location"), LocArray);

	// Rotation
	FRotator Rot = Actor->GetActorRotation();
	TArray<TSharedPtr<FJsonValue>> RotArray;
	RotArray.Add(MakeShareable(new FJsonValueNumber(Rot.Pitch)));
	RotArray.Add(MakeShareable(new FJsonValueNumber(Rot.Yaw)));
	RotArray.Add(MakeShareable(new FJsonValueNumber(Rot.Roll)));
	Result->SetArrayField(TEXT("rotation"), RotArray);

	// Scale
	FVector Scale = Actor->GetActorScale3D();
	TArray<TSharedPtr<FJsonValue>> ScaleArray;
	ScaleArray.Add(MakeShareable(new FJsonValueNumber(Scale.X)));
	ScaleArray.Add(MakeShareable(new FJsonValueNumber(Scale.Y)));
	ScaleArray.Add(MakeShareable(new FJsonValueNumber(Scale.Z)));
	Result->SetArrayField(TEXT("scale"), ScaleArray);

	// Velocity
	FVector Velocity = Actor->GetVelocity();
	Result->SetNumberField(TEXT("speed"), Velocity.Size());

	// Tags
	TArray<TSharedPtr<FJsonValue>> TagsArray;
	for (const FName& Tag : Actor->Tags)
	{
		TagsArray.Add(MakeShareable(new FJsonValueString(Tag.ToString())));
	}
	Result->SetArrayField(TEXT("tags"), TagsArray);

	// Hidden
	Result->SetBoolField(TEXT("hidden"), Actor->IsHidden());

	// Get requested properties
	if (Arguments->HasField(TEXT("properties")))
	{
		const TArray<TSharedPtr<FJsonValue>>* PropsArray;
		if (Arguments->TryGetArrayField(TEXT("properties"), PropsArray))
		{
			TSharedPtr<FJsonObject> PropsResult = MakeShareable(new FJsonObject);

			for (const TSharedPtr<FJsonValue>& PropVal : *PropsArray)
			{
				FString PropName = PropVal->AsString();
				if (PropName.IsEmpty()) continue;

				// Find property by name
				FProperty* Property = Actor->GetClass()->FindPropertyByName(FName(*PropName));
				if (Property)
				{
					TSharedPtr<FJsonValue> Value = GetPropertyValue(Actor, Property);
					if (Value.IsValid())
					{
						PropsResult->SetField(PropName, Value);
					}
					else
					{
						PropsResult->SetStringField(PropName, TEXT("<unable to read>"));
					}
				}
				else
				{
					PropsResult->SetStringField(PropName, TEXT("<property not found>"));
				}
			}

			Result->SetObjectField(TEXT("properties"), PropsResult);
		}
	}

	return FMcpToolResult::Json(Result);
}

AActor* UGetActorStateTool::FindActorByName(UWorld* World, const FString& ActorName) const
{
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && Actor->GetName().Equals(ActorName, ESearchCase::IgnoreCase))
		{
			return Actor;
		}
	}
	return nullptr;
}

TSharedPtr<FJsonValue> UGetActorStateTool::GetPropertyValue(UObject* Object, FProperty* Property) const
{
	if (!Object || !Property)
	{
		return nullptr;
	}

	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Object);

	// Handle common property types
	if (FNumericProperty* NumericProp = CastField<FNumericProperty>(Property))
	{
		if (NumericProp->IsFloatingPoint())
		{
			double Value = 0;
			NumericProp->GetValue_InContainer(Object, &Value);
			return MakeShareable(new FJsonValueNumber(Value));
		}
		else if (NumericProp->IsInteger())
		{
			int64 Value = 0;
			NumericProp->GetValue_InContainer(Object, &Value);
			return MakeShareable(new FJsonValueNumber(static_cast<double>(Value)));
		}
	}
	else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		bool Value = BoolProp->GetPropertyValue(ValuePtr);
		return MakeShareable(new FJsonValueBoolean(Value));
	}
	else if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
	{
		FString Value = StrProp->GetPropertyValue(ValuePtr);
		return MakeShareable(new FJsonValueString(Value));
	}
	else if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
	{
		FName Value = NameProp->GetPropertyValue(ValuePtr);
		return MakeShareable(new FJsonValueString(Value.ToString()));
	}
	else if (FTextProperty* TextProp = CastField<FTextProperty>(Property))
	{
		FText Value = TextProp->GetPropertyValue(ValuePtr);
		return MakeShareable(new FJsonValueString(Value.ToString()));
	}
	else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
	{
		FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();
		int64 Value = UnderlyingProp->GetSignedIntPropertyValue(ValuePtr);
		UEnum* Enum = EnumProp->GetEnum();
		if (Enum)
		{
			FString EnumName = Enum->GetNameStringByValue(Value);
			return MakeShareable(new FJsonValueString(EnumName));
		}
		return MakeShareable(new FJsonValueNumber(static_cast<double>(Value)));
	}
	else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		// Handle common struct types
		if (StructProp->Struct == TBaseStructure<FVector>::Get())
		{
			FVector Value = *StructProp->ContainerPtrToValuePtr<FVector>(Object);
			TArray<TSharedPtr<FJsonValue>> Arr;
			Arr.Add(MakeShareable(new FJsonValueNumber(Value.X)));
			Arr.Add(MakeShareable(new FJsonValueNumber(Value.Y)));
			Arr.Add(MakeShareable(new FJsonValueNumber(Value.Z)));
			return MakeShareable(new FJsonValueArray(Arr));
		}
		else if (StructProp->Struct == TBaseStructure<FRotator>::Get())
		{
			FRotator Value = *StructProp->ContainerPtrToValuePtr<FRotator>(Object);
			TArray<TSharedPtr<FJsonValue>> Arr;
			Arr.Add(MakeShareable(new FJsonValueNumber(Value.Pitch)));
			Arr.Add(MakeShareable(new FJsonValueNumber(Value.Yaw)));
			Arr.Add(MakeShareable(new FJsonValueNumber(Value.Roll)));
			return MakeShareable(new FJsonValueArray(Arr));
		}
	}
	else if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
	{
		UObject* Value = ObjProp->GetPropertyValue(ValuePtr);
		if (Value)
		{
			return MakeShareable(new FJsonValueString(Value->GetName()));
		}
		return MakeShareable(new FJsonValueNull());
	}

	// Fallback: export as string
	FString ExportedText;
	Property->ExportTextItem_Direct(ExportedText, ValuePtr, nullptr, nullptr, PPF_None);
	return MakeShareable(new FJsonValueString(ExportedText));
}
