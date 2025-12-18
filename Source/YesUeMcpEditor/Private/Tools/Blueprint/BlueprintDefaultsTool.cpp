// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/Blueprint/BlueprintDefaultsTool.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Tools/McpToolResult.h"

FString UBlueprintDefaultsTool::GetToolDescription() const
{
	return TEXT("Read CDO (Class Default Object) property values from Blueprint. "\
		"Returns all property defaults with types, categories, and flags.");
}

TMap<FString, FMcpSchemaProperty> UBlueprintDefaultsTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty AssetPath;
	AssetPath.Type = TEXT("string");
	AssetPath.Description = TEXT("Asset path to the Blueprint (e.g., /Game/Blueprints/BP_Character)");
	AssetPath.bRequired = true;
	Schema.Add(TEXT("asset_path"), AssetPath);

	FMcpSchemaProperty PropertyFilter;
	PropertyFilter.Type = TEXT("string");
	PropertyFilter.Description = TEXT("Filter by property name (wildcards supported, e.g., '*Health*')");
	PropertyFilter.bRequired = false;
	Schema.Add(TEXT("property_filter"), PropertyFilter);

	FMcpSchemaProperty CategoryFilter;
	CategoryFilter.Type = TEXT("string");
	CategoryFilter.Description = TEXT("Filter by property category (e.g., 'Stats', 'Movement')");
	CategoryFilter.bRequired = false;
	Schema.Add(TEXT("category_filter"), CategoryFilter);

	return Schema;
}

TArray<FString> UBlueprintDefaultsTool::GetRequiredParams() const
{
	return { TEXT("asset_path") };
}

FMcpToolResult UBlueprintDefaultsTool::Execute(
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	// Get asset path
	FString AssetPath = GetStringArg(Arguments, TEXT("asset_path"), TEXT(""));
	if (AssetPath.IsEmpty())
	{
		return FMcpToolResult::Error(TEXT("Missing required parameter: asset_path"));
	}

	// Optional filters
	FString PropertyFilter = GetStringArgOrDefault(Arguments, TEXT("property_filter"), TEXT(""));
	FString CategoryFilter = GetStringArgOrDefault(Arguments, TEXT("category_filter"), TEXT(""));

	// Load Blueprint
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
	if (!Blueprint)
	{
		return FMcpToolResult::Error(FString::Printf(TEXT("Failed to load Blueprint at path: %s"), *AssetPath));
	}

	// Get generated class
	UBlueprintGeneratedClass* GenClass = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass);
	if (!GenClass)
	{
		return FMcpToolResult::Error(FString::Printf(TEXT("Blueprint has no generated class: %s"), *AssetPath));
	}

	// Get CDO
	UObject* CDO = GenClass->GetDefaultObject();
	if (!CDO)
	{
		return FMcpToolResult::Error(FString::Printf(TEXT("Failed to get CDO for Blueprint: %s"), *AssetPath));
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetStringField(TEXT("blueprint"), AssetPath);
	Result->SetStringField(TEXT("parent_class"), GenClass->GetSuperClass() ? GenClass->GetSuperClass()->GetName() : TEXT("None"));
	Result->SetStringField(TEXT("generated_class"), GenClass->GetName());

	// Iterate properties
	TArray<TSharedPtr<FJsonValue>> PropertiesArray;
	for (TFieldIterator<FProperty> PropIt(GenClass, EFieldIteratorFlags::ExcludeSuper); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		if (!Property)
		{
			continue;
		}

		// Apply property name filter
		if (!PropertyFilter.IsEmpty() && !MatchesFilter(Property->GetName(), PropertyFilter))
		{
			continue;
		}

		// Apply category filter
		if (!CategoryFilter.IsEmpty())
		{
			FString Category = Property->GetMetaData(TEXT("Category"));
			if (Category.IsEmpty() || !MatchesFilter(Category, CategoryFilter))
			{
				continue;
			}
		}

		// Get property value from CDO
		void* ValuePtr = Property->ContainerPtrToValuePtr<void>(CDO);
		if (!ValuePtr)
		{
			continue;
		}

		TSharedPtr<FJsonObject> PropertyJson = PropertyToJson(Property, ValuePtr, CDO);
		if (PropertyJson.IsValid())
		{
			PropertiesArray.Add(MakeShareable(new FJsonValueObject(PropertyJson)));
		}
	}

	Result->SetArrayField(TEXT("properties"), PropertiesArray);
	Result->SetNumberField(TEXT("property_count"), PropertiesArray.Num());

	return FMcpToolResult::Json(Result);
}

