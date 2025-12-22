// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/Asset/InspectAssetTool.h"
#include "Tools/McpToolResult.h"
#include "UObject/UnrealType.h"
#include "UObject/EnumProperty.h"

FString UInspectAssetTool::GetToolDescription() const
{
	return TEXT("Inspect any UObject-derived asset using reflection. "
		"Returns class hierarchy, properties with types and values. "
		"Works with all asset types including ChooserTable, PoseSearchDatabase, etc.");
}

TMap<FString, FMcpSchemaProperty> UInspectAssetTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty AssetPath;
	AssetPath.Type = TEXT("string");
	AssetPath.Description = TEXT("Asset path to inspect (e.g., /Game/Data/CHT_MyChooser)");
	AssetPath.bRequired = true;
	Schema.Add(TEXT("asset_path"), AssetPath);

	FMcpSchemaProperty Depth;
	Depth.Type = TEXT("integer");
	Depth.Description = TEXT("Maximum recursion depth for nested objects (default: 2, max: 5)");
	Depth.bRequired = false;
	Schema.Add(TEXT("depth"), Depth);

	FMcpSchemaProperty IncludeDefaults;
	IncludeDefaults.Type = TEXT("boolean");
	IncludeDefaults.Description = TEXT("Include properties with default/empty values (default: false)");
	IncludeDefaults.bRequired = false;
	Schema.Add(TEXT("include_defaults"), IncludeDefaults);

	FMcpSchemaProperty PropertyFilter;
	PropertyFilter.Type = TEXT("string");
	PropertyFilter.Description = TEXT("Filter properties by name (wildcards supported, e.g., '*Health*')");
	PropertyFilter.bRequired = false;
	Schema.Add(TEXT("property_filter"), PropertyFilter);

	FMcpSchemaProperty CategoryFilter;
	CategoryFilter.Type = TEXT("string");
	CategoryFilter.Description = TEXT("Filter by UPROPERTY category (e.g., 'Stats', 'Movement')");
	CategoryFilter.bRequired = false;
	Schema.Add(TEXT("category_filter"), CategoryFilter);

	return Schema;
}

TArray<FString> UInspectAssetTool::GetRequiredParams() const
{
	return { TEXT("asset_path") };
}

FMcpToolResult UInspectAssetTool::Execute(
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
	int32 MaxDepth = FMath::Clamp(GetIntArgOrDefault(Arguments, TEXT("depth"), 2), 1, 5);
	bool bIncludeDefaults = GetBoolArgOrDefault(Arguments, TEXT("include_defaults"), false);
	FString PropertyFilter = GetStringArgOrDefault(Arguments, TEXT("property_filter"), TEXT(""));
	FString CategoryFilter = GetStringArgOrDefault(Arguments, TEXT("category_filter"), TEXT(""));

	// Load asset
	UObject* Asset = LoadObject<UObject>(nullptr, *AssetPath);
	if (!Asset)
	{
		return FMcpToolResult::Error(FString::Printf(TEXT("Failed to load asset at path: %s"), *AssetPath));
	}

	// Reset visited objects tracking
	VisitedObjects.Empty();

	// Build result
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetStringField(TEXT("asset_path"), Asset->GetPathName());
	Result->SetStringField(TEXT("class"), Asset->GetClass()->GetName());

	// Class hierarchy
	TArray<FString> Hierarchy = GetClassHierarchy(Asset->GetClass());
	TArray<TSharedPtr<FJsonValue>> HierarchyArray;
	for (const FString& ClassName : Hierarchy)
	{
		HierarchyArray.Add(MakeShareable(new FJsonValueString(ClassName)));
	}
	Result->SetArrayField(TEXT("class_hierarchy"), HierarchyArray);

	// Get properties using reflection
	TSharedPtr<FJsonObject> PropertiesJson = ObjectToJson(Asset, 0, MaxDepth, PropertyFilter, CategoryFilter, bIncludeDefaults);
	if (PropertiesJson.IsValid())
	{
		Result->SetObjectField(TEXT("properties"), PropertiesJson);
	}

	return FMcpToolResult::Json(Result);
}

