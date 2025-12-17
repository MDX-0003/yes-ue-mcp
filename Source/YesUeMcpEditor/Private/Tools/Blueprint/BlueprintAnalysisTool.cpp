// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/Blueprint/BlueprintAnalysisTool.h"
#include "YesUeMcpEditor.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "EdGraph/EdGraph.h"
#include "K2Node.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_CustomEvent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"

TMap<FString, FMcpSchemaProperty> UBlueprintAnalysisTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty AssetPath;
	AssetPath.Type = TEXT("string");
	AssetPath.Description = TEXT("Asset path to the Blueprint (e.g., /Game/Blueprints/BP_Character)");
	AssetPath.bRequired = true;
	Schema.Add(TEXT("asset_path"), AssetPath);

	FMcpSchemaProperty IncludeGraph;
	IncludeGraph.Type = TEXT("boolean");
	IncludeGraph.Description = TEXT("Whether to include event graph node information (default: true)");
	IncludeGraph.bRequired = false;
	Schema.Add(TEXT("include_graph"), IncludeGraph);

	return Schema;
}

FMcpToolResult UBlueprintAnalysisTool::Execute(
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	FString AssetPath;
	if (!GetStringArg(Arguments, TEXT("asset_path"), AssetPath))
	{
		return FMcpToolResult::Error(TEXT("Missing required parameter: asset_path"));
	}

	bool bIncludeGraph = GetBoolArgOrDefault(Arguments, TEXT("include_graph"), true);

	return AnalyzeBlueprintInternal(AssetPath, bIncludeGraph);
}

FMcpToolResult UBlueprintAnalysisTool::AnalyzeBlueprintInternal(const FString& AssetPath, bool bIncludeGraph)
{
	// Load the blueprint
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
	if (!Blueprint)
	{
		return FMcpToolResult::Error(FString::Printf(TEXT("Failed to load Blueprint: %s"), *AssetPath));
	}

	// Build result JSON
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetStringField(TEXT("name"), Blueprint->GetName());
	Result->SetStringField(TEXT("path"), AssetPath);

	// Parent class info
	if (Blueprint->ParentClass)
	{
		Result->SetStringField(TEXT("parent_class"), Blueprint->ParentClass->GetName());
		Result->SetStringField(TEXT("parent_class_path"), Blueprint->ParentClass->GetPathName());
	}
	else
	{
		Result->SetStringField(TEXT("parent_class"), TEXT("None"));
	}

	// Blueprint type
	FString BlueprintTypeStr;
	switch (Blueprint->BlueprintType)
	{
	case BPTYPE_Normal:
		BlueprintTypeStr = TEXT("Normal");
		break;
	case BPTYPE_Const:
		BlueprintTypeStr = TEXT("Const");
		break;
	case BPTYPE_MacroLibrary:
		BlueprintTypeStr = TEXT("MacroLibrary");
		break;
	case BPTYPE_Interface:
		BlueprintTypeStr = TEXT("Interface");
		break;
	case BPTYPE_LevelScript:
		BlueprintTypeStr = TEXT("LevelScript");
		break;
	case BPTYPE_FunctionLibrary:
		BlueprintTypeStr = TEXT("FunctionLibrary");
		break;
	default:
		BlueprintTypeStr = TEXT("Unknown");
	}
	Result->SetStringField(TEXT("blueprint_type"), BlueprintTypeStr);

	// Add detailed sections
	Result->SetObjectField(TEXT("functions"), ExtractFunctions(Blueprint));
	Result->SetObjectField(TEXT("variables"), ExtractVariables(Blueprint));
	Result->SetObjectField(TEXT("components"), ExtractComponents(Blueprint));

	if (bIncludeGraph)
	{
		Result->SetObjectField(TEXT("event_graph"), ExtractEventGraphSummary(Blueprint));
	}

	return FMcpToolResult::Json(Result);
}

