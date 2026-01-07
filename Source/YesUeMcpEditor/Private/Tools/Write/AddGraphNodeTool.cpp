// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/Write/AddGraphNodeTool.h"
#include "Utils/McpAssetModifier.h"
#include "Utils/McpPropertySerializer.h"
#include "YesUeMcpEditor.h"
#include "Engine/Blueprint.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "K2Node.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"

FString UAddGraphNodeTool::GetToolDescription() const
{
	return TEXT("Add a node to a Blueprint or Material graph by class name. "
		"For Materials: use expression class names like 'MaterialExpressionAdd', 'MaterialExpressionSceneTexture', 'MaterialExpressionCollectionParameter'. "
		"For Blueprints: use node class names like 'K2Node_CallFunction', 'K2Node_VariableGet', 'K2Node_Event'. "
		"Properties can be set via the 'properties' parameter.");
}

TMap<FString, FMcpSchemaProperty> UAddGraphNodeTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty AssetPath;
	AssetPath.Type = TEXT("string");
	AssetPath.Description = TEXT("Asset path to the Blueprint or Material");
	AssetPath.bRequired = true;
	Schema.Add(TEXT("asset_path"), AssetPath);

	FMcpSchemaProperty NodeClass;
	NodeClass.Type = TEXT("string");
	NodeClass.Description = TEXT("Node class name. For Materials: 'MaterialExpressionAdd', 'MaterialExpressionSceneTexture', etc. "
		"For Blueprints: 'K2Node_CallFunction', 'K2Node_VariableGet', etc.");
	NodeClass.bRequired = true;
	Schema.Add(TEXT("node_class"), NodeClass);

	FMcpSchemaProperty GraphName;
	GraphName.Type = TEXT("string");
	GraphName.Description = TEXT("Graph name (for Blueprints). Default is 'EventGraph'");
	GraphName.bRequired = false;
	Schema.Add(TEXT("graph_name"), GraphName);

	FMcpSchemaProperty Position;
	Position.Type = TEXT("array");
	Position.ItemsType = TEXT("number");
	Position.Description = TEXT("Node position as [x, y]");
	Position.bRequired = false;
	Schema.Add(TEXT("position"), Position);

	FMcpSchemaProperty Properties;
	Properties.Type = TEXT("object");
	Properties.Description = TEXT("Properties to set on the node after creation (e.g., {'ParameterName': 'MyParam', 'DefaultValue': 1.0})");
	Properties.bRequired = false;
	Schema.Add(TEXT("properties"), Properties);

	return Schema;
}

TArray<FString> UAddGraphNodeTool::GetRequiredParams() const
{
	return { TEXT("asset_path"), TEXT("node_class") };
}

