"""Synthetic checker bounds for inset postprocess draws and cleared R8 margins."""
import copy
from importlib.util import module_from_spec, spec_from_file_location
from pathlib import Path
import sys
import tempfile
import unittest

import numpy as np


DIRECTORY = Path(__file__).resolve().parent
sys.path.insert(0, str(DIRECTORY))
spec = spec_from_file_location('postprocess_capture_checker', DIRECTORY / 'check-postprocess-capture.py')
checker = module_from_spec(spec)
spec.loader.exec_module(checker)


def bits(values):
    return np.asarray(values, dtype='<f4').view('<u4').tolist()


class ClearBackgroundTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='postprocess-clear-')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.root.joinpath('original-color.bin').write_bytes(bytes(6 * 6 * 4))
        self.root.joinpath('input-mask.bin').write_bytes(bytes([80]))
        self.output = np.zeros((6, 6), dtype=np.uint8)
        self.output[1:5, 1:5] = 80
        sampler = 1 | (1 << 2) | (2 << 6) | (2 << 9)
        n, p = -2 / 3, 2 / 3
        self.event = {
            'recorded': True, 'published': True, 'submission_serial': 1,
            'render_frame': 12, 'geometry_epoch': 13, 'color_allocation': 14,
            'draw_ordinal': 15, 'source_revision': 15, 'source_stage': 1,
            'ps_hash': '7c260eacff1d681d', 'vs_hash': '2f6bbed8149a7804',
            'actual_topology': 'TRIANGLE_LIST', 'indexed': True, 'index_count': 6,
            'output_extent': [6, 6], 'viewport': [0, 0, 6, 6, 0, 1],
            'scissor': [0, 0, 6, 6], 'published_valid_rect': [0, 0, 6, 6],
            'draw_written_rect': [1, 1, 4, 4], 'inset_geometry_ok': True,
            'clear_background_available': True, 'geometry_supported': True,
            'clear_background': {
                'frame': 12, 'epoch': 13, 'allocation': 14, 'extent': [6, 6],
                'ordinal': 15, 'kind': 'depth_color_tile_clear',
                'affected_rect': [0, 0, 6, 6], 'invalidated_by': None},
            'vertices_in_index_order': [
                {'clip_position_bits': bits([x, y, 0, 1]), 'base_uv_bits': bits([.5, .5])}
                for x, y in [(n, n), (p, n), (n, p), (n, p), (p, n), (p, p)]],
            'vs_c0_c7_u32x4': bits([0] * 32),
            'ps_c0_c15_u32x4': bits([0] * 64), 'ps_c255_u32x4': bits([0] * 4),
            'shared_flags': 8, 'vtx_fmt': 4, 'ndc_scale_u32': bits([1] * 4),
            'ndc_offset_u32': bits([0] * 4), 'half_pixel_u32': bits([0] * 2),
            'point_slots': 0, 'output_mask_image': 17,
            'inputs': [{'slot': 0, 'effective_sampler_key': sampler, 'sampler_key': sampler,
                        'mip_levels': 1, 'crop': [1, 1], 'valid_rect': [0, 0, 1, 1],
                        'actual_color_image': 19, 'mask_image': 20}],
            'snapshots': [
                self.snapshot('original-color-before', 'original-color.bin', 6, 6, 4),
                self.snapshot('original-color-after', 'original-color.bin', 6, 6, 4),
                self.snapshot('input-t0-mask', 'input-mask.bin', 1, 1, 1),
                self.snapshot('output-mask', 'output-mask.bin', 6, 6, 1)]}

    @staticmethod
    def snapshot(label, file, w, h, bpp):
        return {'label': label, 'file': file, 'status': 'complete',
                'extent': [w, h], 'bytes_per_pixel': bpp, 'row_pitch': w * bpp, 'format': 1}

    def check(self, event=None):
        self.root.joinpath('output-mask.bin').write_bytes(self.output.tobytes())
        return checker.compare_draw(event if event is not None else self.event, self.root)

    def test_inset_clear_background(self):
        row = self.check()
        self.assertGreater(row['interior_compared_pixels'], 0)
        self.assertEqual(row['background_compared_pixels'], 20)
        self.assertEqual(row['uncovered_declared_pixels'], 20)
        self.assertEqual(row['uncovered_unchecked_pixels'], 0)
        self.assertFalse(checker.draw_failed(row))

    def test_clear_identity_required(self):
        changes = (
            ('absent', 'clear_background', None),
            ('invalidated', 'invalidated_by', 'rgb_writer'),
            ('frame', 'frame', 11), ('epoch', 'epoch', 14),
            ('allocation', 'allocation', 15), ('extent', 'extent', [5, 6]),
            ('order', 'ordinal', 16), ('coverage', 'affected_rect', [1, 0, 5, 6]),
            ('kind', 'kind', 'unrelated_clear'))
        for label, field, value in changes:
            with self.subTest(label=label):
                event = copy.deepcopy(self.event)
                if field == 'clear_background':
                    event[field] = value
                else:
                    event['clear_background'][field] = value
                with self.assertRaisesRegex(ValueError, 'clear background|clear_background'):
                    self.check(event)
        event = copy.deepcopy(self.event)
        del event['geometry_epoch']
        with self.assertRaisesRegex(ValueError, 'missing postprocess identity field: geometry_epoch'):
            self.check(event)

    def test_nonzero_background_fails(self):
        self.output[0, 0] = 1
        row = self.check()
        self.assertEqual(row['background_mismatch_pixels'], 1)
        self.assertTrue(checker.draw_failed(row))

    def test_interior_mismatch_fails(self):
        self.output[2, 1] = 81
        row = self.check()
        self.assertEqual(row['interior_mismatch_pixels'], 1)
        self.assertTrue(checker.draw_failed(row))

    def test_pixel_center_on_exterior_edge_is_not_clear_background(self):
        event = copy.deepcopy(self.event)
        # The first pixel center lies on the left geometric edge, while the
        # conservative draw_written_rect excludes that column altogether.
        for index in (0, 2, 3):
            position = checker.floats(event['vertices_in_index_order'][index]['clip_position_bits']).copy()
            position[0] = -5 / 6
            event['vertices_in_index_order'][index]['clip_position_bits'] = bits(position)
        row = self.check(event)
        self.assertGreater(row['uncovered_unchecked_pixels'], 0)
        self.assertTrue(checker.draw_failed(row))

    def test_full_quad_without_clear(self):
        event = copy.deepcopy(self.event)
        coordinates = [(-1, -1), (1, -1), (-1, 1), (-1, 1), (1, -1), (1, 1)]
        for vertex, (x, y) in zip(event['vertices_in_index_order'], coordinates):
            vertex['clip_position_bits'] = bits([x, y, 0, 1])
        event['draw_written_rect'] = [0, 0, 6, 6]
        event['clear_background'] = None
        event['inset_geometry_ok'] = False
        event['clear_background_available'] = False
        self.output.fill(80)
        row = self.check(event)
        self.assertEqual(row['uncovered_declared_pixels'], 0)
        self.assertEqual(row['background_compared_pixels'], 0)
        self.assertFalse(checker.draw_failed(row))


if __name__ == '__main__':
    unittest.main()
