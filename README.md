# CubeGrid Toolset

MPC toolset for Unreal Engine 5.8+ to provide direct control over [Cube Grid blockout tool](https://dev.epicgames.com/documentation/unreal-engine/cubegrid-tool-in-unreal-engine?application_version=5.8&lang=en-US).

## How to setup

1. Install to (your project)/Plugins.

2. Update C++ project and build it.

3. Open your project in Unreal Editor, and in Edit -> Plugins enable "Model Context Protocol", "All Toolsets", "CubeGrid Toolset".

4. Turn on auto-start server in Edit -> Editor Settings -> Model Context Protocol and read UE [documentation](https://dev.epicgames.com/documentation/unreal-engine/unreal-mcp-in-unreal-editor?application_version=5.8&lang=en-US) for future instructions.

5. Launch your LLM client (Codex, Claude Code, OpenCode) from the project's directory.

## How to use

Best-use scenario is "concept-art to interior". Provide to LLM at least two views of the same area you need to build. Recommend it to use simple shapes (cylinder, sphere) instead of Cube Grid when they're more suitable. See demo: https://www.youtube.com/watch?v=_0FC-z8TCvs

You also can provide data table with pre-defined materials. Recommended structure:

{
- reference to material asset
- target (walls, floor, ceiling etc)
- description (color, style of the material)

}

Currently (October 2026) GPT models are far better in spatial tasks than Claude.

