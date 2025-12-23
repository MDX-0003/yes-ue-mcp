// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/Blueprint/BlueprintFunctionsTool.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "EdGraphSchema_K2.h"

TMap<FString, FMcpSchemaProperty> UBlueprintFunctionsTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty AssetPath;
	AssetPath.Type = TEXT("string");
	AssetPath.Description = TEXT("Asset path to the Blueprint");
	AssetPath.bRequired = true;
	Schema.Add(TEXT("asset_path"), AssetPath);

	return Schema;
}

FMcpToolResult UBlueprintFunctionsTool::Execute(
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	FString AssetPath;
	if (!GetStringArg(Arguments, TEXT("asset_path"), AssetPath))
	{
		return FMcpToolResult::Error(TEXT("Missing required parameter: asset_path"));
	}

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
	if (!Blueprint)
	{
		return FMcpToolResult::Error(FString::Printf(TEXT("Failed to load Blueprint: %s"), *AssetPath));
	}

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	TArray<TSharedPtr<FJsonValue>> FunctionArray;

	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (!Graph)
		{
			continue;
		}

		TSharedPtr<FJsonObject> FuncObj = MakeShareable(new FJsonObject);
		FuncObj->SetStringField(TEXT("name"), Graph->GetName());
		FuncObj->SetStringField(TEXT("graph_name"), Graph->GetFName().ToString());

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
			for (UEdGraphPin* Pin : EntryNode->Pins)
			{
				if (Pin && Pin->Direction == EGPD_Output &&
					Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
				{
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

					InputParams.Add(MakeShareable(new FJsonValueObject(ParamObj)));
				}
			}
			FuncObj->SetArrayField(TEXT("inputs"), InputParams);
		}

		// Return values
		if (ResultNode)
		{
			TArray<TSharedPtr<FJsonValue>> OutputParams;
			for (UEdGraphPin* Pin : ResultNode->Pins)
			{
				if (Pin && Pin->Direction == EGPD_Input &&
					Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
				{
					TSharedPtr<FJsonObject> ParamObj = MakeShareable(new FJsonObject);
					ParamObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
					ParamObj->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());

					if (Pin->PinType.PinSubCategoryObject.IsValid())
					{
						ParamObj->SetStringField(TEXT("object_type"), Pin->PinType.PinSubCategoryObject->GetName());
					}

					OutputParams.Add(MakeShareable(new FJsonValueObject(ParamObj)));
				}
			}
			FuncObj->SetArrayField(TEXT("outputs"), OutputParams);
		}

		// Node count in graph
		FuncObj->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());

		FunctionArray.Add(MakeShareable(new FJsonValueObject(FuncObj)));
	}

	Result->SetStringField(TEXT("blueprint"), Blueprint->GetName());
	Result->SetArrayField(TEXT("functions"), FunctionArray);
	Result->SetNumberField(TEXT("count"), FunctionArray.Num());

	return FMcpToolResult::Json(Result);
}
