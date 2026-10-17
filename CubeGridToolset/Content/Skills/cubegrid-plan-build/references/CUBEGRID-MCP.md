# CubeGrid MCP contract

Discover the live schema of `CubeGridToolset.CubeGridToolset` with `describe_toolset` before calls. Use `call_tool` with that `toolset_name`, the unprefixed `tool_name`, and an `arguments` object. This reference documents the geometry contract; the live schema is authoritative for wire types, required fields, and parameter spelling.

This is the **exact callable subset**, not a summary of all features in the interactive Unreal editor. The official CubeGrid editor documentation describes more interactions than this MCP exposes. Do not invent callable functions from editor hotkeys.

## Portable skill discovery

Before geometry generation, call `GetSkillCatalog` and load the relevant `SKILL.md` with `ReadSkill(skillName, relativePath)`. Read required references through the same tool; paths are relative to the selected skill, for example `references/BUILD-RECIPES.md`. The plugin resolves its own Content directory in either a project or Engine installation; no absolute installation path is needed.

Use `cubegrid-plan-build` for floor plans and direct dimensioned geometry, `cubegrid-concept-build` for concept views, and `ue-selection-describe` only for selected-object inspection/cataloging. Reading a Python helper through `ReadSkill` does not execute it.

## Functions available through `UCubeGridToolset`

| Call | Input | Key semantics |
| --- | --- | --- |
| `StartCubeGrid` | `gridSize` (cm, 0.001..1,000,000), `Origin` (world cm), `Rotation` (world degrees), `OutputType`: `StaticMesh \| DynamicMesh \| Volume` | Starts a **new, empty** session; explicitly does **not** modify a selected mesh. One grid orientation per session. |
| `GetState` | none | Snapshot: active/busy, current grid and selection, corner mode, mesh bounds/triangle count, UV, material, generated asset paths. |
| `SetGridSize` | `GridSize` (cm) | Clears rectangle; **does not move existing geometry**; unavailable during corner mode. |
| `SelectRectangle` | `MinCorner`, `MaxCorner` in **integer grid-cell boundary coordinates**, `Face` | Selection must be a plane: on the face-normal axis, min=max; on the other two axes, min<max. Max is **exclusive** on those two axes. `Face`: `PositiveX`, `NegativeX`, `PositiveY`, `NegativeY`, `PositiveZ`, `NegativeZ`. |
| `PullRectangle` | `Blocks` positive integer | Adds solid **outward** from selected face. Asynchronous: await completed value before next operation. |
| `PushRectangle` | `Blocks` positive integer | Subtracts solid **inward** from selected face. Asynchronous: await completed value before next operation. |
| `SetMaterialUVScale` | `Scale` positive finite, `bWorldSpace` | Applies to **later** pull/push operations only; does not retroactively rewrite UVs. |
| `ApplyMaterialToRectangle` | `MaterialPath`, `bPaintExisting` | Paints matching outward-facing **coplanar** triangles inside rectangle; splits triangles at edges. Also sets the next extrusion's material. `false` only changes the next extrusion material (may be used on an empty selection plane); `true` requests painting. |
| `EnterCornerMode` | `Corners` (indices 0..3), `bCrosswiseDiagonal` | Requires rectangle. Corner order is *not provided in the header*: it references an external `SKILL.md`. Inspect `CornerPositionsCells` and test; never fabricate an index→world mapping. Asynchronous. |
| `LeaveCornerMode` | `bApply` | Commits or discards **corner preview**, asynchronously. Finish before confirmation. |
| `ConfirmMesh` | `ActorLabel` | Rejects empty mesh or unresolved corner preview. Returns `CreatedActorPaths` and `CreatedAssetPaths`. Output follows Modeling Mode's asset generation/autosave settings. Check save status and save this build's packages separately; confirmation is not proof the map was saved. |
| `CancelCubeGrid` | none | Discards only the current unconfirmed session; cannot delete previously confirmed meshes. |

`FCubeGridState` includes `bActive`, `bBusy`, `bCornerMode`, `bHasSelection`, `GridSize`, `GridOrigin`, `GridRotation`, `SelectionMinCells`, `SelectionMaxCells`, `Face`, `SelectedCorners`, `CornerPositionsCells`, `TriangleCount`, `MeshBoundsMin/Max`, `UVScale`, `bWorldSpaceUVScale`, `MaterialPath`, `CreatedActorPaths`, `CreatedAssetPaths`, `PaintedTriangles`.

## MCP JSON spelling and limits

The function table uses readable C++ names. Actual MCP arguments use `gridSize`, `origin`, `rotation`, `outputType`, `minCorner`, `maxCorner`, `face`, `blocks`, `scale`, `bWorldSpace`, `materialPath`, `bPaintExisting`, `corners`, `bCrosswiseDiagonal`, `bApply`, and `actorLabel`. Supply `origin={x,y,z}` and `rotation={pitch,yaw,roll}` explicitly: the observed schema requires them even though C++ declares defaults.

Rectangle boundaries must be integer cells within +/-1,000,000; depths are integers 1..1,000,000. Positive finite UV multipliers are required. State fields use the live schema's casing, e.g. `gridSize`, `selectionMinCells`, `triangleCount`, `createdActorPaths`, `paintedTriangles`; the current UV field is `uVScale`. Do not pass C++ capitalization blindly in JSON.

```json
{"toolset_name":"CubeGridToolset.CubeGridToolset","tool_name":"StartCubeGrid","arguments":{"gridSize":50,"origin":{"x":0,"y":0,"z":0},"rotation":{"pitch":0,"yaw":0,"roll":0},"outputType":"StaticMesh"}}
```

## Coordinate conversions and orientation

