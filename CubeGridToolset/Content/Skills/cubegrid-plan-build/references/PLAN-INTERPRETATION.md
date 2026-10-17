# From a top-view plan to a stable architecture manifest

This is a condensed transfer of the useful ideas from the supplied Radiant `plan-describe` / `shape-build` skills. It deliberately discards the idTech-3 BSP assumptions.

## Layer separation

1. **Geometry**: structural wall centerlines (single lines or traces between the centers of drawn double lines), thickness, platforms/floors, steps, curbs, ceiling-height indications.
2. **Apertures**: doors, passageways, windows, gates, viewports, force fields, airlocks. These are bounded portions **of a real wall**; do not substitute a whole "open side".
3. **Gameplay**: consoles, elevators, control panels, switches, obstacles; preserve links and notes without interpreting diagram symbols as CubeGrid meshes.
4. **Descriptive**: room names, zones, annotations, dimensions, compass, room-to-room semantic relationships. Do not generate walls from room labels or framing boxes.
5. **Drafting noise**: leader arrows, hatch lines, furniture edges, circles around labels, grids. Explicit markings may annotate an architectural object; they are not that object.

## Extraction rules

- Split a segment at corner, T/+ junction, thickness/material/height change, or genuine free endpoint. Preserve a single segment between joints. Do not count both lines of a thick drawn wall as two walls.
- For a short isolated stroke, distinguish structural partition from furniture/detail using adjacency, room division and continuity; label uncertain cases, do not silently convert them to walls.
- The source coordinate convention is **normalized, image-origin top-left**: `u = x_pixel / image_width`, `v = y_pixel / image_height`.
- An `opening` belongs to a segment and records `center_t` (dimensionless fraction measured A→B), width (`width_cm` **or** `width_t`, explicitly chosen), bottom and clear height. A wide door occupying most of a wall remains one precisely positioned opening, not a missing wall.
- Curves: trace the **observed boundary** as consecutive chords. For a genuinely circular arc, start from an error limit on the sagitta: `e = r * (1 - cos(Δθ / 2))`, choosing chord angle small enough for the permitted geometric error, not blindly 20°; preserve any observed asymmetric vertices.
- Identify vertical connections and floor areas separately from walls. A sketch of "stairs up" does not establish a numeric top-Z unless measurements or other evidence exist.
- Record source confidence on every important inferred dimension, wall and opening; do not replace uncertainty with unwarranted precision.

## Compact manifest (example; not a discovered level)

```yaml
source:
  width_px: 1600
  height_px: 900
  world_width_cm: 2400 # chosen blockout assumption
  world_height_cm: 1350 # preserves aspect ratio
  world_origin_cm: [0, 0, 0]
wall_segments:
  - id: W01
    a_uv: [0.15, 0.30]
    b_uv: [0.65, 0.30]
    thickness_cm: 25
    bottom_cm: 25
    height_cm: 275
    confidence: 0.93
openings:
  - id: D01
    wall_segment_id: W01
    center_t: 0.42
    width_cm: 100
    bottom_cm: 25
    clear_height_cm: 220
    kind: door
    traversable: true
    confidence: 0.8
spaces:
  - id: HUB
    label: Central hub
    bordering_segments: [W01]
    geometry_authoritative: false
```

`bottom_cm` is world Z in this example. Treat `width_cm` as real centimeters; a normalized opening width must instead use `width_t` (fraction of segment length). Use separate `floor_top_cm` if the scene's slab top is not level 0.

## Scale and quantization

Prefer explicit scale bars or dimensions, otherwise cross-check a known doorway/elevator/character against proportions. If none exists, state a **single default blockout scale** in manifest and apply it consistently. Maintain relative room dimensions. Snapping accuracy is best evaluated by the **actual world displacement** of each moved endpoint relative to source, not only by line angle. Snap joints together once, otherwise independent endpoint rounding may create misaligned neighboring walls.

## Final checklist

- Every important drawn wall has a corresponding centerline.
- Every aperture occupies an explicit sub-interval of a wall.
- Staircases/lifts, bridges, windows and force fields remain semantically distinct.
- A shared wall between rooms is not duplicated.
- Inferred spaces are derived from geometry, not used to overwrite it.
- Confidence and assumptions are stated.
