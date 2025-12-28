// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/Blueprint/QueryBlueprintGraphTool.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_Event.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_Tunnel.h"
#include "EdGraphSchema_K2.h"
#include "Tools/McpToolResult.h"
#include "YesUeMcpEditor.h"

FString UQueryBlueprintGraphTool::GetToolDescription() const
{
	return TEXT("Query Blueprint graphs: list all graphs, get specific node by GUID, get callable details, or list callables. "
		"Use node_guid for specific node, callable_name for callable graph, list_callables=true for callable summary.");
}

TMap<FString, FMcpSchemaProperty> UQueryBlueprintGraphTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty AssetPath;
	AssetPath.Type = TEXT("string");
	AssetPath.Description = TEXT("Asset path to the Blueprint (e.g., /Game/Blueprints/BP_Character)");
	AssetPath.bRequired = true;
	Schema.Add(TEXT("asset_path"), AssetPath);

	// Mode selectors (mutually exclusive)
	FMcpSchemaProperty NodeGuid;
	NodeGuid.Type = TEXT("string");
	NodeGuid.Description = TEXT("Get specific node by GUID (e.g., 12345678-1234-1234-1234-123456789ABC)");
	NodeGuid.bRequired = false;
	Schema.Add(TEXT("node_guid"), NodeGuid);

	FMcpSchemaProperty CallableName;
	CallableName.Type = TEXT("string");
	CallableName.Description = TEXT("Get specific callable's graph by name (event, function, or macro)");
	CallableName.bRequired = false;
	Schema.Add(TEXT("callable_name"), CallableName);

	FMcpSchemaProperty ListCallables;
	ListCallables.Type = TEXT("boolean");
	ListCallables.Description = TEXT("List all callables (events, functions, macros) without full graph details (default: false)");
	ListCallables.bRequired = false;
	Schema.Add(TEXT("list_callables"), ListCallables);

	// Filtering options
	FMcpSchemaProperty GraphName;
	GraphName.Type = TEXT("string");
	GraphName.Description = TEXT("Filter by specific graph name");
	GraphName.bRequired = false;
	Schema.Add(TEXT("graph_name"), GraphName);

	FMcpSchemaProperty GraphType;
	GraphType.Type = TEXT("string");
	GraphType.Description = TEXT("Filter by graph type: 'event', 'function', or 'macro'");
	GraphType.bRequired = false;
	Schema.Add(TEXT("graph_type"), GraphType);

	FMcpSchemaProperty IncludePositions;
	IncludePositions.Type = TEXT("boolean");
	IncludePositions.Description = TEXT("Include node X/Y positions (default: false)");
	IncludePositions.bRequired = false;
	Schema.Add(TEXT("include_positions"), IncludePositions);

	return Schema;
}

TArray<FString> UQueryBlueprintGraphTool::GetRequiredParams() const
{
	return { TEXT("asset_path") };
}

