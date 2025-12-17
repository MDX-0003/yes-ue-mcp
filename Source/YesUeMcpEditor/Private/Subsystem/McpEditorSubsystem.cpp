// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Subsystem/McpEditorSubsystem.h"
#include "Server/McpServer.h"
#include "YesUeMcpEditor.h"

void UMcpEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogYesUeMcpEditor, Log, TEXT("MCP Editor Subsystem initializing"));

	LoadConfiguration();

	// Create server instance
	Server = MakeUnique<FMcpServer>();

	// Auto-start if configured
	if (Settings && Settings->bAutoStartServer)
	{
		StartServer();
	}
}

void UMcpEditorSubsystem::Deinitialize()
{
	UE_LOG(LogYesUeMcpEditor, Log, TEXT("MCP Editor Subsystem deinitializing"));

	StopServer();
	Server.Reset();

	Super::Deinitialize();
}

void UMcpEditorSubsystem::LoadConfiguration()
{
	Settings = GetMutableDefault<UMcpServerSettings>();

	UE_LOG(LogYesUeMcpEditor, Log, TEXT("MCP Settings - Port: %d, AutoStart: %s, BindAddress: %s"),
		Settings->ServerPort,
		Settings->bAutoStartServer ? TEXT("true") : TEXT("false"),
		*Settings->BindAddress);
}

bool UMcpEditorSubsystem::IsServerRunning() const
{
	return Server.IsValid() && Server->IsRunning();
}

bool UMcpEditorSubsystem::StartServer()
{
	if (!Server.IsValid())
	{
		Server = MakeUnique<FMcpServer>();
	}

	if (Server->IsRunning())
	{
		UE_LOG(LogYesUeMcpEditor, Warning, TEXT("MCP Server is already running"));
		return true;
	}

	int32 Port = Settings ? Settings->ServerPort : 8080;
	FString BindAddress = Settings ? Settings->BindAddress : TEXT("127.0.0.1");

	bool bSuccess = Server->Start(Port, BindAddress);

	if (bSuccess)
	{
		UE_LOG(LogYesUeMcpEditor, Log, TEXT("MCP Server started successfully on port %d"), Port);
	}
	else
	{
		UE_LOG(LogYesUeMcpEditor, Error, TEXT("Failed to start MCP Server on port %d"), Port);
	}

	return bSuccess;
}

void UMcpEditorSubsystem::StopServer()
{
	if (Server.IsValid())
	{
		Server->Stop();
		UE_LOG(LogYesUeMcpEditor, Log, TEXT("MCP Server stopped"));
	}
}

void UMcpEditorSubsystem::RestartServer()
{
	StopServer();
	StartServer();
}

FString UMcpEditorSubsystem::GetServerStatus() const
{
	if (!Server.IsValid())
	{
		return TEXT("Server not initialized");
	}

	if (Server->IsRunning())
	{
		return FString::Printf(TEXT("Running on port %d"), Server->GetPort());
	}

	return TEXT("Stopped");
}

UMcpServerSettings* UMcpEditorSubsystem::GetSettings() const
{
	return GetMutableDefault<UMcpServerSettings>();
}
