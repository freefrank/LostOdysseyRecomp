"""Suffix release identity checks with mocked Git; no builds or live tags."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import build_provenance as p
import package_release as package


class SuffixVersionTests(unittest.TestCase):
    def setUp(self):
        self.version = '0.5.1-updaterfix'
        self.state = {'commit': 'a' * 40, 'dirty': False, 'identity': 'source-a'}
        self.stamp = {'source_version': self.version, 'source': self.state}

    def validate(self, tag=None, state=None, stamp=None):
        return p.validate_formal('.', tag or 'v' + self.version, self.version,
                                 state or self.state, [stamp or self.stamp])

    def test_exact_suffix_tag_source_and_commit(self):
        with patch.object(p, 'git', return_value=self.state['commit'].encode()) as git:
            self.assertEqual(self.validate(), 'v0.5.1-updaterfix')
            git.assert_called_once_with('.', 'rev-parse', '--verify',
                                        'refs/tags/v0.5.1-updaterfix^{commit}')

    def test_suffix_mismatch_rejected_before_tag_lookup(self):
        with patch.object(p, 'git') as git:
            for tag in ('v0.5.1', 'v0.5.1-other', 'v0.5.1-Updaterfix', 'v0.5.2-updaterfix'):
                with self.subTest(tag=tag), self.assertRaises(ValueError):
                    self.validate(tag)
            git.assert_not_called()

    def test_missing_exact_tag_rejected(self):
        with patch.object(p, 'git', side_effect=subprocess.CalledProcessError(128, 'git')):
            with self.assertRaises(subprocess.CalledProcessError):
                self.validate()

    def test_tag_and_linked_commit_mismatch_rejected(self):
        with patch.object(p, 'git', return_value=b'b' * 40):
            with self.assertRaises(ValueError):
                self.validate()
        with patch.object(p, 'git', return_value=self.state['commit'].encode()):
            with self.assertRaises(ValueError):
                self.validate(stamp=dict(self.stamp, source=dict(self.state, commit='b' * 40)))

    def test_dirty_checkout_and_linked_build_rejected(self):
        with patch.object(p, 'git') as git:
            with self.assertRaises(ValueError):
                self.validate(state=dict(self.state, dirty=True))
            with self.assertRaises(ValueError):
                self.validate(stamp=dict(self.stamp, source=dict(self.state, dirty=True)))
            git.assert_not_called()

    def test_package_source_and_release_syntax(self):
        self.assertTrue(package.valid_source_version(self.version))
        self.assertEqual(package.normalize_release_version('v' + self.version), self.version)
        self.assertEqual(p.normalize_release_version('v0.5'), '0.5.0')
        for version in ('0.5.1-', '0.5.1-updaterfix..1', '0.5.1-updaterfix/other', '0.5.1-updaterfix\n'):
            with self.subTest(version=version):
                self.assertFalse(package.valid_source_version(version))
                with self.assertRaises(ValueError):
                    package.normalize_release_version('v' + version)

    def test_linked_stamp_suffix_identity(self):
        with tempfile.TemporaryDirectory(prefix='lo-suffix-stamp-') as temp:
            binary = Path(temp) / 'fixture.exe'
            binary.write_bytes(b'synthetic binary; never executed')
            stamp = dict(self.stamp, binary_sha256=p.sha(binary))
            binary.with_suffix('.exe.build.json').write_text(json.dumps(stamp), encoding='utf-8')
            self.assertEqual(p.read_stamp(binary, self.version), stamp)
            for version in ('0.5.1', '0.5.1-other'):
                with self.subTest(version=version), self.assertRaises(ValueError):
                    p.read_stamp(binary, version)
            p.validate_link_source(stamp, self.version, self.state)
            with self.assertRaises(ValueError):
                p.validate_link_source(stamp, '0.5.1', self.state)


if __name__ == '__main__':
    unittest.main(verbosity=2)
