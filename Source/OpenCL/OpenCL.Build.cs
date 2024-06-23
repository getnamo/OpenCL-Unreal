// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
    public class OpenCL : ModuleRules
    {
        private string ModulePath
        {
            get { return ModuleDirectory; }
        }

        private string ThirdPartyPath
        {
            get { return Path.GetFullPath(Path.Combine(ModulePath, "../../ThirdParty/")); }
        }

        public OpenCL(ReadOnlyTargetRules Target) : base(Target)
        {
            //PCHUsage = PCHUsageMode.NoSharedPCHs;
            PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

            PublicIncludePaths.AddRange(new string[] {
                Path.Combine(ThirdPartyPath, "OpenCL", "Include")
            });

            PublicDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
                    "CoreUObject",
                    "Engine",
                    "RenderCore",
                    "RHI"
                }
                );

            string PlatformString = (Target.Platform == UnrealTargetPlatform.Win64) ? "Win64" : "Win32";
            string OpenCLLibrariesPath = Path.Combine(ThirdPartyPath, "OpenCL", "Lib");
            string NvidiaLibrariesPath = Path.Combine(OpenCLLibrariesPath, "NVIDIA", PlatformString);
            string IntelLibrariesPath = Path.Combine(OpenCLLibrariesPath, "Intel", PlatformString);
            string AmdLibrariesPath = Path.Combine(OpenCLLibrariesPath, "AMD", PlatformString);
            if (Target.Platform == UnrealTargetPlatform.Win64)
            {
                PublicAdditionalLibraries.Add(Path.Combine(NvidiaLibrariesPath, "OpenCL.lib"));
                PublicAdditionalLibraries.Add(Path.Combine(IntelLibrariesPath, "OpenCL.lib"));
                PublicAdditionalLibraries.Add(Path.Combine(AmdLibrariesPath, "OpenCL.lib"));
            }
            //Todo: add linux support
        }
    }
}
