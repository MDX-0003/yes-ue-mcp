// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/Blueprint/BlueprintCallableListTool.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_Tunnel.h"
#include "EdGraphSchema_K2.h"
#include "Tools/McpToolResult.h"

FString UBlueprintCallableListTool::GetToolDescription() const
{
	return TEXT("List all callable entry points (events, functions, macros) in a Blueprint with lightweight metadata. "
		"Returns names, signatures, and basic info without full graph details.");
}

TMap<FString, FMcpSchemaProperty> UBlueprintCallableListTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty AssetPath;
	AssetPath.Type = TEXT("string");
	AssetPath.Description = TEXT("Asset path to the Blueprint (e.g., /Game/Blueprints/BP_Character)");
	AssetPath.bRequired = true;
	Schema.Add(TEXT("asset_path"), AssetPath);

	return Schema;
}

TArray<FString> UBlueprintCallableListTool::GetRequiredParams() const
{
	return { TEXT("asset_path") };
}

FMcpToolResult UBlueprintCallableListTool::Execute(
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	// Get asset path
	FString AssetPath;
	if (!GetStringArg(Arguments, TEXT("asset_path"), AssetPath))
	{
		return FMcpToolResult::Error(TEXT("Missing required parameter: asset_path"));
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

	TArray<TSharedPtr<FJsonValue>> EventsArray;
	TArray<TSharedPtr<FJsonValue>> FunctionsArray;
	TArray<TSharedPtr<FJsonValue>> MacrosArray;

	// Extract events, functions, and macros
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

void UBlueprintCallableListTool::ExtractEvents(UBlueprint* Blueprint, TArray<TSharedPtr<FJsonValue>>& OutEventsArray) const
{
	if (!Blueprint)
	{
		return;
	}

	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (!Graph)
		{
			continue;
		}

		// Find all event nodes in this graph
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;

			UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
			if (!EventNode)
			{
				continue;
			}

			TSharedPtr<FJsonObject> EventObj = MakeShareable(new FJsonObject);

			// Event name (function name it overrides)
			EventObj->SetStringField(TEXT("name"), EventNode->EventReference.GetMemberName().ToString());

			// Display name (what shows in the graph)
			EventObj->SetStringField(TEXT("display_name"), EventNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString());

			// Node GUID for reference
			EventObj->SetStringField(TEXT("node_guid"), EventNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));

			// Graph name (usually EventGraph)
			EventObj->SetStringField(TEXT("graph_name"), Graph->GetName());

			// Node count in the graph
			EventObj->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());

			OutEventsArray.Add(MakeShareable(new FJsonValueObject(EventObj)));
		}
	}
}

void UBlueprintCallableListTool::ExtractFunctions(UBlueprint* Blueprint, TArray<TSharedPtr<FJsonValue>>& OutFunctionsArray) const
{
	if (!Blueprint)
	{
		return;
	}

	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (!Graph)
		{
			continue;
		}

		TSharedPtr<FJsonObject> FuncObj = MakeShareable(new FJsonObject);
		FuncObj->SetStringField(TEXT("name"), Graph->GetName());

		UK2Node_FunctionEntry* EntryNode = nullptr;
		UK2Node_FunctionResult* ResultNode = nullptr;

		// Find entry and result nodes
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;

			if (!EntryNode)
			{
				EntryNode = Cast<UK2Node_FunctionEntry>(Node);
			}
			if (!ResultNode)
			{
				ResultNode = Cast<UK2Node_FunctionResult>(Node);
			}
		}

		if (EntryNode)
		{
			// Access specifier
			uint32 FunctionFlags = EntryNode->GetFunctionFlags();
			if (FunctionFlags & FUNC_Public)
			{
				FuncObj->SetStringField(TEXT("access"), TEXT("Public"));
			}
			else if (FunctionFlags & FUNC_Protected)
			{
				FuncObj->SetStringField(TEXT("access"), TEXT("Protected"));
			}
			else
			{
				FuncObj->SetStringField(TEXT("access"), TEXT("Private"));
			}

			// Function type flags
			FuncObj->SetBoolField(TEXT("is_pure"), (FunctionFlags & FUNC_BlueprintPure) != 0);
			FuncObj->SetBoolField(TEXT("is_static"), (FunctionFlags & FUNC_Static) != 0);
			FuncObj->SetBoolField(TEXT("is_const"), (FunctionFlags & FUNC_Const) != 0);

			// Input parameters
			TArray<TSharedPtr<FJsonValue>> InputParams;
			ExtractParameterInfo(EntryNode->Pins, EGPD_Output, InputParams);
			FuncObj->SetArrayField(TEXT("inputs"), InputParams);
		}

		// Return values
		if (ResultNode)
		{
			TArray<TSharedPtr<FJsonValue>> OutputParams;
			ExtractParameterInfo(ResultNode->Pins, EGPD_Input, OutputParams);
			FuncObj->SetArrayField(TEXT("outputs"), OutputParams);
		}

		// Node count in graph
		FuncObj->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());

		OutFunctionsArray.Add(MakeShareable(new FJsonValueObject(FuncObj)));
	}
}

