# CubeGrid blockout recipes and approximation decisions

Read `CUBEGRID-MCP.md` first. All numeric examples below are **illustrative plans**, not tool results. Each async `PullRectangle` / `PushRectangle` / corner call must complete before proceeding. `GetState` does not replace viewport inspection.

The call notation below uses readable C++ names. Translate to the live schema's MCP JSON keys (`gridSize`, `minCorner`, `blocks`, etc.), as documented in `CUBEGRID-MCP.md`.

## A. Floor plus straight wall

New session: `StartCubeGrid(GridSize=50, Origin=(0,0,0), Rotation=(0,0,0), OutputType=StaticMesh)`.

1. `SelectRectangle(MinCorner=(0,0,0), MaxCorner=(8,6,0), Face=PositiveZ)`; `PullRectangle(Blocks=1)`. Intended floor slab spans local X 0..8, Y 0..6, Z 0..1 in **cells**, i.e. 4 m × 3 m × 50 cm.
2. `SelectRectangle((0,0,1),(8,1,1),PositiveZ)`; `PullRectangle(5)`. Intended wall starts at the floor's top face and extends to Z=6, for an overall Z 50..300 cm wall region.
3. Inspect `MeshBoundsMin/Max`, topology and placement. On an output actor, actual collision must be checked with other editor tools.

`SelectRectangle` defines a grid plane without raycasting or requiring a surface hit. A pull can create the initial slab, attached walls, detached lintels, and separate islands within one mesh session. Verify the result after each operation. This is the plugin's verified behavior, not an inference from UI hotkeys.

## B. Door in wall (two supported strategies)

Wall spans X=0..8, Y=0..1, Z=1..6. A door spans X=3..5, Z=1..5, width 100 cm, clear height 200 cm. The **floor still exists** below the door, from Z=0..1.

**Strategy B1, cut from side face** (convenient, verify first):

- On outward side Y=1, `SelectRectangle((3,1,1),(5,1,5),PositiveY)`. `PushRectangle(Blocks=1)` cuts *inward* through a wall one cell thick. Verify the opposite side is actually open and the floor top remains intact.
- A top-plane push subtracts a vertical prism. It can cut an opening when the selected XY footprint is exactly the wall thickness and the start/end Z are the intended opening limits; it is not automatically a door. Side-plane subtraction makes the intended normal/depth clearer for ordinary doors.
- For thicker walls, depth must be sufficient to reach the far face without destroying unrelated architecture. Validate on a small wall first if engine semantics are uncertain.

**Strategy B2, pre-segment jambs and bridge a lintel** (predictable if cutting is problematic):

1. From floor Z=1, pull full-height wall strips X=0..3 and X=5..8, both Y=0..1, up 5 cells.
2. The left jamb's end face at X=3 has face normal +X. Select `((3,0,5),(3,1,6),PositiveX)` and `PullRectangle(2)` to bridge over the door only at Z=5..6, where the lintel belongs. Verify geometry is added along +X, not along Z.
3. If lintel extrusion cannot connect as expected, keep it as a separate supported module with a confirmed seam; do not pretend it was created.

**Window**: either side-face-subtract only a Z interval above a solid sill, or make two full-height jambs, a bottom sill spanning the opening's `bottom_cm..sill_top`, and a top lintel. `traversable=false` even if the geometry is not filled with glass yet; add glass as a separate mesh using verified tools when needed.

Do not assume an opening is passable just because the mesh acquired additional triangles or changed bounds.

## C. Non-orthogonal architecture: choose the least destructive representation

| Source geometry | Preferred blockout | Requirement |
| --- | --- | --- |
| Almost straight X/Y wall; small drawing jitter | Snap to nearest axis | Endpoints and door centers stay within selected tolerance |
| Long diagonal hall / 30° wing | Separate session with `Rotation.Yaw≈30°`, local boxes | Preserve true path, width and connections; use rotated local cells |
| Two perpendicular wings joined at an oblique corner | Two separately oriented modules with a landing/transition | No gaps/blocked joints; no coplanar z-fighting |
| Irregular arc important to silhouette | Trace source into a few chords; one rotated-grid module per distinct chord direction where justified | Bound maximum chord/sagitta error |
| Small curved alcove of low gameplay impact | Coarse stepped grid silhouette | Do not eat into walking clearance / place walls through a door |
| Tiny 8° wall kink | Flatten if endpoint displacement is negligible | Record simplification, no new corner inventing geometry |
| Ramp / sloped roof | Validated corner mode on a prepared rectangle, or a stepped placeholder | Do not assert corner-index ordering without observation |

