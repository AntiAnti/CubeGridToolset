# Copyright Epic Games, Inc. All Rights Reserved.

"""Register the native CubeGrid tools when the Editor's Python startup runs."""

import unreal

toolset_class = unreal.load_class(None, "/Script/CubeGridToolset.CubeGridToolset")
if toolset_class is None:
    raise RuntimeError("CubeGridToolset native class is not loaded. Build and enable the plugin.")
if not unreal.ToolsetRegistry.is_available():
    raise RuntimeError("ToolsetRegistry is not available during CubeGridToolset startup.")
if not unreal.ToolsetRegistry.is_toolset_class_registered(toolset_class):
    unreal.ToolsetRegistry.register_toolset_class(toolset_class)
if not unreal.ToolsetRegistry.is_toolset_class_registered(toolset_class):
    raise RuntimeError("CubeGridToolset registration failed. Check LogToolsetRegistry.")
unreal.log("[CubeGridToolset] Native MCP toolset registered.")
