// Copyright softdaddy-o 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/McpToolBase.h"
#include "InspectAssetTool.generated.h"

/**
 * General-purpose tool for inspecting any UObject-derived asset using reflection.
 * Works with any asset type including ChooserTable, PoseSearchDatabase, etc.
 */
UCLASS()
class YESUEMCPEDITOR_API UInspectAssetTool : public UMcpToolBase
{
	GENERATED_BODY()

public:
	virtual FString GetToolName() const override { return TEXT("inspect-asset"); }
	virtual FString GetToolDescription() const override;
	virtual TMap<FString, FMcpSchemaProperty> GetInputSchema() const override;
	virtual TArray<FString> GetRequiredParams() const override;

	virtual FMcpToolResult Execute(
		const TSharedPtr<FJsonObject>& Arguments,
		const FMcpToolContext& Context) override;

private:
	/** Convert a UObject to JSON using reflection */
	TSharedPtr<FJsonObject> ObjectToJson(UObject* Object, int32 CurrentDepth, int32 MaxDepth, const FString& PropertyFilter, const FString& CategoryFilter, bool bIncludeDefaults) const;

	/** Convert struct data to JSON */
	TSharedPtr<FJsonObject> StructToJson(const UScriptStruct* Struct, const void* StructData, int32 CurrentDepth, int32 MaxDepth) const;

	/** Convert a property value to JSON */
	TSharedPtr<FJsonValue> PropertyToJsonValue(FProperty* Property, const void* ValuePtr, int32 CurrentDepth, int32 MaxDepth) const;

	/** Get property category from metadata */
	FString GetPropertyCategory(FProperty* Property) const;

	/** Check if property name matches filter (wildcard support) */
	bool MatchesPropertyFilter(const FString& PropertyName, const FString& Filter) const;

	/** Get class hierarchy as array of class names */
	TArray<FString> GetClassHierarchy(UClass* Class) const;

	/** Track visited objects to prevent infinite recursion */
	mutable TSet<const UObject*> VisitedObjects;
};
