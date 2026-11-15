// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DerinSular : ModuleRules
{
	public DerinSular(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"GameplayTags",
			"UMG",
			"Slate",
			"SlateCore",
			"Niagara"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"DerinSular",
			"DerinSular/Colony",
			"DerinSular/Variant_Platforming",
			"DerinSular/Variant_Platforming/Animation",
			"DerinSular/Variant_Combat",
			"DerinSular/Variant_Combat/AI",
			"DerinSular/Variant_Combat/Animation",
			"DerinSular/Variant_Combat/Gameplay",
			"DerinSular/Variant_Combat/Interfaces",
			"DerinSular/Variant_Combat/UI",
			"DerinSular/Variant_SideScrolling",
			"DerinSular/Variant_SideScrolling/AI",
			"DerinSular/Variant_SideScrolling/Gameplay",
			"DerinSular/Variant_SideScrolling/Interfaces",
			"DerinSular/Variant_SideScrolling/UI",
			"DerinSular/Trust"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
