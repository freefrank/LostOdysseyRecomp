"""Portable regression checks for diagnostic receipts and isolated probe modes."""
from argparse import Namespace
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'perf'))
from native_probe_log import NativeProbeLog, frontend_delta, probe_environment


class ProbeLogTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / 'runtime.log'
        self.log = NativeProbeLog(self.path)

    def append(self, data):
        with self.path.open('ab') as stream:
            stream.write(data)

    def test_early_scene_marker_survives_verbose_tail(self):
        self.append(b"[io] open 'game:\\xenon_scr.fpd'\nframe timing completed=12\n")
        self.assertFalse(self.log.ready('uhra', 3300, False))
        self.append(b'draw trace\n' * 150000 + b'present timing completed=3315\n')
        self.assertTrue(self.log.ready('uhra', 3300, False))
        self.assertEqual(self.log.completed, 3315)
        before = self.log.offset
        self.log.poll()
        self.assertEqual(self.log.offset, before)

    def test_split_receipt_and_heartbeat_gate(self):
        self.append(b"open 'game:\\xenon_scr.fpd'\npresent timing completed=33")
        self.log.poll()
        self.assertEqual(self.log.completed, 0)
        self.append(b"16\nheartbeat: swap #3301 105.0 fps, 1981 draws/frame, last file 'xenon_scr.fpd'\n")
        self.assertFalse(self.log.ready('uhra', 3300, True))
        self.append(b"heartbeat: swap #3361 105.0 fps, 1900 draws/frame, last file 'xenon_scr.fpd'\n")
        self.assertTrue(self.log.ready('uhra', 3300, True))
        self.append(b"heartbeat: swap #3421 60.0 fps, 40 draws/frame, last file 'menu.fpd'\n")
        self.log.poll()
        self.assertIsNone(self.log.uhra_heartbeat())

    def test_truncation_discards_stale_scene_and_frame(self):
        self.append(b"open 'game:\\xenon_scr.fpd'\nframe timing completed=4500\n")
        self.log.poll()
        self.path.write_bytes(b'frame timing completed=1\n')
        self.log.poll()
        self.assertFalse(self.log.scene_loaded)
        self.assertEqual(self.log.completed, 1)
        self.assertEqual(self.log.generation, 1)

    def test_receipt_execution_window(self):
        self.append(b'native frontend: mode=mesh (experimental)\n')
        self.log.poll()
        self.assertIsNone(self.log.frontend)
        for swap, number in ((3360, 100), (3480, 400)):
            self.append(f'native frontend: swap={swap} mode=mesh mesh_commands={number} native_draws={number-1} other_draws=10 predicated_skips=1 words={number*9} state_values=20 producer_revision=900\n'.encode())
            self.log.poll()
            if swap == 3360:
                first = self.log.frontend
        delta = frontend_delta(first, self.log.frontend)
        self.assertEqual(delta['native_draws'], 300)
        self.assertEqual(delta['start_swap'], 3360)
        self.assertEqual(delta['end_swap'], 3480)
        self.assertNotIn('producer_revision', delta)
        self.assertIsNone(frontend_delta(self.log.frontend, first))

    def test_missing_file_and_large_partial_line_are_bounded(self):
        self.log.poll()
        self.append(b'x' * 200000)
        self.log.poll()
        self.assertLessEqual(len(self.log.pending), 16384)
        self.append(b'\nframe timing completed=600\n')
        self.assertTrue(self.log.ready('title', 600, False))

    def test_modes_do_not_inherit_diagnostic_overrides(self):
        args = Namespace(mode='off',native_frontend='mesh',scene='uhra',scene_stats=False,
                         render_timing=False,frontend_stats=False)
        inherited = {'PATH':'preserved','LO_RENDER_TIMING':'1','lo_fps':'999',
                     'LO_NATIVE_COMMANDS':'all','VK_INSTANCE_LAYERS':'validation'}
        env = probe_environment(args, Path('isolated'), inherited)
        self.assertEqual(env['PATH'], 'preserved')
        self.assertEqual(env['LO_NATIVE_FRONTEND'], 'mesh')
        self.assertEqual(env['LO_NATIVE_COMMANDS'], '0')
        self.assertNotIn('LO_RENDER_TIMING',env)
        self.assertNotIn('lo_fps',env)
        self.assertNotIn('VK_INSTANCE_LAYERS',env)
        args.render_timing = True
        args.frontend_stats = True
        env = probe_environment(args, Path('isolated'), inherited)
        self.assertEqual(env['LO_RENDER_TIMING'],'1')
        self.assertEqual(env['LO_NATIVE_FRONTEND_STATS'],'1')


if __name__ == '__main__':
    unittest.main()
