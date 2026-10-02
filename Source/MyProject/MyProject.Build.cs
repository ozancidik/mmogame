// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MyProject : ModuleRules
{
	public MyProject(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"NetCore",
			// Sohbet yazi kutusu (SEditableTextBox) icin.
			"Slate",
			"SlateCore",
			// Oyun ici LLM (NPC diyalogu) icin: HTTP istegi + JSON kur/coz.
			"HTTP",
			"Json",
			"JsonUtilities"
		});
	}
}
