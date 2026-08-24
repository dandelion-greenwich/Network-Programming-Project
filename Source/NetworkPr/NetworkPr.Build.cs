// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NetworkPr : ModuleRules
{
	public NetworkPr(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// HTTP and Json replaced Steam - we talk to the GameLift Lambdas instead of a session interface.
		// UMG is listed properly now that we build widgets from C++ rather than only subclassing them.
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "Niagara", "HTTP", "Json", "UMG", "Slate", "SlateCore"});

		// The GameLift Server SDK plugin restricts its modules to Server targets
		// ("TargetAllowList": ["Server"] in the .uplugin), so it can only be depended on there.
		// For every other target we define WITH_GAMELIFT ourselves, otherwise the plugin's own
		// definition never reaches us and the #if WITH_GAMELIFT guards in our code would be
		// compiling against an undefined macro.
		if (Target.Type == TargetType.Server)
		{
			PublicDependencyModuleNames.Add("GameLiftServerSDK");

			// The SDK headers are built with exceptions enabled and require the same of anything
			// that includes them. Scoped to the server so editor and client builds are unaffected.
			bEnableExceptions = true;
		}
		else
		{
			PublicDefinitions.Add("WITH_GAMELIFT=0");
		}
	}
}
