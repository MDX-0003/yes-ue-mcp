// Copyright softdaddy-o 2024. All Rights Reserved.

#include "YesUeMcpEditor.h"
#include "Tools/McpToolRegistry.h"
#include "UI/McpToolbarExtension.h"

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
#include "Tools/Project/ProjectInfoTool.h"
#include "Tools/Analysis/ClassHierarchyTool.h"
#include "Tools/Data/DataAssetTool.h"
#include "Tools/Asset/AssetSearchTool.h"
#include "Tools/Asset/InspectAssetTool.h"
#include "Tools/References/FindReferencesTool.h"
#include "Tools/Widget/WidgetBlueprintTool.h"
#include "Tools/Material/MaterialGraphTool.h"
#include "Tools/Material/MaterialParametersTool.h"
#include "Tools/Debug/GetLogsTool.h"

// Write tools
#include "Tools/Write/SetPropertyTool.h"
#include "Tools/Write/CompileBlueprintTool.h"
#include "Tools/Write/SaveAssetTool.h"
#include "Tools/Write/AddGraphNodeTool.h"
#include "Tools/Write/RemoveGraphNodeTool.h"
#include "Tools/Write/ConnectGraphPinsTool.h"
#include "Tools/Write/DisconnectGraphPinTool.h"
#include "Tools/Write/CreateAssetTool.h"
#include "Tools/Write/DeleteAssetTool.h"
#include "Tools/Write/SpawnActorTool.h"
#include "Tools/Write/DeleteActorTool.h"
#include "Tools/Write/AddComponentTool.h"
#include "Tools/Write/RemoveComponentTool.h"
#include "Tools/Write/AddWidgetTool.h"
#include "Tools/Write/RemoveWidgetTool.h"
#include "Tools/Write/AddDataTableRowTool.h"
#include "Tools/Write/RemoveDataTableRowTool.h"

DEFINE_LOG_CATEGORY(LogYesUeMcpEditor);
DEFINE_LOG_CATEGORY(LogYesUeMcp);

#define LOCTEXT_NAMESPACE "FYesUeMcpEditorModule"

void FYesUeMcpEditorModule::StartupModule()
{
	UE_LOG(LogYesUeMcpEditor, Log, TEXT("YesUeMcpEditor module starting up"));

	RegisterBuiltInTools();

	// Initialize toolbar status icon
	FMcpToolbarExtension::Initialize();
}

void FYesUeMcpEditorModule::ShutdownModule()
{
	UE_LOG(LogYesUeMcpEditor, Log, TEXT("YesUeMcpEditor module shutting down"));

	// Cleanup toolbar extension
	FMcpToolbarExtension::Shutdown();
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
	Registry.RegisterToolClass(UProjectInfoTool::StaticClass());

	// Analysis tools
	Registry.RegisterToolClass(UClassHierarchyTool::StaticClass());

	// Data tools
	Registry.RegisterToolClass(UDataAssetTool::StaticClass());

	// Asset tools
	Registry.RegisterToolClass(UAssetSearchTool::StaticClass());
	Registry.RegisterToolClass(UInspectAssetTool::StaticClass());

	// Reference tools
	Registry.RegisterToolClass(UFindReferencesTool::StaticClass());

	// Widget tools
	Registry.RegisterToolClass(UWidgetBlueprintTool::StaticClass());

	// Material tools
	Registry.RegisterToolClass(UMaterialGraphTool::StaticClass());
	Registry.RegisterToolClass(UMaterialParametersTool::StaticClass());

	// Debug tools
	Registry.RegisterToolClass(UGetLogsTool::StaticClass());

	// Write tools - Property
	Registry.RegisterToolClass(USetPropertyTool::StaticClass());
	Registry.RegisterToolClass(UCompileBlueprintTool::StaticClass());
	Registry.RegisterToolClass(USaveAssetTool::StaticClass());

	// Write tools - Graph
	Registry.RegisterToolClass(UAddGraphNodeTool::StaticClass());
	Registry.RegisterToolClass(URemoveGraphNodeTool::StaticClass());
	Registry.RegisterToolClass(UConnectGraphPinsTool::StaticClass());
	Registry.RegisterToolClass(UDisconnectGraphPinTool::StaticClass());

	// Write tools - Asset creation
	Registry.RegisterToolClass(UCreateAssetTool::StaticClass());
	Registry.RegisterToolClass(UDeleteAssetTool::StaticClass());

	// Write tools - Level editing
	Registry.RegisterToolClass(USpawnActorTool::StaticClass());
	Registry.RegisterToolClass(UDeleteActorTool::StaticClass());
	Registry.RegisterToolClass(UAddComponentTool::StaticClass());
	Registry.RegisterToolClass(URemoveComponentTool::StaticClass());

	// Write tools - Widget
	Registry.RegisterToolClass(UAddWidgetTool::StaticClass());
	Registry.RegisterToolClass(URemoveWidgetTool::StaticClass());

	// Write tools - DataTable
	Registry.RegisterToolClass(UAddDataTableRowTool::StaticClass());
	Registry.RegisterToolClass(URemoveDataTableRowTool::StaticClass());

	UE_LOG(LogYesUeMcpEditor, Log, TEXT("Registered %d MCP tools"), Registry.GetToolCount());
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FYesUeMcpEditorModule, YesUeMcpEditor)
