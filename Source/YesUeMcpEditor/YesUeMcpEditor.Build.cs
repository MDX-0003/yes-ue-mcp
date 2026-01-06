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
			"HTTPServer",

			// Editor Framework
			"UnrealEd",
			"EditorSubsystem",
			"ToolMenus",
			"Slate",
			"SlateCore",
			"StatusBar",

			// Blueprint/Kismet
			"Kismet",
			"KismetCompiler",
			"BlueprintGraph",

			// Animation Blueprint
			"AnimGraph",

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
			"PropertyPath",

			// Animation/Sequencer (for UMG animations)
			"MovieScene",

			// JSON
			"Json",
			"JsonUtilities",

			// Input
			"InputCore",

			// Project Settings
			"GameplayTags",
			"EngineSettings",
			"Projects",

			// StateTree
			"StateTreeModule",
			"StateTreeEditorModule"
		});
	}
}