TSharedPtr<FJsonObject> UBlueprintDefaultsTool::PropertyToJson(FProperty* Property, void* ValuePtr, UObject* CDO) const
{
	if (!Property || !ValuePtr)
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> PropertyJson = MakeShareable(new FJsonObject);

	// Basic info
	PropertyJson->SetStringField(TEXT("name"), Property->GetName());
	PropertyJson->SetStringField(TEXT("type"), GetPropertyTypeString(Property));

	// Display name (from metadata)
	FString DisplayName = Property->GetMetaData(TEXT("DisplayName"));
	if (!DisplayName.IsEmpty())
	{
		PropertyJson->SetStringField(TEXT("display_name"), DisplayName);
	}

	// Category
	FString Category = Property->GetMetaData(TEXT("Category"));
	if (!Category.IsEmpty())
	{
		PropertyJson->SetStringField(TEXT("category"), Category);
	}

	// Tooltip
	FString Tooltip = Property->GetMetaData(TEXT("ToolTip"));
	if (!Tooltip.IsEmpty())
	{
		PropertyJson->SetStringField(TEXT("tooltip"), Tooltip);
	}

	// Export default value as string
	FString DefaultValue;
	Property->ExportText_Direct(DefaultValue, ValuePtr, ValuePtr, CDO, PPF_None);
	PropertyJson->SetStringField(TEXT("default_value"), DefaultValue);

	// Property flags
	TArray<FString> Flags = GetPropertyFlags(Property);
	if (Flags.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> FlagsArray;
		for (const FString& Flag : Flags)
		{
			FlagsArray.Add(MakeShareable(new FJsonValueString(Flag)));
		}
		PropertyJson->SetArrayField(TEXT("flags"), FlagsArray);
	}

	// Additional metadata
	FString ClampMin = Property->GetMetaData(TEXT("ClampMin"));
	if (!ClampMin.IsEmpty())
	{
		PropertyJson->SetStringField(TEXT("clamp_min"), ClampMin);
	}

	FString ClampMax = Property->GetMetaData(TEXT("ClampMax"));
	if (!ClampMax.IsEmpty())
	{
		PropertyJson->SetStringField(TEXT("clamp_max"), ClampMax);
	}

	FString UIMin = Property->GetMetaData(TEXT("UIMin"));
	if (!UIMin.IsEmpty())
	{
		PropertyJson->SetStringField(TEXT("ui_min"), UIMin);
	}

	FString UIMax = Property->GetMetaData(TEXT("UIMax"));
	if (!UIMax.IsEmpty())
	{
		PropertyJson->SetStringField(TEXT("ui_max"), UIMax);
	}

	return PropertyJson;
}

FString UBlueprintDefaultsTool::GetPropertyTypeString(FProperty* Property) const
{
	if (!Property)
	{
		return TEXT("unknown");
	}

	// Check for specific property types
	if (Property->IsA<FBoolProperty>())
	{
		return TEXT("bool");
	}
	else if (Property->IsA<FIntProperty>())
	{
		return TEXT("int32");
	}
	else if (Property->IsA<FInt64Property>())
	{
		return TEXT("int64");
	}
	else if (Property->IsA<FFloatProperty>())
	{
		return TEXT("float");
	}
	else if (Property->IsA<FDoubleProperty>())
	{
		return TEXT("double");
	}
	else if (Property->IsA<FNameProperty>())
	{
		return TEXT("FName");
	}
	else if (Property->IsA<FStrProperty>())
	{
		return TEXT("FString");
	}
	else if (Property->IsA<FTextProperty>())
	{
		return TEXT("FText");
	}
	else if (FObjectProperty* ObjectProp = CastField<FObjectProperty>(Property))
	{
		if (ObjectProp->PropertyClass)
		{
			return FString::Printf(TEXT("TObjectPtr<%s>"), *ObjectProp->PropertyClass->GetName());
		}
		return TEXT("TObjectPtr<UObject>");
	}
	else if (FClassProperty* ClassProp = CastField<FClassProperty>(Property))
	{
		if (ClassProp->MetaClass)
		{
			return FString::Printf(TEXT("TSubclassOf<%s>"), *ClassProp->MetaClass->GetName());
		}
		return TEXT("TSubclassOf<UObject>");
	}
	else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		if (StructProp->Struct)
		{
			return StructProp->Struct->GetName();
		}
		return TEXT("struct");
	}
	else if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
	{
		FString InnerType = GetPropertyTypeString(ArrayProp->Inner);
		return FString::Printf(TEXT("TArray<%s>"), *InnerType);
	}
	else if (FMapProperty* MapProp = CastField<FMapProperty>(Property))
	{
		FString KeyType = GetPropertyTypeString(MapProp->KeyProp);
		FString ValueType = GetPropertyTypeString(MapProp->ValueProp);
		return FString::Printf(TEXT("TMap<%s, %s>"), *KeyType, *ValueType);
	}
	else if (FSetProperty* SetProp = CastField<FSetProperty>(Property))
	{
		FString ElementType = GetPropertyTypeString(SetProp->ElementProp);
		return FString::Printf(TEXT("TSet<%s>"), *ElementType);
	}
	else if (Property->IsA<FEnumProperty>())
	{
		FEnumProperty* EnumProp = CastField<FEnumProperty>(Property);
		if (EnumProp && EnumProp->GetEnum())
		{
			return EnumProp->GetEnum()->GetName();
		}
		return TEXT("enum");
	}
	else if (Property->IsA<FByteProperty>())
	{
		FByteProperty* ByteProp = CastField<FByteProperty>(Property);
		if (ByteProp && ByteProp->Enum)
		{
			return ByteProp->Enum->GetName();
		}
		return TEXT("uint8");
	}

	// Fallback to class name
	return Property->GetClass()->GetName();
}

