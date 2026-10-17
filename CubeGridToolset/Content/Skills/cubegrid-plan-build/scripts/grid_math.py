#!/usr/bin/env python3
"""Convert a normalized image plan to explicit Unreal CubeGrid cell indices.

This is an offline planning aid. It does not connect to Unreal or create meshes.
Input and output are JSON; all world distances are centimeters.
"""

import argparse
import json
import math
from pathlib import Path


def require_pair(value, name):
    if not isinstance(value, list) or len(value) != 2:
        raise ValueError(f"{name} must be an array of two numbers")
    result = (float(value[0]), float(value[1]))
    if not all(map(math.isfinite, result)):
        raise ValueError(f"{name} coordinates must be finite")
    return result


def require_triplet(value, name):
    if not isinstance(value, list) or len(value) != 3:
        raise ValueError(f"{name} must be an array of three numbers")
    result = (float(value[0]), float(value[1]), float(value[2]))
    if not all(map(math.isfinite, result)):
        raise ValueError(f"{name} coordinates must be finite")
    return result


def point_to_world(uv, image_width_px, image_height_px, world_width_cm, world_origin_cm):
    u, v = require_pair(uv, "point")
    if not 0 <= u <= 1 or not 0 <= v <= 1:
        raise ValueError("Normalized point must lie in [0,1] x [0,1]")
    if image_width_px <= 0 or image_height_px <= 0 or world_width_cm <= 0:
        raise ValueError("Image dimensions and world width must be positive")
    h = world_width_cm * image_height_px / image_width_px
    return (
        world_origin_cm[0] + u * world_width_cm,
        world_origin_cm[1] + (1.0 - v) * h,
        world_origin_cm[2],
    )


def world_to_local_cells(world, grid_origin, grid_cm, yaw_deg):
    if grid_cm <= 0 or not math.isfinite(grid_cm):
        raise ValueError("grid_cm must be finite and positive")
    a = math.radians(yaw_deg)
    c, s = math.cos(a), math.sin(a)
    dx, dy, dz = (world[i] - grid_origin[i] for i in range(3))
    return (
        (c * dx + s * dy) / grid_cm,
        (-s * dx + c * dy) / grid_cm,
        dz / grid_cm,
    )


def local_cells_to_world(cell, grid_origin, grid_cm, yaw_deg):
    a = math.radians(yaw_deg)
    c, s = math.cos(a), math.sin(a)
    x, y, z = cell
    return (
        grid_origin[0] + grid_cm * (c * x - s * y),
        grid_origin[1] + grid_cm * (s * x + c * y),
        grid_origin[2] + grid_cm * z,
    )


def nearest_int(x):
    # Avoid banker's rounding for midpoint ties; keep negative coordinates symmetric.
    return math.floor(x + 0.5) if x >= 0 else math.ceil(x - 0.5)


def azimuth_deg(a, b):
    return math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))


def cardinal_deviation_deg(azimuth):
    return abs((azimuth + 45.0) % 90.0 - 45.0)


def convert_manifest(spec):
    image_width = float(spec["image_width_px"])
    image_height = float(spec["image_height_px"])
    world_width = float(spec["world_width_cm"])
    world_origin = require_triplet(spec.get("world_origin_cm", [0, 0, 0]), "world_origin_cm")
    grid_origin = require_triplet(spec.get("grid_origin_cm", list(world_origin)), "grid_origin_cm")
    grid_cm = float(spec["grid_cm"])
    yaw = float(spec.get("grid_yaw_deg", 0.0))
    if not all(map(math.isfinite, [image_width, image_height, world_width, grid_cm, yaw])):
        raise ValueError("All scale and rotation values must be finite")
    if min(image_width, image_height, world_width, grid_cm) <= 0:
        raise ValueError("Image dimensions, world width, and grid size must be positive")
    if not isinstance(spec.get("points"), dict):
        raise ValueError("points must be a dictionary of point IDs to normalized [u,v]")

    out = {
        "units": "cm",
        "grid_cm": grid_cm,
        "grid_yaw_deg": yaw,
        "grid_origin_cm": list(grid_origin),
        "world_height_cm": world_width * image_height / image_width,
        "points": {},
        "segments": [],
    }
    for pid, uv in spec["points"].items():
        world = point_to_world(uv, image_width, image_height, world_width, world_origin)
        cells_float = world_to_local_cells(world, grid_origin, grid_cm, yaw)
        cells = [nearest_int(v) for v in cells_float]
        snapped = local_cells_to_world(cells, grid_origin, grid_cm, yaw)
        error = math.dist(world, snapped)
        out["points"][pid] = {
            "source_uv": uv,
            "world_cm": list(world),
            "cell_float": list(cells_float),
            "cell_index": cells,
            "snapped_world_cm": list(snapped),
            "snapping_error_cm": round(error, 4),
        }
    for segment in spec.get("segments", []):
        a_id, b_id = segment["a"], segment["b"]
        if a_id not in out["points"] or b_id not in out["points"]:
            raise ValueError(f"Segment {segment.get('id')} references unknown point")
        a = out["points"][a_id]["world_cm"]
        b = out["points"][b_id]["world_cm"]
        aa = out["points"][a_id]["snapped_world_cm"]
        bb = out["points"][b_id]["snapped_world_cm"]
        bearing = azimuth_deg(a, b)
        out["segments"].append({
            "id": segment["id"],
            "a": a_id,
            "b": b_id,
            "world_length_cm": round(math.dist(a, b), 3),
            "snapped_length_cm": round(math.dist(aa, bb), 3),
            "world_bearing_deg": round(bearing, 3),
            "nearest_cardinal_deviation_deg": round(cardinal_deviation_deg(bearing), 3),
            "endpoint_max_snapping_error_cm": round(max(
                out["points"][a_id]["snapping_error_cm"],
                out["points"][b_id]["snapping_error_cm"]), 4),
        })
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="JSON source plan configuration")
    parser.add_argument("-o", "--output", type=Path, help="Optional JSON output path")
    args = parser.parse_args()
    result = convert_manifest(json.loads(args.input.read_text(encoding="utf-8")))
    output = json.dumps(result, ensure_ascii=False, indent=2)
    if args.output:
        args.output.write_text(output + "\n", encoding="utf-8")
    else:
        print(output)


if __name__ == "__main__":
    main()