TSharedPtr<FJsonObject> UBlueprintAnalysisTool::ExtractFunctions(UBlueprint* Blueprint)
{
	TSharedPtr<FJsonObject> FunctionsObj = MakeShareable(new FJsonObject);
	TArray<TSharedPtr<FJsonValue>> FunctionArray;

	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (!Graph)
		{
			continue;
		}

		TSharedPtr<FJsonObject> FuncObj = MakeShareable(new FJsonObject);
		FuncObj->SetStringField(TEXT("name"), Graph->GetName());

		// Find function entry node for parameters
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node))
			{
				// Get function flags
				TArray<TSharedPtr<FJsonValue>> FlagsArray;
				if (EntryNode->GetFunctionFlags() & FUNC_Public)
				{
					FlagsArray.Add(MakeShareable(new FJsonValueString(TEXT("Public"))));
				}
				if (EntryNode->GetFunctionFlags() & FUNC_Protected)
				{
					FlagsArray.Add(MakeShareable(new FJsonValueString(TEXT("Protected"))));
				}
				if (EntryNode->GetFunctionFlags() & FUNC_Private)
				{
					FlagsArray.Add(MakeShareable(new FJsonValueString(TEXT("Private"))));
				}
				if (EntryNode->GetFunctionFlags() & FUNC_Static)
				{
					FlagsArray.Add(MakeShareable(new FJsonValueString(TEXT("Static"))));
				}
				if (EntryNode->GetFunctionFlags() & FUNC_BlueprintPure)
				{
					FlagsArray.Add(MakeShareable(new FJsonValueString(TEXT("Pure"))));
				}
				if (EntryNode->GetFunctionFlags() & FUNC_BlueprintCallable)
				{
					FlagsArray.Add(MakeShareable(new FJsonValueString(TEXT("BlueprintCallable"))));
				}
				FuncObj->SetArrayField(TEXT("flags"), FlagsArray);

				// Get parameters from pins
				TArray<TSharedPtr<FJsonValue>> ParamsArray;
				for (UEdGraphPin* Pin : EntryNode->Pins)
				{
					if (Pin && Pin->Direction == EGPD_Output && !Pin->PinName.IsEqual(UEdGraphSchema_K2::PN_Then))
					{
						TSharedPtr<FJsonObject> ParamObj = MakeShareable(new FJsonObject);
						ParamObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
						ParamObj->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
						if (Pin->PinType.PinSubCategoryObject.IsValid())
						{
							ParamObj->SetStringField(TEXT("sub_type"), Pin->PinType.PinSubCategoryObject->GetName());
						}
						ParamsArray.Add(MakeShareable(new FJsonValueObject(ParamObj)));
					}
				}
				FuncObj->SetArrayField(TEXT("parameters"), ParamsArray);
				break;
			}
		}

		FunctionArray.Add(MakeShareable(new FJsonValueObject(FuncObj)));
	}

	FunctionsObj->SetArrayField(TEXT("items"), FunctionArray);
	FunctionsObj->SetNumberField(TEXT("count"), FunctionArray.Num());

	return FunctionsObj;
}

TSharedPtr<FJsonObject> UBlueprintAnalysisTool::ExtractVariables(UBlueprint* Blueprint)
{
	TSharedPtr<FJsonObject> VariablesObj = MakeShareable(new FJsonObject);
	TArray<TSharedPtr<FJsonValue>> VarArray;

	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		TSharedPtr<FJsonObject> VarObj = MakeShareable(new FJsonObject);
		VarObj->SetStringField(TEXT("name"), Var.VarName.ToString());
		VarObj->SetStringField(TEXT("type"), Var.VarType.PinCategory.ToString());

		if (Var.VarType.PinSubCategoryObject.IsValid())
		{
			VarObj->SetStringField(TEXT("sub_type"), Var.VarType.PinSubCategoryObject->GetName());
		}

		VarObj->SetBoolField(TEXT("is_array"), Var.VarType.IsArray());
		VarObj->SetBoolField(TEXT("is_set"), Var.VarType.IsSet());
		VarObj->SetBoolField(TEXT("is_map"), Var.VarType.IsMap());

		// Replication
		if (Var.HasMetaData(FBlueprintMetadata::MD_RepNotifyFunc))
		{
			VarObj->SetStringField(TEXT("replication"), TEXT("ReplicatedUsing"));
			VarObj->SetStringField(TEXT("rep_notify_func"), Var.GetMetaData(FBlueprintMetadata::MD_RepNotifyFunc));
		}
		else if (Var.PropertyFlags & CPF_Net)
		{
			VarObj->SetStringField(TEXT("replication"), TEXT("Replicated"));
		}
		else
		{
			VarObj->SetStringField(TEXT("replication"), TEXT("None"));
		}

		// Category
		if (!Var.Category.IsEmpty())
		{
			VarObj->SetStringField(TEXT("category"), Var.Category.ToString());
		}

		// Flags
		TArray<TSharedPtr<FJsonValue>> FlagsArray;
		if (Var.PropertyFlags & CPF_Edit)
		{
			FlagsArray.Add(MakeShareable(new FJsonValueString(TEXT("EditAnywhere"))));
		}
		if (Var.PropertyFlags & CPF_BlueprintVisible)
		{
			FlagsArray.Add(MakeShareable(new FJsonValueString(TEXT("BlueprintReadWrite"))));
		}
		if (Var.PropertyFlags & CPF_ExposeOnSpawn)
		{
			FlagsArray.Add(MakeShareable(new FJsonValueString(TEXT("ExposeOnSpawn"))));
		}
		VarObj->SetArrayField(TEXT("flags"), FlagsArray);

		VarArray.Add(MakeShareable(new FJsonValueObject(VarObj)));
	}

	VariablesObj->SetArrayField(TEXT("items"), VarArray);
	VariablesObj->SetNumberField(TEXT("count"), VarArray.Num());

	return VariablesObj;
}

