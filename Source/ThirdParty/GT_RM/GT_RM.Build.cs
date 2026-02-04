// Fill out your copyright notice in the Description page of Project Settings.

using System.IO;
using UnrealBuildTool;

public class GT_RM : ModuleRules
{
	public GT_RM(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string GT_esminiDir = Path.Combine(ModuleDirectory, "modules", "esmini", "GT_esmini");

		PublicIncludePaths.AddRange(
			new string[] {
				GT_esminiDir,
			}
		);

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "lib", "GT_esminiLib.lib"));
			PublicDelayLoadDLLs.Add("GT_esminiLib.dll");
			RuntimeDependencies.Add(Path.Combine(ModuleDirectory, "bin", "GT_esminiLib.dll"));
		}
	}
}
