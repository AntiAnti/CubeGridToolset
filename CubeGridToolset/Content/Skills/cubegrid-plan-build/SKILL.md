---
name: cubegrid-plan-build
description: Reconstruct a playable Unreal Engine level-architecture blockout from a top-down image, scanned plan, or ASCII floor plan using the CubeGrid MCP toolset. Use when the user asks to reproduce a floor plan or level layout in Unreal, including diagonal walls, irregular rooms, curves, doors, windows, and multi-height areas.
---

# Build architecture from a plan with CubeGrid

## Objective and scope

Turn the provided top-view plan into **real, editable Unreal Editor mesh actors** using `UCubeGridToolset`. Preserve the plan's **traversable topology, wall centerlines, actual openings, major proportions, and vertical transitions**, while consciously simplifying shapes that cannot be expressed efficiently on a rectilinear grid. The output is a navigable **blockout**, not production-ready topology or an idTech-3 BSP map.

**Read `references/CUBEGRID-MCP.md` and `references/PLAN-INTERPRETATION.md` before execution.** Read `references/BUILD-RECIPES.md` for actionable CubeGrid calls. The live `describe_toolset` schema is authoritative for callable methods, JSON spelling, and required arguments; editor documentation describes UI interactions that may not be exposed. In a new project, retrieve this skill and its required references using the plugin's `GetSkillCatalog` and `ReadSkill` tools.

Other Unreal MCP toolsets may inspect actors/assets, manipulate the viewport, save levels, and create assets. **Discover their actual callable names and signatures at runtime.** Never assume those abilities are methods of `UCubeGridToolset`.

## Workflow

### 1. Inspect scene, references and available tools

- Identify the active world/map and its units; inspect existing level geometry, actors, collision, and any user-imposed build bounds through other *verified* editor tools. Respect existing content: this skill does **not** authorize deleting or overwriting it.
- Read the image visually. If there is a second image or a textual label, cross-check it. Separate walls/structures from doors, furniture, symbols, dimensions, colored annotation, gameplay devices and drafting lines.
- If editing an existing scene, check whether the intended area is occupied. Record any conflicts before starting.
- Call `GetState()` to check whether CubeGrid already has an active session. Do not discard another session without authorization. `StartCubeGrid` creates a **new empty** mesh; it does not reopen the selection.

### 2. Extract a geometric plan, **before making tool calls**

Produce an internal, stable plan manifest:

- `source_frame`: image dimensions, normalized `(u,v)` in `[0,1]`, origin top-left, `u` right, `v` down; compass if present.
- `scale`: evidenced cm-per-plan length, or an explicit reasonable *blockout assumption* and confidence. One scale for the whole image; do not invent precise measured lengths.
- `wall_segments[]`: `id`, centerline endpoints `[u,v]`, thickness (cm or class), base-Z, wall height, confidence; split at junctions, true corners, thickness/height changes. A double wall line is **one** thick wall.
- `openings[]`: `id`, `wall_segment_id`, `center_t` fraction along segment, `width_cm` after scaling (or `width_t`, a fraction of segment length **explicitly named**), `bottom_cm`, `clear_height_cm`, `kind`, `traversable`, confidence. Multiple openings per wall are allowed. Glass is not an open door.
- `floors[]`: actual playable footprint regions and Z levels, including landings/bridges; `ceilings[]` only when needed for an enclosed blockout.
- `connections[]`: doors, passages, stairs, lifts, ramps and room adjacencies. Label independent height zones. Annotated gameplay objects become notes or actors with other tools, **not imaginary CubeGrid commands**.
- `spaces[]`: room/corridor names and semantic roles for reporting, **never the source of wall coordinates**.

Trace a genuine curved wall by its actual observed points or chords, preserving irregularity. Do not turn an asymmetric bay into a symmetric circle. Preserve consequential recesses and notches; merge only sub-grid noise and negligible drafting kinks. See `PLAN-INTERPRETATION.md`.

### 3. Fix the world frame and choose simplifications deliberately

