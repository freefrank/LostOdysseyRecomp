#!/usr/bin/env python3
"""Execute the Android workflow's actual private-input selector with fake secrets."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class AndroidBuildInputTest(unittest.TestCase):
    def select(self, url='', key=''):
        workflow = (ROOT / '.github/workflows/android-apk.yml').read_text()
        block = workflow.split('      - name: Select authorized private build input\n', 1)[1]
        block = block.split('        run: |\n', 1)[1].split('      - name:', 1)[0]
        command = '\n'.join(line[10:] for line in block.splitlines() if line.startswith('          '))
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / 'output'
            env = dict(os.environ, LO_BUILD_XEX_URL=url, LO_BUILD_INPUT_KEY=key, GITHUB_OUTPUT=str(output))
            result = subprocess.run(['bash', '-e', '-o', 'pipefail', '-c', command], env=env,
                                    capture_output=True, text=True)
            value = output.read_text() if output.exists() else ''
        for secret in (url, key):
            if secret:
                self.assertNotIn(secret, result.stdout + result.stderr + value)
        return result, value

    def test_missing_input_fails_before_build(self):
        result, value = self.select()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('No game code or APK has been built', result.stdout)
        self.assertEqual(value, '')

    def test_private_url(self):
        result, value = self.select(url='https://example.invalid/secret-disc1?private=sentinel')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(value, 'source=url\n')

    def test_authorized_repository(self):
        result, value = self.select(key='fake-private-deploy-key-sentinel')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(value, 'source=repository\n')

    def test_url_has_precedence(self):
        result, value = self.select(url='https://example.invalid/private', key='fake-private-key')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(value, 'source=url\n')


if __name__ == '__main__':
    unittest.main()