void UBlueprintCallableListTool::ExtractMacros(UBlueprint* Blueprint, TArray<TSharedPtr<FJsonValue>>& OutMacrosArray) const
{
	if (!Blueprint)
	{
		return;
	}

	for (UEdGraph* Graph : Blueprint->MacroGraphs)
	{
		if (!Graph)
		{
			continue;
		}

		TSharedPtr<FJsonObject> MacroObj = MakeShareable(new FJsonObject);
		MacroObj->SetStringField(TEXT("name"), Graph->GetName());

		UK2Node_Tunnel* EntryNode = nullptr;
		UK2Node_Tunnel* ResultNode = nullptr;

		// Find tunnel entry and exit nodes (macros use tunnels)
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;

			UK2Node_Tunnel* TunnelNode = Cast<UK2Node_Tunnel>(Node);
			if (TunnelNode)
			{
				// Entry nodes have output pins (they provide data to the macro)
				// Exit nodes have input pins (they return data from the macro)
				if (TunnelNode->bCanHaveOutputs && !EntryNode)
				{
					EntryNode = TunnelNode;
				}
				else if (TunnelNode->bCanHaveInputs && !ResultNode)
				{
					ResultNode = TunnelNode;
				}
			}
		}

		if (EntryNode)
		{
			// Input parameters
			TArray<TSharedPtr<FJsonValue>> InputParams;
			ExtractParameterInfo(EntryNode->Pins, EGPD_Output, InputParams);
			MacroObj->SetArrayField(TEXT("inputs"), InputParams);
		}

		if (ResultNode)
		{
			// Output parameters
			TArray<TSharedPtr<FJsonValue>> OutputParams;
			ExtractParameterInfo(ResultNode->Pins, EGPD_Input, OutputParams);
			MacroObj->SetArrayField(TEXT("outputs"), OutputParams);
		}

		// Node count in graph
		MacroObj->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());

		OutMacrosArray.Add(MakeShareable(new FJsonValueObject(MacroObj)));
	}
}

void UBlueprintCallableListTool::ExtractParameterInfo(const TArray<UEdGraphPin*>& Pins, EEdGraphPinDirection Direction, TArray<TSharedPtr<FJsonValue>>& OutParams) const
{
	for (UEdGraphPin* Pin : Pins)
	{
		if (!Pin || Pin->Direction != Direction)
		{
			continue;
		}

		// Skip exec pins
		if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
		{
			continue;
		}

		TSharedPtr<FJsonObject> ParamObj = MakeShareable(new FJsonObject);
		ParamObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
		ParamObj->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());

		if (Pin->PinType.PinSubCategoryObject.IsValid())
		{
			ParamObj->SetStringField(TEXT("object_type"), Pin->PinType.PinSubCategoryObject->GetName());
		}

		ParamObj->SetBoolField(TEXT("is_reference"), Pin->PinType.bIsReference);
		ParamObj->SetBoolField(TEXT("is_array"), Pin->PinType.IsArray());

		if (!Pin->DefaultValue.IsEmpty())
		{
			ParamObj->SetStringField(TEXT("default_value"), Pin->DefaultValue);
		}

		OutParams.Add(MakeShareable(new FJsonValueObject(ParamObj)));
	}
}