Default world frame, unless project/user specifies otherwise: image right = world +X (east); image up = world +Y (north); world +Z = up. For plan physical width `W` cm, physical height `H = W × pixel_height/pixel_width`:

`world_X = origin_X + u*W`

`world_Y = origin_Y + (1-v)*H`

`world_Z = assigned_floor_level_cm`

For a CubeGrid session with cell size `g` and orientation `R`, local grid cells are `q = R^-1 * (world_position - GridOrigin) / g`; round only **after** selecting the final module origin, rotation and grid. `SelectRectangle` consumes **integer cell-boundary indices**, not centimeters.

Choose a single primary grid, commonly 25 or 50 cm for quick architecture, finer only where openings and wall thickness require it. Let `g` divide the intended thickness, doorway width and height where feasible. Standard dimensions (e.g. a ~100 cm door) are **provisional when not evidenced**, never asserted as measured.

Classify each non-orthogonal feature and freeze its chosen treatment in the manifest:

1. **Near-axis line**: snap to cardinal only if angle deviation and displacement at endpoints are small (illustrative initial checks: ≤10° *and* endpoint shift within half the chosen acceptable feature tolerance). Preserve actual intersections and door placement.
2. **Coherent diagonal wing or long corridor**: **preferred** method is a separate CubeGrid session with a rotated grid (`StartCubeGrid(..., Rotation=...)`); build its floor and walls axis-aligned in that *local* frame. Note joints with the main grid and ensure playable overlap/continuity. Rotation is per session, not per rectangle.
3. **Short oblique detail of low importance**: simplify to a short axial offset, a 45°-ish chamfer using rotated small modules, or an L/dogleg outline. Select the option that least changes connectivity, free passage width and silhouette. A staircase/Manhattan trace should not accidentally become a longer winding corridor.
4. **Arc/round wall**: approximate with a small number of straight chords, using short rotated-grid modules for visually important curvature or a coarse axial stepped silhouette for minor curves. Choose number of segments from error tolerance, not an arbitrary universal octagon; see `BUILD-RECIPES.md`.
5. **Slopes/ramp/roof**: consider `EnterCornerMode` only if the actual corner mapping can be confirmed from `CornerPositionsCells`; it is **not** a general arbitrary-polygon generator. Otherwise use stepped blockout or additional verified editor tools.

No accidental blocking of a doorway, no phantom walls from drafting decoration, no lost branching corridors. A right-angle simplification may change visual shape but **must preserve gameplay topology**.

### 4. Plan geometry as modules and an operation ledger

Before editing, produce a table with `module_id, grid_cm, world_origin, world_rotation, bounds, geometry, openings, materials, output actor label, validation`. Group by floor, corridor run, room shell, staircase and rotated wing. Prefer separable modules over one vast monolithic mesh; a rotated module inherently requires a separate CubeGrid session with this API.

For each module, calculate its **exact** rectangular face selections and extrusion depth in cells. A valid rectangle lies in a plane: the face-normal coordinate of Min/Max is equal; the two in-plane Min coordinates are less than Max. `MaxCorner` is exclusive along the spanning axes.

Prefer one footprint floor slab as the start of a coherent session, then pull vertical wall strips from real horizontal faces. Use horizontal face extrusion or a validated side-face subtraction to make openings; the door space must be empty **at player height**, not merely painted as a door. Reference call sequences in `BUILD-RECIPES.md`.

Use a ledger (`step, tool, parameters, expected bounds, expected state, status`) rather than relying on a lengthy verbal chain of arithmetic. If available, perform multi-feature coordinate computations in a script and inspect the generated integer cells. The included `scripts/grid_math.py` can convert normalized plan points to snapped session-local grid cells.

### 5. Execute one session at a time