FMcpToolResult UAddGraphNodeTool::Execute(
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	FString AssetPath = GetStringArgOrDefault(Arguments, TEXT("asset_path"));
	FString NodeClass = GetStringArgOrDefault(Arguments, TEXT("node_class"));
	FString GraphName = GetStringArgOrDefault(Arguments, TEXT("graph_name"), TEXT("EventGraph"));

	// Get position
	FVector2D Position(0, 0);
	const TArray<TSharedPtr<FJsonValue>>* PositionArray;
	if (Arguments->TryGetArrayField(TEXT("position"), PositionArray) && PositionArray->Num() >= 2)
	{
		Position.X = (*PositionArray)[0]->AsNumber();
		Position.Y = (*PositionArray)[1]->AsNumber();
	}

	// Get properties object
	TSharedPtr<FJsonObject> Properties;
	const TSharedPtr<FJsonObject>* PropertiesPtr;
	if (Arguments->TryGetObjectField(TEXT("properties"), PropertiesPtr))
	{
		Properties = *PropertiesPtr;
	}

	if (AssetPath.IsEmpty() || NodeClass.IsEmpty())
	{
		return FMcpToolResult::Error(TEXT("asset_path and node_class are required"));
	}

	UE_LOG(LogYesUeMcp, Log, TEXT("add-graph-node: %s, class=%s"), *AssetPath, *NodeClass);

	// Load the asset
	FString LoadError;
	UObject* Object = FMcpAssetModifier::LoadAssetByPath(AssetPath, LoadError);
	if (!Object)
	{
		return FMcpToolResult::Error(LoadError);
	}

	// Begin transaction
	TSharedPtr<FScopedTransaction> Transaction = FMcpAssetModifier::BeginTransaction(
		FText::Format(NSLOCTEXT("MCP", "AddNode", "Add {0} node to {1}"),
			FText::FromString(NodeClass),
			FText::FromString(AssetPath)));

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetStringField(TEXT("asset"), AssetPath);
	Result->SetStringField(TEXT("node_class"), NodeClass);

	// Handle Material
	if (UMaterial* Material = Cast<UMaterial>(Object))
	{
		FString Error;
		UMaterialExpression* Expression = CreateMaterialExpression(Material, NodeClass, Position, Properties, Error);

		if (!Expression)
		{
			return FMcpToolResult::Error(Error);
		}

		Result->SetBoolField(TEXT("success"), true);
		Result->SetStringField(TEXT("expression_name"), Expression->GetName());
		Result->SetStringField(TEXT("expression_class"), Expression->GetClass()->GetName());
		Result->SetBoolField(TEXT("needs_save"), true);

		UE_LOG(LogYesUeMcp, Log, TEXT("add-graph-node: Added material expression %s (%s)"),
			*Expression->GetName(), *Expression->GetClass()->GetName());

		return FMcpToolResult::Json(Result);
	}

	// Handle Blueprint
	if (UBlueprint* Blueprint = Cast<UBlueprint>(Object))
	{
		// Find the target graph
		UEdGraph* TargetGraph = FMcpAssetModifier::FindGraphByName(Blueprint, GraphName);
		if (!TargetGraph)
		{
			return FMcpToolResult::Error(FString::Printf(TEXT("Graph not found: %s"), *GraphName));
		}

		FString Error;
		UEdGraphNode* NewNode = CreateBlueprintNode(Blueprint, TargetGraph, NodeClass, Position, Properties, Error);

		if (!NewNode)
		{
			return FMcpToolResult::Error(Error);
		}

		Result->SetBoolField(TEXT("success"), true);
		Result->SetStringField(TEXT("node_guid"), NewNode->NodeGuid.ToString());
		Result->SetStringField(TEXT("node_class"), NewNode->GetClass()->GetName());
		Result->SetStringField(TEXT("graph"), GraphName);
		Result->SetBoolField(TEXT("needs_compile"), true);
		Result->SetBoolField(TEXT("needs_save"), true);

		UE_LOG(LogYesUeMcp, Log, TEXT("add-graph-node: Added Blueprint node %s (%s)"),
			*NewNode->NodeGuid.ToString(), *NewNode->GetClass()->GetName());

		return FMcpToolResult::Json(Result);
	}

	return FMcpToolResult::Error(TEXT("Asset must be a Blueprint or Material"));
}

UMaterialExpression* UAddGraphNodeTool::CreateMaterialExpression(
	UMaterial* Material,
	const FString& NodeClassName,
	const FVector2D& Position,
	const TSharedPtr<FJsonObject>& Properties,
	FString& OutError)
{
	// Resolve the expression class
	FString ClassError;
	UClass* ExpressionClass = nullptr;

	// Try to find the class directly
	ExpressionClass = FMcpPropertySerializer::ResolveClass(NodeClassName, ClassError);

	// If not found, try with UMaterialExpression prefix
	if (!ExpressionClass && !NodeClassName.StartsWith(TEXT("MaterialExpression")))
	{
		FString PrefixedName = TEXT("MaterialExpression") + NodeClassName;
		ExpressionClass = FMcpPropertySerializer::ResolveClass(PrefixedName, ClassError);
	}

	// Also try with U prefix
	if (!ExpressionClass && !NodeClassName.StartsWith(TEXT("U")))
	{
		FString UPrefixedName = TEXT("U") + NodeClassName;
		ExpressionClass = FMcpPropertySerializer::ResolveClass(UPrefixedName, ClassError);
	}

	if (!ExpressionClass)
	{
		OutError = FString::Printf(TEXT("Material expression class not found: %s. Try 'MaterialExpressionAdd', 'MaterialExpressionSceneTexture', etc."), *NodeClassName);
		return nullptr;
	}

	// Validate it's a material expression
	if (!ExpressionClass->IsChildOf<UMaterialExpression>())
	{
		OutError = FString::Printf(TEXT("Class '%s' is not a UMaterialExpression subclass"), *NodeClassName);
		return nullptr;
	}

	FMcpAssetModifier::MarkModified(Material);

	// Create the expression
	UMaterialExpression* Expression = NewObject<UMaterialExpression>(Material, ExpressionClass, NAME_None, RF_Transactional);
	if (!Expression)
	{
		OutError = FString::Printf(TEXT("Failed to create expression of class %s"), *ExpressionClass->GetName());
		return nullptr;
	}

	// Set position
	Expression->MaterialExpressionEditorX = Position.X;
	Expression->MaterialExpressionEditorY = Position.Y;

	// Apply properties if provided
	if (Properties.IsValid())
	{
		ApplyNodeProperties(Expression, Properties);
	}

	// Add to material
	Material->GetExpressionCollection().AddExpression(Expression);
	Material->PreEditChange(nullptr);
	Material->PostEditChange();

	FMcpAssetModifier::MarkPackageDirty(Material);

	return Expression;
}

