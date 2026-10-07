import unittest
from pathlib import Path
import tempfile
from unittest.mock import patch
import triage

class MentionTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name)
        self.url='https://api.github.com/repos/freefrank/LostOdysseyRecomp/issues/27'
        self.env={'GITHUB_REPOSITORY':'freefrank/LostOdysseyRecomp','ISSUE_NUMBER':'27',
                  'ISSUE_COMMENT_ID':'123','GITHUB_TOKEN':'github-secret','CLAUDE_CODE_OAUTH_TOKEN':'model-secret',
                  'ISSUE_TRIAGE_DRY_RUN':'true','SOURCE_REVISION':'abc123'}
        self.trigger={'id':123,'issue_url':self.url,'user':{'login':'freefrank','type':'User'},
                      'author_association':'OWNER','body':'@codex analyze subtitle loading code'}
        self.issue={'state':'open','title':'Subtitles missing','body':'PAL edition'}
        model=patch.object(triage,'ask_claude',return_value='Analysis with code evidence')
        self.model=model.start()
        self.addCleanup(model.stop)

    def test_mention_context_and_marker(self):
        comments=[{'user':{'login':'player','type':'User'},'body':'additional details'}]
        with patch.object(triage,'request_json',side_effect=[self.trigger,self.issue,[],comments]) as req, patch.object(triage,'retrieve',return_value=[{'file':'video.cpp','start_line':10,'text':'10: subtitle();'}]):
            result=triage.run(self.env,self.root)
        self.assertIn('Dry run',result)
        prompt=self.model.call_args.args[2]
        self.assertIn('source_excerpts',prompt)
        self.assertIn('additional details',prompt)
        self.assertIn('abc123',prompt)
        self.assertIn('codex-comment:123',(self.root/'issue-triage-preview.md').read_text())

    def test_untrusted_and_non_mentions_skip(self):
        for edits in ({'author_association':'NONE'},{'body':'email user@codex.com'},
                      {'body':'@codex-other'}, {'user':{'type':'Bot'}}):
            with self.subTest(edits=edits), patch.object(triage,'request_json',return_value=self.trigger|edits) as req:
                self.assertIn('Skipped',triage.run(self.env,self.root))
                self.assertEqual(req.call_count,1)

    def test_wrong_issue_rejected(self):
        with patch.object(triage,'request_json',return_value=self.trigger|{'issue_url':self.url+'0'}):
            with self.assertRaises(triage.TriageError): triage.run(self.env,self.root)

    def test_closed_issue_can_be_analyzed(self):
        with patch.object(triage,'request_json',side_effect=[self.trigger,self.issue|{'state':'closed'},[],[]]), patch.object(triage,'retrieve',return_value=[]):
            self.assertIn('Dry run',triage.run(self.env,self.root))

    def test_each_mention_has_independent_deduplication(self):
        prior=[{'user':{'login':'github-actions[bot]'},'body':triage.MARKER}]
        with patch.object(triage,'request_json',side_effect=[self.trigger,self.issue,prior,[]]), patch.object(triage,'retrieve',return_value=[]):
            self.assertIn('Dry run',triage.run(self.env,self.root))
        prior[0]['body']='<!-- lost-odyssey-codex-comment:123 -->'
        with patch.object(triage,'request_json',side_effect=[self.trigger,self.issue,prior]) as req:
            self.assertIn('already exists',triage.run(self.env,self.root))
            self.assertEqual(req.call_count,3)

    def test_changed_trigger_prevents_post(self):
        self.env['ISSUE_TRIAGE_DRY_RUN']='false'
        with patch.object(triage,'request_json',side_effect=[self.trigger,self.issue,[],[],self.issue,self.trigger|{'body':'@codex changed request'}]) as req, patch.object(triage,'retrieve',return_value=[]):
            self.assertIn('changed',triage.run(self.env,self.root))
            self.assertEqual(req.call_count,6)

    def test_mention_posts_one_verified_reply(self):
        self.env['ISSUE_TRIAGE_DRY_RUN']='false'
        body='<!-- lost-odyssey-codex-comment:123 -->\n\nAnalysis with code evidence'
        with patch.object(triage,'request_json',side_effect=[self.trigger,self.issue,[],[],self.issue,self.trigger,[],{'id':999,'body':body}]) as req, patch.object(triage,'retrieve',return_value=[]):
            self.assertIn('Posted',triage.run(self.env,self.root))
            self.assertEqual(req.call_args.args,(self.url+'/comments','github-secret','POST',{'body':body}))

if __name__=='__main__': unittest.main()