TSharedPtr<FJsonObject> UInspectAssetTool::ObjectToJson(
	UObject* Object,
	int32 CurrentDepth,
	int32 MaxDepth,
	const FString& PropertyFilter,
	const FString& CategoryFilter,
	bool bIncludeDefaults) const
{
	if (!Object)
	{
		return nullptr;
	}

	// Prevent infinite recursion on circular references
	if (VisitedObjects.Contains(Object))
	{
		TSharedPtr<FJsonObject> CircularRef = MakeShareable(new FJsonObject);
		CircularRef->SetStringField(TEXT("_circular_reference"), Object->GetPathName());
		return CircularRef;
	}
	VisitedObjects.Add(Object);

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);

	// Iterate all properties
	for (TFieldIterator<FProperty> PropIt(Object->GetClass()); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		if (!Property)
		{
			continue;
		}

		// Skip private/transient unless explicitly included
		if (Property->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient))
		{
			continue;
		}

		// Apply property name filter
		FString PropName = Property->GetName();
		if (!PropertyFilter.IsEmpty() && !MatchesPropertyFilter(PropName, PropertyFilter))
		{
			continue;
		}

		// Apply category filter
		if (!CategoryFilter.IsEmpty())
		{
			FString Category = GetPropertyCategory(Property);
			if (!Category.Equals(CategoryFilter, ESearchCase::IgnoreCase))
			{
				continue;
			}
		}

		// Get property value
		const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Object);
		if (!ValuePtr)
		{
			continue;
		}

		// Convert to JSON
		TSharedPtr<FJsonValue> ValueJson = PropertyToJsonValue(Property, ValuePtr, CurrentDepth, MaxDepth);

		// Skip default/empty values unless requested
		if (!bIncludeDefaults && ValueJson.IsValid())
		{
			// Check for null
			if (ValueJson->Type == EJson::Null)
			{
				continue;
			}
			// Check for empty string
			if (ValueJson->Type == EJson::String)
			{
				FString StrValue;
				if (ValueJson->TryGetString(StrValue) && StrValue.IsEmpty())
				{
					continue;
				}
			}
			// Check for empty array
			if (ValueJson->Type == EJson::Array)
			{
				const TArray<TSharedPtr<FJsonValue>>* ArrayValue;
				if (ValueJson->TryGetArray(ArrayValue) && ArrayValue->Num() == 0)
				{
					continue;
				}
			}
		}

		if (ValueJson.IsValid())
		{
			Result->SetField(PropName, ValueJson);
		}
	}

	return Result;
}

TSharedPtr<FJsonObject> UInspectAssetTool::StructToJson(
	const UScriptStruct* Struct,
	const void* StructData,
	int32 CurrentDepth,
	int32 MaxDepth) const
{
	if (!Struct || !StructData)
	{
		return nullptr;
	}

	// Depth limit for structs
	if (CurrentDepth >= MaxDepth)
	{
		TSharedPtr<FJsonObject> DepthLimited = MakeShareable(new FJsonObject);
		DepthLimited->SetStringField(TEXT("_depth_limited"), Struct->GetName());
		return DepthLimited;
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

		TSharedPtr<FJsonValue> ValueJson = PropertyToJsonValue(Property, ValuePtr, CurrentDepth + 1, MaxDepth);
		if (ValueJson.IsValid())
		{
			StructJson->SetField(Property->GetName(), ValueJson);
		}
	}

	return StructJson;
}

