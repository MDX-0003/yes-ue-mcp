// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Protocol/McpTypes.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"

TOptional<FMcpRequest> FMcpRequest::FromJson(const TSharedPtr<FJsonObject>& JsonObject)
{
	if (!JsonObject.IsValid())
	{
		return TOptional<FMcpRequest>();
	}

	FMcpRequest Request;

	// Parse jsonrpc
	if (!JsonObject->TryGetStringField(TEXT("jsonrpc"), Request.JsonRpc) || Request.JsonRpc != JSONRPC_VERSION)
	{
		return TOptional<FMcpRequest>();
	}

	// Parse method (required)
	if (!JsonObject->TryGetStringField(TEXT("method"), Request.Method))
	{
		return TOptional<FMcpRequest>();
	}

	// Parse id (optional for notifications)
	// JSON-RPC 2.0 allows id to be string, number, or null
	if (!JsonObject->TryGetStringField(TEXT("id"), Request.Id))
	{
		// Try as number
		double IdNumber;
		if (JsonObject->TryGetNumberField(TEXT("id"), IdNumber))
		{
			Request.Id = FString::Printf(TEXT("%.0f"), IdNumber);
		}
	}

	// Parse params (optional)
	const TSharedPtr<FJsonObject>* ParamsObject;
	if (JsonObject->TryGetObjectField(TEXT("params"), ParamsObject))
	{
		Request.Params = *ParamsObject;
	}

	// Parse method enum
	if (Request.Method == TEXT("initialize"))
	{
		Request.ParsedMethod = EMcpMethod::Initialize;
	}
	else if (Request.Method == TEXT("notifications/initialized"))
	{
		Request.ParsedMethod = EMcpMethod::Initialized;
	}
	else if (Request.Method == TEXT("shutdown"))
	{
		Request.ParsedMethod = EMcpMethod::Shutdown;
	}
	else if (Request.Method == TEXT("tools/list"))
	{
		Request.ParsedMethod = EMcpMethod::ToolsList;
	}
	else if (Request.Method == TEXT("tools/call"))
	{
		Request.ParsedMethod = EMcpMethod::ToolsCall;
	}
	else if (Request.Method == TEXT("resources/list"))
	{
		Request.ParsedMethod = EMcpMethod::ResourcesList;
	}
	else if (Request.Method == TEXT("resources/read"))
	{
		Request.ParsedMethod = EMcpMethod::ResourcesRead;
	}
	else if (Request.Method == TEXT("prompts/list"))
	{
		Request.ParsedMethod = EMcpMethod::PromptsList;
	}
	else if (Request.Method == TEXT("prompts/get"))
	{
		Request.ParsedMethod = EMcpMethod::PromptsGet;
	}
	else if (Request.Method == TEXT("notifications/cancelled"))
	{
		Request.ParsedMethod = EMcpMethod::CancelledNotification;
	}
	else if (Request.Method == TEXT("notifications/progress"))
	{
		Request.ParsedMethod = EMcpMethod::ProgressNotification;
	}
	else
	{
		Request.ParsedMethod = EMcpMethod::Unknown;
	}

	return Request;
}

TOptional<FMcpRequest> FMcpRequest::FromJsonString(const FString& JsonString)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
	{
		return FromJson(JsonObject);
	}

	return TOptional<FMcpRequest>();
}

FMcpResponse FMcpResponse::Success(const FString& InId, TSharedPtr<FJsonObject> InResult)
{
	FMcpResponse Response;
	Response.Id = InId;
	Response.Result = InResult;
	Response.ErrorData = nullptr;
	return Response;
}

FMcpResponse FMcpResponse::Error(const FString& InId, int32 Code, const FString& Message,
                                  TSharedPtr<FJsonObject> Data)
{
	FMcpResponse Response;
	Response.Id = InId;
	Response.Result = nullptr;

	Response.ErrorData = MakeShareable(new FJsonObject);
	Response.ErrorData->SetNumberField(TEXT("code"), Code);
	Response.ErrorData->SetStringField(TEXT("message"), Message);
	if (Data.IsValid())
	{
		Response.ErrorData->SetObjectField(TEXT("data"), Data);
	}

	return Response;
}

TSharedPtr<FJsonObject> FMcpResponse::ToJson() const
{
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
	JsonObject->SetStringField(TEXT("jsonrpc"), JsonRpc);
	JsonObject->SetStringField(TEXT("id"), Id);

	if (Result.IsValid())
	{
		JsonObject->SetObjectField(TEXT("result"), Result);
	}
	else if (ErrorData.IsValid())
	{
		JsonObject->SetObjectField(TEXT("error"), ErrorData);
	}

	return JsonObject;
}

FString FMcpResponse::ToJsonString() const
{
	TSharedPtr<FJsonObject> JsonObject = ToJson();
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	return OutputString;
}

TSharedPtr<FJsonObject> FMcpSchemaProperty::ToJson() const
{
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);

	// Only set type if it's not empty and not "any"
	// In JSON Schema, omitting "type" means any type is valid
	if (!Type.IsEmpty() && Type != TEXT("any"))
	{
		JsonObject->SetStringField(TEXT("type"), Type);
	}

	JsonObject->SetStringField(TEXT("description"), Description);

	if (Enum.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> EnumValues;
		for (const FString& EnumValue : Enum)
		{
			EnumValues.Add(MakeShareable(new FJsonValueString(EnumValue)));
		}
		JsonObject->SetArrayField(TEXT("enum"), EnumValues);
	}

	if (Default.IsValid())
	{
		JsonObject->SetField(TEXT("default"), Default);
	}

	// Add items schema for array types
	if (Type == TEXT("array") && !ItemsType.IsEmpty())
	{
		TSharedPtr<FJsonObject> ItemsObj = MakeShareable(new FJsonObject);
		ItemsObj->SetStringField(TEXT("type"), ItemsType);
		JsonObject->SetObjectField(TEXT("items"), ItemsObj);
	}

	return JsonObject;
}

TSharedPtr<FJsonObject> FMcpToolDefinition::ToJson() const
{
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
	JsonObject->SetStringField(TEXT("name"), Name);
	JsonObject->SetStringField(TEXT("description"), Description);

	// Build inputSchema
	TSharedPtr<FJsonObject> InputSchemaObj = MakeShareable(new FJsonObject);
	InputSchemaObj->SetStringField(TEXT("type"), TEXT("object"));

	// Properties
	TSharedPtr<FJsonObject> PropertiesObj = MakeShareable(new FJsonObject);
	for (const auto& Pair : InputSchema)
	{
		PropertiesObj->SetObjectField(Pair.Key, Pair.Value.ToJson());
	}
	InputSchemaObj->SetObjectField(TEXT("properties"), PropertiesObj);

	// Required fields
	if (Required.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> RequiredArray;
		for (const FString& RequiredField : Required)
		{
			RequiredArray.Add(MakeShareable(new FJsonValueString(RequiredField)));
		}
		InputSchemaObj->SetArrayField(TEXT("required"), RequiredArray);
	}

	JsonObject->SetObjectField(TEXT("inputSchema"), InputSchemaObj);

	return JsonObject;
}
