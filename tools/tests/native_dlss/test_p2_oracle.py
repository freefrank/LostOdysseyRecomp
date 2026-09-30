import copy
import json
from pathlib import Path
import tempfile
import unittest

import check_p2_oracle as checker


def records():
    result = []
    for frame in (10, 11, 12):
        plan = dict(cpu_serial=frame, geometry_epoch=1, consumer=3, input=[640, 360],
                    output=[1280, 720], output_rect=[0, 0, 1280, 720])
        constants = {"bank_base": "0x4400", "values": [
            dict(constant=i, register=f"0x{0x4400 + i * 4:04x}", u32=[0x3f800000, 0, 0, 0])
            for i in [*range(11), 255]]}
        producer = dict(schema=checker.SCHEMA, event="draw", renderer_frame=frame, draw_id=0,
                        shader={"ps": checker.PRODUCER}, plan=plan, ps_constants=constants,
                        destination=dict(allocation=1, host_format=10, extent=[640, 368]))
        target = dict(guest_base=4096, guest_format=6, allocation=2, host_format=10,
                      write_version=frame, write_ordinal=frame, rect=[0, 0, 640, 360])
        resolve = dict(schema=checker.SCHEMA, event="resolve", kind="color", renderer_frame=frame,
                       source=dict(allocation=1, host_format=10, extent=[640, 368]), destination=target)
        draw = copy.deepcopy(producer)
        draw.update(draw_id=1, sampled_slot0_resolved=dict(**target, write_frame=frame))
        draw["shader"]["ps"] = checker.COPY
        draw["destination"]["allocation"] = 3
        result += [producer, resolve, draw]
    return result


class OracleTest(unittest.TestCase):
    def run_records(self, rows):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "p2-oracle.jsonl"
            path.write_text("\n".join(json.dumps(row) for row in rows), encoding="utf-8")
            return checker.analyze(path)

    def test_complete_never_approves_color(self):
        report = self.run_records(records())
        self.assertEqual(report["status"], "ready_for_manual_color_review")
        self.assertEqual(report["longest_consecutive_run"], 3)
        self.assertEqual(report["chains"][0]["producer_c10_x"], 1.0)
        self.assertEqual(report["color_encoding"], "unknown")
        self.assertFalse(report["p2_accepted"])

    def test_actual_three_frame_directory_layout(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            rows = records()
            for i in range(3):
                folder = root / f"frame-{i+1:02}-f{10+i}"
                folder.mkdir()
                header = dict(schema=checker.SCHEMA, event="capture", renderer_frame=10+i, encoding_claim="unknown")
                (folder / "p2-oracle.jsonl").write_text("\n".join(json.dumps(row) for row in [header, *rows[i*3:i*3+3]]))
                (folder / "unrelated.bin").write_bytes(b"not scanned")
            report = checker.analyze(root)
            self.assertEqual(report["status"], "ready_for_manual_color_review")
            self.assertEqual(report["chains"][0]["producer_source"], "frame-01-f10/p2-oracle.jsonl:2")
            self.assertEqual(checker.main([str(root), "--output", str(root / "review.json")]), 0)

    def test_empty(self):
        self.assertEqual(self.run_records([])["status"], "incomplete")

    def test_missing_constants(self):
        for index in (0, 10, 255):
            with self.subTest(index=index):
                rows = records()
                rows[0]["ps_constants"]["values"] = [e for e in rows[0]["ps_constants"]["values"] if e["constant"] != index]
                self.assertEqual(self.run_records(rows)["status"], "incomplete")

    def test_stale_or_wrong_resolve(self):
        for field in ("allocation", "write_version", "write_ordinal", "host_format", "write_frame", "guest_base"):
            with self.subTest(field=field):
                rows = records(); rows[2]["sampled_slot0_resolved"][field] += 1
                self.assertEqual(self.run_records(rows)["status"], "incomplete")

    def test_missing_or_late_producer(self):
        for rows in (records()[1:], [records()[1], records()[0], *records()[2:]]):
            self.assertEqual(self.run_records(rows)["status"], "incomplete")

    def test_frame_gap(self):
        self.assertEqual(self.run_records(records()[:3] + records()[6:])["status"], "incomplete")

    def test_backwards_frame(self):
        self.assertEqual(self.run_records(records()[3:] + records()[:3])["status"], "incomplete")

    def test_nonfinite_or_invalid_exponent(self):
        for bits in (0x7f800000, 0xff800000, 0x7fc00000, 0, 0xbf800000):
            rows = records(); rows[0]["ps_constants"]["values"][10]["u32"][0] = bits
            self.assertEqual(self.run_records(rows)["status"], "incomplete")

    def test_bad_words_and_register(self):
        for value in (-1, True, 0x100000000, 1.5):
            rows = records(); rows[0]["ps_constants"]["values"][0]["u32"][0] = value
            self.assertEqual(self.run_records(rows)["status"], "incomplete")
        rows = records(); rows[0]["ps_constants"]["values"][0]["register"] = "0x4000"
        self.assertEqual(self.run_records(rows)["status"], "incomplete")

    def test_rectangle_and_plan(self):
        rows = records(); rows[2]["sampled_slot0_resolved"]["rect"] = [0, 0, 320, 360]
        self.assertEqual(self.run_records(rows)["status"], "incomplete")
        rows = records(); rows[2]["plan"]["geometry_epoch"] = 7
        self.assertEqual(self.run_records(rows)["status"], "incomplete")

    def test_malformed_json_and_duplicate_keys(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "trace.jsonl"
            for text in ('{"schema":1,"schema":2}', '{"x":NaN}', '{', 'x' * (checker.MAX_LINE + 1)):
                path.write_text(text)
                with self.assertRaises(ValueError):
                    checker.analyze(path)

    def test_cli_read_only_and_exit_status(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "trace.jsonl"; output = Path(directory) / "review.json"
            path.write_text("\n".join(json.dumps(row) for row in records()))
            before = path.read_bytes()
            self.assertEqual(checker.main([str(path), "--output", str(output)]), 0)
            self.assertEqual(path.read_bytes(), before)
            self.assertEqual(checker.main([str(path), "--output", str(path)]), 1)
            path.write_text("")
            self.assertEqual(checker.main([str(path), "--output", str(output)]), 2)


if __name__ == "__main__":
    unittest.main()
