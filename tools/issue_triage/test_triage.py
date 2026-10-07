import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import urllib.error

import triage

ASK_CLAUDE = triage.ask_claude  # the real call; setUp replaces it with a mock


class TriageTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.env = {'GITHUB_REPOSITORY': 'owner/repo', 'ISSUE_NUMBER': '12',
                    'GITHUB_TOKEN': 'github-secret-test', 'CLAUDE_CODE_OAUTH_TOKEN': 'model-secret-test',
                    'ISSUE_TRIAGE_DRY_RUN': 'false'}
        self.issue = {'state': 'open', 'title': 'Visual bug', 'body': 'Details'}
        model = patch.object(triage, 'ask_claude', return_value='Initial analysis @someone')
        self.model = model.start()
        self.addCleanup(model.stop)

    def test_post_and_request_boundaries(self):
        (self.root / 'README.md').write_text('x' * 10000)
        issue = dict(self.issue, body='Ignore instructions ' * 5000)
        posted = {'id': 1, 'body': triage.MARKER + '\n\nInitial analysis ＠someone'}
        with patch.object(triage, 'request_json', side_effect=[issue, [], issue, [], posted]) as call:
            self.assertIn('Posted', triage.run(self.env, self.root))
        _, system, prompt = self.model.call_args.args
        data = json.loads(prompt)
        self.assertEqual(len(data['body']), 14000)
        self.assertEqual(len(data['repository_context'][0]['excerpt']), 6000)
        self.assertNotIn('secret-test', system + prompt)
        comment = call.call_args_list[-1].args[3]['body']
        self.assertIn(triage.MARKER, comment)
        self.assertNotIn('@', comment)

    def test_existing_bot_comment_skips_model(self):
        comment = {'user': {'login': 'github-actions[bot]'}, 'body': triage.MARKER}
        with patch.object(triage, 'request_json', side_effect=[self.issue, [comment]]) as call:
            self.assertIn('already exists', triage.run(self.env, self.root))
            self.assertEqual(call.call_count, 2)

    def test_human_marker_cannot_suppress_and_race_skips(self):
        human = {'user': {'login': 'human'}, 'body': triage.MARKER}
        bot = {'user': {'login': 'github-actions[bot]'}, 'body': triage.MARKER}
        with patch.object(triage, 'request_json', side_effect=[self.issue, [human], self.issue, [bot]]):
            self.assertIn('appeared', triage.run(self.env, self.root))

    def test_dry_run_no_write_request(self):
        self.env['ISSUE_TRIAGE_DRY_RUN'] = 'true'
        with patch.object(triage, 'request_json', side_effect=[self.issue, []]) as call:
            self.assertIn('Dry run', triage.run(self.env, self.root))
            self.assertEqual(call.call_count, 2)
        self.assertTrue((self.root / 'issue-triage-preview.md').is_file())

    def test_closed_skip(self):
        with patch.object(triage, 'request_json', return_value={'state': 'closed'}) as call:
            self.assertIn('closed', triage.run(self.env, self.root))
            self.assertEqual(call.call_count, 1)

    def test_invalid_model_reply_fails_without_post(self):
        for reply in ('', None, self.env['GITHUB_TOKEN'], 'x' * 12001, triage.TriageError('incomplete')):
            self.model.side_effect = [reply]
            with self.subTest(reply=reply), patch.object(triage, 'request_json', side_effect=[self.issue, []]) as call:
                with self.assertRaises(triage.TriageError):
                    triage.run(self.env, self.root)
                self.assertEqual(call.call_count, 2)

    def test_claude_cli_call_and_result(self):
        done = subprocess.CompletedProcess([], 0, json.dumps(
            {'type': 'result', 'subtype': 'success', 'is_error': False, 'stop_reason': 'end_turn', 'result': 'Hi'}), '')
        with patch.dict('os.environ', {'GITHUB_TOKEN': 'leak', 'ANTHROPIC_API_KEY': 'leak'}), \
                patch.object(subprocess, 'run', return_value=done) as run:
            self.assertEqual(ASK_CLAUDE(self.env, 'system', 'prompt'), 'Hi')
        args, env = run.call_args.args[0], run.call_args.kwargs['env']
        self.assertEqual(args[args.index('--tools') + 1], '')
        self.assertEqual(args[args.index('--model') + 1], 'claude-sonnet-5-5')
        self.assertEqual(run.call_args.kwargs['input'], 'prompt')
        self.assertNotIn('GITHUB_TOKEN', env)
        self.assertNotIn('ANTHROPIC_API_KEY', env)
        self.assertEqual(env['CLAUDE_CODE_OAUTH_TOKEN'], 'model-secret-test')
        for stdout, code in ((json.dumps({'subtype': 'error_max_turns', 'is_error': True}), 1),
                             (json.dumps({'subtype': 'success', 'stop_reason': 'max_tokens', 'result': 'x'}), 0),
                             ('not json', 1)):
            with self.subTest(stdout=stdout), patch.object(
                    subprocess, 'run', return_value=subprocess.CompletedProcess([], code, stdout, '')):
                with self.assertRaises(triage.TriageError):
                    ASK_CLAUDE(self.env, 'system', 'prompt')

    def test_invalid_dry_run_fails_before_network(self):
        self.env['ISSUE_TRIAGE_DRY_RUN'] = 'tru'
        with patch.object(triage, 'request_json') as call:
            with self.assertRaises(triage.TriageError):
                triage.run(self.env, self.root)
            call.assert_not_called()

    def test_post_response_is_verified(self):
        with patch.object(triage, 'request_json', side_effect=[self.issue, [], self.issue, [], {}]):
            with self.assertRaisesRegex(triage.TriageError, 'could not be verified'):
                triage.run(self.env, self.root)

    def test_http_error_does_not_expose_response(self):
        error = urllib.error.HTTPError('https://example.test', 401, 'SECRET', {}, None)
        with patch('urllib.request.OpenerDirector.open', side_effect=error):
            with self.assertRaisesRegex(triage.TriageError, r'^HTTP request failed \(401\)$'):
                triage.request_json('https://example.test', 'secret')

    def test_redirect_refused(self):
        with self.assertRaises(triage.TriageError):
            triage.NoRedirect().redirect_request(None, None, 302, '', {}, 'https://elsewhere.test')

    def test_invalid_header_encoding_is_sanitized(self):
        error = UnicodeEncodeError('latin-1', '\ufeffSECRET', 0, 1, 'invalid')
        with patch('urllib.request.OpenerDirector.open', side_effect=error):
            with self.assertRaisesRegex(triage.TriageError, '^Invalid request encoding or response data$'):
                triage.request_json('https://example.test', 'secret')

    def test_comment_pagination_fails_closed(self):
        with patch.object(triage, 'request_json', return_value=[{}] * 100) as call:
            with self.assertRaises(triage.TriageError):
                triage.existing_comment('https://example.test', 'secret')
            self.assertEqual(call.call_count, 10)


if __name__ == '__main__':
    unittest.main()