TSharedPtr<FJsonValue> UInspectAssetTool::PropertyToJsonValue(
	FProperty* Property,
	const void* ValuePtr,
	int32 CurrentDepth,
	int32 MaxDepth) const
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

	// Numeric types
	if (const FNumericProperty* NumProp = CastField<FNumericProperty>(Property))
	{
		if (NumProp->IsInteger())
		{
			int64 Value = NumProp->GetSignedIntPropertyValue(ValuePtr);
			return MakeShareable(new FJsonValueNumber(static_cast<double>(Value)));
		}
		else if (NumProp->IsFloatingPoint())
		{
			double Value = NumProp->GetFloatingPointPropertyValue(ValuePtr);
			return MakeShareable(new FJsonValueNumber(Value));
		}
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

	// Enum
	if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
	{
		UEnum* Enum = EnumProp->GetEnum();
		FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();
		int64 Value = UnderlyingProp->GetSignedIntPropertyValue(ValuePtr);

		TSharedPtr<FJsonObject> EnumJson = MakeShareable(new FJsonObject);
		EnumJson->SetStringField(TEXT("enum"), Enum->GetName());
		EnumJson->SetStringField(TEXT("value"), Enum->GetNameStringByValue(Value));
		EnumJson->SetNumberField(TEXT("index"), static_cast<double>(Value));
		return MakeShareable(new FJsonValueObject(EnumJson));
	}

	// Byte (could be enum)
	if (const FByteProperty* ByteProp = CastField<FByteProperty>(Property))
	{
		if (ByteProp->Enum)
		{
			uint8 Value = ByteProp->GetPropertyValue(ValuePtr);
			TSharedPtr<FJsonObject> EnumJson = MakeShareable(new FJsonObject);
			EnumJson->SetStringField(TEXT("enum"), ByteProp->Enum->GetName());
			EnumJson->SetStringField(TEXT("value"), ByteProp->Enum->GetNameStringByValue(Value));
			EnumJson->SetNumberField(TEXT("index"), static_cast<double>(Value));
			return MakeShareable(new FJsonValueObject(EnumJson));
		}
		else
		{
			uint8 Value = ByteProp->GetPropertyValue(ValuePtr);
			return MakeShareable(new FJsonValueNumber(static_cast<double>(Value)));
		}
	}

	// Struct
	if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		TSharedPtr<FJsonObject> StructJson = StructToJson(StructProp->Struct, ValuePtr, CurrentDepth, MaxDepth);
		if (StructJson.IsValid())
		{
			return MakeShareable(new FJsonValueObject(StructJson));
		}
		return MakeShareable(new FJsonValueNull());
	}

	// Array
	if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
	{
		FScriptArrayHelper ArrayHelper(ArrayProp, ValuePtr);
		TArray<TSharedPtr<FJsonValue>> ArrayJson;

		for (int32 i = 0; i < ArrayHelper.Num(); i++)
		{
			const void* ElementPtr = ArrayHelper.GetRawPtr(i);
			TSharedPtr<FJsonValue> ElementJson = PropertyToJsonValue(ArrayProp->Inner, ElementPtr, CurrentDepth + 1, MaxDepth);
			if (ElementJson.IsValid())
			{
				ArrayJson.Add(ElementJson);
			}
		}

		return MakeShareable(new FJsonValueArray(ArrayJson));
	}

	// Map
	if (const FMapProperty* MapProp = CastField<FMapProperty>(Property))
	{
		FScriptMapHelper MapHelper(MapProp, ValuePtr);
		TSharedPtr<FJsonObject> MapJson = MakeShareable(new FJsonObject);

		for (int32 i = 0; i < MapHelper.Num(); i++)
		{
			if (MapHelper.IsValidIndex(i))
			{
				const void* KeyPtr = MapHelper.GetKeyPtr(i);
				const void* ValPtr = MapHelper.GetValuePtr(i);

				// Convert key to string
				FString KeyStr;
				MapProp->KeyProp->ExportText_Direct(KeyStr, KeyPtr, KeyPtr, nullptr, PPF_None);

				// Convert value to JSON
				TSharedPtr<FJsonValue> ValJson = PropertyToJsonValue(MapProp->ValueProp, ValPtr, CurrentDepth + 1, MaxDepth);
				if (ValJson.IsValid())
				{
					MapJson->SetField(KeyStr, ValJson);
				}
			}
		}

		return MakeShareable(new FJsonValueObject(MapJson));
	}

	// Set
	if (const FSetProperty* SetProp = CastField<FSetProperty>(Property))
	{
		FScriptSetHelper SetHelper(SetProp, ValuePtr);
		TArray<TSharedPtr<FJsonValue>> SetJson;

		for (int32 i = 0; i < SetHelper.Num(); i++)
		{
			if (SetHelper.IsValidIndex(i))
			{
				const void* ElementPtr = SetHelper.GetElementPtr(i);
				TSharedPtr<FJsonValue> ElementJson = PropertyToJsonValue(SetProp->ElementProp, ElementPtr, CurrentDepth + 1, MaxDepth);
				if (ElementJson.IsValid())
				{
					SetJson.Add(ElementJson);
				}
			}
		}

		return MakeShareable(new FJsonValueArray(SetJson));
	}

	// Object reference
	if (const FObjectProperty* ObjectProp = CastField<FObjectProperty>(Property))
	{
		UObject* ObjectValue = ObjectProp->GetObjectPropertyValue(ValuePtr);
		if (ObjectValue)
		{
			// For nested objects within depth limit, try to expand
			if (CurrentDepth < MaxDepth)
			{
				TSharedPtr<FJsonObject> NestedJson = MakeShareable(new FJsonObject);
				NestedJson->SetStringField(TEXT("_class"), ObjectValue->GetClass()->GetName());
				NestedJson->SetStringField(TEXT("_path"), ObjectValue->GetPathName());

				// Only expand if not at depth limit
				TSharedPtr<FJsonObject> PropsJson = ObjectToJson(ObjectValue, CurrentDepth + 1, MaxDepth, TEXT(""), TEXT(""), false);
				if (PropsJson.IsValid() && PropsJson->Values.Num() > 0)
				{
					NestedJson->SetObjectField(TEXT("properties"), PropsJson);
				}

				return MakeShareable(new FJsonValueObject(NestedJson));
			}
			else
			{
				// Just return the path at depth limit
				return MakeShareable(new FJsonValueString(ObjectValue->GetPathName()));
			}
		}
		return MakeShareable(new FJsonValueNull());
	}

	// Soft object reference
	if (const FSoftObjectProperty* SoftObjectProp = CastField<FSoftObjectProperty>(Property))
	{
		const FSoftObjectPtr& SoftPtr = *static_cast<const FSoftObjectPtr*>(ValuePtr);
		if (!SoftPtr.IsNull())
		{
			return MakeShareable(new FJsonValueString(SoftPtr.ToString()));
		}
		return MakeShareable(new FJsonValueNull());
	}

	// Class reference
	if (const FClassProperty* ClassProp = CastField<FClassProperty>(Property))
	{
		UClass* ClassValue = Cast<UClass>(ClassProp->GetObjectPropertyValue(ValuePtr));
		if (ClassValue)
		{
			return MakeShareable(new FJsonValueString(ClassValue->GetPathName()));
		}
		return MakeShareable(new FJsonValueNull());
	}

	// Soft class reference
	if (const FSoftClassProperty* SoftClassProp = CastField<FSoftClassProperty>(Property))
	{
		const FSoftObjectPtr& SoftPtr = *static_cast<const FSoftObjectPtr*>(ValuePtr);
		if (!SoftPtr.IsNull())
		{
			return MakeShareable(new FJsonValueString(SoftPtr.ToString()));
		}
		return MakeShareable(new FJsonValueNull());
	}

	// Delegate (just note that it exists)
	if (CastField<FDelegateProperty>(Property) || CastField<FMulticastDelegateProperty>(Property))
	{
		return MakeShareable(new FJsonValueString(TEXT("[delegate]")));
	}

	// Interface
	if (const FInterfaceProperty* InterfaceProp = CastField<FInterfaceProperty>(Property))
	{
		const FScriptInterface& Interface = *static_cast<const FScriptInterface*>(ValuePtr);
		if (Interface.GetObject())
		{
			return MakeShareable(new FJsonValueString(Interface.GetObject()->GetPathName()));
		}
		return MakeShareable(new FJsonValueNull());
	}

	// Fallback: export as text
	FString ValueStr;
	Property->ExportText_Direct(ValueStr, ValuePtr, ValuePtr, nullptr, PPF_None);
	return MakeShareable(new FJsonValueString(ValueStr));
}