One CubeGrid **session** has one `GridRotation` for its rectangles. Rotating a module is possible, but per-wall arbitrary yaw in one session is not exposed. Avoid making every 12 cm arc chord its own mesh: choose a meaningful silhouette error budget (e.g., about half a grid cell) and use low-detail facets for blockout.

### Error and geometry budget

For a circular arc radius `r` with maximum permitted deviation `e` (world cm), a chord covering angle `Δθ` has sagitta `e = r(1 − cos(Δθ/2))`. Choose `N = ceil(total_span / (2 acos(1−e/r)))` when `0<e<r`; clamp to a sensible minimum and blockout complexity budget. Real outlines may be irregular, so trace identifiable corners rather than imposing an ideal circle.

For grid rounding, a point may shift up to `g/2` per local axis (Euclidean shift up to `g/√2` for XY). Check **endpoint and doorway displacement in world space** after transforming the snapped point back. For example, a line 8 m long that is 8° away from cardinal can drift by more than 1 m at one end if flattened around the other; *angle alone is not a safe simplification criterion*.

At rotated-module joins, prefer a small purposeful transition module/landing and aligned floor tops. Coincident overlapping planar surfaces may z-fight; visible gaps are not "BSP leaks" but still damage the level and player traversal.

## D. Floors and ceilings without accidental filled rooms

CubeGrid constructs **solid** geometry. A single floor slab spanning the bounding box of a concave room can unintentionally fill an outdoor void, atrium, stairwell or intended hole. Partition irregular floor **footprints** into nonoverlapping rectangles in the relevant local grid, exclude voids, and use several submodules if they need different rotations. A ceiling is optional for a first playtest; when requested, build it as a separate named module, apply the ceiling material, put it above required headroom and do not cap an intended vertical route. Record Editor visibility so an overhead comparison can use a cutaway without implying the ceilings are absent. Structural visual shell and navigation volumes are separate concerns.

## E. Material application, confirmation and persistence

Choose UV world scale before extrusion with `SetMaterialUVScale(Scale, bWorldSpace=true)`. Resolve a verified material path. Set the material for the *next* pull using `ApplyMaterialToRectangle(MaterialPath, bPaintExisting=false)`, or paint matching **existing coplanar outward** surfaces with `true`; `PaintedTriangles` helps diagnose selection, not appearance. Save the output asset and level with separate editor tools **after** `ConfirmMesh`. An unfinished corner preview blocks confirmation.

## F. Simple hollow shell and direct lintel

For a 15 by 20 cell box with a one-cell floor and one-cell-thick, three-cell-high borders, select `(0,0,0)` to `(15,20,0)`, +Z, pull 4; then select `(1,1,4)` to `(14,19,4)`, +Z, push 3. The outer bounds remain 15 by 20 by 4 cells and the interior floor is at Z=1. Walls may need a separate material pass, so a floor-first plus wall-strip recipe is often easier for a palette with different floor/wall materials.

A detached lintel can be built directly by selecting a horizontal rectangle at its lower Z and pulling to its upper Z. The plugin does not require bridging it from a jamb's face. Verify that its ends meet the jambs and leave the requested clear opening.

## G. Failure recovery and verification

Never proceed on a stale asynchronous preview. The call's result state and `GetState` should indicate not busy. In corner mode, use `LeaveCornerMode(false)` to discard an unwanted preview; the header contains **no general undo** for committed operations. To rebuild a damaged *unconfirmed* module, cancel its session and replay a validated operation ledger. To correct a confirmed actor, find a verified actor edit/delete workflow; `CancelCubeGrid` cannot remove it.
