// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/Asset/AssetSearchTool.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "YesUeMcpEditor.h"

TMap<FString, FMcpSchemaProperty> UAssetSearchTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty Query;
	Query.Type = TEXT("string");
	Query.Description = TEXT("Search query (asset name pattern, supports * and ? wildcards)");
	Query.bRequired = false;
	Schema.Add(TEXT("query"), Query);

	FMcpSchemaProperty ClassFilter;
	ClassFilter.Type = TEXT("string");
	ClassFilter.Description = TEXT("Filter by asset class name (e.g., Blueprint, StaticMesh, Material)");
	ClassFilter.bRequired = false;
	Schema.Add(TEXT("class"), ClassFilter);

	FMcpSchemaProperty PathFilter;
	PathFilter.Type = TEXT("string");
	PathFilter.Description = TEXT("Filter by path prefix (e.g., /Game/Blueprints)");
	PathFilter.bRequired = false;
	Schema.Add(TEXT("path"), PathFilter);

	FMcpSchemaProperty Limit;
	Limit.Type = TEXT("integer");
	Limit.Description = TEXT("Maximum number of results to return (default: 100)");
	Limit.bRequired = false;
	Schema.Add(TEXT("limit"), Limit);

	return Schema;
}

FMcpToolResult UAssetSearchTool::Execute(
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	FString Query = GetStringArgOrDefault(Arguments, TEXT("query"), TEXT(""));
	FString ClassFilter = GetStringArgOrDefault(Arguments, TEXT("class"), TEXT(""));
	FString PathFilter = GetStringArgOrDefault(Arguments, TEXT("path"), TEXT(""));
	int32 Limit = GetIntArgOrDefault(Arguments, TEXT("limit"), 100);

	UE_LOG(LogYesUeMcp, Log, TEXT("search-assets: query='%s', class='%s', path='%s', limit=%d"),
		*Query, *ClassFilter, *PathFilter, Limit);

	// At least one filter must be specified
	if (Query.IsEmpty() && ClassFilter.IsEmpty() && PathFilter.IsEmpty())
	{
		UE_LOG(LogYesUeMcp, Warning, TEXT("search-assets: No filter specified, returning error"));
		return FMcpToolResult::Error(TEXT("At least one of 'query', 'class', or 'path' must be specified"));
	}

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	// Build filter
	FARFilter Filter;

	// Path filter
	if (!PathFilter.IsEmpty())
	{
		Filter.PackagePaths.Add(FName(*PathFilter));
		Filter.bRecursivePaths = true;
	}

	// Class filter
	if (!ClassFilter.IsEmpty())
	{
		UClass* FilterClass = nullptr;

		// Use FindFirstObjectSafe - searches all loaded packages by name (built-in UE function)
		FilterClass = FindFirstObjectSafe<UClass>(*ClassFilter);

		// Try with U/A prefix if not found
		if (!FilterClass)
		{
			FilterClass = FindFirstObjectSafe<UClass>(*FString::Printf(TEXT("U%s"), *ClassFilter));
		}
		if (!FilterClass)
		{
			FilterClass = FindFirstObjectSafe<UClass>(*FString::Printf(TEXT("A%s"), *ClassFilter));
		}

		if (FilterClass)
		{
			Filter.ClassPaths.Add(FilterClass->GetClassPathName());
			Filter.bRecursiveClasses = true;
			UE_LOG(LogYesUeMcp, Log, TEXT("search-assets: Resolved class '%s' -> %s"),
				*ClassFilter, *FilterClass->GetClassPathName().ToString());
		}
		else
		{
			UE_LOG(LogYesUeMcp, Warning, TEXT("search-assets: Class '%s' not found (tried: %s, U%s, A%s)"),
				*ClassFilter, *ClassFilter, *ClassFilter, *ClassFilter);
			return FMcpToolResult::Error(FString::Printf(
				TEXT("Asset class '%s' not found"), *ClassFilter));
		}
	}

	// Get assets
	TArray<FAssetData> Assets;
	AssetRegistry.GetAssets(Filter, Assets);

	UE_LOG(LogYesUeMcp, Log, TEXT("search-assets: AssetRegistry returned %d assets (before name filter)"),
		Assets.Num());

	// Filter by name query if specified
	if (!Query.IsEmpty())
	{
		// Convert wildcard pattern to regex-like matching
		FString Pattern = Query.Replace(TEXT("*"), TEXT(".*")).Replace(TEXT("?"), TEXT("."));

		TArray<FAssetData> FilteredAssets;
		for (const FAssetData& Asset : Assets)
		{
			FString AssetName = Asset.AssetName.ToString();
			if (AssetName.MatchesWildcard(Query))
			{
				FilteredAssets.Add(Asset);
			}
		}
		Assets = MoveTemp(FilteredAssets);

		UE_LOG(LogYesUeMcp, Log, TEXT("search-assets: After name filter '%s': %d assets remain"),
			*Query, Assets.Num());
	}

	// Apply limit
	if (Assets.Num() > Limit)
	{
		Assets.SetNum(Limit);
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	TArray<TSharedPtr<FJsonValue>> AssetsArray;

	for (const FAssetData& Asset : Assets)
	{
		TSharedPtr<FJsonObject> AssetObj = MakeShareable(new FJsonObject);
		AssetObj->SetStringField(TEXT("name"), Asset.AssetName.ToString());
		AssetObj->SetStringField(TEXT("path"), Asset.GetObjectPathString());
		AssetObj->SetStringField(TEXT("package"), Asset.PackageName.ToString());
		AssetObj->SetStringField(TEXT("class"), Asset.AssetClassPath.GetAssetName().ToString());

		// Additional metadata
		FAssetDataTagMapSharedView TagsAndValues = Asset.TagsAndValues;

		// Get parent class for Blueprints
		FAssetTagValueRef ParentClassTag = TagsAndValues.FindTag(FBlueprintTags::ParentClassPath);
		if (ParentClassTag.IsSet())
		{
			FString ParentClass = ParentClassTag.GetValue();
			// Extract class name from path
			int32 DotIndex;
			if (ParentClass.FindLastChar('.', DotIndex))
			{
				ParentClass = ParentClass.Mid(DotIndex + 1);
				ParentClass.RemoveFromEnd(TEXT("'"));
			}
			AssetObj->SetStringField(TEXT("parent_class"), ParentClass);
		}

		AssetsArray.Add(MakeShareable(new FJsonValueObject(AssetObj)));
	}

	Result->SetArrayField(TEXT("assets"), AssetsArray);
	Result->SetNumberField(TEXT("count"), AssetsArray.Num());
	Result->SetBoolField(TEXT("truncated"), Assets.Num() == Limit);

	// Add search parameters for context
	TSharedPtr<FJsonObject> SearchParams = MakeShareable(new FJsonObject);
	if (!Query.IsEmpty()) SearchParams->SetStringField(TEXT("query"), Query);
	if (!ClassFilter.IsEmpty()) SearchParams->SetStringField(TEXT("class"), ClassFilter);
	if (!PathFilter.IsEmpty()) SearchParams->SetStringField(TEXT("path"), PathFilter);
	SearchParams->SetNumberField(TEXT("limit"), Limit);
	Result->SetObjectField(TEXT("search"), SearchParams);

	return FMcpToolResult::Json(Result);
}
