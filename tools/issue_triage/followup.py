"""Pure decisions for follow-up triage when the issue author attaches a log in a later comment."""
import re

import logs

INITIAL = '<!-- lost-odyssey-issue-triage:v1 -->'
BOT = 'github-actions[bot]'
MAINTAINER = ('OWNER', 'MEMBER', 'COLLABORATOR')
MAX_FOLLOWUPS = 2
ANALYZED = re.compile(r'<!-- lost-odyssey-issue-triage:v1 analyzed=([0-9,]*) -->')
ID = re.compile(r'/user-attachments/files/(\d+)/')


def marker(ids):
    return '<!-- lost-odyssey-issue-triage:v1 analyzed=' + ','.join(sorted(ids, key=int)) + ' -->'


def parse_analyzed(body):
    """Attachment IDs recorded in a follow-up marker; empty for any other text."""
    found = set()
    for m in ANALYZED.finditer(str(body or '')):
        found.update(i for i in m.group(1).split(',') if i)
    return found


def attachment_ids(text):
    """Attachment IDs of every .log/.txt/.zip link in the text (no cap)."""
    return {ID.search(url).group(1) for url in logs.attachment_urls(text)}


def is_bot(comment):
    return (comment.get('user') or {}).get('type') == 'Bot'


def bot_comments(comments):
    return [c for c in comments if (c.get('user') or {}).get('login') == BOT]


def same_user(a, b):
    return bool(a) and bool(b) and a.lower() == b.lower()


def author_comments(issue, comments, trigger):
    """The issue author's non-bot comments, oldest first, always including the triggering comment."""
    author = (issue.get('user') or {}).get('login')
    result = [c for c in comments if not is_bot(c) and same_user((c.get('user') or {}).get('login'), author)]
    if not any(c.get('id') == trigger.get('id') for c in result):
        result.append(trigger)
    return result


def author_text(issue, comments, trigger):
    """Issue body plus all author comments, newest comment first, so the newest links win ties."""
    parts = [str(c.get('body') or '') for c in reversed(author_comments(issue, comments, trigger))]
    return '\n'.join(parts + [str(issue.get('body') or '')])


def decide(issue, trigger, comments):
    """(ok, reason, new_ids). new_ids are every author attachment not analysed yet, to record in the marker."""
    if issue.get('pull_request'):
        return False, 'pull request', []
    if issue.get('state') != 'open':
        return False, 'issue is closed', []
    if is_bot(trigger):
        return False, 'comment is from a bot', []
    if not same_user((trigger.get('user') or {}).get('login'), (issue.get('user') or {}).get('login')):
        return False, 'commenter is not the issue author', []
    bots = bot_comments(comments)
    if not any(INITIAL in str(c.get('body') or '') for c in bots):
        return False, 'initial analysis has not been posted', []
    analyzed = attachment_ids(issue.get('body'))
    for c in bots:
        analyzed |= parse_analyzed(c.get('body'))
    if not (attachment_ids(trigger.get('body')) - analyzed):
        return False, 'no new attachment in the comment', []
    if sum(1 for c in bots if parse_analyzed(c.get('body'))) >= MAX_FOLLOWUPS:
        return False, 'follow-up limit reached', []
    trigger_id = trigger.get('id') or 0
    if any(c.get('author_association') in MAINTAINER and not is_bot(c) and (c.get('id') or 0) > trigger_id
           for c in comments):
        return False, 'a maintainer replied after the comment', []
    pending = set()
    for c in author_comments(issue, comments, trigger):
        pending |= attachment_ids(c.get('body'))
    return True, 'ok', sorted(pending - analyzed, key=int)
