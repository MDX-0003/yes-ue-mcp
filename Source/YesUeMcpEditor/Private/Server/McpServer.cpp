// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Server/McpServer.h"
#include "YesUeMcpEditor.h"
#include "Tools/McpToolRegistry.h"
#include "HttpServerModule.h"
#include "HttpPath.h"
#include "Async/Async.h"
#include "Misc/Guid.h"

FMcpServer::FMcpServer()
{
	// Initialize capabilities
	Capabilities.bSupportsTools = true;
	Capabilities.bToolsListChanged = false;
	Capabilities.bSupportsResources = false;
	Capabilities.bSupportsPrompts = false;
	Capabilities.bSupportsLogging = false;

	// Initialize server info
	ServerInfo.Name = TEXT("yes-ue-mcp");
	ServerInfo.Version = TEXT("1.0.0");
}

FMcpServer::~FMcpServer()
{
	Stop();
}

bool FMcpServer::Start(int32 Port, const FString& BindAddress)
{
	if (bIsRunning)
	{
		UE_LOG(LogYesUeMcpEditor, Warning, TEXT("MCP Server is already running"));
		return true;
	}

	ServerPort = Port;

	// Get HTTP server module
	FHttpServerModule& HttpServerModule = FHttpServerModule::Get();

	// Get router for our port
	HttpRouter = HttpServerModule.GetHttpRouter(ServerPort);
	if (!HttpRouter.IsValid())
	{
		UE_LOG(LogYesUeMcpEditor, Error, TEXT("Failed to get HTTP router for port %d"), ServerPort);
		return false;
	}

	// Bind MCP route
	McpRouteHandle = HttpRouter->BindRoute(
		FHttpPath(TEXT("/mcp")),
		EHttpServerRequestVerbs::VERB_POST | EHttpServerRequestVerbs::VERB_GET | EHttpServerRequestVerbs::VERB_DELETE | EHttpServerRequestVerbs::VERB_OPTIONS,
		FHttpRequestHandler::CreateRaw(this, &FMcpServer::HandleMcpRequest)
	);

	if (!McpRouteHandle.IsValid())
	{
		UE_LOG(LogYesUeMcpEditor, Error, TEXT("Failed to bind MCP route"));
		return false;
	}

	// Start listener
	HttpServerModule.StartAllListeners();

	bIsRunning = true;
	UE_LOG(LogYesUeMcpEditor, Log, TEXT("MCP Server started on http://%s:%d/mcp"), *BindAddress, ServerPort);

	return true;
}

void FMcpServer::Stop()
{
	if (!bIsRunning)
	{
		return;
	}

	// Unbind route
	if (HttpRouter.IsValid() && McpRouteHandle.IsValid())
	{
		HttpRouter->UnbindRoute(McpRouteHandle);
	}

	// Note: Don't stop all listeners as other modules might be using HTTP

	bIsRunning = false;
	bInitialized = false;
	CurrentSessionId.Empty();

	UE_LOG(LogYesUeMcpEditor, Log, TEXT("MCP Server stopped"));
}

