// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/PIE/PieActorTool.h"
#include "YesUeMcpEditor.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectGlobals.h"
#include "AssetRegistry/AssetRegistryModule.h"

FString UPieActorTool::GetToolDescription() const
{
	return TEXT("Control actors in PIE world. Actions: 'spawn' (create actor), 'destroy' (remove actor), 'get-state' (query properties), 'call-function' (invoke method).");
}

TMap<FString, FMcpSchemaProperty> UPieActorTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty Action;
	Action.Type = TEXT("string");
	Action.Description = TEXT("Action: 'spawn', 'destroy', 'get-state', or 'call-function'");
	Action.bRequired = true;
	Schema.Add(TEXT("action"), Action);

	FMcpSchemaProperty ActorName;
	ActorName.Type = TEXT("string");
	ActorName.Description = TEXT("[get-state/destroy/call-function] Name of the actor (e.g., 'BP_Enemy_C_0')");
	ActorName.bRequired = false;
	Schema.Add(TEXT("actor_name"), ActorName);

	FMcpSchemaProperty ActorClass;
	ActorClass.Type = TEXT("string");
	ActorClass.Description = TEXT("[spawn] Class to spawn (e.g., 'Character', 'BP_Enemy', '/Game/BP/BP_Enemy')");
	ActorClass.bRequired = false;
	Schema.Add(TEXT("actor_class"), ActorClass);

	FMcpSchemaProperty Location;
	Location.Type = TEXT("array");
	Location.Description = TEXT("[spawn] Location as [X, Y, Z] (default: [0,0,0])");
	Location.bRequired = false;
	Schema.Add(TEXT("location"), Location);

	FMcpSchemaProperty Rotation;
	Rotation.Type = TEXT("array");
	Rotation.Description = TEXT("[spawn] Rotation as [Pitch, Yaw, Roll] (default: [0,0,0])");
	Rotation.bRequired = false;
	Schema.Add(TEXT("rotation"), Rotation);

	FMcpSchemaProperty Properties;
	Properties.Type = TEXT("array");
	Properties.Description = TEXT("[get-state] Property names to query (e.g., ['Health', 'bIsDead'])");
	Properties.bRequired = false;
	Schema.Add(TEXT("properties"), Properties);

	FMcpSchemaProperty FunctionName;
	FunctionName.Type = TEXT("string");
	FunctionName.Description = TEXT("[call-function] Name of function to call");
	FunctionName.bRequired = false;
	Schema.Add(TEXT("function_name"), FunctionName);

	FMcpSchemaProperty Arguments;
	Arguments.Type = TEXT("object");
	Arguments.Description = TEXT("[call-function] Function arguments as {\"ParamName\": value}");
	Arguments.bRequired = false;
	Schema.Add(TEXT("arguments"), Arguments);

	return Schema;
}

FMcpToolResult UPieActorTool::Execute(
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	FString Action = GetStringArg(Arguments, TEXT("action")).ToLower();

	// Check PIE is running
	if (!GEditor->IsPlaySessionInProgress())
	{
		return FMcpToolResult::Error(TEXT("No PIE session running. Use pie-session action:start first."));
	}

	UWorld* PIEWorld = GetPIEWorld();
	if (!PIEWorld)
	{
		return FMcpToolResult::Error(TEXT("PIE world not found"));
	}

	if (Action == TEXT("spawn"))
	{
		return ExecuteSpawn(Arguments, PIEWorld);
	}
	else if (Action == TEXT("destroy"))
	{
		return ExecuteDestroy(Arguments, PIEWorld);
	}
	else if (Action == TEXT("get-state") || Action == TEXT("state") || Action == TEXT("query"))
	{
		return ExecuteGetState(Arguments, PIEWorld);
	}
	else if (Action == TEXT("call-function") || Action == TEXT("call"))
	{
		return ExecuteCallFunction(Arguments, PIEWorld);
	}
	else
	{
		return FMcpToolResult::Error(FString::Printf(
			TEXT("Unknown action: '%s'. Valid: spawn, destroy, get-state, call-function"), *Action));
	}
}

