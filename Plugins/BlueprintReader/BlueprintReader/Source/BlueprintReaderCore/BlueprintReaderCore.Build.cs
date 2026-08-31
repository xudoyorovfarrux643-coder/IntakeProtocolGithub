using UnrealBuildTool;

public class BlueprintReaderCore : ModuleRules
{
	public BlueprintReaderCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"BlueprintGraph",
			"Kismet",
			"KismetCompiler",
			"UnrealEd",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
		});
	}
}
