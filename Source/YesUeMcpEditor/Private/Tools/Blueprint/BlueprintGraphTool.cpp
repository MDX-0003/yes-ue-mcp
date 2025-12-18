// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/Blueprint/BlueprintGraphTool.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Tools/McpToolResult.h"

FString UBlueprintGraphTool::GetToolDescription() const
{
	return TEXT("Read complete Blueprint graph structure including all nodes, connections, and pin data. "
		"Returns event graphs, function graphs, and macro graphs with full node and pin information.");
}

TMap<FString, FMcpSchemaProperty> UBlueprintGraphTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty AssetPath;
	AssetPath.Type = TEXT("string");
	AssetPath.Description = TEXT("Asset path to the Blueprint (e.g., /Game/Blueprints/BP_Character)");
	AssetPath.bRequired = true;
	Schema.Add(TEXT("asset_path"), AssetPath);

	FMcpSchemaProperty GraphName;
	GraphName.Type = TEXT("string");
	GraphName.Description = TEXT("Specific graph name to read (optional, returns all graphs if not specified)");
	GraphName.bRequired = false;
	Schema.Add(TEXT("graph_name"), GraphName);

	FMcpSchemaProperty GraphType;
	GraphType.Type = TEXT("string");
	GraphType.Description = TEXT("Filter by graph type: 'event', 'function', or 'macro'");
	GraphType.bRequired = false;
	Schema.Add(TEXT("graph_type"), GraphType);

	FMcpSchemaProperty IncludePositions;
	IncludePositions.Type = TEXT("boolean");
	IncludePositions.Description = TEXT("Include node X/Y positions in the output (default: false)");
	IncludePositions.bRequired = false;
	Schema.Add(TEXT("include_positions"), IncludePositions);

	return Schema;
}

TArray<FString> UBlueprintGraphTool::GetRequiredParams() const
{
	return { TEXT("asset_path") };
}