FMcpToolResult UQueryBlueprintGraphTool::Execute(
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	FString AssetPath;
	if (!GetStringArg(Arguments, TEXT("asset_path"), AssetPath))
	{
		return FMcpToolResult::Error(TEXT("Missing required parameter: asset_path"));
	}

	// Mode selectors
	FString NodeGuidStr = GetStringArgOrDefault(Arguments, TEXT("node_guid"), TEXT(""));
	FString CallableName = GetStringArgOrDefault(Arguments, TEXT("callable_name"), TEXT(""));
	bool bListCallables = GetBoolArgOrDefault(Arguments, TEXT("list_callables"), false);

	// Filtering
	FString GraphNameFilter = GetStringArgOrDefault(Arguments, TEXT("graph_name"), TEXT(""));
	FString GraphTypeFilter = GetStringArgOrDefault(Arguments, TEXT("graph_type"), TEXT(""));
	bool bIncludePositions = GetBoolArgOrDefault(Arguments, TEXT("include_positions"), false);

	UE_LOG(LogYesUeMcp, Log, TEXT("query-blueprint-graph: path='%s'"), *AssetPath);

	// Load Blueprint
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
	if (!Blueprint)
	{
		return FMcpToolResult::Error(FString::Printf(TEXT("Failed to load Blueprint: %s"), *AssetPath));
	}

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetStringField(TEXT("blueprint"), AssetPath);
	Result->SetStringField(TEXT("parent_class"), Blueprint->ParentClass ? Blueprint->ParentClass->GetName() : TEXT("None"));

	// === Mode 1: Get specific node by GUID ===
	if (!NodeGuidStr.IsEmpty())
	{
		FGuid NodeGuid;
		if (!FGuid::Parse(NodeGuidStr, NodeGuid))
		{
			return FMcpToolResult::Error(FString::Printf(TEXT("Invalid GUID format: %s"), *NodeGuidStr));
		}

		FString GraphName, GraphType;
		UEdGraphNode* Node = FindNodeByGuid(Blueprint, NodeGuid, GraphName, GraphType);
		if (!Node)
		{
			return FMcpToolResult::Error(FString::Printf(TEXT("Node not found: %s"), *NodeGuidStr));
		}

		Result->SetStringField(TEXT("graph_name"), GraphName);
		Result->SetStringField(TEXT("graph_type"), GraphType);
		Result->SetObjectField(TEXT("node"), NodeToJson(Node, bIncludePositions));

		return FMcpToolResult::Json(Result);
	}

	// === Mode 2: Get specific callable's graph ===
	if (!CallableName.IsEmpty())
	{
		FString GraphType;
		UEdGraph* Graph = FindGraphByName(Blueprint, CallableName, GraphType);
		if (!Graph)
		{
			return FMcpToolResult::Error(FString::Printf(TEXT("Callable not found: %s"), *CallableName));
		}

		Result->SetObjectField(TEXT("graph"), GraphToJson(Graph, GraphType, bIncludePositions));

		return FMcpToolResult::Json(Result);
	}

	// === Mode 3: List callables (lightweight) ===
	if (bListCallables)
	{
		TArray<TSharedPtr<FJsonValue>> EventsArray;
		TArray<TSharedPtr<FJsonValue>> FunctionsArray;
		TArray<TSharedPtr<FJsonValue>> MacrosArray;

		ExtractEvents(Blueprint, EventsArray);
		ExtractFunctions(Blueprint, FunctionsArray);
		ExtractMacros(Blueprint, MacrosArray);

		Result->SetArrayField(TEXT("events"), EventsArray);
		Result->SetArrayField(TEXT("functions"), FunctionsArray);
		Result->SetArrayField(TEXT("macros"), MacrosArray);
		Result->SetNumberField(TEXT("event_count"), EventsArray.Num());
		Result->SetNumberField(TEXT("function_count"), FunctionsArray.Num());
		Result->SetNumberField(TEXT("macro_count"), MacrosArray.Num());

		return FMcpToolResult::Json(Result);
	}

	// === Mode 4: List all graphs with full node details ===
	TArray<TSharedPtr<FJsonValue>> GraphsArray;

	// Event graphs
	if (GraphTypeFilter.IsEmpty() || GraphTypeFilter == TEXT("event"))
	{
		for (UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			if (!Graph) continue;
			if (!GraphNameFilter.IsEmpty() && Graph->GetName() != GraphNameFilter) continue;

			TSharedPtr<FJsonObject> GraphJson = GraphToJson(Graph, TEXT("event"), bIncludePositions);
			if (GraphJson.IsValid())
			{
				GraphsArray.Add(MakeShareable(new FJsonValueObject(GraphJson)));
			}
		}
	}

	// Function graphs
	if (GraphTypeFilter.IsEmpty() || GraphTypeFilter == TEXT("function"))
	{
		for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		{
			if (!Graph) continue;
			if (!GraphNameFilter.IsEmpty() && Graph->GetName() != GraphNameFilter) continue;

			TSharedPtr<FJsonObject> GraphJson = GraphToJson(Graph, TEXT("function"), bIncludePositions);
			if (GraphJson.IsValid())
			{
				GraphsArray.Add(MakeShareable(new FJsonValueObject(GraphJson)));
			}
		}
	}

	// Macro graphs
	if (GraphTypeFilter.IsEmpty() || GraphTypeFilter == TEXT("macro"))
	{
		for (UEdGraph* Graph : Blueprint->MacroGraphs)
		{
			if (!Graph) continue;
			if (!GraphNameFilter.IsEmpty() && Graph->GetName() != GraphNameFilter) continue;

			TSharedPtr<FJsonObject> GraphJson = GraphToJson(Graph, TEXT("macro"), bIncludePositions);
			if (GraphJson.IsValid())
			{
				GraphsArray.Add(MakeShareable(new FJsonValueObject(GraphJson)));
			}
		}
	}

	Result->SetArrayField(TEXT("graphs"), GraphsArray);
	Result->SetNumberField(TEXT("graph_count"), GraphsArray.Num());

	return FMcpToolResult::Json(Result);
}

// === Graph conversion ===

