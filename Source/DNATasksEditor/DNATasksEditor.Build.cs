// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class DNATasksEditor : ModuleRules
	{
        public DNATasksEditor(TargetInfo Target)
		{
            PrivateIncludePaths.AddRange(
                new string[] {
                    "Editor/GraphEditor/Private",
				    "Editor/Kismet/Private",
					"DNATasksEditor/Private",
                    "Developer/AssetTools/Private",
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
					"ClassViewer",
                    "DNATags",
					"DNATagsEditor",
					"DNATasks",
                    "InputCore",
                    "PropertyEditor",
					"Slate",
					"SlateCore",
                    "EditorStyle",
					"BlueprintGraph",
                    "Kismet",
					"KismetCompiler",
					"GraphEditor",
					"MainFrame",
					"UnrealEd",
                    "EditorWidgets",
					"WorkspaceMenuStructure",
					"ContentBrowser",
					"SourceControl",
				}
			);
		}
	}
}