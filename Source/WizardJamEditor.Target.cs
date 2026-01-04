// WizardJamEditor.Target.cs
// Developer: Marcus Daley
// Date: December 23, 2025
// Editor target configuration for WizardJam

using UnrealBuildTool;

public class WizardJamEditorTarget : TargetRules
{
    public WizardJamEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V4;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_3;
        ExtraModuleNames.Add("WizardJam");
    }
}