// Copyright softdaddy-o 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpTypes.generated.h"

/** MCP Protocol version */
#define MCP_PROTOCOL_VERSION TEXT("2025-06-18")

/** JSON-RPC version */
#define JSONRPC_VERSION TEXT("2.0")

/**
 * MCP request methods
 */
UENUM()
enum class EMcpMethod : uint8
{
	// Lifecycle
	Initialize,
	Initialized,
	Shutdown,

	// Tools
	ToolsList,
	ToolsCall,

	// Resources (future)
	ResourcesList,
	ResourcesRead,
	ResourcesTemplatesList,
	ResourcesSubscribe,
	ResourcesUnsubscribe,

	// Prompts (future)
	PromptsList,
	PromptsGet,

	// Notifications
	CancelledNotification,
	ProgressNotification,
	ResourcesListChanged,
	ToolsListChanged,

	Unknown
};

/**
 * JSON-RPC error codes
 * https://www.jsonrpc.org/specification#error_object
 */
namespace EMcpErrorCode
{
	// JSON-RPC standard errors
	constexpr int32 ParseError = -32700;
	constexpr int32 InvalidRequest = -32600;
	constexpr int32 MethodNotFound = -32601;
	constexpr int32 InvalidParams = -32602;
	constexpr int32 InternalError = -32603;

	// MCP-specific errors
	constexpr int32 ServerError = -32000;
	constexpr int32 ClientError = -32001;
}

/**
 * JSON-RPC Request structure
 */
USTRUCT()
struct YESUEMCP_API FMcpRequest
{
	GENERATED_BODY()

	/** JSON-RPC version (always "2.0") */
	UPROPERTY()
	FString JsonRpc = JSONRPC_VERSION;

	/** Request ID (string or number, stored as string) */
	UPROPERTY()
	FString Id;

	/** Method name */
	UPROPERTY()
	FString Method;

	/** Parameters object */
	TSharedPtr<FJsonObject> Params;

	/** Parsed method enum for fast dispatch */
	EMcpMethod ParsedMethod = EMcpMethod::Unknown;

	/** Check if this is a notification (no ID) */
	bool IsNotification() const { return Id.IsEmpty(); }

	/** Parse from JSON object */
	static TOptional<FMcpRequest> FromJson(const TSharedPtr<FJsonObject>& JsonObject);

	/** Parse from JSON string */
	static TOptional<FMcpRequest> FromJsonString(const FString& JsonString);
};

/**
 * JSON-RPC Response structure
 */
USTRUCT()
struct YESUEMCP_API FMcpResponse
{
	GENERATED_BODY()

	/** JSON-RPC version */
	UPROPERTY()
	FString JsonRpc = JSONRPC_VERSION;

	/** Request ID (matches request) */
	UPROPERTY()
	FString Id;

	/** Result object (mutually exclusive with Error) */
	TSharedPtr<FJsonObject> Result;

	/** Error object (mutually exclusive with Result) */
	TSharedPtr<FJsonObject> Error;

	/** Create success response */
	static FMcpResponse Success(const FString& InId, TSharedPtr<FJsonObject> InResult);

	/** Create error response */
	static FMcpResponse Error(const FString& InId, int32 Code, const FString& Message,
	                          TSharedPtr<FJsonObject> Data = nullptr);

	/** Serialize to JSON string */
	FString ToJsonString() const;

	/** Serialize to JSON object */
	TSharedPtr<FJsonObject> ToJson() const;
};

/**
 * Tool input schema property
 */
USTRUCT()
struct YESUEMCP_API FMcpSchemaProperty
{
	GENERATED_BODY()

	UPROPERTY()
	FString Type;

	UPROPERTY()
	FString Description;

	UPROPERTY()
	bool bRequired = false;

	UPROPERTY()
	TArray<FString> Enum;

	/** Default value (optional) */
	TSharedPtr<FJsonValue> Default;

	TSharedPtr<FJsonObject> ToJson() const;
};

/**
 * Tool definition for MCP tools/list response
 */
USTRUCT()
struct YESUEMCP_API FMcpToolDefinition
{
	GENERATED_BODY()

	/** Unique tool name */
	UPROPERTY()
	FString Name;

	/** Human-readable description */
	UPROPERTY()
	FString Description;

	/** Input schema properties */
	TMap<FString, FMcpSchemaProperty> InputSchema;

	/** Required property names */
	UPROPERTY()
	TArray<FString> Required;

	TSharedPtr<FJsonObject> ToJson() const;
};
