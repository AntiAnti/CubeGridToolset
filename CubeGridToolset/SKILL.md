---
name: cubegrid-level-geometry
description: Route Unreal CubeGrid geometry tasks to the plugin's bundled plan, concept, and selected-architecture skills.
---

# CubeGrid skill routing

The maintained instructions are bundled in [Content/Skills](Content/Skills/README.md), so they travel with CubeGridToolset to other projects.

Before geometry generation, discover `CubeGridToolset.CubeGridToolset`, call `GetSkillCatalog`, and load the relevant `SKILL.md` plus its required references with `ReadSkill(skillName, relativePath)`:

- [cubegrid-plan-build](Content/Skills/cubegrid-plan-build/SKILL.md): floor plans and direct dimensioned blockouts. For simple geometry, focus on its MCP contract and build recipes.
- [cubegrid-concept-build](Content/Skills/cubegrid-concept-build/SKILL.md): reconstruction from concept views and supplied material tables.
- [ue-selection-describe](Content/Skills/ue-selection-describe/SKILL.md): inspection or explicitly requested cataloging of selected architecture.

Use the live schema's exact JSON names. Geometry uses integer grid-cell boundaries and centimeter cell sizes. Serialize Editor calls and await boolean/corner results. `StartCubeGrid` makes a new mesh, while `ConfirmMesh` creates output but is not proof of saved actor placement. Record transforms, material bindings, and output paths, save this build's assets and map, then refresh Actor paths.

The native implementation derives from `UToolsetDefinition`; `Content/Python/init_unreal.py` explicitly registers it. Build the project's Editor target with UE 5.8. New native declarations require an Editor restart after compilation. A tools refresh only re-polls registered providers.