bool FMcpServer::HandleMcpRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	// Handle CORS preflight
	if (Request.Verb == EHttpServerRequestVerbs::VERB_OPTIONS)
	{
		TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(TEXT(""), TEXT("text/plain"));
		Response->Headers.Add(TEXT("Access-Control-Allow-Origin"), TEXT("*"));
		Response->Headers.Add(TEXT("Access-Control-Allow-Methods"), TEXT("GET, POST, DELETE, OPTIONS"));
		Response->Headers.Add(TEXT("Access-Control-Allow-Headers"), TEXT("Content-Type, Accept, Mcp-Session-Id, MCP-Protocol-Version"));
		Response->Code = EHttpServerResponseCodes::NoContent;
		OnComplete(MoveTemp(Response));
		return true;
	}

	// Handle DELETE (session termination)
	if (Request.Verb == EHttpServerRequestVerbs::VERB_DELETE)
	{
		CurrentSessionId.Empty();
		bInitialized = false;

		TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(TEXT(""), TEXT("text/plain"));
		Response->Headers.Add(TEXT("Access-Control-Allow-Origin"), TEXT("*"));
		Response->Code = EHttpServerResponseCodes::NoContent;
		OnComplete(MoveTemp(Response));
		return true;
	}

	// Handle GET (SSE stream - not implemented yet)
	if (Request.Verb == EHttpServerRequestVerbs::VERB_GET)
	{
		SendErrorResponse(OnComplete, 501, EMcpErrorCode::ServerError, TEXT("SSE streaming not implemented"));
		return true;
	}

	// Handle POST (JSON-RPC requests)
	if (Request.Verb != EHttpServerRequestVerbs::VERB_POST)
	{
		SendErrorResponse(OnComplete, 405, EMcpErrorCode::InvalidRequest, TEXT("Method not allowed"));
		return true;
	}

	// Parse body
	FString Body;
	if (Request.Body.Num() > 0)
	{
		Body = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Request.Body.GetData())));
	}

	if (Body.IsEmpty())
	{
		SendErrorResponse(OnComplete, 400, EMcpErrorCode::ParseError, TEXT("Empty request body"));
		return true;
	}

	UE_LOG(LogYesUeMcpEditor, Verbose, TEXT("MCP Request: %s"), *Body);

	// Parse JSON-RPC request
	TOptional<FMcpRequest> ParsedRequest = FMcpRequest::FromJsonString(Body);
	if (!ParsedRequest.IsSet())
	{
		SendErrorResponse(OnComplete, 400, EMcpErrorCode::ParseError, TEXT("Invalid JSON-RPC request"));
		return true;
	}

	// Process on game thread for UE API access
	FMcpRequest McpRequest = ParsedRequest.GetValue();

	AsyncTask(ENamedThreads::GameThread, [this, McpRequest, OnComplete]()
	{
		FMcpResponse Response = ProcessRequest(McpRequest);

		// Handle notification (no response needed)
		if (McpRequest.IsNotification())
		{
			TUniquePtr<FHttpServerResponse> HttpResponse = FHttpServerResponse::Create(TEXT(""), TEXT("text/plain"));
			HttpResponse->Headers.Add(TEXT("Access-Control-Allow-Origin"), TEXT("*"));
			HttpResponse->Code = EHttpServerResponseCodes::Accepted;
			OnComplete(MoveTemp(HttpResponse));
			return;
		}

		SendResponse(OnComplete, Response);
	});

	return true;
}

FMcpResponse FMcpServer::ProcessRequest(const FMcpRequest& Request)
{
	switch (Request.ParsedMethod)
	{
	case EMcpMethod::Initialize:
		return HandleInitialize(Request);

	case EMcpMethod::Initialized:
		// Notification, no response needed
		bInitialized = true;
		return FMcpResponse::Success(Request.Id, MakeShareable(new FJsonObject));

	case EMcpMethod::ToolsList:
		return HandleToolsList(Request);

	case EMcpMethod::ToolsCall:
		return HandleToolsCall(Request);

	case EMcpMethod::Shutdown:
		bInitialized = false;
		CurrentSessionId.Empty();
		return FMcpResponse::Success(Request.Id, MakeShareable(new FJsonObject));

	default:
		return FMcpResponse::Error(Request.Id, EMcpErrorCode::MethodNotFound,
			FString::Printf(TEXT("Unknown method: %s"), *Request.Method));
	}
}

