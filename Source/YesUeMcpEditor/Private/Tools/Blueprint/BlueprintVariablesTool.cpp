// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/Blueprint/BlueprintVariablesTool.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/BlueprintEditorUtils.h"

TMap<FString, FMcpSchemaProperty> UBlueprintVariablesTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty AssetPath;
	AssetPath.Type = TEXT("string");
	AssetPath.Description = TEXT("Asset path to the Blueprint");
	AssetPath.bRequired = true;
	Schema.Add(TEXT("asset_path"), AssetPath);

	return Schema;
}

FMcpToolResult UBlueprintVariablesTool::Execute(
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
	TArray<TSharedPtr<FJsonValue>> VarArray;

	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		TSharedPtr<FJsonObject> VarObj = MakeShareable(new FJsonObject);
		VarObj->SetStringField(TEXT("name"), Var.VarName.ToString());

		// Type information
		FString TypeStr = Var.VarType.PinCategory.ToString();
		VarObj->SetStringField(TEXT("type"), TypeStr);

		if (Var.VarType.PinSubCategoryObject.IsValid())
		{
			VarObj->SetStringField(TEXT("object_type"), Var.VarType.PinSubCategoryObject->GetName());
			VarObj->SetStringField(TEXT("object_path"), Var.VarType.PinSubCategoryObject->GetPathName());
		}

		// Container type
		if (Var.VarType.IsArray())
		{
			VarObj->SetStringField(TEXT("container"), TEXT("Array"));
		}
		else if (Var.VarType.IsSet())
		{
			VarObj->SetStringField(TEXT("container"), TEXT("Set"));
		}
		else if (Var.VarType.IsMap())
		{
			VarObj->SetStringField(TEXT("container"), TEXT("Map"));
			if (Var.VarType.PinValueType.TerminalCategory != NAME_None)
			{
				VarObj->SetStringField(TEXT("value_type"), Var.VarType.PinValueType.TerminalCategory.ToString());
			}
		}
		else
		{
			VarObj->SetStringField(TEXT("container"), TEXT("None"));
		}

		// Reference
		VarObj->SetBoolField(TEXT("is_reference"), Var.VarType.bIsReference);

		// Category
		if (!Var.Category.IsEmpty())
		{
			VarObj->SetStringField(TEXT("category"), Var.Category.ToString());
		}

		// Default value
		if (!Var.DefaultValue.IsEmpty())
		{
			VarObj->SetStringField(TEXT("default_value"), Var.DefaultValue);
		}

		// Tooltip - check HasMetaData first to avoid crash on invalid variables
		if (Var.HasMetaData(TEXT("Tooltip")))
		{
			FString Tooltip = Var.GetMetaData(TEXT("Tooltip"));
			if (!Tooltip.IsEmpty())
			{
				VarObj->SetStringField(TEXT("tooltip"), Tooltip);
			}
		}

		// Replication
		TSharedPtr<FJsonObject> ReplicationObj = MakeShareable(new FJsonObject);
		if (Var.PropertyFlags & CPF_Net)
		{
			ReplicationObj->SetBoolField(TEXT("replicated"), true);

			if (Var.ReplicationCondition != COND_None)
			{
				FString ConditionStr;
				switch (Var.ReplicationCondition)
				{
				case COND_InitialOnly: ConditionStr = TEXT("InitialOnly"); break;
				case COND_OwnerOnly: ConditionStr = TEXT("OwnerOnly"); break;
				case COND_SkipOwner: ConditionStr = TEXT("SkipOwner"); break;
				case COND_SimulatedOnly: ConditionStr = TEXT("SimulatedOnly"); break;
				case COND_AutonomousOnly: ConditionStr = TEXT("AutonomousOnly"); break;
				case COND_SimulatedOrPhysics: ConditionStr = TEXT("SimulatedOrPhysics"); break;
				case COND_InitialOrOwner: ConditionStr = TEXT("InitialOrOwner"); break;
				case COND_Custom: ConditionStr = TEXT("Custom"); break;
				default: ConditionStr = TEXT("None"); break;
				}
				ReplicationObj->SetStringField(TEXT("condition"), ConditionStr);
			}

			// Note: MD_RepNotifyFunc was removed in UE 5.7
			// RepNotify function name can be retrieved from RepNotifyFunc property if needed
			if (Var.RepNotifyFunc != NAME_None)
			{
				ReplicationObj->SetStringField(TEXT("notify_func"), Var.RepNotifyFunc.ToString());
			}
		}
		else
		{
			ReplicationObj->SetBoolField(TEXT("replicated"), false);
		}
		VarObj->SetObjectField(TEXT("replication"), ReplicationObj);

		// Property flags
		TArray<TSharedPtr<FJsonValue>> FlagsArray;
		if (Var.PropertyFlags & CPF_Edit)
		{
			FlagsArray.Add(MakeShareable(new FJsonValueString(TEXT("EditAnywhere"))));
		}
		if (Var.PropertyFlags & CPF_DisableEditOnInstance)
		{
			FlagsArray.Add(MakeShareable(new FJsonValueString(TEXT("EditDefaultsOnly"))));
		}
		if (Var.PropertyFlags & CPF_DisableEditOnTemplate)
		{
			FlagsArray.Add(MakeShareable(new FJsonValueString(TEXT("EditInstanceOnly"))));
		}
		if (Var.PropertyFlags & CPF_BlueprintVisible)
		{
			if (Var.PropertyFlags & CPF_BlueprintReadOnly)
			{
				FlagsArray.Add(MakeShareable(new FJsonValueString(TEXT("BlueprintReadOnly"))));
			}
			else
			{
				FlagsArray.Add(MakeShareable(new FJsonValueString(TEXT("BlueprintReadWrite"))));
			}
		}
		if (Var.PropertyFlags & CPF_ExposeOnSpawn)
		{
			FlagsArray.Add(MakeShareable(new FJsonValueString(TEXT("ExposeOnSpawn"))));
		}
		if (Var.PropertyFlags & CPF_SaveGame)
		{
			FlagsArray.Add(MakeShareable(new FJsonValueString(TEXT("SaveGame"))));
		}
		if (Var.PropertyFlags & CPF_Transient)
		{
			FlagsArray.Add(MakeShareable(new FJsonValueString(TEXT("Transient"))));
		}
		VarObj->SetArrayField(TEXT("flags"), FlagsArray);

		VarArray.Add(MakeShareable(new FJsonValueObject(VarObj)));
	}

	Result->SetStringField(TEXT("blueprint"), Blueprint->GetName());
	Result->SetArrayField(TEXT("variables"), VarArray);
	Result->SetNumberField(TEXT("count"), VarArray.Num());

	return FMcpToolResult::Json(Result);
}
