// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/Blueprint/BlueprintComponentsTool.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Components/SceneComponent.h"

TMap<FString, FMcpSchemaProperty> UBlueprintComponentsTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty AssetPath;
	AssetPath.Type = TEXT("string");
	AssetPath.Description = TEXT("Asset path to the Blueprint");
	AssetPath.bRequired = true;
	Schema.Add(TEXT("asset_path"), AssetPath);

	return Schema;
}

FMcpToolResult UBlueprintComponentsTool::Execute(
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
	Result->SetStringField(TEXT("blueprint"), Blueprint->GetName());

	if (!Blueprint->SimpleConstructionScript)
	{
		Result->SetArrayField(TEXT("components"), TArray<TSharedPtr<FJsonValue>>());
		Result->SetNumberField(TEXT("count"), 0);
		return FMcpToolResult::Json(Result);
	}

	USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
	TArray<TSharedPtr<FJsonValue>> ComponentsArray;

	// Build tree from root nodes
	for (USCS_Node* RootNode : SCS->GetRootNodes())
	{
		if (RootNode)
		{
			TSharedPtr<FJsonObject> NodeObj = BuildComponentNode(RootNode, SCS);
			if (NodeObj.IsValid())
			{
				ComponentsArray.Add(MakeShareable(new FJsonValueObject(NodeObj)));
			}
		}
	}

	Result->SetArrayField(TEXT("components"), ComponentsArray);
	Result->SetNumberField(TEXT("count"), SCS->GetAllNodes().Num());

	// Also list flat array of all components
	TArray<TSharedPtr<FJsonValue>> FlatArray;
	for (USCS_Node* Node : SCS->GetAllNodes())
	{
		if (Node && Node->ComponentTemplate)
		{
			TSharedPtr<FJsonObject> CompObj = MakeShareable(new FJsonObject);
			CompObj->SetStringField(TEXT("name"), Node->GetVariableName().ToString());
			CompObj->SetStringField(TEXT("class"), Node->ComponentClass ? Node->ComponentClass->GetName() : TEXT("Unknown"));

			if (Node->ParentComponentOrVariableName != NAME_None)
			{
				CompObj->SetStringField(TEXT("parent"), Node->ParentComponentOrVariableName.ToString());
			}

			FlatArray.Add(MakeShareable(new FJsonValueObject(CompObj)));
		}
	}
	Result->SetArrayField(TEXT("flat_list"), FlatArray);

	return FMcpToolResult::Json(Result);
}

TSharedPtr<FJsonObject> UBlueprintComponentsTool::BuildComponentNode(USCS_Node* Node, USimpleConstructionScript* SCS)
{
	if (!Node)
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> NodeObj = MakeShareable(new FJsonObject);
	NodeObj->SetStringField(TEXT("name"), Node->GetVariableName().ToString());

	// Component class
	if (Node->ComponentClass)
	{
		NodeObj->SetStringField(TEXT("class"), Node->ComponentClass->GetName());
		NodeObj->SetStringField(TEXT("class_path"), Node->ComponentClass->GetPathName());

		// Check if it's a scene component
		if (Node->ComponentClass->IsChildOf(USceneComponent::StaticClass()))
		{
			NodeObj->SetBoolField(TEXT("is_scene_component"), true);
		}
	}

	// Component template properties
	if (Node->ComponentTemplate)
	{
		UActorComponent* Template = Node->ComponentTemplate;

		// Common properties
		NodeObj->SetBoolField(TEXT("auto_activate"), Template->bAutoActivate);

		// Scene component specific
		if (USceneComponent* SceneComp = Cast<USceneComponent>(Template))
		{
			TSharedPtr<FJsonObject> TransformObj = MakeShareable(new FJsonObject);

			FVector Location = SceneComp->GetRelativeLocation();
			TSharedPtr<FJsonObject> LocObj = MakeShareable(new FJsonObject);
			LocObj->SetNumberField(TEXT("x"), Location.X);
			LocObj->SetNumberField(TEXT("y"), Location.Y);
			LocObj->SetNumberField(TEXT("z"), Location.Z);
			TransformObj->SetObjectField(TEXT("location"), LocObj);

			FRotator Rotation = SceneComp->GetRelativeRotation();
			TSharedPtr<FJsonObject> RotObj = MakeShareable(new FJsonObject);
			RotObj->SetNumberField(TEXT("pitch"), Rotation.Pitch);
			RotObj->SetNumberField(TEXT("yaw"), Rotation.Yaw);
			RotObj->SetNumberField(TEXT("roll"), Rotation.Roll);
			TransformObj->SetObjectField(TEXT("rotation"), RotObj);

			FVector Scale = SceneComp->GetRelativeScale3D();
			TSharedPtr<FJsonObject> ScaleObj = MakeShareable(new FJsonObject);
			ScaleObj->SetNumberField(TEXT("x"), Scale.X);
			ScaleObj->SetNumberField(TEXT("y"), Scale.Y);
			ScaleObj->SetNumberField(TEXT("z"), Scale.Z);
			TransformObj->SetObjectField(TEXT("scale"), ScaleObj);

			NodeObj->SetObjectField(TEXT("transform"), TransformObj);
			NodeObj->SetStringField(TEXT("mobility"), StaticEnum<EComponentMobility::Type>()->GetValueAsString(SceneComp->Mobility));
		}
	}

	// Is root component
	NodeObj->SetBoolField(TEXT("is_root"), SCS->GetRootNodes().Contains(Node));

	// Child components
	TArray<TSharedPtr<FJsonValue>> ChildrenArray;
	for (USCS_Node* ChildNode : Node->GetChildNodes())
	{
		TSharedPtr<FJsonObject> ChildObj = BuildComponentNode(ChildNode, SCS);
		if (ChildObj.IsValid())
		{
			ChildrenArray.Add(MakeShareable(new FJsonValueObject(ChildObj)));
		}
	}

	if (ChildrenArray.Num() > 0)
	{
		NodeObj->SetArrayField(TEXT("children"), ChildrenArray);
	}

	return NodeObj;
}