FString UInspectAssetTool::GetPropertyCategory(FProperty* Property) const
{
	if (!Property)
	{
		return TEXT("");
	}

	// Get category from metadata
	const FString& Category = Property->GetMetaData(TEXT("Category"));
	return Category;
}

bool UInspectAssetTool::MatchesPropertyFilter(const FString& PropertyName, const FString& Filter) const
{
	if (Filter.IsEmpty())
	{
		return true;
	}

	// Simple wildcard matching
	if (Filter.Contains(TEXT("*")))
	{
		if (Filter.StartsWith(TEXT("*")) && Filter.EndsWith(TEXT("*")))
		{
			// *substring*
			FString Substring = Filter.Mid(1, Filter.Len() - 2);
			return PropertyName.Contains(Substring, ESearchCase::IgnoreCase);
		}
		else if (Filter.StartsWith(TEXT("*")))
		{
			// *suffix
			FString Suffix = Filter.Mid(1);
			return PropertyName.EndsWith(Suffix, ESearchCase::IgnoreCase);
		}
		else if (Filter.EndsWith(TEXT("*")))
		{
			// prefix*
			FString Prefix = Filter.Left(Filter.Len() - 1);
			return PropertyName.StartsWith(Prefix, ESearchCase::IgnoreCase);
		}
	}

	// Exact match (case insensitive)
	return PropertyName.Equals(Filter, ESearchCase::IgnoreCase);
}

TArray<FString> UInspectAssetTool::GetClassHierarchy(UClass* Class) const
{
	TArray<FString> Hierarchy;

	UClass* Current = Class;
	while (Current)
	{
		Hierarchy.Add(Current->GetName());
		Current = Current->GetSuperClass();
	}

	return Hierarchy;
}
