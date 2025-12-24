// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/Blueprint/CallableDetailsTool.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Tools/McpToolResult.h"
#include "YesUeMcpEditor.h"

FString UCallableDetailsTool::GetToolDescription() const
{
	return TEXT("Get complete graph structure for a specific callable (event, function, or macro). "
		"Returns all nodes, pins, and connections for the specified graph.");
}

TMap<FString, FMcpSchemaProperty> UCallableDetailsTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty AssetPath;
	AssetPath.Type = TEXT("string");
	AssetPath.Description = TEXT("Asset path to the Blueprint (e.g., /Game/Blueprints/BP_Character)");
	AssetPath.bRequired = true;
	Schema.Add(TEXT("asset_path"), AssetPath);

	FMcpSchemaProperty CallableName;
	CallableName.Type = TEXT("string");
	CallableName.Description = TEXT("Name of the event/function/macro to retrieve details for");
	CallableName.bRequired = true;
	Schema.Add(TEXT("callable_name"), CallableName);

	FMcpSchemaProperty IncludePositions;
	IncludePositions.Type = TEXT("boolean");
	IncludePositions.Description = TEXT("Include node X/Y positions in the output (default: false)");
	IncludePositions.bRequired = false;
	Schema.Add(TEXT("include_positions"), IncludePositions);

	return Schema;
}

TArray<FString> UCallableDetailsTool::GetRequiredParams() const
{
	return { TEXT("asset_path"), TEXT("callable_name") };
}

FMcpToolResult UCallableDetailsTool::Execute(
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	// Get required parameters
	FString AssetPath;
	if (!GetStringArg(Arguments, TEXT("asset_path"), AssetPath))
	{
		return FMcpToolResult::Error(TEXT("Missing required parameter: asset_path"));
	}

	FString CallableName;
	if (!GetStringArg(Arguments, TEXT("callable_name"), CallableName))
	{
		return FMcpToolResult::Error(TEXT("Missing required parameter: callable_name"));
	}

	// Optional parameter
	bool bIncludePositions = GetBoolArgOrDefault(Arguments, TEXT("include_positions"), false);

	UE_LOG(LogYesUeMcp, Log, TEXT("get-callable-details: path='%s', callable='%s'"), *AssetPath, *CallableName);

	// Load Blueprint
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
	if (!Blueprint)
	{
		UE_LOG(LogYesUeMcp, Warning, TEXT("get-callable-details: Failed to load Blueprint '%s'"), *AssetPath);
		return FMcpToolResult::Error(FString::Printf(TEXT("Failed to load Blueprint at path: %s"), *AssetPath));
	}

	// Find the graph by name
	FString GraphType;
	UEdGraph* Graph = FindGraphByName(Blueprint, CallableName, GraphType);
	if (!Graph)
	{
		UE_LOG(LogYesUeMcp, Warning, TEXT("get-callable-details: Callable '%s' not found in Blueprint '%s'"), *CallableName, *AssetPath);
		return FMcpToolResult::Error(FString::Printf(
			TEXT("Callable '%s' not found in Blueprint. Make sure the name matches exactly."), *CallableName));
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetStringField(TEXT("blueprint"), AssetPath);
	Result->SetStringField(TEXT("parent_class"), Blueprint->ParentClass ? Blueprint->ParentClass->GetName() : TEXT("None"));

	// Convert graph to JSON
	TSharedPtr<FJsonObject> GraphJson = GraphToJson(Graph, GraphType, bIncludePositions);
	if (!GraphJson.IsValid())
	{
		return FMcpToolResult::Error(TEXT("Failed to convert graph to JSON"));
	}

	Result->SetObjectField(TEXT("graph"), GraphJson);

	return FMcpToolResult::Json(Result);
}

UEdGraph* UCallableDetailsTool::FindGraphByName(UBlueprint* Blueprint, const FString& GraphName, FString& OutGraphType) const
{
	if (!Blueprint)
	{
		return nullptr;
	}

	// Search in event graphs (UbergraphPages)
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (Graph && Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
		{
			OutGraphType = TEXT("event");
			return Graph;
		}
	}

	// Search in function graphs
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
		{
			OutGraphType = TEXT("function");
			return Graph;
		}
	}

	// Search in macro graphs
	for (UEdGraph* Graph : Blueprint->MacroGraphs)
	{
		if (Graph && Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
		{
			OutGraphType = TEXT("macro");
			return Graph;
		}
	}

	return nullptr;
}

TSharedPtr<FJsonObject> UCallableDetailsTool::GraphToJson(UEdGraph* Graph, const FString& GraphType, bool bIncludePositions) const
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

TSharedPtr<FJsonObject> UCallableDetailsTool::NodeToJson(UEdGraphNode* Node, bool bIncludePositions) const
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

TSharedPtr<FJsonObject> UCallableDetailsTool::PinToJson(UEdGraphPin* Pin) const
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

FString UCallableDetailsTool::GetPinCategoryString(FName Category) const
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

FString UCallableDetailsTool::GetPinDirectionString(EEdGraphPinDirection Direction) const
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