TSharedPtr<FJsonObject> UBlueprintAnalysisTool::ExtractComponents(UBlueprint* Blueprint)
{
	TSharedPtr<FJsonObject> ComponentsObj = MakeShareable(new FJsonObject);
	TArray<TSharedPtr<FJsonValue>> CompArray;

	if (Blueprint->SimpleConstructionScript)
	{
		TArray<USCS_Node*> AllNodes = Blueprint->SimpleConstructionScript->GetAllNodes();

		for (USCS_Node* Node : AllNodes)
		{
			if (!Node || !Node->ComponentTemplate)
			{
				continue;
			}

			TSharedPtr<FJsonObject> CompObj = MakeShareable(new FJsonObject);
			CompObj->SetStringField(TEXT("name"), Node->GetVariableName().ToString());
			CompObj->SetStringField(TEXT("class"), Node->ComponentClass ? Node->ComponentClass->GetName() : TEXT("Unknown"));

			// Parent component
			if (Node->ParentComponentOrVariableName != NAME_None)
			{
				CompObj->SetStringField(TEXT("parent"), Node->ParentComponentOrVariableName.ToString());
			}

			// Is root
			if (Blueprint->SimpleConstructionScript->GetRootNodes().Contains(Node))
			{
				CompObj->SetBoolField(TEXT("is_root"), true);
			}

			CompArray.Add(MakeShareable(new FJsonValueObject(CompObj)));
		}
	}

	ComponentsObj->SetArrayField(TEXT("items"), CompArray);
	ComponentsObj->SetNumberField(TEXT("count"), CompArray.Num());

	return ComponentsObj;
}

TSharedPtr<FJsonObject> UBlueprintAnalysisTool::ExtractEventGraphSummary(UBlueprint* Blueprint)
{
	TSharedPtr<FJsonObject> GraphObj = MakeShareable(new FJsonObject);
	TArray<TSharedPtr<FJsonValue>> EventsArray;
	TArray<TSharedPtr<FJsonValue>> CustomEventsArray;

	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (!Graph)
		{
			continue;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			// Standard events (BeginPlay, Tick, etc.)
			if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
			{
				if (!Cast<UK2Node_CustomEvent>(EventNode))
				{
					TSharedPtr<FJsonObject> EventObj = MakeShareable(new FJsonObject);
					EventObj->SetStringField(TEXT("name"), EventNode->GetFunctionName().ToString());
					EventObj->SetStringField(TEXT("node_title"), EventNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
					EventsArray.Add(MakeShareable(new FJsonValueObject(EventObj)));
				}
			}

			// Custom events
			if (UK2Node_CustomEvent* CustomEventNode = Cast<UK2Node_CustomEvent>(Node))
			{
				TSharedPtr<FJsonObject> EventObj = MakeShareable(new FJsonObject);
				EventObj->SetStringField(TEXT("name"), CustomEventNode->CustomFunctionName.ToString());

				// Get parameters
				TArray<TSharedPtr<FJsonValue>> ParamsArray;
				for (UEdGraphPin* Pin : CustomEventNode->Pins)
				{
					if (Pin && Pin->Direction == EGPD_Output &&
						!Pin->PinName.IsEqual(UEdGraphSchema_K2::PN_Then) &&
						Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
					{
						TSharedPtr<FJsonObject> ParamObj = MakeShareable(new FJsonObject);
						ParamObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
						ParamObj->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
						ParamsArray.Add(MakeShareable(new FJsonValueObject(ParamObj)));
					}
				}
				EventObj->SetArrayField(TEXT("parameters"), ParamsArray);

				CustomEventsArray.Add(MakeShareable(new FJsonValueObject(EventObj)));
			}
		}
	}

	GraphObj->SetArrayField(TEXT("events"), EventsArray);
	GraphObj->SetArrayField(TEXT("custom_events"), CustomEventsArray);
	GraphObj->SetNumberField(TEXT("event_count"), EventsArray.Num());
	GraphObj->SetNumberField(TEXT("custom_event_count"), CustomEventsArray.Num());

	return GraphObj;
}
