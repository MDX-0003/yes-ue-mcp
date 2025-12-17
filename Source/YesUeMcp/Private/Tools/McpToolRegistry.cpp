// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/McpToolRegistry.h"
#include "YesUeMcp.h"

FMcpToolRegistry* FMcpToolRegistry::Instance = nullptr;

FMcpToolRegistry& FMcpToolRegistry::Get()
{
	if (!Instance)
	{
		Instance = new FMcpToolRegistry();
	}
	return *Instance;
}

FMcpToolRegistry::~FMcpToolRegistry()
{
	ClearAllTools();
}

void FMcpToolRegistry::RegisterToolClass(UClass* ToolClass)
{
	if (!ToolClass || !ToolClass->IsChildOf(UMcpToolBase::StaticClass()))
	{
		UE_LOG(LogYesUeMcp, Warning, TEXT("Attempted to register invalid tool class"));
		return;
	}

	FScopeLock ScopeLock(&Lock);

	// Get tool name from CDO
	UMcpToolBase* CDO = ToolClass->GetDefaultObject<UMcpToolBase>();
	if (!CDO)
	{
		UE_LOG(LogYesUeMcp, Warning, TEXT("Failed to get CDO for tool class: %s"), *ToolClass->GetName());
		return;
	}

	FString ToolName = CDO->GetToolName();
	if (ToolName.IsEmpty())
	{
		UE_LOG(LogYesUeMcp, Warning, TEXT("Tool class %s has empty name"), *ToolClass->GetName());
		return;
	}

	if (ToolClasses.Contains(ToolName))
	{
		UE_LOG(LogYesUeMcp, Warning, TEXT("Tool '%s' is already registered, replacing"), *ToolName);
		ToolInstances.Remove(ToolName);
	}

	ToolClasses.Add(ToolName, ToolClass);
	UE_LOG(LogYesUeMcp, Log, TEXT("Registered MCP tool: %s"), *ToolName);
}

void FMcpToolRegistry::UnregisterTool(const FString& ToolName)
{
	FScopeLock ScopeLock(&Lock);

	ToolClasses.Remove(ToolName);
	ToolInstances.Remove(ToolName);

	UE_LOG(LogYesUeMcp, Log, TEXT("Unregistered MCP tool: %s"), *ToolName);
}

void FMcpToolRegistry::ClearAllTools()
{
	FScopeLock ScopeLock(&Lock);

	ToolClasses.Empty();
	ToolInstances.Empty();

	UE_LOG(LogYesUeMcp, Log, TEXT("Cleared all MCP tools"));
}

TArray<FMcpToolDefinition> FMcpToolRegistry::GetAllToolDefinitions() const
{
	FScopeLock ScopeLock(&Lock);

	TArray<FMcpToolDefinition> Definitions;
	Definitions.Reserve(ToolClasses.Num());

	for (const auto& Pair : ToolClasses)
	{
		UMcpToolBase* CDO = Pair.Value->GetDefaultObject<UMcpToolBase>();
		if (CDO)
		{
			Definitions.Add(CDO->GetDefinition());
		}
	}

	return Definitions;
}

TArray<FString> FMcpToolRegistry::GetAllToolNames() const
{
	FScopeLock ScopeLock(&Lock);

	TArray<FString> Names;
	ToolClasses.GetKeys(Names);
	return Names;
}

UMcpToolBase* FMcpToolRegistry::FindTool(const FString& ToolName)
{
	FScopeLock ScopeLock(&Lock);

	// Check if already instantiated
	if (TObjectPtr<UMcpToolBase>* ExistingTool = ToolInstances.Find(ToolName))
	{
		return ExistingTool->Get();
	}

	// Check if class is registered
	UClass** ToolClassPtr = ToolClasses.Find(ToolName);
	if (!ToolClassPtr || !*ToolClassPtr)
	{
		return nullptr;
	}

	// Create instance
	return CreateToolInstance(*ToolClassPtr);
}

bool FMcpToolRegistry::HasTool(const FString& ToolName) const
{
	FScopeLock ScopeLock(&Lock);
	return ToolClasses.Contains(ToolName);
}

int32 FMcpToolRegistry::GetToolCount() const
{
	FScopeLock ScopeLock(&Lock);
	return ToolClasses.Num();
}

FMcpToolResult FMcpToolRegistry::ExecuteTool(
	const FString& ToolName,
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	UMcpToolBase* Tool = FindTool(ToolName);
	if (!Tool)
	{
		return FMcpToolResult::Error(FString::Printf(TEXT("Tool not found: %s"), *ToolName));
	}

	// Check cancellation before execution
	if (Context.IsCancelled())
	{
		return FMcpToolResult::Error(TEXT("Request cancelled"));
	}

	return Tool->Execute(Arguments, Context);
}

UMcpToolBase* FMcpToolRegistry::CreateToolInstance(UClass* ToolClass)
{
	// Create the tool instance - use NewObject without outer to make it persistent
	UMcpToolBase* Tool = NewObject<UMcpToolBase>(GetTransientPackage(), ToolClass);
	if (!Tool)
	{
		UE_LOG(LogYesUeMcp, Error, TEXT("Failed to create tool instance for class: %s"), *ToolClass->GetName());
		return nullptr;
	}

	// Prevent garbage collection
	Tool->AddToRoot();

	// Cache the instance
	FString ToolName = Tool->GetToolName();
	ToolInstances.Add(ToolName, Tool);

	UE_LOG(LogYesUeMcp, Log, TEXT("Created MCP tool instance: %s"), *ToolName);
	return Tool;
}