FMcpResponse FMcpServer::HandleInitialize(const FMcpRequest& Request)
{
	// Parse client info
	if (Request.Params.IsValid())
	{
		const TSharedPtr<FJsonObject>* ClientInfoObj;
		if (Request.Params->TryGetObjectField(TEXT("clientInfo"), ClientInfoObj))
		{
			ClientInfo = FMcpClientInfo::FromJson(*ClientInfoObj);
		}

		const TSharedPtr<FJsonObject>* CapabilitiesObj;
		if (Request.Params->TryGetObjectField(TEXT("capabilities"), CapabilitiesObj))
		{
			ClientCapabilities = FMcpClientCapabilities::FromJson(*CapabilitiesObj);
		}
	}

	// Generate session ID
	CurrentSessionId = GenerateSessionId();

	UE_LOG(LogYesUeMcpEditor, Log, TEXT("MCP Initialize from client: %s %s"),
		*ClientInfo.Name, *ClientInfo.Version);

	// Build response
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetStringField(TEXT("protocolVersion"), MCP_PROTOCOL_VERSION);
	Result->SetObjectField(TEXT("capabilities"), Capabilities.ToJson());
	Result->SetObjectField(TEXT("serverInfo"), ServerInfo.ToJson());

	return FMcpResponse::Success(Request.Id, Result);
}

FMcpResponse FMcpServer::HandleToolsList(const FMcpRequest& Request)
{
	TArray<FMcpToolDefinition> Tools = FMcpToolRegistry::Get().GetAllToolDefinitions();

	TArray<TSharedPtr<FJsonValue>> ToolsArray;
	for (const FMcpToolDefinition& Tool : Tools)
	{
		ToolsArray.Add(MakeShareable(new FJsonValueObject(Tool.ToJson())));
	}

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetArrayField(TEXT("tools"), ToolsArray);

	return FMcpResponse::Success(Request.Id, Result);
}

FMcpResponse FMcpServer::HandleToolsCall(const FMcpRequest& Request)
{
	if (!Request.Params.IsValid())
	{
		return FMcpResponse::Error(Request.Id, EMcpErrorCode::InvalidParams, TEXT("Missing params"));
	}

	FString ToolName;
	if (!Request.Params->TryGetStringField(TEXT("name"), ToolName))
	{
		return FMcpResponse::Error(Request.Id, EMcpErrorCode::InvalidParams, TEXT("Missing tool name"));
	}

	// Get arguments (optional)
	TSharedPtr<FJsonObject> Arguments;
	const TSharedPtr<FJsonObject>* ArgsObj;
	if (Request.Params->TryGetObjectField(TEXT("arguments"), ArgsObj))
	{
		Arguments = *ArgsObj;
	}
	else
	{
		Arguments = MakeShareable(new FJsonObject);
	}

	// Execute tool
	FMcpToolContext Context;
	Context.RequestId = Request.Id;

	FMcpToolResult ToolResult = FMcpToolRegistry::Get().ExecuteTool(ToolName, Arguments, Context);

	return FMcpResponse::Success(Request.Id, ToolResult.ToJson());
}

FString FMcpServer::GenerateSessionId() const
{
	return FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
}

void FMcpServer::SendResponse(const FHttpResultCallback& OnComplete, const FMcpResponse& Response, int32 StatusCode)
{
	FString JsonString = Response.ToJsonString();

	UE_LOG(LogYesUeMcpEditor, Verbose, TEXT("MCP Response: %s"), *JsonString);

	TUniquePtr<FHttpServerResponse> HttpResponse = FHttpServerResponse::Create(JsonString, TEXT("application/json"));
	HttpResponse->Headers.Add(TEXT("Access-Control-Allow-Origin"), TEXT("*"));

	if (!CurrentSessionId.IsEmpty())
	{
		HttpResponse->Headers.Add(TEXT("Mcp-Session-Id"), CurrentSessionId);
	}

	HttpResponse->Code = static_cast<EHttpServerResponseCodes>(StatusCode);
	OnComplete(MoveTemp(HttpResponse));
}

void FMcpServer::SendErrorResponse(const FHttpResultCallback& OnComplete, int32 HttpStatus, int32 JsonRpcCode, const FString& Message)
{
	FMcpResponse Response = FMcpResponse::Error(TEXT(""), JsonRpcCode, Message);
	SendResponse(OnComplete, Response, HttpStatus);
}
