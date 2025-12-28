// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/Write/RemoveWidgetTool.h"
#include "Utils/McpAssetModifier.h"
#include "YesUeMcpEditor.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Components/PanelWidget.h"
#include "ScopedTransaction.h"

FString URemoveWidgetTool::GetToolDescription() const
{
	return TEXT("Remove a widget from a WidgetBlueprint tree.");
}

TMap<FString, FMcpSchemaProperty> URemoveWidgetTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty AssetPath;
	AssetPath.Type = TEXT("string");
	AssetPath.Description = TEXT("WidgetBlueprint asset path");
	AssetPath.bRequired = true;
	Schema.Add(TEXT("asset_path"), AssetPath);

	FMcpSchemaProperty WidgetName;
	WidgetName.Type = TEXT("string");
	WidgetName.Description = TEXT("Name of the widget to remove");
	WidgetName.bRequired = true;
	Schema.Add(TEXT("widget_name"), WidgetName);

	return Schema;
}

TArray<FString> URemoveWidgetTool::GetRequiredParams() const
{
	return { TEXT("asset_path"), TEXT("widget_name") };
}

FMcpToolResult URemoveWidgetTool::Execute(
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	FString AssetPath = GetStringArgOrDefault(Arguments, TEXT("asset_path"));
	FString WidgetName = GetStringArgOrDefault(Arguments, TEXT("widget_name"));

	if (AssetPath.IsEmpty() || WidgetName.IsEmpty())
	{
		return FMcpToolResult::Error(TEXT("asset_path and widget_name are required"));
	}

	UE_LOG(LogYesUeMcp, Log, TEXT("remove-widget: %s from %s"), *WidgetName, *AssetPath);

	// Load the WidgetBlueprint
	FString LoadError;
	UWidgetBlueprint* WidgetBP = FMcpAssetModifier::LoadAssetByPath<UWidgetBlueprint>(AssetPath, LoadError);
	if (!WidgetBP)
	{
		return FMcpToolResult::Error(LoadError);
	}

	UWidgetTree* WidgetTree = WidgetBP->WidgetTree;
	if (!WidgetTree)
	{
		return FMcpToolResult::Error(TEXT("WidgetBlueprint has no WidgetTree"));
	}

	// Find the widget
	UWidget* FoundWidget = nullptr;

	WidgetTree->ForEachWidget([&](UWidget* Widget)
	{
		if (Widget && Widget->GetName().Equals(WidgetName, ESearchCase::IgnoreCase))
		{
			FoundWidget = Widget;
		}
	});

	if (!FoundWidget)
	{
		return FMcpToolResult::Error(FString::Printf(TEXT("Widget not found: %s"), *WidgetName));
	}

	// Begin transaction
	TSharedPtr<FScopedTransaction> Transaction = FMcpAssetModifier::BeginTransaction(
		FText::Format(NSLOCTEXT("MCP", "RemoveWidget", "Remove {0} from {1}"),
			FText::FromString(WidgetName), FText::FromString(AssetPath)));

	FMcpAssetModifier::MarkModified(WidgetBP);

	// Remove from parent
	UPanelWidget* Parent = FoundWidget->GetParent();
	if (Parent)
	{
		Parent->RemoveChild(FoundWidget);
	}
	else if (WidgetTree->RootWidget == FoundWidget)
	{
		WidgetTree->RootWidget = nullptr;
	}

	// Remove from widget tree
	WidgetTree->RemoveWidget(FoundWidget);

	FMcpAssetModifier::MarkPackageDirty(WidgetBP);

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("asset"), AssetPath);
	Result->SetStringField(TEXT("widget_name"), WidgetName);
	Result->SetBoolField(TEXT("needs_compile"), true);
	Result->SetBoolField(TEXT("needs_save"), true);

	UE_LOG(LogYesUeMcp, Log, TEXT("remove-widget: Removed %s"), *WidgetName);

	return FMcpToolResult::Json(Result);
}
