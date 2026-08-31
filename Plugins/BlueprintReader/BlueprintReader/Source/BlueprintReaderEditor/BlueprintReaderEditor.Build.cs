using UnrealBuildTool;

public class BlueprintReaderEditor : ModuleRules
{
	public BlueprintReaderEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"BlueprintReaderCore",
			"BlueprintReaderExporters",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"ContentBrowser",
			"ContentBrowserData",
			"ToolMenus",
			"AssetRegistry",
			"UnrealEd",
			"EditorStyle",
			"ApplicationCore",
			"DesktopPlatform",
		});
	}
}
