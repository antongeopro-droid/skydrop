using UnrealBuildTool;

public class Skydrop : ModuleRules
{
	public Skydrop(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore", "AIModule", "ProceduralMeshComponent", "Slate", "SlateCore", "RenderCore"
		});
	}
}
