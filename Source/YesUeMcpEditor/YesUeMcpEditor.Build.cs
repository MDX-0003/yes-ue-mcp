// Copyright softdaddy-o 2024. All Rights Reserved.

using UnrealBuildTool;

public class YesUeMcpEditor : ModuleRules
{
	public YesUeMcpEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"YesUeMcp"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// HTTP Server
			"HTTP",
			"HttpServer",

			// Editor Framework
			"UnrealEd",
			"EditorSubsystem",
			"ToolMenus",
			"Slate",
			"SlateCore",

			// Blueprint/Kismet
			"Kismet",
			"KismetCompiler",
			"BlueprintGraph",

			// Asset Management
			"AssetTools",
			"AssetRegistry",
			"ContentBrowser",

			// Level Editing
			"LevelEditor",
			"EditorScriptingUtilities",

			// UI/Widgets
			"UMG",
			"UMGEditor",
			"PropertyEditor",

			// JSON
			"Json",
			"JsonUtilities",

			// Input
			"InputCore"
		});
	}
}