TArray<FString> UBlueprintDefaultsTool::GetPropertyFlags(FProperty* Property) const
{
	TArray<FString> Flags;

	if (!Property)
	{
		return Flags;
	}

	// Common property flags
	if (Property->HasAnyPropertyFlags(CPF_Edit))
	{
		Flags.Add(TEXT("Edit"));
	}
	if (Property->HasAnyPropertyFlags(CPF_BlueprintVisible))
	{
		Flags.Add(TEXT("BlueprintVisible"));
	}
	if (Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly))
	{
		Flags.Add(TEXT("BlueprintReadOnly"));
	}
	if (Property->HasMetaData(TEXT("AllowPrivateAccess")))
	{
		Flags.Add(TEXT("AllowPrivateAccess"));
	}
	if (Property->HasAnyPropertyFlags(CPF_Config))
	{
		Flags.Add(TEXT("Config"));
	}
	if (Property->HasAnyPropertyFlags(CPF_GlobalConfig))
	{
		Flags.Add(TEXT("GlobalConfig"));
	}
	if (Property->HasAnyPropertyFlags(CPF_Transient))
	{
		Flags.Add(TEXT("Transient"));
	}
	if (Property->HasAnyPropertyFlags(CPF_SaveGame))
	{
		Flags.Add(TEXT("SaveGame"));
	}
	if (Property->HasAnyPropertyFlags(CPF_Net))
	{
		Flags.Add(TEXT("Replicated"));
	}
	if (Property->HasAnyPropertyFlags(CPF_RepNotify))
	{
		Flags.Add(TEXT("ReplicatedUsing"));
	}
	if (Property->HasAnyPropertyFlags(CPF_EditConst))
	{
		Flags.Add(TEXT("EditConst"));
	}
	if (Property->HasAnyPropertyFlags(CPF_AdvancedDisplay))
	{
		Flags.Add(TEXT("AdvancedDisplay"));
	}
	if (Property->HasAnyPropertyFlags(CPF_DisableEditOnInstance))
	{
		Flags.Add(TEXT("DisableEditOnInstance"));
	}
	if (Property->HasAnyPropertyFlags(CPF_DisableEditOnTemplate))
	{
		Flags.Add(TEXT("DisableEditOnTemplate"));
	}

	// Check for EditAnywhere, EditDefaultsOnly, EditInstanceOnly
	if (Property->HasAnyPropertyFlags(CPF_Edit))
	{
		if (Property->HasAnyPropertyFlags(CPF_DisableEditOnInstance))
		{
			Flags.Add(TEXT("EditDefaultsOnly"));
		}
		else if (Property->HasAnyPropertyFlags(CPF_DisableEditOnTemplate))
		{
			Flags.Add(TEXT("EditInstanceOnly"));
		}
		else
		{
			Flags.Add(TEXT("EditAnywhere"));
		}
	}

	// Check for BlueprintReadWrite
	if (Property->HasAnyPropertyFlags(CPF_BlueprintVisible) && !Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly))
	{
		Flags.Add(TEXT("BlueprintReadWrite"));
	}

	return Flags;
}

bool UBlueprintDefaultsTool::MatchesFilter(const FString& Name, const FString& Filter) const
{
	if (Filter.IsEmpty())
	{
		return true;
	}

	// Simple wildcard matching
	if (Filter.Contains(TEXT("*")))
	{
		FString Pattern = Filter;
		Pattern.ReplaceInline(TEXT("*"), TEXT(".*"));

		// Simple regex-like matching
		if (Pattern.StartsWith(TEXT(".*")) && Pattern.EndsWith(TEXT(".*")))
		{
			// *substring*
			FString Substring = Pattern.Mid(2, Pattern.Len() - 4);
			return Name.Contains(Substring);
		}
		else if (Pattern.StartsWith(TEXT(".*")))
		{
			// *suffix
			FString Suffix = Pattern.Mid(2);
			return Name.EndsWith(Suffix);
		}
		else if (Pattern.EndsWith(TEXT(".*")))
		{
			// prefix*
			FString Prefix = Pattern.Left(Pattern.Len() - 2);
			return Name.StartsWith(Prefix);
		}
	}

	// Exact match
	return Name.Equals(Filter, ESearchCase::IgnoreCase);
}
