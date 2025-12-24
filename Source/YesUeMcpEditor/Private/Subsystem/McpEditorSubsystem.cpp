// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Subsystem/McpEditorSubsystem.h"
#include "Server/McpServer.h"
#include "YesUeMcpEditor.h"
#include "Log/McpLogCapture.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "YesUeMcp"

void UMcpEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogYesUeMcpEditor, Log, TEXT("MCP Editor Subsystem initializing"));

	// Initialize log capture before anything else
	FMcpLogCapture::Get().Initialize();

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

	// Shutdown log capture after server
	FMcpLogCapture::Get().Shutdown();

	Super::Deinitialize();
}

void UMcpEditorSubsystem::LoadConfiguration()
{
	Settings = GetMutableDefault<UMcpServerSettings>();

	UE_LOG(LogYesUeMcpEditor, Log, TEXT("MCP Settings - Port: %d, AutoStart: %s, BindAddress: %s, PortFallback: %s (max %d)"),
		Settings->ServerPort,
		Settings->bAutoStartServer ? TEXT("true") : TEXT("false"),
		*Settings->BindAddress,
		Settings->bEnablePortFallback ? TEXT("enabled") : TEXT("disabled"),
		Settings->MaxPortRetries);
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

	const int32 ConfiguredPort = Settings ? Settings->ServerPort : 8080;
	const FString BindAddress = Settings ? Settings->BindAddress : TEXT("127.0.0.1");
	const bool bEnableFallback = Settings ? Settings->bEnablePortFallback : true;
	const int32 MaxRetries = Settings ? Settings->MaxPortRetries : 10;
	const bool bNotifyFallback = Settings ? Settings->bNotifyOnPortFallback : true;

	// Reset fallback state
	bUsedFallbackPort = false;
	ActualBoundPort = 0;

	// Try ports starting from configured port
	int32 PortToTry = ConfiguredPort;
	const int32 MaxAttempts = bEnableFallback ? MaxRetries : 1;

	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		// Ensure port is valid
		if (PortToTry > 65535)
		{
			UE_LOG(LogYesUeMcpEditor, Error, TEXT("Port fallback exceeded valid port range"));
			break;
		}

		UE_LOG(LogYesUeMcpEditor, Log, TEXT("Attempting to start MCP Server on port %d (attempt %d/%d)"),
			PortToTry, Attempt + 1, MaxAttempts);

		if (Server->Start(PortToTry, BindAddress))
		{
			ActualBoundPort = PortToTry;
			bUsedFallbackPort = (PortToTry != ConfiguredPort);

			if (bUsedFallbackPort)
			{
				UE_LOG(LogYesUeMcpEditor, Warning,
					TEXT("MCP Server started on fallback port %d (configured port %d was unavailable)"),
					ActualBoundPort, ConfiguredPort);

				if (bNotifyFallback)
				{
					ShowPortFallbackNotification(ConfiguredPort, ActualBoundPort);
				}
			}
			else
			{
				UE_LOG(LogYesUeMcpEditor, Log, TEXT("MCP Server started successfully on port %d"), ActualBoundPort);
			}

			return true;
		}

		PortToTry = ConfiguredPort + Attempt + 1;
	}

	UE_LOG(LogYesUeMcpEditor, Error,
		TEXT("Failed to start MCP Server. Tried ports %d-%d, all unavailable."),
		ConfiguredPort, ConfiguredPort + MaxAttempts - 1);

	return false;
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
		if (bUsedFallbackPort)
		{
			const int32 ConfiguredPort = Settings ? Settings->ServerPort : 8080;
			return FString::Printf(TEXT("Running on port %d (fallback from %d)"),
				Server->GetPort(), ConfiguredPort);
		}
		return FString::Printf(TEXT("Running on port %d"), Server->GetPort());
	}

	return TEXT("Stopped");
}

UMcpServerSettings* UMcpEditorSubsystem::GetSettings() const
{
	return GetMutableDefault<UMcpServerSettings>();
}

int32 UMcpEditorSubsystem::GetActualPort() const
{
	if (Server.IsValid() && Server->IsRunning())
	{
		return Server->GetPort();
	}
	return 0;
}

void UMcpEditorSubsystem::ShowPortFallbackNotification(int32 ConfiguredPort, int32 ActualPort)
{
	FNotificationInfo Info(FText::Format(
		LOCTEXT("McpPortFallback", "MCP Server: Port {0} unavailable, using port {1}"),
		FText::AsNumber(ConfiguredPort),
		FText::AsNumber(ActualPort)
	));

	Info.bFireAndForget = true;
	Info.ExpireDuration = 5.0f;
	Info.bUseSuccessFailIcons = false;

	FSlateNotificationManager::Get().AddNotification(Info);
}

#undef LOCTEXT_NAMESPACE
