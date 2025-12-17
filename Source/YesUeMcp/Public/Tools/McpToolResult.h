// Copyright softdaddy-o 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpToolResult.generated.h"

/**
 * Content type for tool results
 */
UENUM()
enum class EMcpContentType : uint8
{
	Text,
	Image,
	Resource
};

/**
 * Tool execution result
 */
USTRUCT()
struct YESUEMCP_API FMcpToolResult
{
	GENERATED_BODY()

	/** Whether execution succeeded */
	UPROPERTY()
	bool bSuccess = false;

	/** Result content (array of content items) */
	TArray<TSharedPtr<FJsonObject>> Content;

	/** Is this an error result */
	UPROPERTY()
	bool bIsError = false;

	/** Create success result with text content */
	static FMcpToolResult Text(const FString& InText);

	/** Create success result with multiple text items */
	static FMcpToolResult TextArray(const TArray<FString>& InTexts);

	/** Create success result with JSON content */
	static FMcpToolResult Json(TSharedPtr<FJsonObject> JsonContent);

	/** Create success result with JSON content as formatted text */
	static FMcpToolResult JsonAsText(TSharedPtr<FJsonObject> JsonContent);

	/** Create error result */
	static FMcpToolResult Error(const FString& Message);

	/** Convert to JSON for tools/call response */
	TSharedPtr<FJsonObject> ToJson() const;
};
