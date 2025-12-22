// Copyright softdaddy-o 2024. All Rights Reserved.

#include "Tools/Project/ProjectInfoTool.h"
#include "Misc/Paths.h"
#include "Interfaces/IPluginManager.h"

FString UProjectInfoTool::GetToolDescription() const
{
	return TEXT("Get basic project and plugin information including project name, path, and plugin version.");
}

TMap<FString, FMcpSchemaProperty> UProjectInfoTool::GetInputSchema() const
{
	// No parameters needed
	return TMap<FString, FMcpSchemaProperty>();
}

TArray<FString> UProjectInfoTool::GetRequiredParams() const
{
	// No required parameters
	return TArray<FString>();
}

FMcpToolResult UProjectInfoTool::Execute(
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);

	// Project information
	FString ProjectName = FApp::GetProjectName();
	FString ProjectPath = FPaths::GetProjectFilePath();
	FString ProjectDir = FPaths::ProjectDir();

	Result->SetStringField(TEXT("project_name"), ProjectName);
	Result->SetStringField(TEXT("project_path"), ProjectPath);
	Result->SetStringField(TEXT("project_directory"), ProjectDir);

	// Engine information
	Result->SetStringField(TEXT("engine_version"), FEngineVersion::Current().ToString());

	// Plugin information
	TSharedPtr<FJsonObject> PluginInfo = MakeShareable(new FJsonObject);
	PluginInfo->SetStringField(TEXT("name"), TEXT("YesUeMcp"));
	PluginInfo->SetStringField(TEXT("version"), GetPluginVersion());
	Result->SetObjectField(TEXT("plugin"), PluginInfo);

	return FMcpToolResult::Json(Result);
}

FString UProjectInfoTool::GetPluginVersion() const
{
	// Try to get version from plugin manager
	IPluginManager& PluginManager = IPluginManager::Get();
	TSharedPtr<IPlugin> Plugin = PluginManager.FindPlugin(TEXT("YesUeMcp"));

	if (Plugin.IsValid())
	{
		const FPluginDescriptor& Descriptor = Plugin->GetDescriptor();
		return Descriptor.VersionName;
	}

	// Fallback version
	return TEXT("1.0.0");
}
