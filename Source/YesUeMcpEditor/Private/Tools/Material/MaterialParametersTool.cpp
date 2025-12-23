// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/Material/MaterialParametersTool.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionStaticBoolParameter.h"
#include "Tools/McpToolResult.h"

FString UMaterialParametersTool::GetToolDescription() const
{
	return TEXT("Get material parameters from Materials and MaterialInstances. "
		"Returns scalar, vector, texture, and static switch parameters with their values and defaults.");
}

TMap<FString, FMcpSchemaProperty> UMaterialParametersTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty AssetPath;
	AssetPath.Type = TEXT("string");
	AssetPath.Description = TEXT("Asset path to the Material or MaterialInstance (e.g., /Game/Materials/M_Character)");
	AssetPath.bRequired = true;
	Schema.Add(TEXT("asset_path"), AssetPath);

	FMcpSchemaProperty IncludeDefaults;
	IncludeDefaults.Type = TEXT("boolean");
	IncludeDefaults.Description = TEXT("Include default values from parent material (default: true)");
	IncludeDefaults.bRequired = false;
	Schema.Add(TEXT("include_defaults"), IncludeDefaults);

	FMcpSchemaProperty ParameterFilter;
	ParameterFilter.Type = TEXT("string");
	ParameterFilter.Description = TEXT("Filter parameters by name (wildcards supported, e.g., '*Color*')");
	ParameterFilter.bRequired = false;
	Schema.Add(TEXT("parameter_filter"), ParameterFilter);

	return Schema;
}

TArray<FString> UMaterialParametersTool::GetRequiredParams() const
{
	return { TEXT("asset_path") };
}

FMcpToolResult UMaterialParametersTool::Execute(
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	// Get asset path
	FString AssetPath;
	if (!GetStringArg(Arguments, TEXT("asset_path"), AssetPath))
	{
		return FMcpToolResult::Error(TEXT("Missing required parameter: asset_path"));
	}

	// Optional parameters
	bool bIncludeDefaults = GetBoolArgOrDefault(Arguments, TEXT("include_defaults"), true);
	FString ParameterFilter = GetStringArgOrDefault(Arguments, TEXT("parameter_filter"), TEXT(""));

	// Build result
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetStringField(TEXT("asset_path"), AssetPath);

	TArray<TSharedPtr<FJsonValue>> ScalarParams;
	TArray<TSharedPtr<FJsonValue>> VectorParams;
	TArray<TSharedPtr<FJsonValue>> TextureParams;
	TArray<TSharedPtr<FJsonValue>> SwitchParams;

	// Try to load as MaterialInstance first (more common case)
	UMaterialInstance* MatInstance = LoadObject<UMaterialInstance>(nullptr, *AssetPath);
	if (MatInstance)
	{
		Result->SetStringField(TEXT("asset_type"), TEXT("MaterialInstance"));

		// Get parent material
		if (MatInstance->Parent)
		{
			Result->SetStringField(TEXT("parent_material"), MatInstance->Parent->GetPathName());
		}

		GetMaterialInstanceParameters(MatInstance, ScalarParams, VectorParams, TextureParams, SwitchParams, bIncludeDefaults, ParameterFilter);
	}
	else
	{
		// Try to load as base Material
		UMaterial* Material = LoadObject<UMaterial>(nullptr, *AssetPath);
		if (!Material)
		{
			return FMcpToolResult::Error(FString::Printf(
				TEXT("Failed to load Material or MaterialInstance at path: %s"), *AssetPath));
		}

		Result->SetStringField(TEXT("asset_type"), TEXT("Material"));
		GetMaterialParameters(Material, ScalarParams, VectorParams, TextureParams, SwitchParams, ParameterFilter);
	}

	// Set parameter arrays
	Result->SetArrayField(TEXT("scalar_parameters"), ScalarParams);
	Result->SetArrayField(TEXT("vector_parameters"), VectorParams);
	Result->SetArrayField(TEXT("texture_parameters"), TextureParams);
	Result->SetArrayField(TEXT("static_switch_parameters"), SwitchParams);

	// Statistics
	TSharedPtr<FJsonObject> Stats = MakeShareable(new FJsonObject);
	Stats->SetNumberField(TEXT("total_parameters"), ScalarParams.Num() + VectorParams.Num() + TextureParams.Num() + SwitchParams.Num());
	Stats->SetNumberField(TEXT("scalar_count"), ScalarParams.Num());
	Stats->SetNumberField(TEXT("vector_count"), VectorParams.Num());
	Stats->SetNumberField(TEXT("texture_count"), TextureParams.Num());
	Stats->SetNumberField(TEXT("switch_count"), SwitchParams.Num());
	Result->SetObjectField(TEXT("statistics"), Stats);

	return FMcpToolResult::Json(Result);
}

