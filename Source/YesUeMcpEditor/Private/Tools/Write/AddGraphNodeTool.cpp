// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/Write/AddGraphNodeTool.h"
#include "Utils/McpAssetModifier.h"
#include "YesUeMcpEditor.h"
#include "Engine/Blueprint.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"

FString UAddGraphNodeTool::GetToolDescription() const
{
	return TEXT("Add a node to a Blueprint or Material graph. For Blueprints, supports function calls and events. "
		"For Materials, supports expression nodes like Constant, ScalarParameter, Add, Multiply, etc.");
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
	NodeClass.Description = TEXT("Node class or type. For Blueprints: 'CallFunction', 'Event', 'VariableGet', 'VariableSet'. "
		"For Materials: 'Constant', 'ScalarParameter', 'VectorParameter', 'TextureSample', 'Add', 'Multiply', etc.");
	NodeClass.bRequired = true;
	Schema.Add(TEXT("node_class"), NodeClass);

	FMcpSchemaProperty GraphName;
	GraphName.Type = TEXT("string");
	GraphName.Description = TEXT("Graph name (for Blueprints). Default is 'EventGraph'");
	GraphName.bRequired = false;
	Schema.Add(TEXT("graph_name"), GraphName);

	FMcpSchemaProperty FunctionName;
	FunctionName.Type = TEXT("string");
	FunctionName.Description = TEXT("Function name (for CallFunction nodes)");
	FunctionName.bRequired = false;
	Schema.Add(TEXT("function_name"), FunctionName);

	FMcpSchemaProperty VariableName;
	VariableName.Type = TEXT("string");
	VariableName.Description = TEXT("Variable name (for VariableGet/VariableSet nodes)");
	VariableName.bRequired = false;
	Schema.Add(TEXT("variable_name"), VariableName);

	FMcpSchemaProperty ParameterName;
	ParameterName.Type = TEXT("string");
	ParameterName.Description = TEXT("Parameter name (for Material parameter nodes)");
	ParameterName.bRequired = false;
	Schema.Add(TEXT("parameter_name"), ParameterName);

	FMcpSchemaProperty Position;
	Position.Type = TEXT("array");
	Position.Description = TEXT("Node position as [x, y]");
	Position.bRequired = false;
	Schema.Add(TEXT("position"), Position);

	FMcpSchemaProperty Value;
	Value.Type = TEXT("number");
	Value.Description = TEXT("Default value (for Constant nodes)");
	Value.bRequired = false;
	Schema.Add(TEXT("value"), Value);

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
	FString FunctionName = GetStringArgOrDefault(Arguments, TEXT("function_name"));
	FString VariableName = GetStringArgOrDefault(Arguments, TEXT("variable_name"));
	FString ParameterName = GetStringArgOrDefault(Arguments, TEXT("parameter_name"));
	double Value = Arguments->HasField(TEXT("value")) ? Arguments->GetNumberField(TEXT("value")) : 0.0;

	// Get position
	FVector2D Position(0, 0);
	const TArray<TSharedPtr<FJsonValue>>* PositionArray;
	if (Arguments->TryGetArrayField(TEXT("position"), PositionArray) && PositionArray->Num() >= 2)
	{
		Position.X = (*PositionArray)[0]->AsNumber();
		Position.Y = (*PositionArray)[1]->AsNumber();
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
		FMcpAssetModifier::MarkModified(Material);

		UMaterialExpression* Expression = nullptr;

		// Create the appropriate expression type
		if (NodeClass.Equals(TEXT("Constant"), ESearchCase::IgnoreCase))
		{
			UMaterialExpressionConstant* Const = NewObject<UMaterialExpressionConstant>(Material);
			Const->R = Value;
			Expression = Const;
		}
		else if (NodeClass.Equals(TEXT("ScalarParameter"), ESearchCase::IgnoreCase))
		{
			UMaterialExpressionScalarParameter* Param = NewObject<UMaterialExpressionScalarParameter>(Material);
			Param->ParameterName = ParameterName.IsEmpty() ? FName(TEXT("ScalarParam")) : FName(*ParameterName);
			Param->DefaultValue = Value;
			Expression = Param;
		}
		else if (NodeClass.Equals(TEXT("VectorParameter"), ESearchCase::IgnoreCase))
		{
			UMaterialExpressionVectorParameter* Param = NewObject<UMaterialExpressionVectorParameter>(Material);
			Param->ParameterName = ParameterName.IsEmpty() ? FName(TEXT("VectorParam")) : FName(*ParameterName);
			Expression = Param;
		}
		else if (NodeClass.Equals(TEXT("TextureSample"), ESearchCase::IgnoreCase))
		{
			UMaterialExpressionTextureSample* TexSample = NewObject<UMaterialExpressionTextureSample>(Material);
			Expression = TexSample;
		}
		else if (NodeClass.Equals(TEXT("Add"), ESearchCase::IgnoreCase))
		{
			Expression = NewObject<UMaterialExpressionAdd>(Material);
		}
		else if (NodeClass.Equals(TEXT("Multiply"), ESearchCase::IgnoreCase))
		{
			Expression = NewObject<UMaterialExpressionMultiply>(Material);
		}
		else
		{
			return FMcpToolResult::Error(FString::Printf(TEXT("Unknown material expression type: %s"), *NodeClass));
		}

		if (Expression)
		{
			Expression->MaterialExpressionEditorX = Position.X;
			Expression->MaterialExpressionEditorY = Position.Y;

			Material->GetExpressionCollection().AddExpression(Expression);
			Material->PreEditChange(nullptr);
			Material->PostEditChange();

			FMcpAssetModifier::MarkPackageDirty(Material);

			Result->SetBoolField(TEXT("success"), true);
			Result->SetStringField(TEXT("expression_name"), Expression->GetName());
			Result->SetBoolField(TEXT("needs_save"), true);

			UE_LOG(LogYesUeMcp, Log, TEXT("add-graph-node: Added material expression %s"), *Expression->GetName());
		}

		return FMcpToolResult::Json(Result);
	}

	// Handle Blueprint
	if (UBlueprint* Blueprint = Cast<UBlueprint>(Object))
	{
		FMcpAssetModifier::MarkModified(Blueprint);

		// Find the target graph
		UEdGraph* TargetGraph = nullptr;
		for (UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			if (Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
			{
				TargetGraph = Graph;
				break;
			}
		}

		if (!TargetGraph)
		{
			// Try function graphs
			for (UEdGraph* Graph : Blueprint->FunctionGraphs)
			{
				if (Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
				{
					TargetGraph = Graph;
					break;
				}
			}
		}

		if (!TargetGraph)
		{
			return FMcpToolResult::Error(FString::Printf(TEXT("Graph not found: %s"), *GraphName));
		}

		UEdGraphNode* NewNode = nullptr;

		if (NodeClass.Equals(TEXT("CallFunction"), ESearchCase::IgnoreCase))
		{
			if (FunctionName.IsEmpty())
			{
				return FMcpToolResult::Error(TEXT("function_name is required for CallFunction nodes"));
			}

			// Find the function
			UFunction* Function = FindObject<UFunction>(ANY_PACKAGE, *FunctionName);
			if (!Function)
			{
				// Try finding in common classes
				Function = AActor::StaticClass()->FindFunctionByName(*FunctionName);
				if (!Function)
				{
					Function = UObject::StaticClass()->FindFunctionByName(*FunctionName);
				}
			}

			if (!Function)
			{
				return FMcpToolResult::Error(FString::Printf(TEXT("Function not found: %s"), *FunctionName));
			}

			UK2Node_CallFunction* CallNode = NewObject<UK2Node_CallFunction>(TargetGraph);
			CallNode->SetFromFunction(Function);
			CallNode->NodePosX = Position.X;
			CallNode->NodePosY = Position.Y;
			CallNode->AllocateDefaultPins();
			TargetGraph->AddNode(CallNode, true, false);
			NewNode = CallNode;
		}
		else if (NodeClass.Equals(TEXT("VariableGet"), ESearchCase::IgnoreCase))
		{
			if (VariableName.IsEmpty())
			{
				return FMcpToolResult::Error(TEXT("variable_name is required for VariableGet nodes"));
			}

			UK2Node_VariableGet* GetNode = NewObject<UK2Node_VariableGet>(TargetGraph);
			GetNode->VariableReference.SetSelfMember(FName(*VariableName));
			GetNode->NodePosX = Position.X;
			GetNode->NodePosY = Position.Y;
			GetNode->AllocateDefaultPins();
			TargetGraph->AddNode(GetNode, true, false);
			NewNode = GetNode;
		}
		else if (NodeClass.Equals(TEXT("VariableSet"), ESearchCase::IgnoreCase))
		{
			if (VariableName.IsEmpty())
			{
				return FMcpToolResult::Error(TEXT("variable_name is required for VariableSet nodes"));
			}

			UK2Node_VariableSet* SetNode = NewObject<UK2Node_VariableSet>(TargetGraph);
			SetNode->VariableReference.SetSelfMember(FName(*VariableName));
			SetNode->NodePosX = Position.X;
			SetNode->NodePosY = Position.Y;
			SetNode->AllocateDefaultPins();
			TargetGraph->AddNode(SetNode, true, false);
			NewNode = SetNode;
		}
		else
		{
			return FMcpToolResult::Error(FString::Printf(TEXT("Unknown Blueprint node class: %s"), *NodeClass));
		}

		if (NewNode)
		{
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
			FMcpAssetModifier::MarkPackageDirty(Blueprint);

			Result->SetBoolField(TEXT("success"), true);
			Result->SetStringField(TEXT("node_guid"), NewNode->NodeGuid.ToString());
			Result->SetStringField(TEXT("graph"), GraphName);
			Result->SetBoolField(TEXT("needs_compile"), true);
			Result->SetBoolField(TEXT("needs_save"), true);

			UE_LOG(LogYesUeMcp, Log, TEXT("add-graph-node: Added Blueprint node %s"), *NewNode->NodeGuid.ToString());
		}

		return FMcpToolResult::Json(Result);
	}

	return FMcpToolResult::Error(TEXT("Asset must be a Blueprint or Material"));
}
