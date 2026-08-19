// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class NetworkPrServerTarget : TargetRules
{
    public NetworkPrServerTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Server;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_5;
        ExtraModuleNames.Add("NetworkPr");

        // The GameLift Server SDK drags in windows.h (via spdlog / websocketpp / asio) through its
        // public headers. windows.h turns UpdateResource, GetObject and friends into A/W macros,
        // which then mangle Unreal's own method names in any engine header parsed afterwards
        // (e.g. UTextureCube::UpdateResource -> UpdateResourceW, error C3668).
        //
        // Unity builds hide this, because some earlier file in the blob has already pulled the
        // engine headers in cleanly. Adaptive unity pulls "files you are working on" out of the
        // unity blob and compiles them standalone, which exposes it - and because the plugin is
        // untracked in git, UBT treats every one of its files as a working-set file.
        //
        // Disabling adaptive unity for the server target keeps the SDK in unity blobs and builds
        // cleanly. Remove this once the plugin's include order is fixed upstream.
        bUseAdaptiveUnityBuild = false;
    }
}
