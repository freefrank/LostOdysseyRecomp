"""Captured tone constants, synthetic one-pixel inputs (no game snapshots)."""
import unittest

import numpy as np

from postprocess_mask_reference import tonemap_reference, tonemap_weights


class TonemapFltMinTest(unittest.TestCase):
    def test_captured_scene_tap_stays_active(self):
        f = np.float32
        constants = np.zeros((256, 4), dtype=f)
        constants[0, 2:4] = [0.10010010004043579, 0.00010009881225414574]
        constants[1, 0] = 917.7822265625
        constants[2, :2] = [114.7227783203125, 56.2491455078125]
        constants[3, :2] = [573.6138916015625, 82588.1171875]
        constants[4, :2] = .5
        constants[5, :2] = 1
        constants[6, :3] = 1
        constants[7, 0] = .1725
        constants[255, :2] = [.001, 1]
        depth = np.array([[.0027652892749756575]], dtype=f)

        scene, dof, bloom, branch, weight = tonemap_weights(depth, constants)
        self.assertTrue(branch[0, 0])
        self.assertAlmostEqual(float(weight[0, 0]), .2381763607263565, places=7)
        self.assertGreater(float(scene[0, 0]), 0)
        self.assertTrue(np.any(dof != 0))
        self.assertGreater(float(bloom), 0)

        interpolants = np.zeros((1, 1, 2, 4), dtype=f)
        interpolants[..., :2] = .5
        masks = {0: np.array([[1]], np.uint8),
                 2: np.array([[0]], np.uint8), 3: np.array([[0]], np.uint8)}
        rects = {slot: [0, 0, 1, 1] for slot in masks}
        result, valid, diag = tonemap_reference(
            interpolants, constants, depth, masks, rects,
            {0: False, 2: True, 3: True}, depth_linear=False)
        self.assertTrue(valid[0, 0])
        self.assertTrue(diag['needed_slots'][0][0, 0])
        self.assertEqual(int(result[0, 0]), 1)


if __name__ == '__main__':
    unittest.main()