TSharedPtr<FJsonObject> UQueryBlueprintGraphTool::GraphToJson(UEdGraph* Graph, const FString& GraphType, bool bIncludePositions) const
{
	if (!Graph)
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> GraphJson = MakeShareable(new FJsonObject);
	GraphJson->SetStringField(TEXT("name"), Graph->GetName());
	GraphJson->SetStringField(TEXT("type"), GraphType);

	TArray<TSharedPtr<FJsonValue>> NodesArray;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node) continue;

		TSharedPtr<FJsonObject> NodeJson = NodeToJson(Node, bIncludePositions);
		if (NodeJson.IsValid())
		{
			NodesArray.Add(MakeShareable(new FJsonValueObject(NodeJson)));
		}
	}

	GraphJson->SetArrayField(TEXT("nodes"), NodesArray);
	GraphJson->SetNumberField(TEXT("node_count"), NodesArray.Num());

	return GraphJson;
}

TSharedPtr<FJsonObject> UQueryBlueprintGraphTool::NodeToJson(UEdGraphNode* Node, bool bIncludePositions) const
{
	if (!Node)
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> NodeJson = MakeShareable(new FJsonObject);

	NodeJson->SetStringField(TEXT("guid"), Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
	NodeJson->SetStringField(TEXT("class"), Node->GetClass()->GetName());
	NodeJson->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());

	if (!Node->NodeComment.IsEmpty())
	{
		NodeJson->SetStringField(TEXT("comment"), Node->NodeComment);
	}

	if (bIncludePositions)
	{
		TSharedPtr<FJsonObject> PositionJson = MakeShareable(new FJsonObject);
		PositionJson->SetNumberField(TEXT("x"), Node->NodePosX);
		PositionJson->SetNumberField(TEXT("y"), Node->NodePosY);
		NodeJson->SetObjectField(TEXT("position"), PositionJson);
	}

	// Pins
	TArray<TSharedPtr<FJsonValue>> PinsArray;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin) continue;

		TSharedPtr<FJsonObject> PinJson = PinToJson(Pin);
		if (PinJson.IsValid())
		{
			PinsArray.Add(MakeShareable(new FJsonValueObject(PinJson)));
		}
	}
	NodeJson->SetArrayField(TEXT("pins"), PinsArray);

	return NodeJson;
}

TSharedPtr<FJsonObject> UQueryBlueprintGraphTool::PinToJson(UEdGraphPin* Pin) const
{
	if (!Pin)
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> PinJson = MakeShareable(new FJsonObject);

	PinJson->SetStringField(TEXT("name"), Pin->PinName.ToString());
	PinJson->SetStringField(TEXT("direction"), GetPinDirectionString(Pin->Direction));
	PinJson->SetStringField(TEXT("category"), GetPinCategoryString(Pin->PinType.PinCategory));

	if (Pin->PinType.PinSubCategoryObject.IsValid())
	{
		PinJson->SetStringField(TEXT("sub_category_object"), Pin->PinType.PinSubCategoryObject->GetName());
	}

	PinJson->SetBoolField(TEXT("is_array"), Pin->PinType.IsArray());

	if (!Pin->DefaultValue.IsEmpty())
	{
		PinJson->SetStringField(TEXT("default_value"), Pin->DefaultValue);
	}

	// Connections
	TArray<TSharedPtr<FJsonValue>> ConnectionsArray;
	for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
	{
		if (!LinkedPin) continue;
		UEdGraphNode* LinkedNode = LinkedPin->GetOwningNode();
		if (!LinkedNode) continue;

		TSharedPtr<FJsonObject> ConnectionJson = MakeShareable(new FJsonObject);
		ConnectionJson->SetStringField(TEXT("node_guid"), LinkedNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
		ConnectionJson->SetStringField(TEXT("pin_name"), LinkedPin->PinName.ToString());
		ConnectionsArray.Add(MakeShareable(new FJsonValueObject(ConnectionJson)));
	}

	if (ConnectionsArray.Num() > 0)
	{
		PinJson->SetArrayField(TEXT("connections"), ConnectionsArray);
	}

	return PinJson;
}

// === Node search ===

UEdGraphNode* UQueryBlueprintGraphTool::FindNodeByGuid(UBlueprint* Blueprint, const FGuid& NodeGuid, FString& OutGraphName, FString& OutGraphType) const
{
	if (!Blueprint)
	{
		return nullptr;
	}

	// Search event graphs
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (!Graph) continue;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && Node->NodeGuid == NodeGuid)
			{
				OutGraphName = Graph->GetName();
				OutGraphType = TEXT("event");
				return Node;
			}
		}
	}

	// Search function graphs
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (!Graph) continue;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && Node->NodeGuid == NodeGuid)
			{
				OutGraphName = Graph->GetName();
				OutGraphType = TEXT("function");
				return Node;
			}
		}
	}

	// Search macro graphs
	for (UEdGraph* Graph : Blueprint->MacroGraphs)
	{
		if (!Graph) continue;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && Node->NodeGuid == NodeGuid)
			{
				OutGraphName = Graph->GetName();
				OutGraphType = TEXT("macro");
				return Node;
			}
		}
	}

	return nullptr;
}

