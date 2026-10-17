// YuriNK, 2026, All Right Reserved

using UnrealBuildTool;

public class CubeGridToolset : ModuleRules
{
	public CubeGridToolset(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.Add("Core");

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Engine",
			"Json",
			"ToolsetRegistry",
			"Projects",
			"UnrealEd",
			"InteractiveToolsFramework",
			"ModelingToolsEditorMode",
			"MeshModelingToolsExp",
			"MeshModelingTools",
			"ModelingComponents",
			"ModelingOperators",
			"GeometryCore",
			"DynamicMesh",
		});
	}
}
