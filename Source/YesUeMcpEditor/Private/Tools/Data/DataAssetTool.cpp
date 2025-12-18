// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/Data/DataAssetTool.h"
#include "Engine/DataTable.h"
#include "Engine/DataAsset.h"
#include "Tools/McpToolResult.h"

FString UDataAssetTool::GetToolDescription() const
{
	return TEXT("Inspect DataTable and DataAsset contents. "\
		"Returns row data for DataTables and property values for DataAssets.");
}

TMap<FString, FMcpSchemaProperty> UDataAssetTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty AssetPath;
	AssetPath.Type = TEXT("string");
	AssetPath.Description = TEXT("Asset path to the DataTable or DataAsset (e.g., /Game/Data/DT_Items)");
	AssetPath.bRequired = true;
	Schema.Add(TEXT("asset_path"), AssetPath);

	FMcpSchemaProperty RowFilter;
	RowFilter.Type = TEXT("string");
	RowFilter.Description = TEXT("Filter rows by name (wildcards supported, DataTable only)");
	RowFilter.bRequired = false;
	Schema.Add(TEXT("row_filter"), RowFilter);

	return Schema;
}

TArray<FString> UDataAssetTool::GetRequiredParams() const
{
	return { TEXT("asset_path") };
}

FMcpToolResult UDataAssetTool::Execute(
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	// Get asset path
	FString AssetPath = GetStringArg(Arguments, TEXT("asset_path"), TEXT(""));
	if (AssetPath.IsEmpty())
	{
		return FMcpToolResult::Error(TEXT("Missing required parameter: asset_path"));
	}

	// Optional row filter (for DataTables)
	FString RowFilter = GetStringArgOrDefault(Arguments, TEXT("row_filter"), TEXT(""));

	// Try loading as DataTable first
	UDataTable* DataTable = LoadObject<UDataTable>(nullptr, *AssetPath);
	if (DataTable)
	{
		TSharedPtr<FJsonObject> Result = InspectDataTable(DataTable, RowFilter);
		return FMcpToolResult::Json(Result);
	}

	// Try loading as DataAsset
	UDataAsset* DataAsset = LoadObject<UDataAsset>(nullptr, *AssetPath);
	if (DataAsset)
	{
		TSharedPtr<FJsonObject> Result = InspectDataAsset(DataAsset);
		return FMcpToolResult::Json(Result);
	}

	return FMcpToolResult::Error(FString::Printf(
		TEXT("Failed to load DataTable or DataAsset at path: %s"), *AssetPath));
}

TSharedPtr<FJsonObject> UDataAssetTool::InspectDataTable(UDataTable* DataTable, const FString& RowFilter) const
{
	if (!DataTable)
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);

	Result->SetStringField(TEXT("type"), TEXT("DataTable"));
	Result->SetStringField(TEXT("asset_path"), DataTable->GetPathName());

	// Row struct
	if (DataTable->GetRowStruct())
	{
		Result->SetStringField(TEXT("row_struct"), DataTable->GetRowStruct()->GetName());
	}

	// Get all rows
	TArray<FName> RowNames = DataTable->GetRowNames();
	TArray<TSharedPtr<FJsonValue>> RowsArray;

	for (const FName& RowName : RowNames)
	{
		// Apply row filter
		if (!RowFilter.IsEmpty() && !MatchesRowFilter(RowName, RowFilter))
		{
			continue;
		}

		uint8* RowData = DataTable->FindRowUnchecked(RowName);
		if (!RowData)
		{
			continue;
		}

		TSharedPtr<FJsonObject> RowJson = MakeShareable(new FJsonObject);
		RowJson->SetStringField(TEXT("row_name"), RowName.ToString());

		// Convert row struct to JSON
		TSharedPtr<FJsonObject> RowDataJson = StructToJson(DataTable->GetRowStruct(), RowData);
		if (RowDataJson.IsValid())
		{
			RowJson->SetObjectField(TEXT("data"), RowDataJson);
		}

		RowsArray.Add(MakeShareable(new FJsonValueObject(RowJson)));
	}

	Result->SetArrayField(TEXT("rows"), RowsArray);
	Result->SetNumberField(TEXT("row_count"), RowsArray.Num());
	Result->SetNumberField(TEXT("total_rows"), RowNames.Num());

	return Result;
}

TSharedPtr<FJsonObject> UDataAssetTool::InspectDataAsset(UDataAsset* DataAsset) const
{
	if (!DataAsset)
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);

	Result->SetStringField(TEXT("type"), TEXT("DataAsset"));
	Result->SetStringField(TEXT("asset_path"), DataAsset->GetPathName());
	Result->SetStringField(TEXT("class"), DataAsset->GetClass()->GetName());

	// Get all properties
	TArray<TSharedPtr<FJsonValue>> PropertiesArray;

	for (TFieldIterator<FProperty> PropIt(DataAsset->GetClass(), EFieldIteratorFlags::ExcludeSuper); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		if (!Property)
		{
			continue;
		}

		void* ValuePtr = Property->ContainerPtrToValuePtr<void>(DataAsset);
		if (!ValuePtr)
		{
			continue;
		}

		TSharedPtr<FJsonObject> PropertyJson = MakeShareable(new FJsonObject);
		PropertyJson->SetStringField(TEXT("name"), Property->GetName());

		TSharedPtr<FJsonValue> ValueJson = PropertyToJsonValue(Property, ValuePtr);
		if (ValueJson.IsValid())
		{
			PropertyJson->SetField(TEXT("value"), ValueJson);
		}

		PropertiesArray.Add(MakeShareable(new FJsonValueObject(PropertyJson)));
	}

	Result->SetArrayField(TEXT("properties"), PropertiesArray);
	Result->SetNumberField(TEXT("property_count"), PropertiesArray.Num());

	return Result;
}

