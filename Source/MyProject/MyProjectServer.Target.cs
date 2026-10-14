// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class MyProjectServerTarget : TargetRules
{
	public MyProjectServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
		bUseUnityBuild = true;
		bUsePCHFiles = true;

		// Shipping server keeps UE_LOG: trade audit (LogMMOTradeAudit), [MMOMETRIC] lines and anti-cheat
		// warnings must not be compiled out. Console/debug commands stay off (Shipping default, ALLOW_CONSOLE=0).
		// Changing a logging default needs a unique build environment (source engine; Server targets need one anyway).
		BuildEnvironment = TargetBuildEnvironment.Unique;
		bUseLoggingInShipping = true;

		ExtraModuleNames.Add("MyProject");
	}
}