using UnrealBuildTool;

public class RPGGraphEditor : ModuleRules
{
    public RPGGraphEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "GraphEditor",
                "BlueprintGraph",
                "Kismet",
                "KismetWidgets",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "UnrealEd",
                "KismetCompiler",
                "ApplicationCore",
                "PropertyEditor",
                "Projects",
                "ToolMenus",
                "InputCore",
            }
        );
    }
}
