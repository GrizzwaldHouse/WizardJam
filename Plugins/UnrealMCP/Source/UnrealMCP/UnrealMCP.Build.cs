// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealMCP : ModuleRules
{
    public UnrealMCP(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDefinitions.Add("UNREALMCP_EXPORTS=1");

        PublicIncludePaths.AddRange(
            new string[] {
				// ... add public include paths required here ...
			}
        );

        PrivateIncludePaths.AddRange(
            new string[] {
				// ... add other private include paths required here ...
			}
        );

        // Core runtime dependencies (always needed)
        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "InputCore",
                "HTTP",
                "Json",
                "JsonUtilities",
                "Slate",
                "SlateCore",
                "UMG",
                "Networking",
                "Sockets"
            }
        );

        // Private runtime dependencies
        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "DeveloperSettings",
                "PhysicsCore"
            }
        );

       
        if (Target.bBuildEditor == true)
        {
            PublicDependencyModuleNames.AddRange(
                new string[]
                {
                    "UnrealEd",     // Required for UEditorSubsystem (used in public headers)
                    "EditorSubsystem",
                    "Blutility",    // Editor Utility Widgets
					"UMGEditor"     // UMG editor support
				}
            );

            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "EditorScriptingUtilities",
                    "Kismet",
                    "KismetCompiler",
                    "BlueprintGraph",
                    "PropertyEditor",
                    "ToolMenus",
                    "LevelEditor",
                    "AssetRegistry",
                    "Projects"
                }
            );
        }

        DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {
				// ... add any modules that your module loads dynamically here ...
			}
        );
    }
}