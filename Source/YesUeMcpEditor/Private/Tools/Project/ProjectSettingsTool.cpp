// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/Project/ProjectSettingsTool.h"
#include "GameFramework/InputSettings.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Engine/CollisionProfile.h"
#include "GameplayTagsSettings.h"
#include "GameMapsSettings.h"
#include "Tools/McpToolResult.h"

FString UProjectSettingsTool::GetToolDescription() const
{
	return TEXT("Query project configuration including input mappings, collision settings, gameplay tags, and default maps.");
}

TMap<FString, FMcpSchemaProperty> UProjectSettingsTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	FMcpSchemaProperty Section;
	Section.Type = TEXT("string");
	Section.Description = TEXT("Section to query: 'input', 'collision', 'tags', 'maps', 'all' (default: 'all')");
	Section.bRequired = false;
	Schema.Add(TEXT("section"), Section);

	return Schema;
}

TArray<FString> UProjectSettingsTool::GetRequiredParams() const
{
	return {}; // No required params
}

FMcpToolResult UProjectSettingsTool::Execute(
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	FString Section = GetStringArgOrDefault(Arguments, TEXT("section"), TEXT("all"));
	Section = Section.ToLower();

	// Validate section
	if (!Section.IsEmpty() &&
		Section != TEXT("all") &&
		Section != TEXT("input") &&
		Section != TEXT("collision") &&
		Section != TEXT("tags") &&
		Section != TEXT("maps"))
	{
		return FMcpToolResult::Error(FString::Printf(
			TEXT("Invalid section '%s'. Must be 'input', 'collision', 'tags', 'maps', or 'all'"), *Section));
	}

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);

	// Add requested sections
	if (Section == TEXT("all") || Section == TEXT("input"))
	{
		Result->SetObjectField(TEXT("input"), GetInputMappings());
	}

	if (Section == TEXT("all") || Section == TEXT("collision"))
	{
		Result->SetObjectField(TEXT("collision"), GetCollisionSettings());
	}

	if (Section == TEXT("all") || Section == TEXT("tags"))
	{
		Result->SetObjectField(TEXT("tags"), GetGameplayTags());
	}

	if (Section == TEXT("all") || Section == TEXT("maps"))
	{
		Result->SetObjectField(TEXT("maps"), GetDefaultMapsAndModes());
	}

	return FMcpToolResult::Json(Result);
}

TSharedPtr<FJsonObject> UProjectSettingsTool::GetInputMappings() const
{
	TSharedPtr<FJsonObject> InputJson = MakeShareable(new FJsonObject);

	const UInputSettings* InputSettings = GetDefault<UInputSettings>();
	if (!InputSettings)
	{
		return InputJson;
	}

	// Action mappings
	TArray<TSharedPtr<FJsonValue>> ActionMappingsArray;
	for (const FInputActionKeyMapping& ActionMapping : InputSettings->GetActionMappings())
	{
		TSharedPtr<FJsonObject> MappingJson = MakeShareable(new FJsonObject);
		MappingJson->SetStringField(TEXT("action_name"), ActionMapping.ActionName.ToString());
		MappingJson->SetStringField(TEXT("key"), ActionMapping.Key.ToString());
		MappingJson->SetBoolField(TEXT("shift"), ActionMapping.bShift);
		MappingJson->SetBoolField(TEXT("ctrl"), ActionMapping.bCtrl);
		MappingJson->SetBoolField(TEXT("alt"), ActionMapping.bAlt);
		MappingJson->SetBoolField(TEXT("cmd"), ActionMapping.bCmd);

		ActionMappingsArray.Add(MakeShareable(new FJsonValueObject(MappingJson)));
	}
	InputJson->SetArrayField(TEXT("action_mappings"), ActionMappingsArray);

	// Axis mappings
	TArray<TSharedPtr<FJsonValue>> AxisMappingsArray;
	for (const FInputAxisKeyMapping& AxisMapping : InputSettings->GetAxisMappings())
	{
		TSharedPtr<FJsonObject> MappingJson = MakeShareable(new FJsonObject);
		MappingJson->SetStringField(TEXT("axis_name"), AxisMapping.AxisName.ToString());
		MappingJson->SetStringField(TEXT("key"), AxisMapping.Key.ToString());
		MappingJson->SetNumberField(TEXT("scale"), AxisMapping.Scale);

		AxisMappingsArray.Add(MakeShareable(new FJsonValueObject(MappingJson)));
	}
	InputJson->SetArrayField(TEXT("axis_mappings"), AxisMappingsArray);

	return InputJson;
}