FMcpToolResult UPieActorTool::ExecuteSpawn(const TSharedPtr<FJsonObject>& Arguments, UWorld* PIEWorld)
{
	FString ClassName = GetStringArgOrDefault(Arguments, TEXT("actor_class"));
	if (ClassName.IsEmpty())
	{
		return FMcpToolResult::Error(TEXT("actor_class is required for spawn action"));
	}

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

	UClass* ActorClass = FindActorClass(ClassName);
	if (!ActorClass)
	{
		return FMcpToolResult::Error(FString::Printf(TEXT("Actor class not found: %s"), *ClassName));
	}

	if (!ActorClass->IsChildOf(AActor::StaticClass()))
	{
		return FMcpToolResult::Error(FString::Printf(TEXT("Class is not an Actor: %s"), *ClassName));
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AActor* SpawnedActor = PIEWorld->SpawnActor<AActor>(ActorClass, Location, Rotation, SpawnParams);
	if (!SpawnedActor)
	{
		return FMcpToolResult::Error(FString::Printf(TEXT("Failed to spawn: %s"), *ClassName));
	}

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("actor_name"), SpawnedActor->GetName());
	Result->SetStringField(TEXT("class"), ActorClass->GetName());

	FVector ActualLoc = SpawnedActor->GetActorLocation();
	TArray<TSharedPtr<FJsonValue>> LocArray;
	LocArray.Add(MakeShareable(new FJsonValueNumber(ActualLoc.X)));
	LocArray.Add(MakeShareable(new FJsonValueNumber(ActualLoc.Y)));
	LocArray.Add(MakeShareable(new FJsonValueNumber(ActualLoc.Z)));
	Result->SetArrayField(TEXT("location"), LocArray);

	UE_LOG(LogYesUeMcp, Log, TEXT("pie-actor: Spawned %s as %s"), *ClassName, *SpawnedActor->GetName());
	return FMcpToolResult::Json(Result);
}

FMcpToolResult UPieActorTool::ExecuteDestroy(const TSharedPtr<FJsonObject>& Arguments, UWorld* PIEWorld)
{
	FString ActorName = GetStringArgOrDefault(Arguments, TEXT("actor_name"));
	if (ActorName.IsEmpty())
	{
		return FMcpToolResult::Error(TEXT("actor_name is required for destroy action"));
	}

	AActor* Actor = FindActorByName(PIEWorld, ActorName);
	if (!Actor)
	{
		return FMcpToolResult::Error(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	Actor->Destroy();

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("destroyed"), ActorName);

	UE_LOG(LogYesUeMcp, Log, TEXT("pie-actor: Destroyed %s"), *ActorName);
	return FMcpToolResult::Json(Result);
}

FMcpToolResult UPieActorTool::ExecuteGetState(const TSharedPtr<FJsonObject>& Arguments, UWorld* PIEWorld)
{
	FString ActorName = GetStringArgOrDefault(Arguments, TEXT("actor_name"));
	if (ActorName.IsEmpty())
	{
		return FMcpToolResult::Error(TEXT("actor_name is required for get-state action"));
	}

	AActor* Actor = FindActorByName(PIEWorld, ActorName);
	if (!Actor)
	{
		return FMcpToolResult::Error(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

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

	Result->SetNumberField(TEXT("speed"), Actor->GetVelocity().Size());
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
					PropsResult->SetStringField(PropName, TEXT("<not found>"));
				}
			}
			Result->SetObjectField(TEXT("properties"), PropsResult);
		}
	}

	return FMcpToolResult::Json(Result);
}

FMcpToolResult UPieActorTool::ExecuteCallFunction(const TSharedPtr<FJsonObject>& Arguments, UWorld* PIEWorld)
{
	FString ActorName = GetStringArgOrDefault(Arguments, TEXT("actor_name"));
	FString FunctionName = GetStringArgOrDefault(Arguments, TEXT("function_name"));

	if (ActorName.IsEmpty())
	{
		return FMcpToolResult::Error(TEXT("actor_name is required for call-function action"));
	}
	if (FunctionName.IsEmpty())
	{
		return FMcpToolResult::Error(TEXT("function_name is required for call-function action"));
	}

	AActor* Actor = FindActorByName(PIEWorld, ActorName);
	if (!Actor)
	{
		return FMcpToolResult::Error(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	UFunction* Function = Actor->FindFunction(FName(*FunctionName));
	if (!Function)
	{
		return FMcpToolResult::Error(FString::Printf(TEXT("Function not found: %s"), *FunctionName));
	}

	// Allocate and init parameters
	uint8* ParamBuffer = (uint8*)FMemory_Alloca(Function->ParmsSize);
	FMemory::Memzero(ParamBuffer, Function->ParmsSize);

	for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It)
	{
		It->InitializeValue_InContainer(ParamBuffer);
	}

	// Set parameters from JSON
	if (Arguments->HasField(TEXT("arguments")))
	{
		const TSharedPtr<FJsonObject>* FuncArgs;
		if (Arguments->TryGetObjectField(TEXT("arguments"), FuncArgs))
		{
			for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It)
			{
				FProperty* Param = *It;
				if (Param->PropertyFlags & CPF_ReturnParm) continue;

				FString ParamName = Param->GetName();
				if ((*FuncArgs)->HasField(ParamName))
				{
					TSharedPtr<FJsonValue> ParamValue = (*FuncArgs)->TryGetField(ParamName);
					void* ParamAddr = Param->ContainerPtrToValuePtr<void>(ParamBuffer);
					SetPropertyFromJson(ParamAddr, Param, ParamValue);
				}
			}
		}
	}

	// Call the function
	Actor->ProcessEvent(Function, ParamBuffer);

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("function"), FunctionName);
	Result->SetStringField(TEXT("actor"), ActorName);

	// Check for return value
	for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It)
	{
		FProperty* Param = *It;
		if (Param->PropertyFlags & CPF_ReturnParm)
		{
			void* ReturnAddr = Param->ContainerPtrToValuePtr<void>(ParamBuffer);
			TSharedPtr<FJsonValue> RetVal = GetPropertyValue(ParamBuffer, Param);
			if (RetVal.IsValid())
			{
				Result->SetField(TEXT("return_value"), RetVal);
			}
			break;
		}
	}

	// Cleanup
	for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It)
	{
		It->DestroyValue_InContainer(ParamBuffer);
	}

	UE_LOG(LogYesUeMcp, Log, TEXT("pie-actor: Called %s on %s"), *FunctionName, *ActorName);
	return FMcpToolResult::Json(Result);
}

