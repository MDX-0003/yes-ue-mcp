// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/Material/MaterialGraphTool.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialFunction.h"
#include "Tools/McpToolResult.h"
#include "YesUeMcpEditor.h"

FString UMaterialGraphTool::GetToolDescription() const
{
	return TEXT("Read complete Material expression graph structure including all expressions, connections, and properties. "
		"Returns material outputs, expression nodes with inputs/outputs, and connection data.");
}

TMap<FString, FMcpSchemaProperty> UMaterialGraphTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty AssetPath;
	AssetPath.Type = TEXT("string");
	AssetPath.Description = TEXT("Asset path to the Material (e.g., /Game/Materials/M_Character)");
	AssetPath.bRequired = true;
	Schema.Add(TEXT("asset_path"), AssetPath);

	FMcpSchemaProperty IncludePositions;
	IncludePositions.Type = TEXT("boolean");
	IncludePositions.Description = TEXT("Include expression X/Y positions in the output (default: false)");
	IncludePositions.bRequired = false;
	Schema.Add(TEXT("include_positions"), IncludePositions);

	return Schema;
}

TArray<FString> UMaterialGraphTool::GetRequiredParams() const
{
	return { TEXT("asset_path") };
}

FMcpToolResult UMaterialGraphTool::Execute(
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
	bool bIncludePositions = GetBoolArgOrDefault(Arguments, TEXT("include_positions"), false);

	UE_LOG(LogYesUeMcp, Log, TEXT("get-material-graph: path='%s'"), *AssetPath);

	// Check if it's a MaterialInstance (which doesn't have a graph)
	UMaterialInstance* MatInstance = LoadObject<UMaterialInstance>(nullptr, *AssetPath);
	if (MatInstance)
	{
		return FMcpToolResult::Error(FString::Printf(
			TEXT("Asset '%s' is a MaterialInstance. MaterialInstances don't have expression graphs. Use get-material-parameters instead, or load the parent Material."),
			*AssetPath));
	}

	// Load Material
	UMaterial* Material = LoadObject<UMaterial>(nullptr, *AssetPath);
	if (!Material)
	{
		return FMcpToolResult::Error(FString::Printf(TEXT("Failed to load Material at path: %s"), *AssetPath));
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	Result->SetStringField(TEXT("asset_type"), TEXT("Material"));

	// Material properties
	Result->SetStringField(TEXT("material_domain"), GetMaterialDomainString(Material->MaterialDomain));
	Result->SetStringField(TEXT("blend_mode"), GetBlendModeString(Material->BlendMode));
	Result->SetStringField(TEXT("shading_model"), GetShadingModelString(Material->GetShadingModels().GetFirstShadingModel()));
	Result->SetBoolField(TEXT("two_sided"), Material->IsTwoSided());

	// Material outputs (connections to final material node)
	TSharedPtr<FJsonObject> MaterialOutputs = MakeShareable(new FJsonObject);

	// Access material inputs via editor-only data
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 7
	// UE 5.7+ uses GetEditorOnlyData() without HasEditorOnlyData() check
	if (UMaterialEditorOnlyData* EditorData = Material->GetEditorOnlyData())
	{
		MaterialOutputs->SetObjectField(TEXT("base_color"), MaterialOutputToJson(&EditorData->BaseColor, TEXT("BaseColor"), Material));
		MaterialOutputs->SetObjectField(TEXT("metallic"), MaterialOutputToJson(&EditorData->Metallic, TEXT("Metallic"), Material));
		MaterialOutputs->SetObjectField(TEXT("specular"), MaterialOutputToJson(&EditorData->Specular, TEXT("Specular"), Material));
		MaterialOutputs->SetObjectField(TEXT("roughness"), MaterialOutputToJson(&EditorData->Roughness, TEXT("Roughness"), Material));
		MaterialOutputs->SetObjectField(TEXT("anisotropy"), MaterialOutputToJson(&EditorData->Anisotropy, TEXT("Anisotropy"), Material));
		MaterialOutputs->SetObjectField(TEXT("normal"), MaterialOutputToJson(&EditorData->Normal, TEXT("Normal"), Material));
		MaterialOutputs->SetObjectField(TEXT("tangent"), MaterialOutputToJson(&EditorData->Tangent, TEXT("Tangent"), Material));
		MaterialOutputs->SetObjectField(TEXT("emissive_color"), MaterialOutputToJson(&EditorData->EmissiveColor, TEXT("EmissiveColor"), Material));
		MaterialOutputs->SetObjectField(TEXT("opacity"), MaterialOutputToJson(&EditorData->Opacity, TEXT("Opacity"), Material));
		MaterialOutputs->SetObjectField(TEXT("opacity_mask"), MaterialOutputToJson(&EditorData->OpacityMask, TEXT("OpacityMask"), Material));
		MaterialOutputs->SetObjectField(TEXT("world_position_offset"), MaterialOutputToJson(&EditorData->WorldPositionOffset, TEXT("WorldPositionOffset"), Material));
		MaterialOutputs->SetObjectField(TEXT("subsurface_color"), MaterialOutputToJson(&EditorData->SubsurfaceColor, TEXT("SubsurfaceColor"), Material));
		MaterialOutputs->SetObjectField(TEXT("ambient_occlusion"), MaterialOutputToJson(&EditorData->AmbientOcclusion, TEXT("AmbientOcclusion"), Material));
		MaterialOutputs->SetObjectField(TEXT("refraction"), MaterialOutputToJson(&EditorData->Refraction, TEXT("Refraction"), Material));
		MaterialOutputs->SetObjectField(TEXT("pixel_depth_offset"), MaterialOutputToJson(&EditorData->PixelDepthOffset, TEXT("PixelDepthOffset"), Material));
	}
#else
	// UE 5.6 and earlier
	if (Material->HasEditorOnlyData())
	{
		MaterialOutputs->SetObjectField(TEXT("base_color"), MaterialOutputToJson(&Material->GetEditorOnlyData()->BaseColor, TEXT("BaseColor"), Material));
		MaterialOutputs->SetObjectField(TEXT("metallic"), MaterialOutputToJson(&Material->GetEditorOnlyData()->Metallic, TEXT("Metallic"), Material));
		MaterialOutputs->SetObjectField(TEXT("specular"), MaterialOutputToJson(&Material->GetEditorOnlyData()->Specular, TEXT("Specular"), Material));
		MaterialOutputs->SetObjectField(TEXT("roughness"), MaterialOutputToJson(&Material->GetEditorOnlyData()->Roughness, TEXT("Roughness"), Material));
		MaterialOutputs->SetObjectField(TEXT("anisotropy"), MaterialOutputToJson(&Material->GetEditorOnlyData()->Anisotropy, TEXT("Anisotropy"), Material));
		MaterialOutputs->SetObjectField(TEXT("normal"), MaterialOutputToJson(&Material->GetEditorOnlyData()->Normal, TEXT("Normal"), Material));
		MaterialOutputs->SetObjectField(TEXT("tangent"), MaterialOutputToJson(&Material->GetEditorOnlyData()->Tangent, TEXT("Tangent"), Material));
		MaterialOutputs->SetObjectField(TEXT("emissive_color"), MaterialOutputToJson(&Material->GetEditorOnlyData()->EmissiveColor, TEXT("EmissiveColor"), Material));
		MaterialOutputs->SetObjectField(TEXT("opacity"), MaterialOutputToJson(&Material->GetEditorOnlyData()->Opacity, TEXT("Opacity"), Material));
		MaterialOutputs->SetObjectField(TEXT("opacity_mask"), MaterialOutputToJson(&Material->GetEditorOnlyData()->OpacityMask, TEXT("OpacityMask"), Material));
		MaterialOutputs->SetObjectField(TEXT("world_position_offset"), MaterialOutputToJson(&Material->GetEditorOnlyData()->WorldPositionOffset, TEXT("WorldPositionOffset"), Material));
		MaterialOutputs->SetObjectField(TEXT("subsurface_color"), MaterialOutputToJson(&Material->GetEditorOnlyData()->SubsurfaceColor, TEXT("SubsurfaceColor"), Material));
		MaterialOutputs->SetObjectField(TEXT("ambient_occlusion"), MaterialOutputToJson(&Material->GetEditorOnlyData()->AmbientOcclusion, TEXT("AmbientOcclusion"), Material));
		MaterialOutputs->SetObjectField(TEXT("refraction"), MaterialOutputToJson(&Material->GetEditorOnlyData()->Refraction, TEXT("Refraction"), Material));
		MaterialOutputs->SetObjectField(TEXT("pixel_depth_offset"), MaterialOutputToJson(&Material->GetEditorOnlyData()->PixelDepthOffset, TEXT("PixelDepthOffset"), Material));
	}
#endif

	Result->SetObjectField(TEXT("material_outputs"), MaterialOutputs);

	// Get all expressions - GetExpressions() returns TArrayView in 5.7+, TArray& in earlier versions
	TArray<TObjectPtr<UMaterialExpression>> Expressions;
	for (UMaterialExpression* Expr : Material->GetExpressions())
	{
		Expressions.Add(Expr);
	}

	TArray<TSharedPtr<FJsonValue>> ExpressionsArray;
	int32 ParameterCount = 0;
	int32 TextureCount = 0;
	int32 FunctionCallCount = 0;

	for (const TObjectPtr<UMaterialExpression>& ExprPtr : Expressions)
	{
		UMaterialExpression* Expression = ExprPtr.Get();
		if (!Expression)
		{
			continue;
		}

		TSharedPtr<FJsonObject> ExprJson = ExpressionToJson(Expression, bIncludePositions);
		if (ExprJson.IsValid())
		{
			ExpressionsArray.Add(MakeShareable(new FJsonValueObject(ExprJson)));

			// Count statistics
			if (Expression->bIsParameterExpression)
			{
				ParameterCount++;
			}
			if (Expression->IsA<UMaterialExpressionTextureSample>())
			{
				TextureCount++;
			}
			if (Expression->IsA<UMaterialExpressionMaterialFunctionCall>())
			{
				FunctionCallCount++;
			}
		}
	}

	Result->SetArrayField(TEXT("expressions"), ExpressionsArray);

	// Statistics
	TSharedPtr<FJsonObject> Stats = MakeShareable(new FJsonObject);
	Stats->SetNumberField(TEXT("total_expressions"), ExpressionsArray.Num());
	Stats->SetNumberField(TEXT("parameter_count"), ParameterCount);
	Stats->SetNumberField(TEXT("texture_count"), TextureCount);
	Stats->SetNumberField(TEXT("function_call_count"), FunctionCallCount);
	Result->SetObjectField(TEXT("statistics"), Stats);

	return FMcpToolResult::Json(Result);
}

TSharedPtr<FJsonObject> UMaterialGraphTool::ExpressionToJson(UMaterialExpression* Expression, bool bIncludePositions) const
{
	if (!Expression)
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> ExprJson = MakeShareable(new FJsonObject);

	// Expression identification
	ExprJson->SetStringField(TEXT("id"), GetExpressionId(Expression));
	ExprJson->SetStringField(TEXT("class"), Expression->GetClass()->GetName());

	// Name and description
	FString ExprName = Expression->GetName();
	ExprJson->SetStringField(TEXT("name"), ExprName);

	if (!Expression->Desc.IsEmpty())
	{
		ExprJson->SetStringField(TEXT("description"), Expression->Desc);
	}

	// Parameter info
	ExprJson->SetBoolField(TEXT("is_parameter"), Expression->bIsParameterExpression);

	// Position (if requested)
	if (bIncludePositions)
	{
		TSharedPtr<FJsonObject> PositionJson = MakeShareable(new FJsonObject);
		PositionJson->SetNumberField(TEXT("x"), Expression->MaterialExpressionEditorX);
		PositionJson->SetNumberField(TEXT("y"), Expression->MaterialExpressionEditorY);
		ExprJson->SetObjectField(TEXT("position"), PositionJson);
	}

	// Get material owner for ID lookups
	UMaterial* OwnerMaterial = Expression->Material;

	// Inputs - iterate using GetInput() until nullptr (works in all UE versions)
	TArray<TSharedPtr<FJsonValue>> InputsArray;
	for (int32 i = 0; ; i++)
	{
		FExpressionInput* Input = Expression->GetInput(i);
		if (!Input)
		{
			break; // No more inputs
		}

		FString InputName = Expression->GetInputName(i).ToString();
		TSharedPtr<FJsonObject> InputJson = InputToJson(Input, InputName, OwnerMaterial);
		if (InputJson.IsValid())
		{
			InputsArray.Add(MakeShareable(new FJsonValueObject(InputJson)));
		}
	}
	ExprJson->SetArrayField(TEXT("inputs"), InputsArray);

	// Outputs
	TArray<TSharedPtr<FJsonValue>> OutputsArray;
	TArray<FExpressionOutput>& Outputs = Expression->GetOutputs();

	for (int32 i = 0; i < Outputs.Num(); i++)
	{
		TSharedPtr<FJsonObject> OutputJson = OutputToJson(Outputs[i], i);
		if (OutputJson.IsValid())
		{
			OutputsArray.Add(MakeShareable(new FJsonValueObject(OutputJson)));
		}
	}
	ExprJson->SetArrayField(TEXT("outputs"), OutputsArray);

	// Add type-specific properties
	AddExpressionSpecificProperties(Expression, ExprJson);

	return ExprJson;
}

TSharedPtr<FJsonObject> UMaterialGraphTool::InputToJson(FExpressionInput* Input, const FString& InputName, UMaterial* Material) const
{
	if (!Input)
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> InputJson = MakeShareable(new FJsonObject);
	InputJson->SetStringField(TEXT("name"), InputName);

	if (Input->Expression)
	{
		InputJson->SetBoolField(TEXT("connected"), true);
		InputJson->SetStringField(TEXT("connected_expression"), GetExpressionId(Input->Expression));
		InputJson->SetNumberField(TEXT("output_index"), Input->OutputIndex);

		// Include mask info
		if (Input->Mask)
		{
			FString MaskStr;
			if (Input->MaskR) MaskStr += TEXT("R");
			if (Input->MaskG) MaskStr += TEXT("G");
			if (Input->MaskB) MaskStr += TEXT("B");
			if (Input->MaskA) MaskStr += TEXT("A");
			InputJson->SetStringField(TEXT("mask"), MaskStr);
		}
	}
	else
	{
		InputJson->SetBoolField(TEXT("connected"), false);
	}

	return InputJson;
}

TSharedPtr<FJsonObject> UMaterialGraphTool::OutputToJson(const FExpressionOutput& Output, int32 Index) const
{
	TSharedPtr<FJsonObject> OutputJson = MakeShareable(new FJsonObject);

	OutputJson->SetNumberField(TEXT("index"), Index);
	OutputJson->SetStringField(TEXT("name"), Output.OutputName.ToString());

	// Output mask
	FString MaskStr;
	if (Output.MaskR) MaskStr += TEXT("R");
	if (Output.MaskG) MaskStr += TEXT("G");
	if (Output.MaskB) MaskStr += TEXT("B");
	if (Output.MaskA) MaskStr += TEXT("A");

	if (!MaskStr.IsEmpty())
	{
		OutputJson->SetStringField(TEXT("mask"), MaskStr);
	}
	else if (Output.Mask)
	{
		OutputJson->SetStringField(TEXT("mask"), TEXT("RGBA"));
	}

	return OutputJson;
}

TSharedPtr<FJsonObject> UMaterialGraphTool::MaterialOutputToJson(FExpressionInput* Input, const FString& OutputName, UMaterial* Material) const
{
	TSharedPtr<FJsonObject> OutputJson = MakeShareable(new FJsonObject);

	if (Input && Input->Expression)
	{
		OutputJson->SetBoolField(TEXT("connected"), true);
		OutputJson->SetStringField(TEXT("connected_expression"), GetExpressionId(Input->Expression));
		OutputJson->SetNumberField(TEXT("output_index"), Input->OutputIndex);
	}
	else
	{
		OutputJson->SetBoolField(TEXT("connected"), false);
	}

	return OutputJson;
}

FString UMaterialGraphTool::GetExpressionId(UMaterialExpression* Expression) const
{
	if (!Expression)
	{
		return TEXT("");
	}

	// Generate stable ID from object name and unique ID
	return FString::Printf(TEXT("%s_%u"), *Expression->GetName(), Expression->GetUniqueID());
}

FString UMaterialGraphTool::GetMaterialDomainString(EMaterialDomain Domain) const
{
	switch (Domain)
	{
	case MD_Surface:
		return TEXT("Surface");
	case MD_DeferredDecal:
		return TEXT("DeferredDecal");
	case MD_LightFunction:
		return TEXT("LightFunction");
	case MD_Volume:
		return TEXT("Volume");
	case MD_PostProcess:
		return TEXT("PostProcess");
	case MD_UI:
		return TEXT("UI");
	default:
		return TEXT("Unknown");
	}
}

FString UMaterialGraphTool::GetBlendModeString(EBlendMode BlendMode) const
{
	switch (BlendMode)
	{
	case BLEND_Opaque:
		return TEXT("Opaque");
	case BLEND_Masked:
		return TEXT("Masked");
	case BLEND_Translucent:
		return TEXT("Translucent");
	case BLEND_Additive:
		return TEXT("Additive");
	case BLEND_Modulate:
		return TEXT("Modulate");
	case BLEND_AlphaComposite:
		return TEXT("AlphaComposite");
	case BLEND_AlphaHoldout:
		return TEXT("AlphaHoldout");
	default:
		return TEXT("Unknown");
	}
}

FString UMaterialGraphTool::GetShadingModelString(EMaterialShadingModel ShadingModel) const
{
	switch (ShadingModel)
	{
	case MSM_Unlit:
		return TEXT("Unlit");
	case MSM_DefaultLit:
		return TEXT("DefaultLit");
	case MSM_Subsurface:
		return TEXT("Subsurface");
	case MSM_PreintegratedSkin:
		return TEXT("PreintegratedSkin");
	case MSM_ClearCoat:
		return TEXT("ClearCoat");
	case MSM_SubsurfaceProfile:
		return TEXT("SubsurfaceProfile");
	case MSM_TwoSidedFoliage:
		return TEXT("TwoSidedFoliage");
	case MSM_Hair:
		return TEXT("Hair");
	case MSM_Cloth:
		return TEXT("Cloth");
	case MSM_Eye:
		return TEXT("Eye");
	case MSM_SingleLayerWater:
		return TEXT("SingleLayerWater");
	case MSM_ThinTranslucent:
		return TEXT("ThinTranslucent");
	default:
		return TEXT("Unknown");
	}
}

void UMaterialGraphTool::AddExpressionSpecificProperties(UMaterialExpression* Expression, TSharedPtr<FJsonObject>& OutJson) const
{
	if (!Expression)
	{
		return;
	}

	TSharedPtr<FJsonObject> PropertiesJson = MakeShareable(new FJsonObject);
	bool bHasProperties = false;

	// Parameter expressions
	if (UMaterialExpressionParameter* ParamExpr = Cast<UMaterialExpressionParameter>(Expression))
	{
		OutJson->SetStringField(TEXT("parameter_name"), ParamExpr->ParameterName.ToString());
		PropertiesJson->SetStringField(TEXT("group"), ParamExpr->Group.ToString());
		bHasProperties = true;
	}

	// Scalar parameter
	if (UMaterialExpressionScalarParameter* ScalarParam = Cast<UMaterialExpressionScalarParameter>(Expression))
	{
		PropertiesJson->SetNumberField(TEXT("default_value"), ScalarParam->DefaultValue);
		bHasProperties = true;
	}

	// Vector parameter
	if (UMaterialExpressionVectorParameter* VectorParam = Cast<UMaterialExpressionVectorParameter>(Expression))
	{
		TSharedPtr<FJsonObject> DefaultValue = MakeShareable(new FJsonObject);
		DefaultValue->SetNumberField(TEXT("r"), VectorParam->DefaultValue.R);
		DefaultValue->SetNumberField(TEXT("g"), VectorParam->DefaultValue.G);
		DefaultValue->SetNumberField(TEXT("b"), VectorParam->DefaultValue.B);
		DefaultValue->SetNumberField(TEXT("a"), VectorParam->DefaultValue.A);
		PropertiesJson->SetObjectField(TEXT("default_value"), DefaultValue);
		bHasProperties = true;
	}

	// Texture sample
	if (UMaterialExpressionTextureSample* TexSample = Cast<UMaterialExpressionTextureSample>(Expression))
	{
		if (TexSample->Texture)
		{
			PropertiesJson->SetStringField(TEXT("texture"), TexSample->Texture->GetPathName());
		}
		bHasProperties = true;
	}

	// Texture parameter
	if (UMaterialExpressionTextureSampleParameter* TexParam = Cast<UMaterialExpressionTextureSampleParameter>(Expression))
	{
		OutJson->SetStringField(TEXT("parameter_name"), TexParam->ParameterName.ToString());
		PropertiesJson->SetStringField(TEXT("group"), TexParam->Group.ToString());
		bHasProperties = true;
	}

	// Material function call
	if (UMaterialExpressionMaterialFunctionCall* FuncCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expression))
	{
		if (FuncCall->MaterialFunction)
		{
			PropertiesJson->SetStringField(TEXT("function_path"), FuncCall->MaterialFunction->GetPathName());
			PropertiesJson->SetStringField(TEXT("function_name"), FuncCall->MaterialFunction->GetName());
		}
		bHasProperties = true;
	}

	// Texture coordinate
	if (UMaterialExpressionTextureCoordinate* TexCoord = Cast<UMaterialExpressionTextureCoordinate>(Expression))
	{
		PropertiesJson->SetNumberField(TEXT("coordinate_index"), TexCoord->CoordinateIndex);
		PropertiesJson->SetNumberField(TEXT("u_tiling"), TexCoord->UTiling);
		PropertiesJson->SetNumberField(TEXT("v_tiling"), TexCoord->VTiling);
		bHasProperties = true;
	}

	// Constant
	if (UMaterialExpressionConstant* Const = Cast<UMaterialExpressionConstant>(Expression))
	{
		PropertiesJson->SetNumberField(TEXT("value"), Const->R);
		bHasProperties = true;
	}

	// Constant2Vector
	if (UMaterialExpressionConstant2Vector* Const2 = Cast<UMaterialExpressionConstant2Vector>(Expression))
	{
		PropertiesJson->SetNumberField(TEXT("r"), Const2->R);
		PropertiesJson->SetNumberField(TEXT("g"), Const2->G);
		bHasProperties = true;
	}

	// Constant3Vector
	if (UMaterialExpressionConstant3Vector* Const3 = Cast<UMaterialExpressionConstant3Vector>(Expression))
	{
		TSharedPtr<FJsonObject> ValueJson = MakeShareable(new FJsonObject);
		ValueJson->SetNumberField(TEXT("r"), Const3->Constant.R);
		ValueJson->SetNumberField(TEXT("g"), Const3->Constant.G);
		ValueJson->SetNumberField(TEXT("b"), Const3->Constant.B);
		PropertiesJson->SetObjectField(TEXT("value"), ValueJson);
		bHasProperties = true;
	}

	// Constant4Vector
	if (UMaterialExpressionConstant4Vector* Const4 = Cast<UMaterialExpressionConstant4Vector>(Expression))
	{
		TSharedPtr<FJsonObject> ValueJson = MakeShareable(new FJsonObject);
		ValueJson->SetNumberField(TEXT("r"), Const4->Constant.R);
		ValueJson->SetNumberField(TEXT("g"), Const4->Constant.G);
		ValueJson->SetNumberField(TEXT("b"), Const4->Constant.B);
		ValueJson->SetNumberField(TEXT("a"), Const4->Constant.A);
		PropertiesJson->SetObjectField(TEXT("value"), ValueJson);
		bHasProperties = true;
	}

	if (bHasProperties)
	{
		OutJson->SetObjectField(TEXT("properties"), PropertiesJson);
	}
}