TSharedPtr<FJsonObject> UProjectSettingsTool::GetCollisionSettings() const
{
	TSharedPtr<FJsonObject> CollisionJson = MakeShareable(new FJsonObject);

	const UCollisionProfile* CollisionProfile = UCollisionProfile::Get();
	if (!CollisionProfile)
	{
		return CollisionJson;
	}

	// Collision profiles
	TArray<TSharedPtr<FJsonValue>> ProfilesArray;
	TArray<TSharedPtr<FCollisionResponseTemplate>> Profiles;

	// Use const_cast to access GetProfileTemplates (it's not const but should be)
	UCollisionProfile* MutableProfile = const_cast<UCollisionProfile*>(CollisionProfile);
	for (int32 i = 0; i < MutableProfile->GetNumOfProfiles(); i++)
	{
		const FCollisionResponseTemplate* ProfileTemplate = MutableProfile->GetProfileByIndex(i);
		if (ProfileTemplate)
		{
			TSharedPtr<FJsonObject> ProfileJson = MakeShareable(new FJsonObject);
			ProfileJson->SetStringField(TEXT("name"), ProfileTemplate->Name.ToString());
			ProfileJson->SetStringField(TEXT("object_type"), UCollisionProfile::Get()->ReturnChannelNameFromContainerIndex(ProfileTemplate->ObjectType).ToString());
			ProfileJson->SetBoolField(TEXT("can_modify"), ProfileTemplate->bCanModify);

			ProfilesArray.Add(MakeShareable(new FJsonValueObject(ProfileJson)));
		}
	}
	CollisionJson->SetArrayField(TEXT("profiles"), ProfilesArray);

	// Collision channels
	const UPhysicsSettings* PhysicsSettings = GetDefault<UPhysicsSettings>();
	if (PhysicsSettings)
	{
		// Default channels
		TArray<TSharedPtr<FJsonValue>> ChannelsArray;

		// Add standard channels
		TArray<FString> StandardChannels = {
			TEXT("WorldStatic"),
			TEXT("WorldDynamic"),
			TEXT("Pawn"),
			TEXT("Visibility"),
			TEXT("Camera"),
			TEXT("PhysicsBody"),
			TEXT("Vehicle"),
			TEXT("Destructible")
		};

		for (const FString& Channel : StandardChannels)
		{
			TSharedPtr<FJsonObject> ChannelJson = MakeShareable(new FJsonObject);
			ChannelJson->SetStringField(TEXT("name"), Channel);
			ChannelJson->SetStringField(TEXT("type"), TEXT("standard"));
			ChannelsArray.Add(MakeShareable(new FJsonValueObject(ChannelJson)));
		}

		// Custom channels - Note: DefaultChannelSetups API changed in UE 5.7
		// Commenting out until proper API is determined
		/*
		for (const FCustomChannelSetup& CustomChannel : PhysicsSettings->DefaultChannelSetups)
		{
			TSharedPtr<FJsonObject> ChannelJson = MakeShareable(new FJsonObject);
			ChannelJson->SetStringField(TEXT("name"), CustomChannel.Name.ToString());
			ChannelJson->SetStringField(TEXT("type"), TEXT("custom"));

			FString DefaultResponse;
			switch (CustomChannel.DefaultResponse)
			{
			case ECR_Ignore:
				DefaultResponse = TEXT("Ignore");
				break;
			case ECR_Overlap:
				DefaultResponse = TEXT("Overlap");
				break;
			case ECR_Block:
				DefaultResponse = TEXT("Block");
				break;
			default:
				DefaultResponse = TEXT("Unknown");
			}
			ChannelJson->SetStringField(TEXT("default_response"), DefaultResponse);

			ChannelsArray.Add(MakeShareable(new FJsonValueObject(ChannelJson)));
		}
		*/

		CollisionJson->SetArrayField(TEXT("channels"), ChannelsArray);
	}

	return CollisionJson;
}

