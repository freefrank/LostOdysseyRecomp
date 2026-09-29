"""CPU-only checks; no game, filesystem profile changes or timing claims."""
from argparse import Namespace
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'perf'))
from native_probe_log import frontend_delta, probe_environment


class PreparedProbeTest(unittest.TestCase):
    def receipt(self, swap, prepared=None):
        value = dict(swap=swap, mesh_commands=100, native_draws=50, other_draws=50,
                     predicated_skips=50, words=900, state_values=600)
        if prepared is not None:
            value['prepared_draws'] = prepared
        return value

    def test_legacy_receipt_compatibility(self):
        delta = frontend_delta(self.receipt(120), self.receipt(240))
        self.assertNotIn('prepared_draws', delta)

    def test_predicate_only_progress_is_not_prepared_execution(self):
        first, last = self.receipt(120, 3), self.receipt(240, 3)
        last['mesh_commands'] += 20
        last['predicated_skips'] += 20
        delta = frontend_delta(first, last)
        self.assertEqual(delta['prepared_draws'], 0)
        self.assertEqual(delta['native_draws'], 0)

    def test_execution_and_monotonicity(self):
        first, last = self.receipt(120, 3), self.receipt(240, 7)
        last['native_draws'] += 4
        self.assertEqual(frontend_delta(first, last)['prepared_draws'], 4)
        self.assertIsNone(frontend_delta(last, self.receipt(360, 6)))
        self.assertIsNone(frontend_delta(first, self.receipt(240)))

    def test_explicit_injection_and_mode_guard(self):
        args = Namespace(mode='off', native_frontend='mesh', prepared_tail=False,
                         scene='uhra', scene_stats=False, render_timing=False, frontend_stats=False)
        inherited = {'PATH': 'retained', 'LO_NATIVE_FRONTEND_PREPARED': '1'}
        self.assertNotIn('LO_NATIVE_FRONTEND_PREPARED', probe_environment(args, Path('run'), inherited))
        args.prepared_tail = True
        self.assertEqual(probe_environment(args, Path('run'), inherited)['LO_NATIVE_FRONTEND_PREPARED'], '1')
        args.native_frontend = 'off'
        with self.assertRaises(ValueError):
            probe_environment(args, Path('run'), inherited)


if __name__ == '__main__':
    unittest.main()
