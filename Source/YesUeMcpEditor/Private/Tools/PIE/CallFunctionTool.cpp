// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/PIE/CallFunctionTool.h"
#include "YesUeMcpEditor.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/UnrealType.h"

FString UCallFunctionTool::GetToolDescription() const
{
	return TEXT("Call a Blueprint or C++ function on an actor in the PIE world. Functions must be marked as BlueprintCallable or UFUNCTIONs.");
}

TMap<FString, FMcpSchemaProperty> UCallFunctionTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty ActorName;
	ActorName.Type = TEXT("string");
	ActorName.Description = TEXT("Name of the actor to call the function on (e.g., 'BP_Enemy_C_0')");
	ActorName.bRequired = true;
	Schema.Add(TEXT("actor_name"), ActorName);

	FMcpSchemaProperty FunctionName;
	FunctionName.Type = TEXT("string");
	FunctionName.Description = TEXT("Name of the function to call (e.g., 'TakeDamage', 'SetHealth')");
	FunctionName.bRequired = true;
	Schema.Add(TEXT("function_name"), FunctionName);

	FMcpSchemaProperty Arguments;
	Arguments.Type = TEXT("object");
	Arguments.Description = TEXT("Arguments to pass to the function as {\"ParamName\": value} object");
	Arguments.bRequired = false;
	Schema.Add(TEXT("arguments"), Arguments);

	return Schema;
}

FMcpToolResult UCallFunctionTool::Execute(
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	FString ActorName = GetStringArg(Arguments, TEXT("actor_name"));
	FString FunctionName = GetStringArg(Arguments, TEXT("function_name"));

	UE_LOG(LogYesUeMcp, Log, TEXT("call-function: Calling %s on %s"), *FunctionName, *ActorName);

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

	// Find the function
	UFunction* Function = Actor->FindFunction(FName(*FunctionName));
	if (!Function)
	{
		return FMcpToolResult::Error(FString::Printf(TEXT("Function not found: %s on %s"), *FunctionName, *ActorName));
	}

	// Check if function is callable
	if (!(Function->FunctionFlags & FUNC_BlueprintCallable) && !(Function->FunctionFlags & FUNC_Public))
	{
		UE_LOG(LogYesUeMcp, Warning, TEXT("call-function: Function %s may not be intended to be called externally"), *FunctionName);
	}

	// Allocate parameter buffer
	uint8* ParamBuffer = (uint8*)FMemory_Alloca(Function->ParmsSize);
	FMemory::Memzero(ParamBuffer, Function->ParmsSize);

	// Initialize default values
	for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It)
	{
		It->InitializeValue_InContainer(ParamBuffer);
	}

	// Set parameter values from JSON
	if (Arguments->HasField(TEXT("arguments")))
	{
		const TSharedPtr<FJsonObject>* FuncArgs;
		if (Arguments->TryGetObjectField(TEXT("arguments"), FuncArgs))
		{
			for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It)
			{
				FProperty* Param = *It;
				if (Param->PropertyFlags & CPF_ReturnParm)
				{
					continue; // Skip return parameter
				}

				FString ParamName = Param->GetName();
				if ((*FuncArgs)->HasField(ParamName))
				{
					TSharedPtr<FJsonValue> ParamValue = (*FuncArgs)->TryGetField(ParamName);
					void* ParamAddr = Param->ContainerPtrToValuePtr<void>(ParamBuffer);

					if (!SetPropertyFromJson(ParamAddr, Param, ParamValue))
					{
						UE_LOG(LogYesUeMcp, Warning, TEXT("call-function: Failed to set parameter %s"), *ParamName);
					}
				}
			}
		}
	}

	// Call the function
	Actor->ProcessEvent(Function, ParamBuffer);

	// Build result
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

			// Handle common return types
			if (FNumericProperty* NumProp = CastField<FNumericProperty>(Param))
			{
				if (NumProp->IsFloatingPoint())
				{
					double Value = 0;
					NumProp->GetValue_InContainer(ParamBuffer, &Value);
					Result->SetNumberField(TEXT("return_value"), Value);
				}
				else
				{
					int64 Value = 0;
					NumProp->GetValue_InContainer(ParamBuffer, &Value);
					Result->SetNumberField(TEXT("return_value"), static_cast<double>(Value));
				}
			}
			else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Param))
			{
				Result->SetBoolField(TEXT("return_value"), BoolProp->GetPropertyValue(ReturnAddr));
			}
			else if (FStrProperty* StrProp = CastField<FStrProperty>(Param))
			{
				Result->SetStringField(TEXT("return_value"), StrProp->GetPropertyValue(ReturnAddr));
			}
			else
			{
				// Export as string for other types
				FString ExportedText;
				Param->ExportTextItem_Direct(ExportedText, ReturnAddr, nullptr, nullptr, PPF_None);
				Result->SetStringField(TEXT("return_value"), ExportedText);
			}
			break;
		}
	}

	// Cleanup
	for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It)
	{
		It->DestroyValue_InContainer(ParamBuffer);
	}

	UE_LOG(LogYesUeMcp, Log, TEXT("call-function: Successfully called %s on %s"), *FunctionName, *ActorName);

	return FMcpToolResult::Json(Result);
}

AActor* UCallFunctionTool::FindActorByName(UWorld* World, const FString& ActorName) const
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

bool UCallFunctionTool::SetPropertyFromJson(void* PropertyAddr, FProperty* Property, const TSharedPtr<FJsonValue>& JsonValue) const
{
	if (!PropertyAddr || !Property || !JsonValue.IsValid())
	{
		return false;
	}

	if (FNumericProperty* NumProp = CastField<FNumericProperty>(Property))
	{
		if (NumProp->IsFloatingPoint())
		{
			double Value = JsonValue->AsNumber();
			NumProp->SetFloatingPointPropertyValue(PropertyAddr, Value);
			return true;
		}
		else
		{
			int64 Value = static_cast<int64>(JsonValue->AsNumber());
			NumProp->SetIntPropertyValue(PropertyAddr, Value);
			return true;
		}
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

	// For other types, try import from string
	FString ValueStr = JsonValue->AsString();
	if (!ValueStr.IsEmpty())
	{
		Property->ImportText_Direct(*ValueStr, PropertyAddr, nullptr, PPF_None);
		return true;
	}

	return false;
}