TSharedPtr<FJsonObject> UDataAssetTool::StructToJson(const UScriptStruct* Struct, const void* StructData) const
{
	if (!Struct || !StructData)
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> StructJson = MakeShareable(new FJsonObject);

	for (TFieldIterator<FProperty> PropIt(Struct); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		if (!Property)
		{
			continue;
		}

		const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(StructData);
		if (!ValuePtr)
		{
			continue;
		}

		TSharedPtr<FJsonValue> ValueJson = PropertyToJsonValue(Property, ValuePtr);
		if (ValueJson.IsValid())
		{
			StructJson->SetField(Property->GetName(), ValueJson);
		}
	}

	return StructJson;
}

TSharedPtr<FJsonValue> UDataAssetTool::PropertyToJsonValue(FProperty* Property, const void* ValuePtr) const
{
	if (!Property || !ValuePtr)
	{
		return nullptr;
	}

	// Bool
	if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		bool Value = BoolProp->GetPropertyValue(ValuePtr);
		return MakeShareable(new FJsonValueBoolean(Value));
	}

	// Int
	if (const FIntProperty* IntProp = CastField<FIntProperty>(Property))
	{
		int32 Value = IntProp->GetPropertyValue(ValuePtr);
		return MakeShareable(new FJsonValueNumber(Value));
	}

	// Float
	if (const FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
	{
		float Value = FloatProp->GetPropertyValue(ValuePtr);
		return MakeShareable(new FJsonValueNumber(Value));
	}

	// Double
	if (const FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
	{
		double Value = DoubleProp->GetPropertyValue(ValuePtr);
		return MakeShareable(new FJsonValueNumber(Value));
	}

	// String
	if (const FStrProperty* StrProp = CastField<FStrProperty>(Property))
	{
		FString Value = StrProp->GetPropertyValue(ValuePtr);
		return MakeShareable(new FJsonValueString(Value));
	}

	// Name
	if (const FNameProperty* NameProp = CastField<FNameProperty>(Property))
	{
		FName Value = NameProp->GetPropertyValue(ValuePtr);
		return MakeShareable(new FJsonValueString(Value.ToString()));
	}

	// Text
	if (const FTextProperty* TextProp = CastField<FTextProperty>(Property))
	{
		FText Value = TextProp->GetPropertyValue(ValuePtr);
		return MakeShareable(new FJsonValueString(Value.ToString()));
	}

	// Struct
	if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		TSharedPtr<FJsonObject> StructJson = StructToJson(StructProp->Struct, ValuePtr);
		return MakeShareable(new FJsonValueObject(StructJson));
	}

	// Array
	if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
	{
		FScriptArrayHelper ArrayHelper(ArrayProp, ValuePtr);
		TArray<TSharedPtr<FJsonValue>> ArrayJson;

		for (int32 i = 0; i < ArrayHelper.Num(); i++)
		{
			const void* ElementPtr = ArrayHelper.GetRawPtr(i);
			TSharedPtr<FJsonValue> ElementJson = PropertyToJsonValue(ArrayProp->Inner, ElementPtr);
			if (ElementJson.IsValid())
			{
				ArrayJson.Add(ElementJson);
			}
		}

		return MakeShareable(new FJsonValueArray(ArrayJson));
	}

	// Object
	if (const FObjectProperty* ObjectProp = CastField<FObjectProperty>(Property))
	{
		UObject* ObjectValue = ObjectProp->GetObjectPropertyValue(ValuePtr);
		if (ObjectValue)
		{
			return MakeShareable(new FJsonValueString(ObjectValue->GetPathName()));
		}
		return MakeShareable(new FJsonValueNull());
	}

	// Fallback: export as text
	FString ValueStr;
	Property->ExportText_Direct(ValueStr, ValuePtr, ValuePtr, nullptr, PPF_None);
	return MakeShareable(new FJsonValueString(ValueStr));
}

bool UDataAssetTool::MatchesRowFilter(const FName& RowName, const FString& Filter) const
{
	if (Filter.IsEmpty())
	{
		return true;
	}

	FString RowNameStr = RowName.ToString();

	// Simple wildcard matching
	if (Filter.Contains(TEXT("*")))
	{
		if (Filter.StartsWith(TEXT("*")) && Filter.EndsWith(TEXT("*")))
		{
			// *substring*
			FString Substring = Filter.Mid(1, Filter.Len() - 2);
			return RowNameStr.Contains(Substring);
		}
		else if (Filter.StartsWith(TEXT("*")))
		{
			// *suffix
			FString Suffix = Filter.Mid(1);
			return RowNameStr.EndsWith(Suffix);
		}
		else if (Filter.EndsWith(TEXT("*")))
		{
			// prefix*
			FString Prefix = Filter.Left(Filter.Len() - 1);
			return RowNameStr.StartsWith(Prefix);
		}
	}

	// Exact match
	return RowNameStr.Equals(Filter, ESearchCase::IgnoreCase);
}
