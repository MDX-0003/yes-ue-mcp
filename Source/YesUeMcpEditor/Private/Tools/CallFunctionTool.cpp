// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/CallFunctionTool.h"
#include "YesUeMcpEditor.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"

FString UCallFunctionTool::GetToolDescription() const
{
	return TEXT("Call functions on actors, components, or global Blueprint libraries. "
		"Target format: 'ActorName.FunctionName', 'ActorName.ComponentName.FunctionName', or '/Game/BP_Lib.FunctionName'. "
		"Use 'world' param: 'editor' (default) or 'pie'.");
}

TMap<FString, FMcpSchemaProperty> UCallFunctionTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty Target;
	Target.Type = TEXT("string");
	Target.Description = TEXT("Function target: 'ActorName.Function', 'ActorName.Component.Function', or '/Game/BP.Function'");
	Target.bRequired = true;
	Schema.Add(TEXT("target"), Target);

	FMcpSchemaProperty Arguments;
	Arguments.Type = TEXT("object");
	Arguments.Description = TEXT("Function arguments as {\"ParamName\": value}");
	Arguments.bRequired = false;
	Schema.Add(TEXT("arguments"), Arguments);

	FMcpSchemaProperty WorldParam;
	WorldParam.Type = TEXT("string");
	WorldParam.Description = TEXT("Target world: 'editor' (default) or 'pie'");
	WorldParam.bRequired = false;
	Schema.Add(TEXT("world"), WorldParam);

	return Schema;
}

FMcpToolResult UCallFunctionTool::Execute(
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	FString Target = GetStringArgOrDefault(Arguments, TEXT("target"));
	FString WorldParam = GetStringArgOrDefault(Arguments, TEXT("world"), TEXT("editor"));

	if (Target.IsEmpty())
	{
		return FMcpToolResult::Error(TEXT("target is required"));
	}

	UE_LOG(LogYesUeMcp, Log, TEXT("call-function: Target='%s' World='%s'"), *Target, *WorldParam);

	// Check if it's a global Blueprint function (starts with /)
	if (Target.StartsWith(TEXT("/")))
	{
		// Global function: /Game/BP_Library.FunctionName
		int32 LastDotIndex;
		if (!Target.FindLastChar('.', LastDotIndex))
		{
			return FMcpToolResult::Error(TEXT("Invalid target format. Expected: /Game/BP_Lib.FunctionName"));
		}

		FString BlueprintPath = Target.Left(LastDotIndex);
		FString FunctionName = Target.Mid(LastDotIndex + 1);

		return CallGlobalFunction(BlueprintPath, FunctionName, Arguments);
	}

	// Actor or component function
	UWorld* World = GetTargetWorld(WorldParam);
	if (!World)
	{
		if (WorldParam.Equals(TEXT("pie"), ESearchCase::IgnoreCase))
		{
			return FMcpToolResult::Error(TEXT("No PIE session running. Use pie-session action:start first."));
		}
		return FMcpToolResult::Error(TEXT("No world available. Open a level first."));
	}

	// Parse target: ActorName.FunctionName or ActorName.ComponentName.FunctionName
	TArray<FString> Parts;
	Target.ParseIntoArray(Parts, TEXT("."));

	if (Parts.Num() < 2)
	{
		return FMcpToolResult::Error(TEXT("Invalid target format. Expected: ActorName.Function or ActorName.Component.Function"));
	}

	FString ActorName = Parts[0];
	AActor* Actor = FindActorByName(World, ActorName);
	if (!Actor)
	{
		return FMcpToolResult::Error(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	if (Parts.Num() == 2)
	{
		// ActorName.FunctionName
		FString FunctionName = Parts[1];
		return CallFunctionOnObject(Actor, FunctionName, Arguments);
	}
	else if (Parts.Num() == 3)
	{
		// ActorName.ComponentName.FunctionName
		FString ComponentName = Parts[1];
		FString FunctionName = Parts[2];

		UActorComponent* Component = FindComponentByName(Actor, ComponentName);
		if (!Component)
		{
			return FMcpToolResult::Error(FString::Printf(TEXT("Component not found: %s on actor %s"), *ComponentName, *ActorName));
		}

		return CallFunctionOnObject(Component, FunctionName, Arguments);
	}
	else
	{
		return FMcpToolResult::Error(TEXT("Invalid target format. Too many parts."));
	}
}

UWorld* UCallFunctionTool::GetTargetWorld(const FString& WorldParam) const
{
	if (WorldParam.Equals(TEXT("pie"), ESearchCase::IgnoreCase))
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

	// Editor world (default)
	return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
}

AActor* UCallFunctionTool::FindActorByName(UWorld* World, const FString& ActorName) const
{
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if ((*It)->GetName().Equals(ActorName, ESearchCase::IgnoreCase) ||
			(*It)->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase))
		{
			return *It;
		}
	}
	return nullptr;
}

