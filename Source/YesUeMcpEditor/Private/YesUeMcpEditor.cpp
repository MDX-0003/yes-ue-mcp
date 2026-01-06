// Copyright softdaddy-o 2024. All Rights Reserved.

#include "YesUeMcpEditor.h"
#include "Tools/McpToolRegistry.h"
#include "UI/McpToolbarExtension.h"

// Consolidated Read Tools
#include "Tools/Blueprint/QueryBlueprintTool.h"
#include "Tools/Blueprint/QueryBlueprintGraphTool.h"
#include "Tools/Level/QueryLevelTool.h"
#include "Tools/Project/ProjectInfoTool.h"
#include "Tools/Asset/QueryAssetTool.h"
#include "Tools/Material/QueryMaterialTool.h"
#include "Tools/Analysis/ClassHierarchyTool.h"
#include "Tools/References/FindReferencesTool.h"
#include "Tools/Widget/WidgetBlueprintTool.h"
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

// StateTree tools
#include "Tools/StateTree/QueryStateTreeTool.h"
#include "Tools/StateTree/AddStateTreeStateTool.h"
#include "Tools/StateTree/AddStateTreeTransitionTool.h"
#include "Tools/StateTree/AddStateTreeTaskTool.h"
#include "Tools/StateTree/RemoveStateTreeStateTool.h"

// Scripting tools
#include "Tools/Scripting/RunPythonScriptTool.h"

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

	// === Consolidated Read Tools (10 tools) ===

	// Blueprint tools (was 9, now 2)
	Registry.RegisterToolClass(UQueryBlueprintTool::StaticClass());
	Registry.RegisterToolClass(UQueryBlueprintGraphTool::StaticClass());

	// Level tools (was 2, now 1)
	Registry.RegisterToolClass(UQueryLevelTool::StaticClass());

	// Project tools (was 2, now 1)
	Registry.RegisterToolClass(UProjectInfoTool::StaticClass());

	// Asset tools (was 3, now 1)
	Registry.RegisterToolClass(UQueryAssetTool::StaticClass());

	// Material tools (was 2, now 1)
	Registry.RegisterToolClass(UQueryMaterialTool::StaticClass());

	// Analysis tools
	Registry.RegisterToolClass(UClassHierarchyTool::StaticClass());

	// Reference tools
	Registry.RegisterToolClass(UFindReferencesTool::StaticClass());

	// Widget tools
	Registry.RegisterToolClass(UWidgetBlueprintTool::StaticClass());

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

	// StateTree tools
	Registry.RegisterToolClass(UQueryStateTreeTool::StaticClass());
	Registry.RegisterToolClass(UAddStateTreeStateTool::StaticClass());
	Registry.RegisterToolClass(UAddStateTreeTransitionTool::StaticClass());
	Registry.RegisterToolClass(UAddStateTreeTaskTool::StaticClass());
	Registry.RegisterToolClass(URemoveStateTreeStateTool::StaticClass());

	// Scripting tools
	Registry.RegisterToolClass(URunPythonScriptTool::StaticClass());

	UE_LOG(LogYesUeMcpEditor, Log, TEXT("Registered %d MCP tools"), Registry.GetToolCount());
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FYesUeMcpEditorModule, YesUeMcpEditor)