UEdGraph* UQueryBlueprintGraphTool::FindGraphByName(UBlueprint* Blueprint, const FString& CallableName, FString& OutGraphType) const
{
	if (!Blueprint)
	{
		return nullptr;
	}

	// Search event graphs for matching event
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (!Graph) continue;

		// Check if graph name matches
		if (Graph->GetName().Equals(CallableName, ESearchCase::IgnoreCase))
		{
			OutGraphType = TEXT("event");
			return Graph;
		}

		// Also check for events within the graph
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
			{
				if (EventNode->GetFunctionName().ToString().Equals(CallableName, ESearchCase::IgnoreCase))
				{
					OutGraphType = TEXT("event");
					return Graph;
				}
			}
			if (UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node))
			{
				if (CustomEvent->CustomFunctionName.ToString().Equals(CallableName, ESearchCase::IgnoreCase))
				{
					OutGraphType = TEXT("event");
					return Graph;
				}
			}
		}
	}

	// Search function graphs
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (!Graph) continue;
		if (Graph->GetName().Equals(CallableName, ESearchCase::IgnoreCase))
		{
			OutGraphType = TEXT("function");
			return Graph;
		}
	}

	// Search macro graphs
	for (UEdGraph* Graph : Blueprint->MacroGraphs)
	{
		if (!Graph) continue;
		if (Graph->GetName().Equals(CallableName, ESearchCase::IgnoreCase))
		{
			OutGraphType = TEXT("macro");
			return Graph;
		}
	}

	return nullptr;
}

// === Callable extraction ===

void UQueryBlueprintGraphTool::ExtractEvents(UBlueprint* Blueprint, TArray<TSharedPtr<FJsonValue>>& OutArray) const
{
	if (!Blueprint) return;

	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (!Graph) continue;

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;

			if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
			{
				TSharedPtr<FJsonObject> EventObj = MakeShareable(new FJsonObject);
				EventObj->SetStringField(TEXT("name"), EventNode->GetFunctionName().ToString());
				EventObj->SetStringField(TEXT("type"), Cast<UK2Node_CustomEvent>(EventNode) ? TEXT("custom") : TEXT("native"));
				EventObj->SetStringField(TEXT("graph"), Graph->GetName());

				OutArray.Add(MakeShareable(new FJsonValueObject(EventObj)));
			}
		}
	}
}

void UQueryBlueprintGraphTool::ExtractFunctions(UBlueprint* Blueprint, TArray<TSharedPtr<FJsonValue>>& OutArray) const
{
	if (!Blueprint) return;

	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (!Graph) continue;

		TSharedPtr<FJsonObject> FuncObj = MakeShareable(new FJsonObject);
		FuncObj->SetStringField(TEXT("name"), Graph->GetName());

		// Find entry node for signature
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node))
			{
				TArray<TSharedPtr<FJsonValue>> ParamsArray;
				for (UEdGraphPin* Pin : EntryNode->Pins)
				{
					if (Pin && Pin->Direction == EGPD_Output && !Pin->PinName.IsEqual(UEdGraphSchema_K2::PN_Then))
					{
						TSharedPtr<FJsonObject> ParamObj = MakeShareable(new FJsonObject);
						ParamObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
						ParamObj->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
						ParamsArray.Add(MakeShareable(new FJsonValueObject(ParamObj)));
					}
				}
				FuncObj->SetArrayField(TEXT("parameters"), ParamsArray);
				break;
			}
		}

		OutArray.Add(MakeShareable(new FJsonValueObject(FuncObj)));
	}
}

void UQueryBlueprintGraphTool::ExtractMacros(UBlueprint* Blueprint, TArray<TSharedPtr<FJsonValue>>& OutArray) const
{
	if (!Blueprint) return;

	for (UEdGraph* Graph : Blueprint->MacroGraphs)
	{
		if (!Graph) continue;

		TSharedPtr<FJsonObject> MacroObj = MakeShareable(new FJsonObject);
		MacroObj->SetStringField(TEXT("name"), Graph->GetName());

		OutArray.Add(MakeShareable(new FJsonValueObject(MacroObj)));
	}
}

// === Helpers ===

FString UQueryBlueprintGraphTool::GetPinCategoryString(FName Category) const
{
	return Category.ToString();
}

FString UQueryBlueprintGraphTool::GetPinDirectionString(EEdGraphPinDirection Direction) const
{
	return Direction == EGPD_Input ? TEXT("input") : TEXT("output");
}
