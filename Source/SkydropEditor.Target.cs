using UnrealBuildTool;

public class SkydropEditorTarget : TargetRules
{
	public SkydropEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Skydrop");
	}
}