void UMaterialParametersTool::GetMaterialParameters(UMaterial* Material,
	TArray<TSharedPtr<FJsonValue>>& OutScalarParams,
	TArray<TSharedPtr<FJsonValue>>& OutVectorParams,
	TArray<TSharedPtr<FJsonValue>>& OutTextureParams,
	TArray<TSharedPtr<FJsonValue>>& OutSwitchParams,
	const FString& NameFilter) const
{
	if (!Material)
	{
		return;
	}

	// Get all expressions from the material - GetExpressions() returns TArrayView in 5.7+
	for (UMaterialExpression* Expression : Material->GetExpressions())
	{
		if (!Expression)
		{
			continue;
		}

		// Scalar parameters
		if (UMaterialExpressionScalarParameter* ScalarParam = Cast<UMaterialExpressionScalarParameter>(Expression))
		{
			FString ParamName = ScalarParam->ParameterName.ToString();
			if (!MatchesFilter(ParamName, NameFilter))
			{
				continue;
			}

			TSharedPtr<FJsonObject> ParamJson = MakeShareable(new FJsonObject);
			ParamJson->SetStringField(TEXT("name"), ParamName);
			ParamJson->SetNumberField(TEXT("default_value"), ScalarParam->DefaultValue);
			ParamJson->SetStringField(TEXT("group"), ScalarParam->Group.ToString());

			if (ScalarParam->bUseCustomPrimitiveData)
			{
				ParamJson->SetNumberField(TEXT("primitive_data_index"), ScalarParam->PrimitiveDataIndex);
			}

			OutScalarParams.Add(MakeShareable(new FJsonValueObject(ParamJson)));
		}
		// Vector parameters
		else if (UMaterialExpressionVectorParameter* VectorParam = Cast<UMaterialExpressionVectorParameter>(Expression))
		{
			FString ParamName = VectorParam->ParameterName.ToString();
			if (!MatchesFilter(ParamName, NameFilter))
			{
				continue;
			}

			TSharedPtr<FJsonObject> ParamJson = MakeShareable(new FJsonObject);
			ParamJson->SetStringField(TEXT("name"), ParamName);
			ParamJson->SetStringField(TEXT("group"), VectorParam->Group.ToString());

			TSharedPtr<FJsonObject> DefaultValue = MakeShareable(new FJsonObject);
			DefaultValue->SetNumberField(TEXT("r"), VectorParam->DefaultValue.R);
			DefaultValue->SetNumberField(TEXT("g"), VectorParam->DefaultValue.G);
			DefaultValue->SetNumberField(TEXT("b"), VectorParam->DefaultValue.B);
			DefaultValue->SetNumberField(TEXT("a"), VectorParam->DefaultValue.A);
			ParamJson->SetObjectField(TEXT("default_value"), DefaultValue);

			OutVectorParams.Add(MakeShareable(new FJsonValueObject(ParamJson)));
		}
		// Texture parameters
		else if (UMaterialExpressionTextureSampleParameter* TexParam = Cast<UMaterialExpressionTextureSampleParameter>(Expression))
		{
			FString ParamName = TexParam->ParameterName.ToString();
			if (!MatchesFilter(ParamName, NameFilter))
			{
				continue;
			}

			TSharedPtr<FJsonObject> ParamJson = MakeShareable(new FJsonObject);
			ParamJson->SetStringField(TEXT("name"), ParamName);
			ParamJson->SetStringField(TEXT("group"), TexParam->Group.ToString());

			if (TexParam->Texture)
			{
				ParamJson->SetStringField(TEXT("default_texture"), TexParam->Texture->GetPathName());
			}
			else
			{
				ParamJson->SetField(TEXT("default_texture"), MakeShareable(new FJsonValueNull()));
			}

			OutTextureParams.Add(MakeShareable(new FJsonValueObject(ParamJson)));
		}
		// Static switch parameters
		else if (UMaterialExpressionStaticSwitchParameter* SwitchParam = Cast<UMaterialExpressionStaticSwitchParameter>(Expression))
		{
			FString ParamName = SwitchParam->ParameterName.ToString();
			if (!MatchesFilter(ParamName, NameFilter))
			{
				continue;
			}

			TSharedPtr<FJsonObject> ParamJson = MakeShareable(new FJsonObject);
			ParamJson->SetStringField(TEXT("name"), ParamName);
			ParamJson->SetBoolField(TEXT("default_value"), SwitchParam->DefaultValue);
			ParamJson->SetStringField(TEXT("group"), SwitchParam->Group.ToString());

			OutSwitchParams.Add(MakeShareable(new FJsonValueObject(ParamJson)));
		}
		// Static bool parameters
		else if (UMaterialExpressionStaticBoolParameter* BoolParam = Cast<UMaterialExpressionStaticBoolParameter>(Expression))
		{
			FString ParamName = BoolParam->ParameterName.ToString();
			if (!MatchesFilter(ParamName, NameFilter))
			{
				continue;
			}

			TSharedPtr<FJsonObject> ParamJson = MakeShareable(new FJsonObject);
			ParamJson->SetStringField(TEXT("name"), ParamName);
			ParamJson->SetBoolField(TEXT("default_value"), BoolParam->DefaultValue);
			ParamJson->SetStringField(TEXT("group"), BoolParam->Group.ToString());

			OutSwitchParams.Add(MakeShareable(new FJsonValueObject(ParamJson)));
		}
	}
}

