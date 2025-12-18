// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/Blueprint/BlueprintNodeTool.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Tools/McpToolResult.h"

FString UBlueprintNodeTool::GetToolDescription() const
{
	return TEXT("Get detailed information about a specific Blueprint node by GUID. "\
		"Returns all pins, connections, and node-specific properties.");
}

TMap<FString, FMcpSchemaProperty> UBlueprintNodeTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty AssetPath;
	AssetPath.Type = TEXT("string");
	AssetPath.Description = TEXT("Asset path to the Blueprint (e.g., /Game/Blueprints/BP_Character)");
	AssetPath.bRequired = true;
	Schema.Add(TEXT("asset_path"), AssetPath);

	FMcpSchemaProperty NodeGuid;
	NodeGuid.Type = TEXT("string");
	NodeGuid.Description = TEXT("Node GUID to inspect (e.g., 12345678-1234-1234-1234-123456789ABC)");
	NodeGuid.bRequired = true;
	Schema.Add(TEXT("node_guid"), NodeGuid);

	return Schema;
}

TArray<FString> UBlueprintNodeTool::GetRequiredParams() const
{
	return { TEXT("asset_path"), TEXT("node_guid") };
}

FMcpToolResult UBlueprintNodeTool::Execute(
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	// Get asset path
	FString AssetPath;
	if (!GetStringArg(Arguments, TEXT("asset_path"), AssetPath))
	{
		return FMcpToolResult::Error(TEXT("Missing required parameter: asset_path"));
	}

	// Get node GUID
	FString NodeGuidStr;
	if (!GetStringArg(Arguments, TEXT("node_guid"), NodeGuidStr))
	{
		return FMcpToolResult::Error(TEXT("Missing required parameter: node_guid"));
	}

	// Parse GUID
	FGuid NodeGuid;
	if (!FGuid::Parse(NodeGuidStr, NodeGuid))
	{
		return FMcpToolResult::Error(FString::Printf(
			TEXT("Invalid GUID format: %s"), *NodeGuidStr));
	}

	// Load Blueprint
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
	if (!Blueprint)
	{
		return FMcpToolResult::Error(FString::Printf(TEXT("Failed to load Blueprint at path: %s"), *AssetPath));
	}

	// Find node
	UEdGraphNode* Node = FindNodeByGuid(Blueprint, NodeGuid);
	if (!Node)
	{
		return FMcpToolResult::Error(FString::Printf(
			TEXT("Node with GUID %s not found in Blueprint %s"), *NodeGuidStr, *AssetPath));
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetStringField(TEXT("blueprint"), AssetPath);
	Result->SetStringField(TEXT("node_guid"), NodeGuidStr);

	// Get owning graph info
	if (UEdGraph* Graph = Node->GetGraph())
	{
		Result->SetStringField(TEXT("graph_name"), Graph->GetName());

		// Determine graph type
		FString GraphType = TEXT("unknown");
		if (Blueprint->UbergraphPages.Contains(Graph))
		{
			GraphType = TEXT("event");
		}
		else if (Blueprint->FunctionGraphs.Contains(Graph))
		{
			GraphType = TEXT("function");
		}
		else if (Blueprint->MacroGraphs.Contains(Graph))
		{
			GraphType = TEXT("macro");
		}
		Result->SetStringField(TEXT("graph_type"), GraphType);
	}

	// Add node details
	TSharedPtr<FJsonObject> NodeJson = NodeToJson(Node);
	if (NodeJson.IsValid())
	{
		Result->SetObjectField(TEXT("node"), NodeJson);
	}

	return FMcpToolResult::Json(Result);
}

UEdGraphNode* UBlueprintNodeTool::FindNodeByGuid(UBlueprint* Blueprint, const FGuid& NodeGuid) const
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
				return Node;
			}
		}
	}

	return nullptr;
}

TSharedPtr<FJsonObject> UBlueprintNodeTool::NodeToJson(UEdGraphNode* Node) const
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

	// Node position
	TSharedPtr<FJsonObject> PositionJson = MakeShareable(new FJsonObject);
	PositionJson->SetNumberField(TEXT("x"), Node->NodePosX);
	PositionJson->SetNumberField(TEXT("y"), Node->NodePosY);
	NodeJson->SetObjectField(TEXT("position"), PositionJson);

	// Node enabled state
	NodeJson->SetBoolField(TEXT("is_enabled"), Node->IsNodeEnabled());

	// Advanced display info
	NodeJson->SetBoolField(TEXT("advanced_view"), Node->AdvancedPinDisplay == ENodeAdvancedPins::Shown);

	// Tooltip
	FText TooltipText = Node->GetTooltipText();
	if (!TooltipText.IsEmpty())
	{
		NodeJson->SetStringField(TEXT("tooltip"), TooltipText.ToString());
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
	NodeJson->SetNumberField(TEXT("pin_count"), PinsArray.Num());

	return NodeJson;
}

TSharedPtr<FJsonObject> UBlueprintNodeTool::PinToJson(UEdGraphPin* Pin) const
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

	// Pin state
	PinJson->SetBoolField(TEXT("is_hidden"), Pin->bHidden);
	PinJson->SetBoolField(TEXT("is_advanced"), Pin->bAdvancedView);
	PinJson->SetBoolField(TEXT("is_orphaned"), Pin->bOrphanedPin);

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
		ConnectionJson->SetStringField(TEXT("node_title"), LinkedNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
		ConnectionJson->SetStringField(TEXT("pin_name"), LinkedPin->PinName.ToString());

		ConnectionsArray.Add(MakeShareable(new FJsonValueObject(ConnectionJson)));
	}

	if (ConnectionsArray.Num() > 0)
	{
		PinJson->SetArrayField(TEXT("connections"), ConnectionsArray);
	}
	PinJson->SetNumberField(TEXT("connection_count"), ConnectionsArray.Num());

	return PinJson;
}

FString UBlueprintNodeTool::GetPinCategoryString(FName Category) const
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

FString UBlueprintNodeTool::GetPinDirectionString(EEdGraphPinDirection Direction) const
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