1. `StartCubeGrid(GridSize, Origin, Rotation, OutputType=StaticMesh)` unless a different output is explicitly needed. Do not start a second session before confirming/canceling the first.
2. Select a valid planar rectangle and check its returned state. Build the floor/structural seed with `PullRectangle(Blocks)`.
3. Add wall runs/door jambs using their planned planar rectangles; detached solids are supported too. Wait until each asynchronous `PullRectangle`, `PushRectangle`, `EnterCornerMode`, `LeaveCornerMode` resolves; check `bBusy == false` before the next edit.
4. Make door/window gaps through the wall thickness and height, either by **side-face through-subtraction** (only when verified) or by constructing jambs, lintels and sills. Do not use `PushRectangle` on an unrelated top face and assume a doorway exists.
5. For material assignments, if requested or a material catalog is supplied, resolve actual `UMaterialInterface` object paths via editor tools. `ApplyMaterialToRectangle(..., bPaintExisting=false)` sets **future** extrusion material; `true` additionally paints matching existing coplanar outward faces. `SetMaterialUVScale` affects subsequent edits only. Do not invent material paths.
6. Use `GetState()` and world-bounds/triangle-count checks throughout. An unchanged `TriangleCount` may be fine, so also inspect actual resulting geometry by viewport/mesh tools wherever available. In corner mode, commit/revert with `LeaveCornerMode` **before** confirming.
7. `ConfirmMesh(ActorLabel=...)`; capture `CreatedActorPaths` and `CreatedAssetPaths`, then save the generated assets and world using **separately verified** editor-save tools. Modeling Mode may autosave output assets; check status rather than assuming either saved or dirty. Confirmation does not prove that the map and actor placements were saved. If a session cannot be recovered and undo is unavailable, `CancelCubeGrid()` removes only its *unconfirmed* work; reconstruct from ledger rather than guessing an undo command.

### 6. Verify **playability**, not just polygon counts

Using other verified editor tools when available, inspect orthographic top view and perspective view. For each original plan doorway check world coordinates, passable opening width, absence of blocking solid, useful headroom, floor continuity, and the intended destination. Check roof/ceiling clearance, elevation changes, handoff seams to rotated modules, geometry intersections, non-visible caps, collision, and material orientation. An overlap of meshes may hide a seam but can introduce z-fighting; inspect it, do not universally assume all overlaps are harmless.

Compare the built wall centerlines against the original segments. Flag simplifications explicitly: which diagonals were retained by rotated grids, which were stepped/snapped, and approximate maximum displacement. Do not claim pixel-level fidelity if scale or occluded geometry was inferred.

## Material catalog and saved placement

Consume a provided table by its actual schema: `Material` maps to the asset reference, `Target` or `WhereUsed` to use, and `Description` or `VisualDescription` to optional appearance cues. Existing `Target=walls/floor/ceiling` rows are sufficient. Do not create a replacement catalog or fabricate appearance notes. Set the material before generating each surface role, then verify assignments visually and through component inspection.

Await editor calls sequentially. Record actor transforms as well as labels/asset paths after each confirmed module. Explicitly save the generated assets and map; then refresh actor references, because `/Temp/Untitled` paths do not survive a new saved map. Avoid saving unrelated dirty assets. Use a top-down and perspective check; verify a walking route before claiming playability.

## Deliverables and report

Report the chosen plan scale, coordinate frame, grid size(s), module count, exact paths of created actors/assets, saved/unsaved state, preserved doors/connections, simplifications and deviations, and any unbuilt items or unresolved ambiguities. If tools for scene access or saving are missing, make the geometry plan and call ledger as a precise pending artifact; never report a mesh as created without a successful editor response.

## Critical pitfalls

- **CubeGrid is not Radiant BSP**: no caulk, leak sealing, per-brush indices, planar face winding or arbitrary polygon brushes are implied by the new API. Reuse *plan interpretation*, not BSP-specific mechanics.
- Never drive geometry from room bounding boxes when real wall segments are visible.
- Never let a snap/diagonal approximation close a traversable opening or alter the connectivity graph.
- Do not assume all UI hotkeys from Epic's CubeGrid docs have equivalent MCP calls.
- All new sessions are empty; `ConfirmMesh` cannot reopen and edit previously confirmed meshes.
- `CornerPositionsCells` is observational, not a guaranteed hardcoded corner-index map. Do not invent it.
