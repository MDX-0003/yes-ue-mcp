// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/Build/TriggerLiveCodingTool.h"
#include "YesUeMcpEditor.h"
#include "Engine/Engine.h"
#include "Modules/ModuleManager.h"

// Live Coding module (Windows only)
#if PLATFORM_WINDOWS
	#include "ILiveCodingModule.h"
#endif

FString UTriggerLiveCodingTool::GetToolDescription() const
{
	return TEXT("Trigger Live Coding compilation for C++ code changes. Supports synchronous mode with wait_for_completion. Windows only.");
}

TMap<FString, FMcpSchemaProperty> UTriggerLiveCodingTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty WaitForCompletion;
	WaitForCompletion.Type = TEXT("boolean");
	WaitForCompletion.Description = TEXT("Wait for compilation to complete before returning (default: false, async mode)");
	WaitForCompletion.bRequired = false;
	Schema.Add(TEXT("wait_for_completion"), WaitForCompletion);

	FMcpSchemaProperty Timeout;
	Timeout.Type = TEXT("number");
	Timeout.Description = TEXT("Timeout in seconds for synchronous compilation (default: 300, max: 600)");
	Timeout.bRequired = false;
	Schema.Add(TEXT("timeout"), Timeout);

	return Schema;
}

FMcpToolResult UTriggerLiveCodingTool::Execute(
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	bool bWaitForCompletion = GetBoolArgOrDefault(Arguments, TEXT("wait_for_completion"), false);
	int32 TimeoutSeconds = FMath::Clamp(GetIntArgOrDefault(Arguments, TEXT("timeout"), 300), 10, 600);

	UE_LOG(LogYesUeMcp, Log, TEXT("trigger-live-coding: Triggering Live Coding compilation (wait=%d, timeout=%ds)"),
		bWaitForCompletion, TimeoutSeconds);

#if !PLATFORM_WINDOWS
	return FMcpToolResult::Error(TEXT("Live Coding is only supported on Windows"));
#else

	// Try to use ILiveCodingModule for better control
	ILiveCodingModule* LiveCodingModule = FModuleManager::GetModulePtr<ILiveCodingModule>("LiveCoding");

	if (!LiveCodingModule)
	{
		UE_LOG(LogYesUeMcp, Warning, TEXT("trigger-live-coding: LiveCoding module not available, falling back to console command"));

		// Fallback to console command
		if (!GEngine || !GEngine->Exec(nullptr, TEXT("LiveCoding.Compile")))
		{
			return FMcpToolResult::Error(TEXT("Failed to trigger Live Coding. Enable it in Editor Preferences > General > Live Coding."));
		}

		TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
		Result->SetBoolField(TEXT("success"), true);
		Result->SetStringField(TEXT("status"), TEXT("triggered_async"));
		Result->SetStringField(TEXT("message"), TEXT("Live Coding triggered via console command"));
		return FMcpToolResult::Json(Result);
	}

	// Check if Live Coding is enabled
	if (!LiveCodingModule->IsEnabledForSession())
	{
		return FMcpToolResult::Error(TEXT("Live Coding is not enabled for this session. Enable it in Editor Preferences > General > Live Coding."));
	}

	if (bWaitForCompletion)
	{
		// Synchronous mode: Wait for compilation to complete
		return ExecuteSynchronous(LiveCodingModule, TimeoutSeconds);
	}
	else
	{
		// Asynchronous mode: Just trigger and return immediately
		return ExecuteAsynchronous(LiveCodingModule);
	}
#endif
}

#if PLATFORM_WINDOWS
FMcpToolResult UTriggerLiveCodingTool::ExecuteSynchronous(ILiveCodingModule* LiveCodingModule, int32 TimeoutSeconds)
{
	// State for tracking compilation completion
	bool bCompilationComplete = false;
	bool bCompilationSuccess = false;

	// Bind to completion delegate (FOnPatchCompleteDelegate takes no parameters, only called on success)
	FDelegateHandle OnPatchCompleteHandle = LiveCodingModule->GetOnPatchCompleteDelegate().AddLambda(
		[&bCompilationComplete, &bCompilationSuccess]()
		{
			bCompilationComplete = true;
			bCompilationSuccess = true;

			UE_LOG(LogYesUeMcp, Log, TEXT("trigger-live-coding: Compilation completed successfully"));
		}
	);

	// Start compilation
	UE_LOG(LogYesUeMcp, Log, TEXT("trigger-live-coding: Starting synchronous compilation..."));
	LiveCodingModule->Compile(ELiveCodingCompileFlags::None, nullptr);

	// Wait for completion with timeout
	const double StartTime = FPlatformTime::Seconds();
	const double TimeoutTime = StartTime + TimeoutSeconds;

	while (!bCompilationComplete)
	{
		// Check timeout
		if (FPlatformTime::Seconds() > TimeoutTime)
		{
			LiveCodingModule->GetOnPatchCompleteDelegate().Remove(OnPatchCompleteHandle);

			UE_LOG(LogYesUeMcp, Warning, TEXT("trigger-live-coding: Compilation timeout after %d seconds"), TimeoutSeconds);

			TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
			Result->SetBoolField(TEXT("success"), false);
			Result->SetStringField(TEXT("status"), TEXT("timeout"));
			Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Compilation timeout after %d seconds"), TimeoutSeconds));
			Result->SetNumberField(TEXT("timeout_seconds"), TimeoutSeconds);

			return FMcpToolResult::Json(Result);
		}

		// Sleep briefly to avoid busy-waiting
		FPlatformProcess::Sleep(0.1f);

		// Pump messages to process delegates
		FTSTicker::GetCoreTicker().Tick(0.1f);
	}

	// Clean up delegate
	LiveCodingModule->GetOnPatchCompleteDelegate().Remove(OnPatchCompleteHandle);

	// Calculate compilation time
	const double CompilationTime = FPlatformTime::Seconds() - StartTime;

	// Build result
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), bCompilationSuccess);
	Result->SetStringField(TEXT("status"), bCompilationSuccess ? TEXT("completed") : TEXT("failed"));
	Result->SetNumberField(TEXT("compilation_time_seconds"), CompilationTime);
	Result->SetStringField(TEXT("shortcut"), TEXT("Ctrl+Alt+F11"));

	if (bCompilationSuccess)
	{
		Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Live Coding compilation completed successfully in %.1fs"), CompilationTime));
		UE_LOG(LogYesUeMcp, Log, TEXT("trigger-live-coding: Compilation successful (%.1fs)"), CompilationTime);
	}
	else
	{
		Result->SetStringField(TEXT("message"), TEXT("Live Coding compilation failed. Check compilation_log for details."));
		UE_LOG(LogYesUeMcp, Warning, TEXT("trigger-live-coding: Compilation failed (%.1fs)"), CompilationTime);
	}

	return FMcpToolResult::Json(Result);
}

FMcpToolResult UTriggerLiveCodingTool::ExecuteAsynchronous(ILiveCodingModule* LiveCodingModule)
{
	// Just trigger compilation and return immediately
	LiveCodingModule->Compile(ELiveCodingCompileFlags::None, nullptr);

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("status"), TEXT("triggered_async"));
	Result->SetStringField(TEXT("message"), TEXT("Live Coding compilation initiated. Check Output Log for results."));
	Result->SetStringField(TEXT("shortcut"), TEXT("Ctrl+Alt+F11"));

	UE_LOG(LogYesUeMcp, Log, TEXT("trigger-live-coding: Async compilation triggered"));

	return FMcpToolResult::Json(Result);
}
#endif
