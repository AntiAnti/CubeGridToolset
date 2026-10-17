---
name: cubegrid-concept-build
description: Reconstruct and prototype Unreal Engine level architecture from one or more concept-art images labeled with viewpoint direction (top, east, southwest, etc.) and a user-provided DataTable describing materials, their placement and visual features; optionally use a 2D image or ASCII plan. Build via CubeGrid MCP, interpreting multi-view geometry, estimating hidden structure conservatively, and simplifying diagonal/curved features without changing walkable connectivity.
---

# Build a multi-view concept-art blockout with CubeGrid

## Required inputs and sources

A user supplies:

1. **A real Unreal `DataTable` asset for materials** with a `Material` reference and a surface-role field such as `WhereUsed` or `Target`. Appearance notes such as `VisualDescription` or `Description` are optional and may be blank. Discover and map the real schema, including localized/renamed columns; never require a replacement table merely because its names differ.
2. **One or more labeled concept images**, each with an explicit direction: top, from east, from southwest, and so on. "From east" means the camera is **east of the level, looking west**. Image frame and perspective do not directly equal XY coordinates.
3. **Optional** top-view plan, either an image or ASCII diagram. If present, treat it as strong evidence of layout and topology, not as an aesthetically optional reference.

**Read `references/CUBEGRID-MCP.md`, `references/CONCEPT-RECONSTRUCTION.md`, and `references/BUILD-RECIPES.md` before execution.** Read `references/ASSET-SCHEMA.md` for catalog row interpretation and optional object-description persistence.

In a new project, retrieve this skill and its references through `CubeGridToolset.CubeGridToolset.GetSkillCatalog` and `ReadSkill`; discover the live geometry schema before editing.

The goal is a coherent, traversable **architectural blockout** using actual CubeGrid edits, with source-driven material placements and a record of assumptions. It is not exact 3D photogrammetry and should not claim to recover occluded dimensions from a single illustration.

## Workflow

### 1. Read the actual material catalog and inspect the target world

Discover the project's working **asset-inspection, table-row retrieval, selection, world-query, viewport and save** tools; their actual names and payloads are defined elsewhere, not in `CubeGridToolset.h`. Locate the user-provided DataTable by exact asset path/selection, read its row `Struct`, columns and **every needed row**. Resolve the materials to valid Unreal object paths; classify `WhereUsed` as instructions for surfaces/regions, and retain the appearance cues in `VisualDescription`. Do not replace this table or create a new one merely because a row is inconvenient.

If a referenced material is missing, record the unresolved row; request a substitute only if necessary or use an explicitly declared neutral blockout fallback without mislabeling it as the requested material. If the table cannot be accessed, do not fabricate its rows; make the layout-only blockout if that is still feasible and identify the materials as pending.

Read selected world/map transform, scene bounds, existing actors and CubeGrid `GetState`. Do not wipe an active user session or overwrite placed meshes. `StartCubeGrid` always starts a new empty mesh.

### 2. Register every concept as a camera observation

Create a view registry: `view_id`, image source, camera **position direction** (`top`, `east`, `southwest`), approximate elevation/roll/FOV or `unknown`, image-frame landmarks, confidence. Set a single world frame, by default +X east, +Y north, +Z up. For cardinal and diagonal screen-right conventions, see `CONCEPT-RECONSTRUCTION.md`.

Identify the **same real features across views** (airlock, tower, corner column, central window, stairs, door, skylight, catwalk). Build a correspondence table of feature IDs and their visible appearance/occlusion in each image. A colorful reflection is not a wall; a perspective-converging line is not necessarily an oblique wall in the world's horizontal plane. Use cross-view consistency rather than extrapolating a single screen image into floor dimensions.

Prioritize evidence:

- Explicit numeric dimensions / scale and named orientation > clearly measured references.
- A provided top-down plan sets the **XY wall and opening arrangement** where not contradictory; concept side views determine elevations, facade and material details.
- Views from different sides constrain silhouette, height, recesses, placement of doors/windows, exposed/hidden walls.
- Repeated motifs inform spacing only if an actual repeat can be observed.
- Hidden backs/interiors are **unknown** unless shown or specified. Favor minimal, plausible geometry and report hypotheses.

### 3. Solve a consistent architecture model before editing

Build a model in a common world frame with:

- XY plan: explicit wall centerline segments with joints, thickness, real door/window intervals, playable floor footprints and a **connectivity graph**. If a plan is supplied, parse its *walls and openings* as authoritative instead of redrawing from an inferred room bounding box.
- Floor Z levels, roofline and ceiling profile, parapets, columns/arches, mezzanines, ramps/stairs, bridges and key structure; differentiate visible versus inferred extents.
- Architectural modules (room, corridor, wing, platform, facade, tower) with per-module grid orientation, cell size and intended output actor label.
- Materials: map `DataTable.RowName → surface_role → module/surface → resolved MaterialPath`, with any visual-description constraints (light-gray concrete, dark blue metal, warm-lit glazing, etc.). **Do not alter the material parameters** because prose says a color; use an existing suitable instance or report the mismatch.
- Every inferred dimension and obscured opening has a confidence/provenance label. Choose a consistent *blockout scale* from known references; if scale is absent, declare a provisional one rather than making false measurements.

If no top plan is given, synthesize the **minimum XY graph** compatible with all views. Do not invent an elaborate unseen maze. Use an ASCII sketch as a diagnostic only; its characters are not world coordinates. From one oblique view, depth remains ambiguous: use conservative thickness and state the approximation.

