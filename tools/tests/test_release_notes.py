from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'release'))
from extract_release_notes import extract_release_notes


class ReleaseNotesTests(unittest.TestCase):
    def test_linked_version_preserves_bilingual_body_and_nested_headings(self):
        body = '- First change. / 第一项改动。  \r\n\r\n#### Limits / 边界\r\n\r\nStill experimental.\r\n'
        changelog = ('# Changelog\r\n\r\n## Preparing\r\n\r\n'
                     '### [v0.4.0](https://example.test/releases/tag/v0.4.0) — 2026-09-07\r\n'
                     '\r\n' + body + '\r\n### v0.3.0\r\n\r\nOlder release.\r\n')
        self.assertEqual(extract_release_notes(changelog, 'v0.4.0'), body)

    def test_plain_short_version_and_higher_heading_boundary(self):
        changelog = '## v0.2 ###\n\nCurrent.\n\n# History\nOutside.\n'
        self.assertEqual(extract_release_notes(changelog, 'v0.2'), 'Current.\n')

    def test_exact_version_does_not_match_prefix_or_prose_reference(self):
        changelog = ('## v0.4.0-rc1\nCandidate.\n## v0.4.0.1\nOther.\n'
                     '## Development after v0.4.0\nReference.\n')
        with self.assertRaisesRegex(ValueError, 'No changelog heading'):
            extract_release_notes(changelog, 'v0.4.0')

    def test_duplicate_version_fails_even_at_different_heading_levels(self):
        changelog = ('## [v0.4.0](https://example.test)\nFirst.\n'
                     '### v0.4.0\nDuplicate.\n')
        with self.assertRaisesRegex(ValueError, 'Multiple changelog headings'):
            extract_release_notes(changelog, 'v0.4.0')

    def test_empty_version_section_fails(self):
        for suffix in ('', '\n\t\n## v0.3.0\nOther.\n'):
            with self.subTest(suffix=suffix):
                with self.assertRaisesRegex(ValueError, 'is empty'):
                    extract_release_notes('## v0.4.0\n' + suffix, 'v0.4.0')

    def test_fenced_headings_are_preserved_and_not_matched(self):
        body = ('Example:\n\n````markdown\n## v0.4.0\n```\n'
                '# Sample\n````\n\n~~~text\n## v0.1\n~~~\nFinal.\n')
        changelog = '## v0.4.0\n\n' + body + '\n## v0.3.0\nOld.\n'
        self.assertEqual(extract_release_notes(changelog, 'v0.4.0'), body)

    def test_last_section_keeps_missing_final_newline(self):
        self.assertEqual(extract_release_notes('## v0.2\n\nLast.', 'v0.2'), 'Last.')

    def test_invalid_requested_version_fails(self):
        for version in ('', '0.4.0', 'v0.4.0 Other', 'v0.4.0\n'):
            with self.subTest(version=version):
                with self.assertRaisesRegex(ValueError, 'Invalid version tag'):
                    extract_release_notes('## v0.4.0\nContent.', version)

    def test_cli_writes_utf8_and_does_not_overwrite_output_on_extraction_failure(self):
        script = Path(__file__).resolve().parents[1] / 'release' / 'extract_release_notes.py'
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            changelog = root / 'CHANGELOG.md'
            output = root / 'notes' / 'release.md'
            changelog.write_bytes('\ufeff## v0.4.0\r\n\r\n- 中文。\r\n'.encode('utf-8'))
            command = [sys.executable, str(script), '--changelog', str(changelog),
                       '--version', 'v0.4.0', '--output', str(output)]
            result = subprocess.run(command, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            expected = '- 中文。\r\n'.encode('utf-8')
            self.assertEqual(output.read_bytes(), expected)

            changelog.write_text('## v0.3.0\nOld.\n', encoding='utf-8')
            result = subprocess.run(command, capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b'No changelog heading', result.stderr)
            self.assertEqual(output.read_bytes(), expected)


if __name__ == '__main__':
    unittest.main()
