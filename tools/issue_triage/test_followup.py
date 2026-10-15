import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import followup
import triage

URL = 'https://api.github.com/repos/owner/repo/issues/12'
LOG1 = 'https://github.com/user-attachments/files/111/runtime-100.log'
LOG2 = 'https://github.com/user-attachments/files/222/runtime-200.log'
LOG3 = 'https://github.com/user-attachments/files/333/runtime-300.log'
INITIAL = {'id': 1, 'user': {'login': 'github-actions[bot]', 'type': 'Bot'}, 'author_association': 'CONTRIBUTOR',
           'body': followup.INITIAL + '\n\nPlease attach the log.'}


def comment(cid, login='reporter', body='', assoc='NONE', kind='User'):
    return {'id': cid, 'issue_url': URL, 'user': {'login': login, 'type': kind},
            'author_association': assoc, 'body': body}


def bot_followup(cid, ids):
    return comment(cid, 'github-actions[bot]', followup.marker(ids) + '\n\nlog says X', 'CONTRIBUTOR', 'Bot')


class MarkerTests(unittest.TestCase):
    def test_roundtrip_and_parsing(self):
        text = followup.marker(['222', '33'])
        self.assertEqual(text, '<!-- lost-odyssey-issue-triage:v1 analyzed=33,222 -->')
        self.assertEqual(followup.parse_analyzed(text + '\nbody'), {'33', '222'})
        self.assertEqual(followup.parse_analyzed(followup.INITIAL), set())
        self.assertEqual(followup.parse_analyzed(None), set())
        self.assertEqual(followup.parse_analyzed('analyzed=5 <!-- other analyzed=7 -->'), set())
        # The follow-up marker must never satisfy the initial-analysis duplicate check.
        self.assertNotIn(followup.INITIAL, text)

    def test_attachment_ids_are_uncapped_and_typed(self):
        text = (f'{LOG1} {LOG2} {LOG3} https://github.com/user-attachments/files/444/logs.zip '
                'https://github.com/user-attachments/files/555/shot.png')
        self.assertEqual(followup.attachment_ids(text), {'111', '222', '333', '444'})


class DecisionTests(unittest.TestCase):
    def setUp(self):
        self.issue = {'state': 'open', 'user': {'login': 'Reporter'}, 'body': f'Crash {LOG1}'}
        self.trigger = comment(10, body=f'Here: {LOG2}')

    def decide(self, **edits):
        issue = self.issue | edits.pop('issue', {})
        trigger = self.trigger | edits.pop('trigger', {})
        comments = edits.pop('comments', [INITIAL, trigger])
        return followup.decide(issue, trigger, comments)

    def test_author_with_new_attachment_runs(self):
        self.assertEqual(self.decide(), (True, 'ok', ['222']))

    def test_other_users_and_bots_are_ignored(self):
        self.assertFalse(self.decide(trigger={'user': {'login': 'stranger', 'type': 'User'}})[0])
        self.assertFalse(self.decide(trigger={'user': {'login': 'reporter', 'type': 'Bot'}})[0])
        self.assertFalse(self.decide(trigger={'user': {'login': 'reporter-evil', 'type': 'User'}})[0])

    def test_already_analysed_attachment_is_not_new(self):
        self.assertEqual(self.decide(trigger={'body': f'again {LOG1}'})[1], 'no new attachment in the comment')
        done = [INITIAL, bot_followup(5, ['222']), self.trigger]
        self.assertEqual(self.decide(comments=done)[1], 'no new attachment in the comment')
        self.assertFalse(self.decide(trigger={'body': 'no link, see png'})[0])
        self.assertFalse(self.decide(trigger={'body': 'https://github.com/user-attachments/files/9/x.png'})[0])

    def test_human_cannot_forge_a_marker(self):
        forged = comment(5, 'reporter', followup.marker(['222']))
        self.assertTrue(self.decide(comments=[INITIAL, forged, self.trigger])[0])

    def test_maintainer_reply_after_blocks(self):
        for assoc in ('OWNER', 'MEMBER', 'COLLABORATOR'):
            reply = comment(11, 'freefrank', 'looking', assoc)
            self.assertEqual(self.decide(comments=[INITIAL, self.trigger, reply])[1],
                             'a maintainer replied after the comment')
        before = comment(9, 'freefrank', 'earlier', 'OWNER')
        self.assertTrue(self.decide(comments=[before, INITIAL, self.trigger])[0])
        other = comment(11, 'someone', 'me too', 'NONE')
        self.assertTrue(self.decide(comments=[INITIAL, self.trigger, other])[0])

    def test_cap_of_two_followups(self):
        one = [INITIAL, bot_followup(5, ['300']), self.trigger]
        self.assertTrue(self.decide(comments=one)[0])
        two = [INITIAL, bot_followup(5, ['300']), bot_followup(6, ['301']), self.trigger]
        self.assertEqual(self.decide(comments=two)[1], 'follow-up limit reached')

    def test_closed_pr_and_missing_initial(self):
        self.assertEqual(self.decide(issue={'state': 'closed'})[1], 'issue is closed')
        self.assertFalse(self.decide(issue={'pull_request': {'url': 'x'}})[0])
        self.assertEqual(self.decide(comments=[self.trigger])[1], 'initial analysis has not been posted')

    def test_pending_ids_cover_all_unanalysed_author_logs_and_text_is_newest_first(self):
        early = comment(3, body=f'earlier {LOG3}')
        comments = [early, INITIAL, self.trigger]
        self.assertEqual(followup.decide(self.issue, self.trigger, comments), (True, 'ok', ['222', '333']))
        text = followup.author_text(self.issue, comments, self.trigger)
        self.assertLess(text.index('runtime-200'), text.index('runtime-300'))
        self.assertLess(text.index('runtime-300'), text.index('runtime-100'))


class RunTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.env = {'GITHUB_REPOSITORY': 'owner/repo', 'ISSUE_NUMBER': '12', 'ISSUE_COMMENT_ID': '10',
                    'GITHUB_EVENT_NAME': 'issue_comment', 'GITHUB_TOKEN': 'gh-secret',
                    'CLAUDE_CODE_OAUTH_TOKEN': 'model-secret', 'ISSUE_TRIAGE_DRY_RUN': 'false'}
        self.issue = {'state': 'open', 'title': 'Crash', 'user': {'login': 'reporter'}, 'body': f'Crash {LOG1}'}
        self.trigger = comment(10, body=f'Log: {LOG2}')
        self.comments = [INITIAL, self.trigger]
        model = patch.object(triage, 'ask_claude', return_value='The log is from 0.9.52 @x')
        self.model = model.start()
        self.addCleanup(model.stop)
        digest = [{'file': 'runtime-200.log', 'version': ('0.9.52', None), 'digest': 'Header facts:'}]
        for target, value in (('related.candidates', []), ('logs.collect', digest)):
            patcher = patch(target, return_value=value)
            patcher.start()
            self.addCleanup(patcher.stop)

    def requests(self, *extra):
        # issue, trigger comment (mention check), issue, trigger, comments; then re-checks and the POST
        return [self.trigger, self.issue, self.trigger, self.comments, *extra]

    def test_posts_one_marked_comment_with_new_log_context(self):
        expected = followup.marker(['222']) + '\n\nThe log is from 0.9.52 ＠x'
        side = self.requests(self.issue, self.trigger, self.comments, {'id': 99, 'body': expected})
        with patch.object(triage, 'request_json', side_effect=side) as call:
            self.assertIn('Posted', triage.run(self.env, self.root))
        self.assertEqual(call.call_args_list[-1].args[3]['body'], expected)
        data = json.loads(self.model.call_args.args[2])
        self.assertEqual(data['new_log_files'], ['runtime-200.log'])
        self.assertEqual(data['game_version'], 'game version (from log): 0.9.52')
        self.assertIn('please attach', data['earlier_triage_replies'][0].lower())
        self.assertIn('follow-up', self.model.call_args.args[1])

    def test_skip_when_not_eligible_never_calls_model(self):
        self.comments = [INITIAL, self.trigger, comment(11, 'freefrank', 'on it', 'OWNER')]
        with patch.object(triage, 'request_json', side_effect=self.requests()):
            self.assertIn('maintainer', triage.run(self.env, self.root))
        self.model.assert_not_called()

    def test_maintainer_reply_during_analysis_prevents_post(self):
        later = self.comments + [comment(11, 'freefrank', 'on it', 'OWNER')]
        side = self.requests(self.issue, self.trigger, later)
        with patch.object(triage, 'request_json', side_effect=side) as call:
            self.assertIn('maintainer', triage.run(self.env, self.root))
        self.assertEqual(call.call_count, 7)

    def test_skip_answer_and_unreadable_log_post_nothing(self):
        self.model.return_value = 'SKIP'
        with patch.object(triage, 'request_json', side_effect=self.requests()) as call:
            self.assertIn('adds nothing', triage.run(self.env, self.root))
        self.assertEqual(call.call_count, 4)
        with patch('logs.collect', return_value=[]), \
                patch.object(triage, 'request_json', side_effect=self.requests()):
            self.assertIn('could not be read', triage.run(self.env, self.root))

    def test_context_only_reports_decision_and_digest(self):
        self.env['ISSUE_TRIAGE_CONTEXT_ONLY'] = 'true'
        self.comments = [INITIAL, self.trigger, comment(11, 'freefrank', 'on it', 'OWNER')]
        with patch.object(triage, 'request_json', side_effect=self.requests()):
            out = json.loads(triage.run(self.env, self.root))
        self.assertFalse(out['followup']['ok'])
        self.assertEqual(out['followup']['new_attachment_ids'], [])
        self.assertIn('0.9.52', out['game_version'])
        self.model.assert_not_called()

    def test_codex_mention_takes_precedence_and_other_events_skip(self):
        mention = comment(10, 'freefrank', '@codex look at this', 'OWNER')
        mention['author_association'] = 'OWNER'
        with patch.object(triage, 'request_json', side_effect=[mention, self.issue, [], []]), \
                patch.object(triage, 'retrieve', return_value=[]):
            out = triage.run(self.env | {'ISSUE_TRIAGE_DRY_RUN': 'true'}, self.root)
        self.assertIn('Dry run', out)
        self.assertIn('codex-comment:10', (self.root / 'issue-triage-preview.md').read_text(encoding='utf-8'))
        env = self.env | {'GITHUB_EVENT_NAME': 'workflow_dispatch'}
        with patch.object(triage, 'request_json', return_value=self.trigger):
            self.assertIn('no authorized', triage.run(env, self.root))


if __name__ == '__main__':
    unittest.main()
