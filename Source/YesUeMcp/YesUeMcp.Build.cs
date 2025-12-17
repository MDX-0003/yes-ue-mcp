// Copyright softdaddy-o 2024. All Rights Reserved.

using UnrealBuildTool;

public class YesUeMcp : ModuleRules
{
	public YesUeMcp(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Json",
			"JsonUtilities"
		});
	}
}
