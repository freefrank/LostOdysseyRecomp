"""Portable regression checks for diagnostic receipts and isolated probe modes."""
from argparse import Namespace
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'perf'))
from native_probe_log import NativeProbeLog, frontend_delta, frontend_coverage, scene_window, probe_environment


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
        self.assertFalse(self.log.ready('uhra', 3300, False))  # asset-load evidence is insufficient
        self.append(b"native probe scene: swap=3315 observation=20 available=true map_id=7 package='test_map'\n")
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
        self.append(b"heartbeat: swap #3361 105.0 fps, 1900 draws/frame, last file 'xenon_scr.fpd'\n"
                    b"native probe scene: swap=3315 observation=20 available=true map_id=7 package='test_map'\n")
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
        self.assertNotIn('LO_AUTO_STICK', env)
        self.assertEqual(env['LO_NATIVE_PROBE_SCENE'], '1')
        args.movement = 'fixed-swaps'
        self.assertIn('LO_AUTO_STICK', probe_environment(args, Path('isolated'), inherited))
        args.render_timing = True
        args.frontend_stats = True
        env = probe_environment(args, Path('isolated'), inherited)
        self.assertEqual(env['LO_RENDER_TIMING'],'1')
        self.assertEqual(env['LO_NATIVE_FRONTEND_STATS'],'1')


    def test_skipped_commands_are_not_backend_draws(self):
        first = dict(swap=120, mesh_commands=2, native_draws=0, other_draws=10,
                     predicated_skips=2, words=20, state_values=5)
        last = dict(swap=240, mesh_commands=100, native_draws=0, other_draws=20,
                    predicated_skips=100, words=1000, state_values=500)
        delta = frontend_delta(first, last)
        self.assertGreater(delta['mesh_commands'], 0)
        self.assertEqual(delta['native_draws'], 0)
        self.assertEqual(frontend_coverage(delta)['native_draw_fraction'], 0)
        last['native_draws'] = 5
        coverage = frontend_coverage(frontend_delta(first, last))
        self.assertAlmostEqual(coverage['native_draw_fraction'], 5 / 15)
        self.assertAlmostEqual(coverage['native_draws_per_receipt_swap'], 5 / 120)

    def test_live_scene_stale_change_and_return(self):
        self.append(b"native probe scene: swap=3300 observation=20 available=true map_id=7 package='test_map'\n"
                    b"frame timing completed=3300\n")
        self.assertTrue(self.log.ready('uhra', 3300, False))
        first = self.log.scene
        changes = self.log.scene_changes
        self.append(b"native probe scene: swap=3420 observation=25 available=true map_id=7 package='test_map'\n"
                    b"frame timing completed=3420\n")
        self.log.poll()
        self.assertTrue(scene_window(first, self.log.scene, self.log.scene_changes == changes, self.log.completed))
        self.append(b"native probe scene: swap=3540 observation=30 available=false map_id=0 package=''\n"
                    b"native probe scene: swap=3660 observation=35 available=true map_id=7 package='test_map'\n"
                    b"frame timing completed=3660\n")
        self.log.poll()
        self.assertFalse(scene_window(first, self.log.scene, self.log.scene_changes == changes, self.log.completed))
        self.append(b"frame timing completed=4500\n")
        self.assertFalse(self.log.ready('uhra', 3300, False))  # old serial cannot latch readiness

    def test_frozen_or_different_map_is_not_fresh_evidence(self):
        first = dict(swap=3300, observation=20, available=True, map_id=7, package='test_map')
        same = dict(first, swap=3420)
        self.assertFalse(scene_window(first, same, True, 3420))
        other = dict(first, swap=3420, observation=25, map_id=8)
        self.assertFalse(scene_window(first, other, True, 3420))
        refreshed = dict(first, swap=3420, observation=25)
        self.assertFalse(scene_window(first, refreshed, True, 4000))
        self.assertTrue(scene_window(first, refreshed, True, 3420))


if __name__ == '__main__':
    unittest.main()
