// Copyright softdaddy-o 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Server/McpServer.h"
#include "McpEditorSubsystem.generated.h"

/**
 * Settings for MCP Server
 */
UCLASS(config = YesUeMcp, defaultconfig)
class YESUEMCPEDITOR_API UMcpServerSettings : public UObject
{
	GENERATED_BODY()

public:
	/** HTTP server port for MCP connections */
	UPROPERTY(config, EditAnywhere, Category = "Server", meta = (ClampMin = "1", ClampMax = "65535"))
	int32 ServerPort = 8080;

	/** Auto-start the MCP server when the editor opens */
	UPROPERTY(config, EditAnywhere, Category = "Server")
	bool bAutoStartServer = true;

	/** Bind address for HTTP server (127.0.0.1 for localhost only) */
	UPROPERTY(config, EditAnywhere, Category = "Server")
	FString BindAddress = TEXT("127.0.0.1");

	/** Enable automatic port fallback when the primary port is unavailable */
	UPROPERTY(config, EditAnywhere, Category = "Server")
	bool bEnablePortFallback = true;

	/** Maximum number of alternative ports to try (e.g., 10 means try ports 8080-8089) */
	UPROPERTY(config, EditAnywhere, Category = "Server", meta = (ClampMin = "1", ClampMax = "100", EditCondition = "bEnablePortFallback"))
	int32 MaxPortRetries = 10;

	/** Show editor notification when fallback port is used */
	UPROPERTY(config, EditAnywhere, Category = "Server", meta = (EditCondition = "bEnablePortFallback"))
	bool bNotifyOnPortFallback = true;
};

/**
 * Editor subsystem managing MCP server lifecycle
 */
UCLASS()
class YESUEMCPEDITOR_API UMcpEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	//~ UEditorSubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Get the MCP server instance (C++ only) */
	FMcpServer* GetServer() const { return Server.Get(); }

	/** Check if server is running */
	UFUNCTION(BlueprintCallable, Category = "MCP")
	bool IsServerRunning() const;

	/** Start the MCP server */
	UFUNCTION(BlueprintCallable, Category = "MCP")
	bool StartServer();

	/** Stop the MCP server */
	UFUNCTION(BlueprintCallable, Category = "MCP")
	void StopServer();

	/** Restart the server */
	UFUNCTION(BlueprintCallable, Category = "MCP")
	void RestartServer();

	/** Get server status information */
	UFUNCTION(BlueprintCallable, Category = "MCP")
	FString GetServerStatus() const;

	/** Get settings */
	UFUNCTION(BlueprintCallable, Category = "MCP")
	UMcpServerSettings* GetSettings() const;

	/** Get the actual port the server is bound to (may differ from configured port if fallback was used) */
	UFUNCTION(BlueprintCallable, Category = "MCP")
	int32 GetActualPort() const;

	/** Check if a fallback port was used instead of the configured port */
	UFUNCTION(BlueprintCallable, Category = "MCP")
	bool IsUsingFallbackPort() const { return bUsedFallbackPort; }

private:
	/** MCP server instance */
	TUniquePtr<FMcpServer> Server;

	/** Settings */
	UPROPERTY()
	TObjectPtr<UMcpServerSettings> Settings;

	/** The port the server is actually running on (may differ from configured if fallback was used) */
	int32 ActualBoundPort = 0;

	/** Whether fallback port was used */
	bool bUsedFallbackPort = false;

	/** Load configuration from ini file */
	void LoadConfiguration();

	/** Show editor notification when fallback port is used */
	void ShowPortFallbackNotification(int32 ConfiguredPort, int32 ActualPort);
};
