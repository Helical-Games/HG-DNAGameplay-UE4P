// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class DNAAbilities : ModuleRules
	{
		public DNAAbilities(TargetInfo Target)
		{
			PrivateIncludePaths.Add("DNAAbilities/Private");
			
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"DNATags",
                    "DNATasks",
                }
				);

			if (UEBuildConfiguration.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.Add("UnrealEd");
				PrivateDependencyModuleNames.Add("Slate");
				PrivateDependencyModuleNames.Add("SequenceRecorder");
			}

			if (UEBuildConfiguration.bBuildDeveloperTools || (Target.Configuration != UnrealTargetConfiguration.Shipping && Target.Configuration != UnrealTargetConfiguration.Test))
			{
				PrivateDependencyModuleNames.Add("DNADebugger");
				Definitions.Add("WITH_GAMEPLAY_DEBUGGER=1");
			}
			else
			{
				Definitions.Add("WITH_GAMEPLAY_DEBUGGER=0");
			}
		}
	}
}