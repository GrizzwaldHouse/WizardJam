// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WizardJam : ModuleRules
{
    public WizardJam(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] {
            "NavigationSystem",
            "GameplayTasks",
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "EnhancedInput",
            "AnimGraphRuntime",
            "UMG",
            "AIModule",
        "SlateCore", 
            "Slate",      // For IGenericTeamAgentInterface
    "Niagara"
        });

        PrivateDependencyModuleNames.AddRange(new string[] { });

        // Disable warnings as errors for newer MSVC compilers
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PrivateDefinitions.Add("_SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS=1");
        }

        bEnableExceptions = true;
        bUseUnity = false;
    }
}