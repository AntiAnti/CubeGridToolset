# Multi-view architectural reconstruction for an Unreal CubeGrid blockout

## View directions

Unless the user specifies a different convention, +X = east, +Y = north, +Z = up. "View **from** D" means an observer on side D facing the center, not an outward-looking camera on the opposite side. The table assumes an upright camera and a horizontal look direction; tilt and perspective modify appearance but not the directional logic.

| Labeled view | Horizontal look direction | Image screen-right approximately points to |
| --- | --- | --- |
| From east | west (−X) | north (+Y) |
| From west | east (+X) | south (−Y) |
| From north | south (−Y) | west (−X) |
| From south | north (+Y) | east (+X) |
| From northeast | southwest | northwest |
| From northwest | southeast | southwest |
| From southeast | northwest | northeast |
| From southwest | northeast | southeast |
| Top, north-up | downward (−Z), image up = north | east (+X) |

If the concept explicitly marks compass directions or the map uses a rotated local north, follow those instead. "From southwest" does **not** mean that the right edge of the picture is automatically world east: its screen-right corresponds approximately to southeast, a mix of +X and −Y.

## Registration workflow

1. For each labeled image, identify landmarks that are physically unique: one chimney, distinctive opening, stair tower, overhanging plate, corner column, roof pitch or bridge.
2. Create feature IDs with `seen_in`, `screen_position`, `view_direction`, `feature_type`, `confidence`. Match the **same feature** across images.
3. Use occlusion order: from east, a positive-X facade generally hides a west facade; from southwest, south/west exterior edges are near; this constrains ordering, though stylized perspective may violate camera conventions.
4. Solve layout topology first (what is adjacent and connected); then fit widths/depths and elevations. One view cannot reliably recover hidden depth. If no plan exists, use a coarse set of rectangles in XY, not invented detailed footprints.
5. Resolve scale from labeled sizes or repeated common objects (door, person, vehicle), explicitly stating the uncertainty. Perspective makes screen-space sizes unequal even for identical objects at different depths.
6. Cross-check silhouette and openings from all viewpoints; register contradictions rather than choosing whichever drawing looks nicest.

## Source priority and conflicts

User's explicit dimensional/compass labels are authoritative unless physically contradictory. Given a real floor plan, its walls and openings generally have higher weight for horizontal geometry than an atmospheric rendering; the concepts have higher weight for Z, facade structure and surface appearance. Unlabeled background architecture may be storytelling context, not part of the playable level. A DataTable row is authoritative for the **chosen material and intended use**, but not for the geometry implied by a poetic color description.

If front and side views disagree, list the features and mismatch. Solve by: (1) drawing/style perspective distortions, (2) different visible/hidden portions, (3) simplifying details consistently with topology, or (4) leaving the feature unresolved. Never invent a big room to reconcile a merely ambiguous shadow.

## Multi-view model format

```yaml
frame:
  east: "+X"
  north: "+Y"
  up: "+Z"
  length_units: cm
views:
  - id: east
    camera_from: east
    look_toward: west
    known_roll: false
  - id: southwest
    camera_from: southwest
    look_toward: northeast
features:
  - id: airlock_A
    kind: doorway
    seen_in: [east, southwest]
    orientation: approx_east_facing
    confidence: 0.7
geometry:
  walls: []
  floor_footprints: []
  openings: []
  elevation_zones: []
modules:
  - id: wing_A
    world_yaw_deg: 0
    grid_cm: 25
    build_status: planned
materials:
  - catalog_row: Wall_Pale_Concrete
    apply_to: wing_A.exterior_wall_faces
    resolved: false
unknowns:
  - west_back_facade_not_shown
```

This manifest is a **planning record**, not a fabricated file/actor that already exists in the editor.

## Material mapping

Read `DataTable` rows with `Material`, `WhereUsed`, `VisualDescription`; inspect the real row structure and field types. Examples of matching semantic roles:

- `wall`, `outer shell`, `north facade` → wall faces, but restrict to the indicated side if given.
- `floor`, `walkway`, `treads` → walkable faces/steps, not automatically wall sides.
- `ceiling`, `underside` → lower faces of overhead volumes.
- `frame`, `trim`, `column` → actual geometry or confirmed material slot, not painted rectangles invented at random.
- `window/glass` → pane material if a pane mesh exists; cut/retain an opening independently of material assignment.

Descriptions like "blue oxidized metal" indicate intended appearance; check the referenced instance/parameters in editor if exactness matters. If one material is used both indoors and outdoors with different treatment, preserve separate DataTable rows or role bindings as given by the user.

## Validation order

1. Walkable topology and primary level heights.
2. Orthographic XY outline, doors, stairs, major shafts and bridges.
3. Silhouette and facade alignment from each user-labeled viewpoint.
4. Distinctive features supported by more than one view.
5. Materials, UV scale, collision and possible mesh z-fighting.
6. Save status and exact actor/asset paths.
