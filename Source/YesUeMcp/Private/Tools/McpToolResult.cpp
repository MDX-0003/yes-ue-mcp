// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/McpToolResult.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FMcpToolResult FMcpToolResult::Text(const FString& InText)
{
	FMcpToolResult Result;
	Result.bSuccess = true;
	Result.bIsError = false;

	TSharedPtr<FJsonObject> ContentItem = MakeShareable(new FJsonObject);
	ContentItem->SetStringField(TEXT("type"), TEXT("text"));
	ContentItem->SetStringField(TEXT("text"), InText);
	Result.Content.Add(ContentItem);

	return Result;
}

FMcpToolResult FMcpToolResult::TextArray(const TArray<FString>& InTexts)
{
	FMcpToolResult Result;
	Result.bSuccess = true;
	Result.bIsError = false;

	for (const FString& Text : InTexts)
	{
		TSharedPtr<FJsonObject> ContentItem = MakeShareable(new FJsonObject);
		ContentItem->SetStringField(TEXT("type"), TEXT("text"));
		ContentItem->SetStringField(TEXT("text"), Text);
		Result.Content.Add(ContentItem);
	}

	return Result;
}

FMcpToolResult FMcpToolResult::Json(TSharedPtr<FJsonObject> JsonContent)
{
	FMcpToolResult Result;
	Result.bSuccess = true;
	Result.bIsError = false;

	// Convert JSON to formatted text
	FString JsonString;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&JsonString);
	FJsonSerializer::Serialize(JsonContent.ToSharedRef(), Writer);

	TSharedPtr<FJsonObject> ContentItem = MakeShareable(new FJsonObject);
	ContentItem->SetStringField(TEXT("type"), TEXT("text"));
	ContentItem->SetStringField(TEXT("text"), JsonString);
	Result.Content.Add(ContentItem);

	return Result;
}

FMcpToolResult FMcpToolResult::JsonAsText(TSharedPtr<FJsonObject> JsonContent)
{
	return Json(JsonContent);
}

FMcpToolResult FMcpToolResult::Error(const FString& Message)
{
	FMcpToolResult Result;
	Result.bSuccess = false;
	Result.bIsError = true;

	TSharedPtr<FJsonObject> ContentItem = MakeShareable(new FJsonObject);
	ContentItem->SetStringField(TEXT("type"), TEXT("text"));
	ContentItem->SetStringField(TEXT("text"), Message);
	Result.Content.Add(ContentItem);

	return Result;
}

TSharedPtr<FJsonObject> FMcpToolResult::ToJson() const
{
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);

	TArray<TSharedPtr<FJsonValue>> ContentArray;
	for (const TSharedPtr<FJsonObject>& ContentItem : Content)
	{
		ContentArray.Add(MakeShareable(new FJsonValueObject(ContentItem)));
	}
	JsonObject->SetArrayField(TEXT("content"), ContentArray);

	if (bIsError)
	{
		JsonObject->SetBoolField(TEXT("isError"), true);
	}

	return JsonObject;
}