void UMaterialParametersTool::GetMaterialInstanceParameters(UMaterialInstance* MatInstance,
	TArray<TSharedPtr<FJsonValue>>& OutScalarParams,
	TArray<TSharedPtr<FJsonValue>>& OutVectorParams,
	TArray<TSharedPtr<FJsonValue>>& OutTextureParams,
	TArray<TSharedPtr<FJsonValue>>& OutSwitchParams,
	bool bIncludeDefaults,
	const FString& NameFilter) const
{
	if (!MatInstance)
	{
		return;
	}

	// Scalar parameters
	TArray<FMaterialParameterInfo> ScalarParamInfo;
	TArray<FGuid> ScalarParamGuids;
	MatInstance->GetAllScalarParameterInfo(ScalarParamInfo, ScalarParamGuids);

	for (const FMaterialParameterInfo& Info : ScalarParamInfo)
	{
		FString ParamName = Info.Name.ToString();
		if (!MatchesFilter(ParamName, NameFilter))
		{
			continue;
		}

		float Value = 0.0f;
		float DefaultValue = 0.0f;
		bool bHasValue = MatInstance->GetScalarParameterValue(Info, Value);

		// Get default from parent
		if (bIncludeDefaults && MatInstance->Parent)
		{
			MatInstance->Parent->GetScalarParameterValue(Info, DefaultValue);
		}

		TSharedPtr<FJsonObject> ParamJson = MakeShareable(new FJsonObject);
		ParamJson->SetStringField(TEXT("name"), ParamName);
		ParamJson->SetNumberField(TEXT("value"), Value);
		if (bIncludeDefaults)
		{
			ParamJson->SetNumberField(TEXT("default_value"), DefaultValue);
			ParamJson->SetBoolField(TEXT("is_overridden"), FMath::Abs(Value - DefaultValue) > KINDA_SMALL_NUMBER);
		}

		OutScalarParams.Add(MakeShareable(new FJsonValueObject(ParamJson)));
	}

	// Vector parameters
	TArray<FMaterialParameterInfo> VectorParamInfo;
	TArray<FGuid> VectorParamGuids;
	MatInstance->GetAllVectorParameterInfo(VectorParamInfo, VectorParamGuids);

	for (const FMaterialParameterInfo& Info : VectorParamInfo)
	{
		FString ParamName = Info.Name.ToString();
		if (!MatchesFilter(ParamName, NameFilter))
		{
			continue;
		}

		FLinearColor Value = FLinearColor::White;
		FLinearColor DefaultValue = FLinearColor::White;
		bool bHasValue = MatInstance->GetVectorParameterValue(Info, Value);

		// Get default from parent
		if (bIncludeDefaults && MatInstance->Parent)
		{
			MatInstance->Parent->GetVectorParameterValue(Info, DefaultValue);
		}

		TSharedPtr<FJsonObject> ParamJson = MakeShareable(new FJsonObject);
		ParamJson->SetStringField(TEXT("name"), ParamName);

		TSharedPtr<FJsonObject> ValueJson = MakeShareable(new FJsonObject);
		ValueJson->SetNumberField(TEXT("r"), Value.R);
		ValueJson->SetNumberField(TEXT("g"), Value.G);
		ValueJson->SetNumberField(TEXT("b"), Value.B);
		ValueJson->SetNumberField(TEXT("a"), Value.A);
		ParamJson->SetObjectField(TEXT("value"), ValueJson);

		if (bIncludeDefaults)
		{
			TSharedPtr<FJsonObject> DefaultJson = MakeShareable(new FJsonObject);
			DefaultJson->SetNumberField(TEXT("r"), DefaultValue.R);
			DefaultJson->SetNumberField(TEXT("g"), DefaultValue.G);
			DefaultJson->SetNumberField(TEXT("b"), DefaultValue.B);
			DefaultJson->SetNumberField(TEXT("a"), DefaultValue.A);
			ParamJson->SetObjectField(TEXT("default_value"), DefaultJson);
			ParamJson->SetBoolField(TEXT("is_overridden"), !Value.Equals(DefaultValue));
		}

		OutVectorParams.Add(MakeShareable(new FJsonValueObject(ParamJson)));
	}

	// Texture parameters
	TArray<FMaterialParameterInfo> TextureParamInfo;
	TArray<FGuid> TextureParamGuids;
	MatInstance->GetAllTextureParameterInfo(TextureParamInfo, TextureParamGuids);

	for (const FMaterialParameterInfo& Info : TextureParamInfo)
	{
		FString ParamName = Info.Name.ToString();
		if (!MatchesFilter(ParamName, NameFilter))
		{
			continue;
		}

		UTexture* Value = nullptr;
		UTexture* DefaultValue = nullptr;
		bool bHasValue = MatInstance->GetTextureParameterValue(Info, Value);

		// Get default from parent
		if (bIncludeDefaults && MatInstance->Parent)
		{
			MatInstance->Parent->GetTextureParameterValue(Info, DefaultValue);
		}

		TSharedPtr<FJsonObject> ParamJson = MakeShareable(new FJsonObject);
		ParamJson->SetStringField(TEXT("name"), ParamName);

		if (Value)
		{
			ParamJson->SetStringField(TEXT("value"), Value->GetPathName());
		}
		else
		{
			ParamJson->SetField(TEXT("value"), MakeShareable(new FJsonValueNull()));
		}

		if (bIncludeDefaults)
		{
			if (DefaultValue)
			{
				ParamJson->SetStringField(TEXT("default_value"), DefaultValue->GetPathName());
			}
			else
			{
				ParamJson->SetField(TEXT("default_value"), MakeShareable(new FJsonValueNull()));
			}
			ParamJson->SetBoolField(TEXT("is_overridden"), Value != DefaultValue);
		}

		OutTextureParams.Add(MakeShareable(new FJsonValueObject(ParamJson)));
	}

	// Static switch parameters
	TArray<FMaterialParameterInfo> SwitchParamInfo;
	TArray<FGuid> SwitchParamGuids;
	MatInstance->GetAllStaticSwitchParameterInfo(SwitchParamInfo, SwitchParamGuids);

	for (const FMaterialParameterInfo& Info : SwitchParamInfo)
	{
		FString ParamName = Info.Name.ToString();
		if (!MatchesFilter(ParamName, NameFilter))
		{
			continue;
		}

		bool Value = false;
		FGuid ExpressionGuid;
		bool bHasValue = MatInstance->GetStaticSwitchParameterValue(Info.Name, Value, ExpressionGuid);

		TSharedPtr<FJsonObject> ParamJson = MakeShareable(new FJsonObject);
		ParamJson->SetStringField(TEXT("name"), ParamName);
		ParamJson->SetBoolField(TEXT("value"), Value);

		OutSwitchParams.Add(MakeShareable(new FJsonValueObject(ParamJson)));
	}
}

bool UMaterialParametersTool::MatchesFilter(const FString& ParamName, const FString& Filter) const
{
	if (Filter.IsEmpty())
	{
		return true;
	}

	// Simple wildcard matching
	return ParamName.MatchesWildcard(Filter, ESearchCase::IgnoreCase);
}
