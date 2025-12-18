// Copyright softdaddy-o 2024. All Rights Reserved.

#include "YesUeMcpEditor.h"
#include "Tools/McpToolRegistry.h"

// Include tool headers for registration
#include "Tools/Blueprint/BlueprintAnalysisTool.h"
#include "Tools/Blueprint/BlueprintFunctionsTool.h"
#include "Tools/Blueprint/BlueprintVariablesTool.h"
#include "Tools/Blueprint/BlueprintComponentsTool.h"
#include "Tools/Blueprint/BlueprintGraphTool.h"
#include "Tools/Asset/AssetSearchTool.h"

DEFINE_LOG_CATEGORY(LogYesUeMcpEditor);

#define LOCTEXT_NAMESPACE "FYesUeMcpEditorModule"

void FYesUeMcpEditorModule::StartupModule()
{
	UE_LOG(LogYesUeMcpEditor, Log, TEXT("YesUeMcpEditor module starting up"));

	RegisterBuiltInTools();
}

void FYesUeMcpEditorModule::ShutdownModule()
{
	UE_LOG(LogYesUeMcpEditor, Log, TEXT("YesUeMcpEditor module shutting down"));
}

void FYesUeMcpEditorModule::RegisterBuiltInTools()
{
	FMcpToolRegistry& Registry = FMcpToolRegistry::Get();

	// Blueprint tools
	Registry.RegisterToolClass(UBlueprintAnalysisTool::StaticClass());
	Registry.RegisterToolClass(UBlueprintFunctionsTool::StaticClass());
	Registry.RegisterToolClass(UBlueprintVariablesTool::StaticClass());
	Registry.RegisterToolClass(UBlueprintComponentsTool::StaticClass());
	Registry.RegisterToolClass(UBlueprintGraphTool::StaticClass());

	// Asset tools
	Registry.RegisterToolClass(UAssetSearchTool::StaticClass());

	UE_LOG(LogYesUeMcpEditor, Log, TEXT("Registered %d MCP tools"), Registry.GetToolCount());
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FYesUeMcpEditorModule, YesUeMcpEditor)
