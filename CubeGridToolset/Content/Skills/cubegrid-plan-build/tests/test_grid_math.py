"""Standard-library regression tests for the offline grid planning helper."""

import importlib.util
import math
import unittest
from pathlib import Path


SOURCE = Path(__file__).resolve().parents[1] / "scripts" / "grid_math.py"
SPEC = importlib.util.spec_from_file_location("grid_math", SOURCE)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class GridMathTests(unittest.TestCase):
    def test_top_left_flips_to_world_north(self):
        self.assertEqual(
            MODULE.point_to_world([0, 0], 1600, 900, 1600, [0, 0, 0]),
            (0, 900, 0),
        )

    def test_grid_90_degree_rotation_round_trip(self):
        result = MODULE.local_cells_to_world((4, 2, 1), (10, 20, 0), 50, 90)
        self.assertAlmostEqual(result[0], -90)
        self.assertAlmostEqual(result[1], 220)
        self.assertEqual(result[2], 50)
        local = MODULE.world_to_local_cells(result, (10, 20, 0), 50, 90)
        for actual, expected in zip(local, (4, 2, 1)):
            self.assertAlmostEqual(actual, expected)

    def test_cardinal_deviation(self):
        for a in [0, 90, -90, 180, -180, 270]:
            self.assertAlmostEqual(MODULE.cardinal_deviation_deg(a), 0)
        self.assertAlmostEqual(MODULE.cardinal_deviation_deg(45), 45)

    def test_joint_shares_cell_index(self):
        spec = {
            "image_width_px": 100, "image_height_px": 100,
            "world_width_cm": 1000, "grid_cm": 25,
            "points": {"A": [0.1, 0.1], "B": [0.2, 0.2], "C": [0.3, 0.3]},
            "segments": [
                {"id": "S1", "a": "A", "b": "B"},
                {"id": "S2", "a": "B", "b": "C"},
            ],
        }
        result = MODULE.convert_manifest(spec)
        self.assertEqual(result["points"]["B"]["cell_index"], [8, 32, 0])
        self.assertEqual(len(result["segments"]), 2)


if __name__ == "__main__":
    unittest.main()