### 4. Simplify the shape under CubeGrid's constraints

CubeGrid selects grid-aligned rectangles, extrudes them, and supports a **session-level rotated grid**. Choose in order:

1. Preserve orthogonal X/Y walls as contiguous rectangles on the main grid.
2. Represent a major angled facade/wing with its **own rotated session** (`StartCubeGrid` `Rotation`, then local-cell selections); maintain explicit world-space joints and doorway alignment.
3. Turn minor irregular rooflines/recesses into a small set of stepped profiles or validated corner extrusions. For curved/diagonal silhouettes, use a bounded-error chord/step simplification that retains recognizable form and usable clearances.
4. Do not recreate decorative details as architecture when the user asked for a prototype. Favor floors, enclosure, door/window holes, level transitions and distinctive silhouette first.

Keep real angular relationships where gameplay or silhouette depends on them. Do not automatically Manhattanize every non-90° wall just because CubeGrid uses cubes: rotating the **grid per mesh** is explicitly available. Never assume a single session can rotate each selected rectangle independently. Read `BUILD-RECIPES.md` for doorway, lintel, floor and arc construction.

### 5. Create a geometry/material execution ledger

Before MCP edits write the modules and explicit planned selections/extrusions in **integer grid-cell boundaries**, face normal and block count, with world-space bounds, expected geometry changes and material role. A tool-assisted numeric computation is preferable to dozens of mental coordinate conversions. Trace each important opening as real empty volume on the face of a wall; a colored rectangular mark alone does not count as a window/door.

Build in dependency order: **main traversable floors/landings → primary wall envelope → corridors, openings and connections → higher floors/catwalks → silhouette-defining vertical elements → material and detail pass**. For complex multi-view scenes, verify a rough top view before spending time on facade details. Where meshes span different rotations, check world-space continuity.

### 6. Execute via the actual CubeGrid MCP API

For **each** module: `GetState` → `StartCubeGrid(GridSize, Origin, Rotation, OutputType=StaticMesh)` → supported `SelectRectangle`, `PullRectangle`, `PushRectangle`, material calls, optional corner mode → asynchronous state verification → `ConfirmMesh(ActorLabel)`; then save assets/world using verified external editor tools. Do not chain commands during `bBusy`; do not confirm unresolved corner previews. Record `CreatedActorPaths`/`CreatedAssetPaths` exactly as returned.

Materials may be set for future extrusions with `ApplyMaterialToRectangle(MaterialPath, bPaintExisting=false)` or applied to existing **coplanar matching outward faces** with `bPaintExisting=true`. `SetMaterialUVScale` affects new geometry only. Set materials by catalog **surface role**, not blindly one material per room. If another tool can reassign material slots on a confirmed actor more reliably, it may be used after discovering its real signature; distinguish that external action from CubeGrid's own methods.

If the table calls for a glass surface, differentiate between **a real mesh pane**, an **empty architectural opening**, and a **walkable door gap**. Build only what tools and design require; material transparency alone is not collision or geometry semantics.

### 7. Validate the geometry against each labeled view

Check a top-down viewport against the synthesized plan or supplied plan; then inspect from each **supplied viewpoint**. Match landmark positions, relative heights, facade rhythm, opening location, major recesses, visible silhouette and which side uses each material. Cross-check walking routes, stair/lift connections, minimum clearances, floor continuity, collision and hidden gaps. Never use triangle count or world AABB as sole proof of a successful through-opening.

When a view reveals a contradiction, update the model and **smallest affected module**, preserving already validated neighbors. Keep an error/uncertainty register rather than silently changing the whole level to match whichever view was looked at last.

Optionally invoke the workflow of `ue-selection-describe` to persist the successfully built objects as DataAssets linked to the user's existing material DataTable. Do not claim DataAssets were created unless those editor actions were executed and verified.

## Material catalog and saved placement

Consume a provided table by its actual schema: `Material` maps to the asset reference, `Target` or `WhereUsed` to use, and `Description` or `VisualDescription` to optional appearance cues. Existing `Target=walls/floor/ceiling` rows are sufficient. Do not create a replacement catalog or fabricate appearance notes. Set the material before generating each surface role, then verify assignments visually and through component inspection.

Await editor calls sequentially. Record actor transforms as well as labels/asset paths after each confirmed module. Explicitly save the generated assets and map; then refresh actor references, because `/Temp/Untitled` paths do not survive a new saved map. Avoid saving unrelated dirty assets. Use a top-down and perspective check; verify a walking route before claiming playability.

## Completion report

Report created actor/asset paths and save status, provided views and world orientation, table path and resolved/unresolved material rows, overall scale assumptions, modules, openings and multi-level relationships, main compromises for curves/diagonals, geometry not visible in references that was inferred, and **which views were actually checked**. A partially implemented blockout with explicit gaps is preferable to a confidently described but nonexistent finished level.

## Prohibitions

- No arbitrary interpretation of "view from east" as an east-facing facade: the camera is on the **east** side, looking **west**.
- No guessing physical dimensions from screen-space widths in perspective images without cross-view/calibrating evidence.
- No fabricating hidden rooms or a material palette when a DataTable was required/provided.
- No treating unsupported UI CubeGrid hotkeys as MCP calls.
- No smoothing distinctive asymmetric construction into convenient symmetric architecture.
