// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CustomRender : ModuleRules
{
	public CustomRender(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				"CustomRender/Public"
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				"CustomRender/Private",
                "LevelSequenceEditor/Private",
				// ... add other private include paths required here ...
			}
            );
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
                "Core", "CoreUObject", "Engine", "InputCore", "UnrealEd"
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
            {
                "Engine",
                "UnrealEd",
                "CoreUObject",
                "InputCore",
                "Slate",
                "SlateCore",
                "Projects",
                "LevelEditor",
                "PropertyEditor",
                "Sequencer",
                "LevelSequence",
                "LevelSequenceEditor",
                "Sequencer",
                "MovieScene",
                "MovieSceneTools",
                "MovieSceneTracks",
                "CinematicCamera",
                "AssetRegistry"
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
                "AssetTools"
            }
			);
	}
}