FMcpToolResult UBlueprintGraphTool::Execute(
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	// Get asset path
	FString AssetPath = GetStringArg(Arguments, TEXT("asset_path"), TEXT(""));
	if (AssetPath.IsEmpty())
	{
		return FMcpToolResult::Error(TEXT("Missing required parameter: asset_path"));
	}

	// Optional parameters
	FString GraphNameFilter = GetStringArgOrDefault(Arguments, TEXT("graph_name"), TEXT(""));
	FString GraphTypeFilter = GetStringArgOrDefault(Arguments, TEXT("graph_type"), TEXT(""));
	bool bIncludePositions = GetBoolArgOrDefault(Arguments, TEXT("include_positions"), false);

	// Validate graph type filter
	if (!GraphTypeFilter.IsEmpty() &&
		GraphTypeFilter != TEXT("event") &&
		GraphTypeFilter != TEXT("function") &&
		GraphTypeFilter != TEXT("macro"))
	{
		return FMcpToolResult::Error(FString::Printf(
			TEXT("Invalid graph_type '%s'. Must be 'event', 'function', or 'macro'"), *GraphTypeFilter));
	}

	// Load Blueprint
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
	if (!Blueprint)
	{
		return FMcpToolResult::Error(FString::Printf(TEXT("Failed to load Blueprint at path: %s"), *AssetPath));
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetStringField(TEXT("blueprint"), AssetPath);
	Result->SetStringField(TEXT("parent_class"), Blueprint->ParentClass ? Blueprint->ParentClass->GetName() : TEXT("None"));

	TArray<TSharedPtr<FJsonValue>> GraphsArray;

	// Process event graphs (UbergraphPages)
	if (GraphTypeFilter.IsEmpty() || GraphTypeFilter == TEXT("event"))
	{
		for (UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			if (!Graph) continue;

			// Filter by name if specified
			if (!GraphNameFilter.IsEmpty() && Graph->GetName() != GraphNameFilter)
			{
				continue;
			}

			TSharedPtr<FJsonObject> GraphJson = GraphToJson(Graph, TEXT("event"), bIncludePositions);
			if (GraphJson.IsValid())
			{
				GraphsArray.Add(MakeShareable(new FJsonValueObject(GraphJson)));
			}
		}
	}

	// Process function graphs
	if (GraphTypeFilter.IsEmpty() || GraphTypeFilter == TEXT("function"))
	{
		for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		{
			if (!Graph) continue;

			// Filter by name if specified
			if (!GraphNameFilter.IsEmpty() && Graph->GetName() != GraphNameFilter)
			{
				continue;
			}

			TSharedPtr<FJsonObject> GraphJson = GraphToJson(Graph, TEXT("function"), bIncludePositions);
			if (GraphJson.IsValid())
			{
				GraphsArray.Add(MakeShareable(new FJsonValueObject(GraphJson)));
			}
		}
	}

	// Process macro graphs
	if (GraphTypeFilter.IsEmpty() || GraphTypeFilter == TEXT("macro"))
	{
		for (UEdGraph* Graph : Blueprint->MacroGraphs)
		{
			if (!Graph) continue;

			// Filter by name if specified
			if (!GraphNameFilter.IsEmpty() && Graph->GetName() != GraphNameFilter)
			{
				continue;
			}

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

TSharedPtr<FJsonObject> UBlueprintGraphTool::GraphToJson(UEdGraph* Graph, const FString& GraphType, bool bIncludePositions) const
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

TSharedPtr<FJsonObject> UBlueprintGraphTool::NodeToJson(UEdGraphNode* Node, bool bIncludePositions) const
{
	if (!Node)
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> NodeJson = MakeShareable(new FJsonObject);

	// Node identification
	NodeJson->SetStringField(TEXT("guid"), Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
	NodeJson->SetStringField(TEXT("class"), Node->GetClass()->GetName());
	NodeJson->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());

	// Node comment (if any)
	if (!Node->NodeComment.IsEmpty())
	{
		NodeJson->SetStringField(TEXT("comment"), Node->NodeComment);
	}

	// Node position (if requested)
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

TSharedPtr<FJsonObject> UBlueprintGraphTool::PinToJson(UEdGraphPin* Pin) const
{
	if (!Pin)
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> PinJson = MakeShareable(new FJsonObject);

	// Basic pin info
	PinJson->SetStringField(TEXT("name"), Pin->PinName.ToString());
	PinJson->SetStringField(TEXT("direction"), GetPinDirectionString(Pin->Direction));
	PinJson->SetStringField(TEXT("category"), GetPinCategoryString(Pin->PinType.PinCategory));

	// Pin type details
	if (Pin->PinType.PinSubCategory != NAME_None)
	{
		PinJson->SetStringField(TEXT("sub_category"), Pin->PinType.PinSubCategory.ToString());
	}

	if (Pin->PinType.PinSubCategoryObject.IsValid())
	{
		UObject* SubCategoryObj = Pin->PinType.PinSubCategoryObject.Get();
		if (SubCategoryObj)
		{
			PinJson->SetStringField(TEXT("sub_category_object"), SubCategoryObj->GetName());
		}
	}

	// Type modifiers
	PinJson->SetBoolField(TEXT("is_array"), Pin->PinType.IsArray());
	PinJson->SetBoolField(TEXT("is_reference"), Pin->PinType.bIsReference);
	PinJson->SetBoolField(TEXT("is_const"), Pin->PinType.bIsConst);

	// Default value
	if (!Pin->DefaultValue.IsEmpty())
	{
		PinJson->SetStringField(TEXT("default_value"), Pin->DefaultValue);
	}

	if (Pin->DefaultObject)
	{
		PinJson->SetStringField(TEXT("default_object"), Pin->DefaultObject->GetName());
	}

	if (!Pin->DefaultTextValue.IsEmpty())
	{
		PinJson->SetStringField(TEXT("default_text_value"), Pin->DefaultTextValue.ToString());
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

FString UBlueprintGraphTool::GetPinCategoryString(FName Category) const
{
	// Common pin categories
	static const TMap<FName, FString> CategoryMap = {
		{ TEXT("exec"), TEXT("exec") },
		{ TEXT("boolean"), TEXT("boolean") },
		{ TEXT("byte"), TEXT("byte") },
		{ TEXT("int"), TEXT("int") },
		{ TEXT("int64"), TEXT("int64") },
		{ TEXT("real"), TEXT("real") },
		{ TEXT("float"), TEXT("float") },
		{ TEXT("double"), TEXT("double") },
		{ TEXT("name"), TEXT("name") },
		{ TEXT("string"), TEXT("string") },
		{ TEXT("text"), TEXT("text") },
		{ TEXT("struct"), TEXT("struct") },
		{ TEXT("object"), TEXT("object") },
		{ TEXT("interface"), TEXT("interface") },
		{ TEXT("class"), TEXT("class") },
		{ TEXT("softobject"), TEXT("softobject") },
		{ TEXT("softclass"), TEXT("softclass") },
		{ TEXT("delegate"), TEXT("delegate") },
		{ TEXT("mcdelegate"), TEXT("mcdelegate") },
		{ TEXT("wildcard"), TEXT("wildcard") }
	};

	const FString* Found = CategoryMap.Find(Category);
	return Found ? *Found : Category.ToString();
}

FString UBlueprintGraphTool::GetPinDirectionString(EEdGraphPinDirection Direction) const
{
	switch (Direction)
	{
	case EGPD_Input:
		return TEXT("input");
	case EGPD_Output:
		return TEXT("output");
	default:
		return TEXT("unknown");
	}
}
