---
name: ue-selection-describe
description: Inspect selected Unreal Engine actors/components, identify the logical architectural object and visible material assignments, and persist a reusable object description as DataAsset records plus a material DataTable whose row Structure asset is created first. Use when the user asks to describe, catalog, remember, or save selected level architecture, rooms, corridors, modules, platforms or meshes with their materials.
---

# Describe selected Unreal actors into DataAsset + DataTable

## Objective

Use this skill when the user asks to inspect, describe, or catalog selected architecture. For a description-only request, inspect and report without creating assets. Execute the persistence steps below when cataloging/saving descriptions is requested. Do not automatically invoke this workflow merely because geometry was generated.

In another project, retrieve this skill and its reference through the plugin's `GetSkillCatalog` and `ReadSkill`; its inspection and asset operations still use separately discovered editor toolsets.

Describe **what the selected object is**, how it is assembled, dimensions and connections, and **which real materials are used on which surfaces**. Preserve this description in actual Unreal assets: first a `Structure` **asset**, then a `DataTable` using that row type; create/modify `DataAsset` instances for selected logical objects. The `DataTable` catalogs material roles, not the entire geometry of the selection. A `DataAsset` is the shape/object record.

**Read `references/ASSET-SCHEMA.md` before writing assets.** In particular, never confuse an asset class with an actual DataAsset instance, a DataTable row with an Actor, or a displayed material name with a valid material object path.

`CubeGridToolset.h` has **no selected-actor reader or DataAsset/DataTable API**. Use the project's other Unreal MCP tools: **discover** selection, component inspection, material slots, asset creation, property reading/writing, DataTable rows, blueprint class/struct authoring, save and read-back functions by actual registered names and parameters. Do not invent tool names or falsely claim persistence.

## Procedure

### 1. Inspect the selection and establish provenance

- Read active world, selected actor count, selection identifiers/paths, actor classes, labels, transforms, bounds and components. If selection is empty, **stop**; never describe arbitrary scene actors as selected.
- Obtain detailed mesh/component data when possible: mesh asset paths, material slots and overrides, per-section material-to-face associations if exposed, collision and mobility, instance counts, local/world mesh bounds. If only material slots are available, record **slot usage**, not fabricated exact face coverage.
- Distinguish architectural pieces (walls, roof, slab, frame), trims, lighting/props, mechanical/glass elements, gameplay actors and accidental context selection. User selection does not automatically mean one module; explain excluded outliers.
- Resolve a stable logical identity from selected Actor GUIDs/editor identifiers plus source world, or a user-provided name. Labels are only display names and may change; component indices/selection order are never permanent IDs.
- Check whether several actors form one logical object (room, corridor, bridge, platform, airlock, facade or module) or several independent objects. Emit a DataAsset per **logical object**, with an optional grouping/parent description if appropriate.

### 2. Recover shape and function, do not overwrite evidence

Classify `room | corridor | bridge | platform | wall | floor | ceiling | doorway | stairs | ramp | module | prop | unknown`. Detect structural shell, clear interior dimensions vs outer bounds, height zones, openings, end connections, repeat elements, transform/orientation and asymmetric features. Doors and windows should have position, approximate size, sill, destination and `traversable` state, only when inspection makes this defensible. A mere wall outline is not automatically a closed room.

Use measured actor/component bounds in centimeters, **not** hardcoded dimension conventions. If an aperture or interior cannot be measured from available tool outputs, set `unknown` and describe the evidence rather than invent a number. Semantic/gameplay role and style should be concise prose plus structured fields; both are useful for a future rebuilding skill.

### 3. Catalog actual materials and how they are used

For each included architectural component, enumerate effective material references (including instance overrides), their valid `/Game/...Asset.Asset` paths, slots and, if a suitable tool supports them, material/instance parameters and surface coverage. Identify the **visible** role: wall face, floor walking surface, ceiling, trim, door frame, glass, rail, underside, etc. Never equate a slot named `Metal` with a verified metallic finish; never pretend to have read a parameter that the tool did not return.

Record both **observations** (slot name, asset path, observed section/material index, inspected parameter values) and **visual inferences** (for example `blue-gray, rough-looking`) with confidence. If an assignment is unknown, mark `role=unknown` and preserve the verified material reference anyway. Record hidden/backface/helper materials only when they matter to reconstruction; never introduce Radiant-only `caulk` rules.