TSharedPtr<FJsonObject> UProjectSettingsTool::GetGameplayTags() const
{
	TSharedPtr<FJsonObject> TagsJson = MakeShareable(new FJsonObject);

	const UGameplayTagsSettings* TagsSettings = GetDefault<UGameplayTagsSettings>();
	if (!TagsSettings)
	{
		return TagsJson;
	}

	// Tag sources - Note: GameplayTagTableList is now FSoftObjectPath in UE 5.7
	TArray<TSharedPtr<FJsonValue>> SourcesArray;
	for (const FSoftObjectPath& SourcePath : TagsSettings->GameplayTagTableList)
	{
		TSharedPtr<FJsonObject> SourceJson = MakeShareable(new FJsonObject);
		SourceJson->SetStringField(TEXT("source_path"), SourcePath.ToString());

		SourcesArray.Add(MakeShareable(new FJsonValueObject(SourceJson)));
	}
	TagsJson->SetArrayField(TEXT("tag_sources"), SourcesArray);

	// Common tags (if any are defined as common) - Note: CommonlyReplicatedTags is now FName array in UE 5.7
	TArray<TSharedPtr<FJsonValue>> CommonTagsArray;
	for (const FName& CommonTag : TagsSettings->CommonlyReplicatedTags)
	{
		CommonTagsArray.Add(MakeShareable(new FJsonValueString(CommonTag.ToString())));
	}
	if (CommonTagsArray.Num() > 0)
	{
		TagsJson->SetArrayField(TEXT("commonly_replicated_tags"), CommonTagsArray);
	}

	// Import tags from INI
	TagsJson->SetBoolField(TEXT("import_tags_from_config"), TagsSettings->ImportTagsFromConfig);
	TagsJson->SetBoolField(TEXT("warn_on_invalid_tags"), TagsSettings->WarnOnInvalidTags);
	TagsJson->SetBoolField(TEXT("fast_replication"), TagsSettings->FastReplication);

	return TagsJson;
}

TSharedPtr<FJsonObject> UProjectSettingsTool::GetDefaultMapsAndModes() const
{
	TSharedPtr<FJsonObject> MapsJson = MakeShareable(new FJsonObject);

	const UGameMapsSettings* MapsSettings = GetDefault<UGameMapsSettings>();
	if (!MapsSettings)
	{
		return MapsJson;
	}

	// Default maps
	MapsJson->SetStringField(TEXT("editor_startup_map"), MapsSettings->EditorStartupMap.GetLongPackageName());
	MapsJson->SetStringField(TEXT("game_default_map"), MapsSettings->GetGameDefaultMap());

	// Note: ServerDefaultMap, TransitionMap, GlobalDefaultGameMode, and GameInstanceClass are private in UE 5.7
	// Using public getters where available, omitting private fields without public accessors

	// ServerDefaultMap - no public getter available in UE 5.7
	// MapsJson->SetStringField(TEXT("server_default_map"), MapsSettings->ServerDefaultMap.GetLongPackageName());

	// TransitionMap - no public getter available in UE 5.7
	// MapsJson->SetStringField(TEXT("transition_map"), MapsSettings->TransitionMap.GetLongPackageName());

	// GlobalDefaultGameMode - Note: This is now FSoftClassPath in UE 5.7, not a pointer
	// if (MapsSettings->GlobalDefaultGameMode.IsValid())
	// {
	//     MapsJson->SetStringField(TEXT("global_default_game_mode"), MapsSettings->GlobalDefaultGameMode.ToString());
	// }

	// GameInstanceClass - Note: This is now FSoftClassPath in UE 5.7, not a pointer
	// if (MapsSettings->GameInstanceClass.IsValid())
	// {
	//     MapsJson->SetStringField(TEXT("game_instance_class"), MapsSettings->GameInstanceClass.ToString());
	// }

	// Local map options - private in UE 5.7
	// MapsJson->SetStringField(TEXT("local_map_options"), MapsSettings->LocalMapOptions);

	return MapsJson;
}