Use a single level frame: Unreal +X = east, +Y = north, +Z = up (unless the user supplies a different world frame). World space and grid size are in **centimeters**. Rectangle corners are **cell boundary indices**, never pixels and never centimeters.

For an **unrotated** grid:

`world_position_cm = GridOrigin_cm + GridSize_cm * (i,j,k)`.

For a **rotated** grid:

`world_position_cm = GridOrigin_cm + R(GridRotation) * (GridSize_cm * (i,j,k))`.

Always generate rectangle coordinates in the session's **local** grid frame, especially for rotated sessions. `Face` is relative to that local grid frame. An upright wall can be placed in a rotated session, but you cannot apply different yaw angles to individual rectangles of the same session through this API.

Examples on a 50-cm, unrotated grid:

- Floor covering 4m by 3m, thickness 50cm: `SelectRectangle((0,0,0),(8,6,0), PositiveZ)` → `PullRectangle(1)`; nominal Z range 0..50cm.
- Starting from a face at local `z=1`, `SelectRectangle((0,0,1),(8,1,1), PositiveZ)` → `PullRectangle(5)` extrudes a 4m long, 50cm thick wall 250cm vertically. The plugin accepts any valid planar grid rectangle; it need not coincide with an existing surface. Pull can seed an empty session or add detached solids. Check the returned geometry rather than imposing a surface-hit requirement.
- To construct a true through-door, either subtract on the wall's existing planar **side face** using correctly oriented `Face` and sufficient depth, or assemble jambs/lintel from supported attached surfaces / separate meshes. A `PushRectangle` on a top horizontal face is **not** automatically a door.

## Safe execution loop

1. Call `GetState`. If a session is active, do not silently cancel someone else's work. Resolve ownership or continue only when it is clearly this build's session.
2. `StartCubeGrid` with explicit `GridSize`, `Origin`, `Rotation`, `OutputType`.
3. Set a valid selection; check `bHasSelection` and selected rectangle/face in `GetState`.
4. Await all async push/pull/corner operations. Unreal executes tools on the game thread: serialize even independent editor calls; do not parallelize MCP edits. Never chain a second edit while `bBusy` is true. An operation can take up to 60 seconds; after an error or timeout inspect state before retrying, because an edit may already have been applied. Inspect selection bounds after extrusion: the selected plane normally slides with the operation.
5. Compare `TriangleCount` and `MeshBoundsMin/Max` before/after; for subtraction confirm some relevant state changed. **Bounds and triangle counts do not prove a doorway is passable or geometry watertight.** Use visual/mesh inspection tools, if available, to validate topology.
6. Make corner previews reversible; use `LeaveCornerMode(false)` if wrong. If a non-preview operation corrupts the unconfirmed session and no genuine undo MCP exists, cancel and rebuild the session from the operation ledger. Do not invent a CubeGrid undo API.
7. `ConfirmMesh` only when the entire coherent module is correct. Capture exact Actor/Asset paths and actor transforms immediately in the ledger. Save those assets and the owning map with verified editor-save tools; use a new/versioned map when appropriate. Paths under `/Temp/Untitled...` are transient and change on save. Reread current actor paths after saving. No save operation exists in the CubeGrid geometry API.

## Capabilities that are **not** in this header

- Inspecting the editor's selection, actors, components, world hierarchy, material slot values, level transforms, or collision.
- Looking up/creating material assets, struct assets, DataTables, DataAssets, or their rows/properties.
- Deleting confirmed actors, changing transforms of confirmed meshes, reopening an existing mesh in this MCP session, saving packages/levels, screenshots/viewport control, raycasts, arbitrary polygons, per-face custom UV transforms, vertex painting, or arbitrary mesh pivot setting.

Such work **may** be possible via other advertised Unreal MCP toolsets, Unreal Editor scripting or user actions. First discover the actual tools and their parameters; use only those present. If unavailable, produce an explicit unexecuted action/specification rather than claiming the editor was changed.

## Material behavior

- Use valid Unreal material/material-instance **object paths**; display names and invented package paths are not enough.
- `SetMaterialUVScale` is for later operations. Choose a world-space texel density convention before extrusion. `ApplyMaterialToRectangle` can paint coplanar surfaces; it is not a generic "assign all material slots of any Actor" operation.
- Inspect the supplied DataTable schema and rows first. Map `Material` to the material object path; `Target` or `WhereUsed` to the surface role; `Description` or `VisualDescription` to optional appearance notes. Blank notes do not invalidate a material row. Preserve the table and its rows; do not require a new schema just to build geometry.
- `PaintedTriangles` is a diagnostic, not proof of color/appearance. Material parameter colors should be checked on the actual material instance if other tools expose them.

## Confirmation and Editor verification

Record generated actor/asset paths, labels, transforms, module origin/rotation, operations, and material row bindings after every successful module. A stored mesh asset without a saved map does not preserve actor placement. Inspect material slots and visible surfaces separately: having blue and grey slots alone does not prove the intended faces use them.

Place floors clear of existing coplanar landscape surfaces; visual stripes can be z-fighting, not CubeGrid topology failure. Check rotated segment seams and doorway alignment. Keep ceilings in separately named actors/folders and record whether they are temporarily hidden, persistently hidden, or enabled at runtime. Temporary Editor hiding may not survive reload.

When building gameplay blockouts, verify collision, player step height, ramp slope, doorway clearance, and floor continuity through available editor tools or a manual playtest. CubeGrid grid size is not an appropriate universal stair riser. Report lifts/forcefields/destructible supports as static proxies unless gameplay behavior was actually implemented.

## Source links

- User-supplied `CubeGridToolset.h` is authoritative for callable functions and fields.
- Epic UE 5.8 CubeGrid: https://dev.epicgames.com/documentation/unreal-engine/cubegrid-tool-in-unreal-engine?application_version=5.8&lang=en-US
