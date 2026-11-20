using UnrealBuildTool;

public class SkydropTarget : TargetRules
{
	public SkydropTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Skydrop");
	}
}
