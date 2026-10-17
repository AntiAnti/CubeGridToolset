# Persistent architecture metadata: DataTable + DataAsset

This reference is shared by `ue-selection-describe` and `cubegrid-concept-build`. It describes **real Unreal asset types**, not JSON files disguised as assets. Work with existing project conventions if compatible. Creation and editing require editor asset APIs **not present in `CubeGridToolset.h`**.

## Ownership and purpose

1. **`ST_LevelMaterialRow`** — *Struct asset* created first (`User Defined Structure`) or an existing suitable native `USTRUCT(BlueprintType)` row type. For a newly requested catalog use these properties (reuse equivalent existing fields such as `Target` and `Description` without migration):
   - `Material`: **Soft Object Reference to Material Interface**, resolving to a `UMaterialInterface`/material instance. If the project's struct editor cannot expose that type, use a compatible Object/Soft Object Reference with validation, not a fabricated material.
   - `WhereUsed`: `Text` — surfaces/locations/roles (e.g. `external walls, pillars, north facade`).
   - `VisualDescription`: `Text` — color, roughness, gloss, wear, transparency, pattern, trim, reference-image cues. Distinguish visually inferred qualities from inspected material parameters.
2. **`DT_LevelMaterials`** — *DataTable* whose row struct is `ST_LevelMaterialRow` (or the native counterpart). Stable `RowName` identifies the *role*, e.g. `Wall_Concrete_Pale`, not a transient row number. Different roles can reference the same material with different `WhereUsed` and `VisualDescription` entries. Never create the DataTable before the row struct.
3. **`BP_LevelObjectDescription`** — definition of a `UDataAsset`/`UPrimaryDataAsset` subclass with editable fields, preferably native subclass when tooling supports it. It is a **class definition**, not itself a record. Alternative: an existing compatible project DataAsset class. Do not assume that an abstract bare `UDataAsset` instance can store arbitrary added fields.
4. **`DA_<StableObjectId>`** — one **DataAsset instance per selected logical architectural object**; for multi-actor selection that clearly forms one room/bridge/module, one DataAsset can describe the assembly with multiple actor paths. Reuse/update the DataAsset for an already described object; do not emit duplicates for each new selection.

### Suggested properties of the DataAsset class

| Property | Unreal-friendly type | Meaning |
| --- | --- | --- |
| `ObjectId` | `Name` | Stable identifier, e.g. `Room_Airlock_01`. |
| `DisplayName` | `Text` | Human-readable name. |
| `Kind` | `Name` or project enum | `room`, `corridor`, `bridge`, `platform`, `module`, `wall`, `floor`, `doorway`, `stair`, `prop`, `unknown`. |
| `PurposeAndStyle` | `Text` | Role, visual character, scale impression, context. |
| `LevelOrMap` | soft reference or asset path | Owning world/level; path may be unresolvable until saved. |
| `Actors` | array of actor soft refs / verified actor paths | Stable references; do **not** use selection indices or transient enumeration order as IDs. |
| `BoundsMinCm`, `BoundsMaxCm` | `Vector` | Measured world-space AABB and coordinate frame. |
| `LocalFrame` | `Transform` | Useful rebuild/placement frame if available. |
| `InteriorSizeCm` | `Vector` | Clear dimension estimate; never confuse with overall bounds. Optional/unknown allowed. |
| `ThicknessCm`, `ClearHeightCm` | float, optional validity flags | Measured rather than guessed. |
| `Connections` | array of struct/text | Each opening: local/world position, orientation, width, height, sill, destination, passable/blocked state. |
| `MaterialCatalog` | `DataTable` object ref | Reference to `DT_LevelMaterials`. |
| `MaterialUses` | array of `ST_LevelMaterialUse` or equivalent | `SurfaceRole`, `MaterialRowName`, `ComponentOrSlot`, `ObservedMaterialPath`, `CoverageNotes`, `Confidence`. Prefer `DataTableRowHandle` if the project's asset tooling supports it. |
| `RepeatsAndDetails` | array of struct/text | Rails, panels, steps, trim, spacing/count and exceptional features. |
| `ScalingRules` | `Text` | Stretchable axes vs. fixed wall thickness/door clearance/repeats. |
| `Provenance` | array of text/struct | Actor and source-image refs, observation method, author, source confidence, unresolved issues, excluded accidental selections. |

An asset may implement a smaller schema if that's the actual project class, but **do not silently drop `ObjectId`, `Kind`, actors, bounds, material catalog/uses, or known openings**. Extend the class with editor tools if allowed; otherwise flag missing fields and preserve the description in an external manifest awaiting implementation.

### Optional `ST_LevelMaterialUse`

A struct for `MaterialUses` entries: `SurfaceRole (Name)`, `MaterialRow (DataTableRowHandle)`, `ComponentOrSlot (String)`, `ObservedMaterialPath (SoftObjectPath or String)`, `Notes (Text)`, `Confidence (float)`.

### Example DataTable rows

| RowName | Material | WhereUsed | VisualDescription |
| --- | --- | --- | --- |
| `Wall_Concrete_Pale` | `/Game/Materials/MI_Concrete_Pale.MI_Concrete_Pale` | `outer walls, west gallery` | `light gray; matte; mildly stained` |
| `Floor_Metal_Dark` | `/Game/Materials/MI_Metal_Dark.MI_Metal_Dark` | `walkable platforms and stairs` | `dark bluish gray; satin; narrow seams` |

**Examples are illustrative, not discovered project assets.** Only write them into a live DataTable after checking that those exact assets exist; otherwise use verified user-supplied references or record `unresolved_material` with the desired appearance.

## Creation workflow and checks

1. Look for matching existing `ST_LevelMaterialRow` / `DT_LevelMaterials`. Confirm the fields and underlying types; if incompatible, create a versioned structure/table rather than breaking existing rows.
2. **Create and save the struct asset** (or compile a native `FTableRowBase`-derived row `USTRUCT`) **before** creating its DataTable.
3. Create `DT_LevelMaterials` with that row struct; write or upsert rows by stable role name; validate reference resolution and row contents via a reread.
4. Resolve the DataAsset class (existing compatible subclass or create and compile one). Make `DA_<ObjectId>` asset instances; upsert verified values and catalog row references. Save each package, then **reread** both DataTable and DataAsset.
5. If save is not supported, distinguish `created_in_memory (unsaved)` from `persisted_to_disk (verified)`.
6. If Unreal asset-editing actions are missing, **do not claim creation**. Return complete struct definition, DataTable row values and DataAsset record as a proposed manifest, with precise blocking dependencies.

## Reuse across skills

- The concept-art builder consumes a *user-provided* DataTable with a verified material reference and surface-role fields; appearance notes may be blank or omitted. It should not quietly replace material references with approximations or create a new catalog if one was provided.
- The selection-describe skill creates/updates this catalog from actually observed materials; descriptors such as `dark brass` seen on a screenshot are *visual hypotheses*, not assertions of asset parameter values.
- A DataTable is not the same thing as a DataAsset, and a Blueprint class is not an instance of either one.

Official Epic reference: https://dev.epicgames.com/documentation/unreal-engine/data-assets-in-unreal-engine and https://dev.epicgames.com/documentation/en-us/unreal-engine/coder-05-manage-item-and-data-in-an-unreal-engine-game