UWorld* UPieActorTool::GetPIEWorld() const
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

AActor* UPieActorTool::FindActorByName(UWorld* World, const FString& ActorName) const
{
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if ((*It)->GetName().Equals(ActorName, ESearchCase::IgnoreCase))
		{
			return *It;
		}
	}
	return nullptr;
}

UClass* UPieActorTool::FindActorClass(const FString& ClassName) const
{
	// Try native class
	UClass* FoundClass = FindObject<UClass>(nullptr, *ClassName);
	if (FoundClass) return FoundClass;

	// Try common prefixes
	for (const FString& Prefix : {TEXT("/Script/Engine."), TEXT("/Script/CoreUObject.")})
	{
		FoundClass = FindObject<UClass>(nullptr, *(Prefix + ClassName));
		if (FoundClass) return FoundClass;
	}

	// Try as Blueprint
	FString BlueprintPath = ClassName;
	if (!BlueprintPath.StartsWith(TEXT("/")))
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		TArray<FAssetData> Assets;
		FARFilter Filter;
		Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
		Filter.bRecursiveClasses = true;
		AssetRegistryModule.Get().GetAssets(Filter, Assets);

		for (const FAssetData& Asset : Assets)
		{
			if (Asset.AssetName.ToString().Equals(ClassName, ESearchCase::IgnoreCase))
			{
				BlueprintPath = Asset.GetSoftObjectPath().ToString();
				break;
			}
		}
	}

	if (!BlueprintPath.EndsWith(TEXT("_C")))
	{
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
	}

	return LoadClass<AActor>(nullptr, *BlueprintPath);
}

TSharedPtr<FJsonValue> UPieActorTool::GetPropertyValue(UObject* Object, FProperty* Property) const
{
	if (!Object || !Property) return nullptr;

	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Object);

	if (FNumericProperty* NumProp = CastField<FNumericProperty>(Property))
	{
		if (NumProp->IsFloatingPoint())
		{
			double Value = 0;
			NumProp->GetValue_InContainer(Object, &Value);
			return MakeShareable(new FJsonValueNumber(Value));
		}
		else
		{
			int64 Value = 0;
			NumProp->GetValue_InContainer(Object, &Value);
			return MakeShareable(new FJsonValueNumber(static_cast<double>(Value)));
		}
	}
	else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		return MakeShareable(new FJsonValueBoolean(BoolProp->GetPropertyValue(ValuePtr)));
	}
	else if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
	{
		return MakeShareable(new FJsonValueString(StrProp->GetPropertyValue(ValuePtr)));
	}
	else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		if (StructProp->Struct == TBaseStructure<FVector>::Get())
		{
			FVector Value = *StructProp->ContainerPtrToValuePtr<FVector>(Object);
			TArray<TSharedPtr<FJsonValue>> Arr;
			Arr.Add(MakeShareable(new FJsonValueNumber(Value.X)));
			Arr.Add(MakeShareable(new FJsonValueNumber(Value.Y)));
			Arr.Add(MakeShareable(new FJsonValueNumber(Value.Z)));
			return MakeShareable(new FJsonValueArray(Arr));
		}
	}

	// Fallback
	FString ExportedText;
	Property->ExportTextItem_Direct(ExportedText, ValuePtr, nullptr, nullptr, PPF_None);
	return MakeShareable(new FJsonValueString(ExportedText));
}

bool UPieActorTool::SetPropertyFromJson(void* PropertyAddr, FProperty* Property, const TSharedPtr<FJsonValue>& JsonValue) const
{
	if (!PropertyAddr || !Property || !JsonValue.IsValid()) return false;

	if (FNumericProperty* NumProp = CastField<FNumericProperty>(Property))
	{
		if (NumProp->IsFloatingPoint())
		{
			NumProp->SetFloatingPointPropertyValue(PropertyAddr, JsonValue->AsNumber());
		}
		else
		{
			NumProp->SetIntPropertyValue(PropertyAddr, static_cast<int64>(JsonValue->AsNumber()));
		}
		return true;
	}
	else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		BoolProp->SetPropertyValue(PropertyAddr, JsonValue->AsBool());
		return true;
	}
	else if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
	{
		StrProp->SetPropertyValue(PropertyAddr, JsonValue->AsString());
		return true;
	}

	Property->ImportText_Direct(*JsonValue->AsString(), PropertyAddr, nullptr, PPF_None);
	return true;
}
