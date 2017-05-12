// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class DNATagsEditor : ModuleRules
	{
		public DNATagsEditor(TargetInfo Target)
		{
			PrivateIncludePaths.AddRange(
				new string[] {
					"DNATagsEditor/Private",
                    "Developer/AssetTools/Private",
				}
			);

            PublicIncludePathModuleNames.AddRange(
                new string[] {
                    "AssetTools",
                    "AssetRegistry",
			    }
            );


			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					// ... add private dependencies that you statically link with here ...
					"Core",
					"CoreUObject",
					"Engine",
					"AssetTools",
                    "AssetRegistry",
					"DNATags",
                    "InputCore",
					"Slate",
					"SlateCore",
                    "EditorStyle",
					"BlueprintGraph",
					"KismetCompiler",
					"GraphEditor",
					"ContentBrowser",
					"MainFrame",
					"UnrealEd",
                    "SourceControl",
				}
			);

            PrivateIncludePathModuleNames.AddRange(
                new string[] {
				    "Settings"
			    }
            );
		}
	}
}