### 4. When persistence is requested, create/reuse the material Struct and DataTable, in order

1. Search for an existing compatible `ST_LevelMaterialRow` `UserDefinedStruct` and confirm its **actual** field names and types. If absent, **create a real Structure asset first** (not a CSV header; not a loose JSON object).
2. Required columns (exact semantic meanings): `Material` (prefer `Soft Object Reference<MaterialInterface>`, otherwise a validated reference-compatible type or exact path string), `WhereUsed` (`Text`), `VisualDescription` (`Text`). `VisualDescription` describes color, gloss, wear, pattern and other distinctive visual features. `WhereUsed` describes surface role and context. `RowName` is the DataTable row identifier, **not an extra material field**.
3. Save/compile the Struct and verify it is available as a DataTable row structure. If incompatible existing schema cannot be changed safely, create a versioned compatible struct instead of corrupting the user's table.
4. Only **then** find/create `DT_LevelMaterials` with the verified Struct. If a table already exists, inspect its row struct/rows before editing. Never replace a supplied, populated material table with an empty one.
5. Upsert deterministic rows by the **combination of actual material and role**, e.g. `Wall_Concrete_Gray`, `Floor_Concrete_Gray`; a material may have multiple role-specific rows. Preserve user-authored descriptions unless user explicitly requests updates. Verify reference resolution and avoid duplicate semantic entries.

A provided catalog with `Material`, `Target`, and optional `Description` is equivalent for these roles; map the actual fields to Material/WhereUsed/VisualDescription rather than migrating it. When a project already has an equivalent struct/table, **reuse it**, mapping columns by observed schema rather than silently creating a conflicting second catalog. If no compatible row type or asset-writing capability exists, stop writing but return a precise proposed schema and row manifest.

### 5. Create/reuse a true DataAsset describing each logical object

- Find/create a suitable `UDataAsset` / `UPrimaryDataAsset` **subclass with editable fields** (`BP_LevelObjectDescription` or an established project class). The class is the schema; **`DA_<ObjectId>` is a DataAsset instance**. A bare classless `UDataAsset` cannot magically hold new arbitrary fields. Verify whether the editor's asset factory accepts the selected subclass; if the project requires a native class, do not disguise a Blueprint class asset as a DataAsset instance.
- Populate at minimum: `ObjectId`, `DisplayName`, `Kind`, `PurposeAndStyle`, `LevelOrMap`, stable selected Actor references/paths, measured bounds/world/local transform, `MaterialCatalog` reference to `DT_LevelMaterials`, and `MaterialUses` mapping actual component/slot/face roles to **DataTable row names** (ideally `DataTableRowHandle`). Add openings/connections, repeats, interior dimensions, fixed/stretchable dimensions, assumptions, source paths and confidence if known. See full schema.
- Preserve asymmetry: a wall with one niche remains asymmetric; different material on opposite sides is two surface-role entries. Preserve an intentionally off-center door and repeated mullion spacing if observed.
- Upsert by stable identity: reread existing `DA_<ObjectId>`, change measured fields as requested, retain unrelated handcrafted metadata, and do not spawn duplicates on every re-description. Do not store transient selection ordinals as identifiers.

### 6. Save, reread and report

Persist only the requested Struct, DataTable, class/instances and packages through separately verified editor tools. Save the owning map before recording persistent Actor paths; `/Temp/Untitled` references and selection ordinals cannot identify actors after reload. **Reread from disk/editor after save**: compare row count, material refs and descriptions, DataAsset property values, Actor linkage, source world, catalog row handles. Distinguish `created`, `updated`, `already present`, `pending`, `dirty/unsaved` precisely.

Report logical object IDs/kinds, estimated or measured dimensions, openings, materials per surface, excluded actors, exact object paths for `Struct`, `DataTable` and each `DataAsset`, confidence caveats and persistence results. If inspection only returned slots, say face distribution could not be confirmed.

## Non-negotiable safeguards

- Do not describe a level from the current camera view when selection API returns an empty set.
- Do not create the DataTable before creating/validating its row **Struct asset**.
- Do not fill a DataAsset with made-up per-face material assignments or imaginary material paths.
- Do not rewrite unrelated table rows or destroy preexisting user metadata.
- Do not assert a DataAsset exists just because a Blueprint class was created.
