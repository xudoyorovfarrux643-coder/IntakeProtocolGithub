using UnrealBuildTool;

public class BlueprintReaderExporters : ModuleRules
{
	public BlueprintReaderExporters(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Json",
			"BlueprintReaderCore",
		});
	}
}
