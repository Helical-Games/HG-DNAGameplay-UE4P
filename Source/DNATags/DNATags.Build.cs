// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class DNATags : ModuleRules
	{
		public DNATags(TargetInfo Target)
		{
			PrivateIncludePaths.AddRange(
				new string[] {
					"DNATags/Private",
				}
				);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine"
				}
				);
		}
	}
}