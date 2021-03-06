// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class DNATasks : ModuleRules
	{
        public DNATasks(TargetInfo Target)
		{
            PrivateIncludePaths.Add("DNATasks/Private");
            
			PublicIncludePaths.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					"DNATags",
				}
				);

			PrivateIncludePaths.AddRange(
				new string[] {
					// ... add other private include paths required here ...
				}
				);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
                    "DNATags",
					// ... add other public dependencies that you statically link with here ...
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					// ... add private dependencies that you statically link with here ...
				}
				);

			DynamicallyLoadedModuleNames.AddRange(
				new string[]
				{
					// ... add any modules that your module loads dynamically here ...
				}
				);

			if (UEBuildConfiguration.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.Add("UnrealEd");
				CircularlyReferencedDependentModules.Add("UnrealEd");
                PrivateDependencyModuleNames.Add("DNATagsEditor");
			}
		}
	}
}