UEdGraphNode* UAddGraphNodeTool::CreateBlueprintNode(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const FString& NodeClassName,
	const FVector2D& Position,
	const TSharedPtr<FJsonObject>& Properties,
	FString& OutError)
{
	// Resolve the node class
	FString ClassError;
	UClass* NodeClass = nullptr;

	// Try to find the class directly
	NodeClass = FMcpPropertySerializer::ResolveClass(NodeClassName, ClassError);

	// If not found, try with UK2Node_ prefix
	if (!NodeClass && !NodeClassName.StartsWith(TEXT("K2Node")))
	{
		FString PrefixedName = TEXT("K2Node_") + NodeClassName;
		NodeClass = FMcpPropertySerializer::ResolveClass(PrefixedName, ClassError);

		// Also try UK2Node_ prefix
		if (!NodeClass)
		{
			PrefixedName = TEXT("UK2Node_") + NodeClassName;
			NodeClass = FMcpPropertySerializer::ResolveClass(PrefixedName, ClassError);
		}
	}

	if (!NodeClass)
	{
		OutError = FString::Printf(TEXT("Blueprint node class not found: %s. Try 'K2Node_CallFunction', 'K2Node_VariableGet', etc."), *NodeClassName);
		return nullptr;
	}

	// Validate it's a graph node
	if (!NodeClass->IsChildOf<UEdGraphNode>())
	{
		OutError = FString::Printf(TEXT("Class '%s' is not a UEdGraphNode subclass"), *NodeClassName);
		return nullptr;
	}

	FMcpAssetModifier::MarkModified(Blueprint);

	// Create the node
	UEdGraphNode* NewNode = NewObject<UEdGraphNode>(Graph, NodeClass, NAME_None, RF_Transactional);
	if (!NewNode)
	{
		OutError = FString::Printf(TEXT("Failed to create node of class %s"), *NodeClass->GetName());
		return nullptr;
	}

	// Initialize node
	NewNode->CreateNewGuid();
	NewNode->NodePosX = Position.X;
	NewNode->NodePosY = Position.Y;

	// Apply properties if provided
	if (Properties.IsValid())
	{
		ApplyNodeProperties(NewNode, Properties);
	}

	// Special handling for certain node types that need extra setup
	if (UK2Node* K2Node = Cast<UK2Node>(NewNode))
	{
		// For CallFunction nodes, try to set up the function reference from properties
		if (UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(NewNode))
		{
			// Function setup is handled via properties (FunctionReference.MemberName)
		}
		else if (UK2Node_VariableGet* GetNode = Cast<UK2Node_VariableGet>(NewNode))
		{
			// Variable reference setup is handled via properties
		}
		else if (UK2Node_VariableSet* SetNode = Cast<UK2Node_VariableSet>(NewNode))
		{
			// Variable reference setup is handled via properties
		}

		// Allocate default pins
		K2Node->AllocateDefaultPins();
	}

	// Add to graph
	Graph->AddNode(NewNode, true, false);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FMcpAssetModifier::MarkPackageDirty(Blueprint);

	return NewNode;
}

void UAddGraphNodeTool::ApplyNodeProperties(UObject* Node, const TSharedPtr<FJsonObject>& Properties)
{
	if (!Node || !Properties.IsValid())
	{
		return;
	}

	for (const auto& Pair : Properties->Values)
	{
		const FString& PropertyName = Pair.Key;
		const TSharedPtr<FJsonValue>& Value = Pair.Value;

		// Find the property - first try direct lookup
		FProperty* Property = Node->GetClass()->FindPropertyByName(*PropertyName);
		void* Container = Node;

		if (!Property)
		{
			// Try nested property path
			FString FindError;
			if (!FMcpAssetModifier::FindPropertyByPath(Node, PropertyName, Property, Container, FindError))
			{
				UE_LOG(LogYesUeMcp, Warning, TEXT("Property not found: %s - %s"), *PropertyName, *FindError);
				continue;
			}
		}

		// Set the property value
		FString SetError;
		if (!FMcpPropertySerializer::DeserializePropertyValue(Property, Container, Value, SetError))
		{
			UE_LOG(LogYesUeMcp, Warning, TEXT("Failed to set property %s: %s"), *PropertyName, *SetError);
		}
	}
}