UActorComponent* UCallFunctionTool::FindComponentByName(AActor* Actor, const FString& ComponentName) const
{
	TArray<UActorComponent*> Components;
	Actor->GetComponents(Components);

	for (UActorComponent* Component : Components)
	{
		if (Component && Component->GetName().Equals(ComponentName, ESearchCase::IgnoreCase))
		{
			return Component;
		}
	}
	return nullptr;
}

FMcpToolResult UCallFunctionTool::CallFunctionOnObject(UObject* Object, const FString& FunctionName, const TSharedPtr<FJsonObject>& Arguments)
{
	if (!Object)
	{
		return FMcpToolResult::Error(TEXT("Object is null"));
	}

	UFunction* Function = Object->FindFunction(FName(*FunctionName));
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
	Object->ProcessEvent(Function, ParamBuffer);

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("function"), FunctionName);
	Result->SetStringField(TEXT("object"), Object->GetName());
	Result->SetStringField(TEXT("object_class"), Object->GetClass()->GetName());

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

	UE_LOG(LogYesUeMcp, Log, TEXT("call-function: Called %s on %s"), *FunctionName, *Object->GetName());
	return FMcpToolResult::Json(Result);
}

FMcpToolResult UCallFunctionTool::CallGlobalFunction(const FString& BlueprintPath, const FString& FunctionName, const TSharedPtr<FJsonObject>& Arguments)
{
	// Load the Blueprint
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
	if (!Blueprint)
	{
		return FMcpToolResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	UClass* GeneratedClass = Blueprint->GeneratedClass;
	if (!GeneratedClass)
	{
		return FMcpToolResult::Error(FString::Printf(TEXT("Blueprint has no generated class: %s"), *BlueprintPath));
	}

	// For static functions, we need the CDO
	UObject* CDO = GeneratedClass->GetDefaultObject();
	if (!CDO)
	{
		return FMcpToolResult::Error(FString::Printf(TEXT("Could not get CDO for: %s"), *BlueprintPath));
	}

	return CallFunctionOnObject(CDO, FunctionName, Arguments);
}

TSharedPtr<FJsonValue> UCallFunctionTool::GetPropertyValue(void* Container, FProperty* Property) const
{
	if (!Property || !Container) return nullptr;

	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);

	if (FNumericProperty* NumProp = CastField<FNumericProperty>(Property))
	{
		if (NumProp->IsFloatingPoint())
		{
			double Value = 0;
			NumProp->GetValue_InContainer(Container, &Value);
			return MakeShareable(new FJsonValueNumber(Value));
		}
		else
		{
			int64 Value = 0;
			NumProp->GetValue_InContainer(Container, &Value);
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
	else if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
	{
		return MakeShareable(new FJsonValueString(NameProp->GetPropertyValue(ValuePtr).ToString()));
	}

	// Fallback: export as string
	FString ExportedText;
	Property->ExportTextItem_Direct(ExportedText, ValuePtr, nullptr, nullptr, PPF_None);
	return MakeShareable(new FJsonValueString(ExportedText));
}

bool UCallFunctionTool::SetPropertyFromJson(void* PropertyAddr, FProperty* Property, const TSharedPtr<FJsonValue>& JsonValue) const
{
	if (!Property || !PropertyAddr || !JsonValue.IsValid()) return false;

	if (FNumericProperty* NumProp = CastField<FNumericProperty>(Property))
	{
		if (NumProp->IsFloatingPoint())
		{
			double Value = JsonValue->AsNumber();
			NumProp->SetFloatingPointPropertyValue(PropertyAddr, Value);
		}
		else
		{
			int64 Value = static_cast<int64>(JsonValue->AsNumber());
			NumProp->SetIntPropertyValue(PropertyAddr, Value);
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
	else if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
	{
		NameProp->SetPropertyValue(PropertyAddr, FName(*JsonValue->AsString()));
		return true;
	}

	return false;
}
