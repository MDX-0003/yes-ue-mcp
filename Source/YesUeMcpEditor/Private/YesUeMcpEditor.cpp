// Copyright softdaddy-o 2024. All Rights Reserved.

#include "YesUeMcpEditor.h"
#include "Tools/McpToolRegistry.h"

// Include tool headers for registration
#include "Tools/Blueprint/BlueprintAnalysisTool.h"
#include "Tools/Blueprint/BlueprintFunctionsTool.h"
#include "Tools/Blueprint/BlueprintVariablesTool.h"
#include "Tools/Blueprint/BlueprintComponentsTool.h"
#include "Tools/Blueprint/BlueprintGraphTool.h"
#include "Tools/Blueprint/BlueprintNodeTool.h"
#include "Tools/Blueprint/BlueprintDefaultsTool.h"
#include "Tools/Blueprint/BlueprintCallableListTool.h"
#include "Tools/Blueprint/CallableDetailsTool.h"
#include "Tools/Level/QueryLevelTool.h"
#include "Tools/Level/ActorDetailsTool.h"
#include "Tools/Project/ProjectSettingsTool.h"
#include "Tools/Analysis/ClassHierarchyTool.h"
#include "Tools/Data/DataAssetTool.h"
#include "Tools/Asset/AssetSearchTool.h"
#include "Tools/Asset/InspectAssetTool.h"

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
	Registry.RegisterToolClass(UBlueprintNodeTool::StaticClass());
	Registry.RegisterToolClass(UBlueprintDefaultsTool::StaticClass());
	Registry.RegisterToolClass(UBlueprintCallableListTool::StaticClass());
	Registry.RegisterToolClass(UCallableDetailsTool::StaticClass());

	// Level/World tools
	Registry.RegisterToolClass(UQueryLevelTool::StaticClass());
	Registry.RegisterToolClass(UActorDetailsTool::StaticClass());

	// Project configuration tools
	Registry.RegisterToolClass(UProjectSettingsTool::StaticClass());

	// Analysis tools
	Registry.RegisterToolClass(UClassHierarchyTool::StaticClass());

	// Data tools
	Registry.RegisterToolClass(UDataAssetTool::StaticClass());

	// Asset tools
	Registry.RegisterToolClass(UAssetSearchTool::StaticClass());
	Registry.RegisterToolClass(UInspectAssetTool::StaticClass());

	UE_LOG(LogYesUeMcpEditor, Log, TEXT("Registered %d MCP tools"), Registry.GetToolCount());
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FYesUeMcpEditorModule, YesUeMcpEditor)
