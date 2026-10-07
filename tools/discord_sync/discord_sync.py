"""Copy Discord forum posts and replies into GitHub issues or Discussions; no external dependencies.

One-way: Discord -> GitHub. DISCORD_FORUMS maps each forum channel id to "issues" or to a
Discussions category slug. Each forum thread becomes one issue or Discussion; replies become
comments. Progress lives in a hidden marker in the issue or Discussion body, so no state
file is needed. Edits, deletions and replies written on GitHub are not synced.
"""
from datetime import datetime, timezone
import json
import os
import re
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

DISCORD_API = 'https://discord.com/api/v10'
GITHUB_API = 'https://api.github.com'
USER_AGENT = 'DiscordBot (https://github.com/freefrank/LostOdysseyRecomp, 1)'
BODY_MARKER = re.compile(r'<!-- discord-sync thread=(\d+) last=(\d+) -->')
COMMENT_MARKER = re.compile(r'<!-- discord-msg (\d+) -->')
MESSAGE_TYPES = {0, 19}  # default and reply; everything else is a system message
MESSAGE_CONTENT_FLAGS = (1 << 18) | (1 << 19)  # GATEWAY_MESSAGE_CONTENT and its _LIMITED form
WRITE_DELAY = 1.0


class SyncError(Exception):
    pass


def http_json(url, headers, method='GET', payload=None):
    data = None if payload is None else json.dumps(payload).encode('utf-8')
    headers = {'User-Agent': USER_AGENT, 'Content-Type': 'application/json', **headers}
    for attempt in range(5):
        req = urllib.request.Request(url, data=data, method=method, headers=headers)
        try:
            with urllib.request.urlopen(req, timeout=60) as response:
                return json.loads(response.read() or b'null')
        except urllib.error.HTTPError as exc:
            body = exc.read()
            if exc.code == 429 and attempt < 4:
                try:
                    wait = float(json.loads(body).get('retry_after', 1))
                except (ValueError, AttributeError):
                    wait = float(exc.headers.get('Retry-After') or 5)
                time.sleep(min(wait, 60) + 0.5)
                continue
            raise SyncError(f'{method} {urllib.parse.urlsplit(url).path} failed '
                            f'({exc.code}): {body[:300].decode("utf-8", "replace")}') from None
        except (urllib.error.URLError, TimeoutError, OSError) as exc:
            if attempt < 2:
                time.sleep(5)
                continue
            raise SyncError(f'{method} {urllib.parse.urlsplit(url).hostname} failed: '
                            f'{type(exc).__name__}') from None
    raise SyncError(f'{method} {url} kept hitting rate limits')


class Discord:
    def __init__(self, token):
        self.headers = {'Authorization': 'Bot ' + token}

    def get(self, path, **params):
        query = '?' + urllib.parse.urlencode(params) if params else ''
        return http_json(DISCORD_API + path + query, self.headers)

    def message(self, channel_id, message_id):
        try:
            return self.get(f'/channels/{channel_id}/messages/{message_id}')
        except SyncError as exc:
            if '(404)' in str(exc):
                return None
            raise

    def messages_after(self, channel_id, after):
        result = []
        while True:
            page = self.get(f'/channels/{channel_id}/messages', after=after, limit=100)
            if not page:
                return result
            page.sort(key=lambda m: int(m['id']))
            result.extend(page)
            after = page[-1]['id']
            if len(page) < 100:
                return result


class GitHub:
    def __init__(self, token, dry_run):
        self.headers = {'Authorization': 'Bearer ' + token}
        self.dry_run = dry_run

    def query(self, query, **variables):
        reply = http_json(GITHUB_API + '/graphql', self.headers, 'POST',
                          {'query': query, 'variables': variables})
        if reply.get('errors'):
            raise SyncError('GraphQL: ' + '; '.join(e.get('message', '?') for e in reply['errors']))
        return reply['data']

    def mutate(self, description, query, **variables):
        print(('[dry run] ' if self.dry_run else '') + description)
        if self.dry_run:
            return None
        data = self.query(query, **variables)
        time.sleep(WRITE_DELAY)
        return data

    def post(self, description, path, payload):
        print(('[dry run] ' if self.dry_run else '') + description)
        if not self.dry_run:
            http_json(GITHUB_API + path, self.headers, 'POST', payload)


# GraphQL per target kind: create, comment, update body, close.
MUTATIONS = {
    'issue': {
        'create': '''mutation($repo: ID!, $title: String!, $body: String!, $labels: [ID!]) {
          createIssue(input: {repositoryId: $repo, title: $title, body: $body, labelIds: $labels}) {
            issue { id number url body closed }
          }
        }''',
        'comment': '''mutation($id: ID!, $body: String!) {
          addComment(input: {subjectId: $id, body: $body}) { clientMutationId }
        }''',
        'update': '''mutation($id: ID!, $body: String!) {
          updateIssue(input: {id: $id, body: $body}) { clientMutationId }
        }''',
        'close': '''mutation($id: ID!) {
          closeIssue(input: {issueId: $id, stateReason: COMPLETED}) { clientMutationId }
        }''',
    },
    'discussion': {
        'create': '''mutation($repo: ID!, $category: ID!, $title: String!, $body: String!) {
          createDiscussion(input: {repositoryId: $repo, categoryId: $category, title: $title, body: $body}) {
            discussion { id number url body closed }
          }
        }''',
        'comment': '''mutation($id: ID!, $body: String!) {
          addDiscussionComment(input: {discussionId: $id, body: $body}) { clientMutationId }
        }''',
        'update': '''mutation($id: ID!, $body: String!) {
          updateDiscussion(input: {discussionId: $id, body: $body}) { clientMutationId }
        }''',
        'close': '''mutation($id: ID!) {
          closeDiscussion(input: {discussionId: $id, reason: RESOLVED}) { clientMutationId }
        }''',
    },
}


def env(name, default=None):
    value = os.environ.get(name, '').strip()
    if value:
        return value
    if default is None:
        raise SyncError(f'{name} is not set')
    return default


def snowflake_time(value):
    return datetime.fromtimestamp(((int(value) >> 22) + 1420070400000) / 1000, timezone.utc)


def stamp(iso):
    when = datetime.fromisoformat(iso.replace('Z', '+00:00')).astimezone(timezone.utc)
    return when.strftime('%Y-%m-%d %H:%M UTC')


def author_name(user):
    name = user.get('global_name') or user.get('username') or 'unknown'
    return re.sub(r'([\\*_~`|\[\]<>#])', r'\\\1', name)


def convert(message, channel_names):
    """Turn Discord message text into GitHub-safe Markdown."""
    text = message.get('content') or ''
    for user in message.get('mentions', []):
        text = re.sub(rf'<@!?{user["id"]}>', lambda _, u=user: '@' + author_name(u), text)
    text = re.sub(r'<@&\d+>', '@role', text)
    text = re.sub(r'<#(\d+)>', lambda m: '#' + channel_names.get(m.group(1), 'channel'), text)
    text = re.sub(r'<a?(:\w+:)\d+>', r'\1', text)
    text = re.sub(r'<t:(\d+)(?::\w)?>', lambda m: datetime.fromtimestamp(
        int(m.group(1)), timezone.utc).strftime('%Y-%m-%d %H:%M UTC'), text)
    # A word joiner after @ keeps Discord names from pinging GitHub users.
    return re.sub(r'@(?=[\w-])', '@⁠', text).strip()


def attachment_line(message, link):
    files = message.get('attachments') or []
    stickers = message.get('sticker_items') or []
    parts = []
    if files:
        names = ', '.join(f'`{f.get("filename", "file")}`' for f in files[:10])
        parts.append(f'{len(files)} attachment(s) in [Discord]({link}): {names}')
    if stickers:
        parts.append(f'Sticker: {", ".join(s.get("name", "?") for s in stickers)}')
    return '\n'.join(f'_{p}_' for p in parts)


def message_text(message, link, channel_names):
    text = convert(message, channel_names)
    extra = attachment_line(message, link)
    body = '\n\n'.join(p for p in (text, extra) if p)
    return body or '_(empty message)_'


def post_body(thread, starter, tags, links, channel_names, last):
    header = (f'Posted by **{author_name(starter["author"]) if starter else "unknown"}** in Discord '
              f'[#{links["forum_name"]}]({links["thread"]}) · '
              f'{stamp(starter["timestamp"]) if starter else snowflake_time(thread["id"]).strftime("%Y-%m-%d %H:%M UTC")}')
    if tags:
        header += ' · ' + ', '.join(tags)
    note = '_Copied from Discord. Replies here do not reach Discord; reply in the Discord thread to reach the author._'
    content = message_text(starter, links['thread'], channel_names) if starter else '_(original post deleted)_'
    return f'{header}\n\n{content}\n\n---\n{note}\n\n<!-- discord-sync thread={thread["id"]} last={last} -->'


def comment_body(message, guild_id, thread_id, channel_names, by_id):
    link = f'https://discord.com/channels/{guild_id}/{thread_id}/{message["id"]}'
    header = f'**{author_name(message["author"])}** · [Discord]({link}) · {stamp(message["timestamp"])}'
    ref = message.get('referenced_message') or by_id.get((message.get('message_reference') or {}).get('message_id'))
    if ref:
        header += f' · reply to {author_name(ref["author"])}'
    return f'{header}\n\n{message_text(message, link, channel_names)}\n\n<!-- discord-msg {message["id"]} -->'


MIRRORS_QUERY = '''
query($owner: String!, $name: String!, $label: [String!], $issues: String, $discussions: String,
      $withIssues: Boolean!, $withDiscussions: Boolean!) {
  repository(owner: $owner, name: $name) {
    issues(first: 50, after: $issues, labels: $label, states: [OPEN, CLOSED]) @include(if: $withIssues) {
      pageInfo { hasNextPage endCursor }
      nodes { id number url body closed comments(last: 30) { nodes { body } } }
    }
    discussions(first: 50, after: $discussions) @include(if: $withDiscussions) {
      pageInfo { hasNextPage endCursor }
      nodes { id number url body closed comments(last: 30) { nodes { body } } }
    }
  }
}'''


def load_mirrors(github, owner, name, label):
    """Find every issue (with the label) and Discussion that already mirrors a Discord thread."""
    mirrors = {}
    cursors = {'issues': None, 'discussions': None}
    pending = {'issues': True, 'discussions': True}
    while any(pending.values()):
        repo = github.query(MIRRORS_QUERY, owner=owner, name=name, label=[label],
                            issues=cursors['issues'], discussions=cursors['discussions'],
                            withIssues=pending['issues'], withDiscussions=pending['discussions'])['repository']
        for field, kind in (('issues', 'issue'), ('discussions', 'discussion')):
            if not pending[field]:
                continue
            page = repo[field]
            for node in page['nodes']:
                match = BODY_MARKER.search(node['body'] or '')
                if not match:
                    continue
                last = int(match.group(2))
                for comment in node['comments']['nodes']:
                    for found in COMMENT_MARKER.findall(comment['body'] or ''):
                        last = max(last, int(found))
                mirrors[match.group(1)] = {**node, 'kind': kind, 'last': last}
            pending[field] = page['pageInfo']['hasNextPage']
            cursors[field] = page['pageInfo']['endCursor']
    return mirrors


def main():
    discord_token = os.environ.get('DISCORD_BOT_TOKEN', '').strip()
    if not discord_token:
        print('DISCORD_BOT_TOKEN is not set; nothing to sync.')
        return 0
    dry_run = env('DRY_RUN', 'false').lower() == 'true'
    owner, name = env('GITHUB_REPOSITORY').split('/', 1)
    guild_id = env('DISCORD_GUILD_ID')
    targets = json.loads(env('DISCORD_FORUMS'))
    label = env('SYNC_LABEL', 'discord')
    triage = os.environ.get('TRIAGE_WORKFLOW', '').strip()
    triage_ref = env('TRIAGE_REF', 'main')
    budget = int(env('MAX_WRITES', '40'))

    discord = Discord(discord_token)
    github = GitHub(env('GITHUB_TOKEN'), dry_run)

    # Without the Message Content intent Discord returns empty text, which would be
    # copied to GitHub as blank posts.
    if not discord.get('/applications/@me').get('flags', 0) & MESSAGE_CONTENT_FLAGS:
        raise SyncError('Message Content Intent is off for the bot (Developer Portal > Bot)')

    channel_names = {c['id']: c['name'] for c in discord.get(f'/guilds/{guild_id}/channels')}
    active = discord.get(f'/guilds/{guild_id}/threads/active')['threads']
    forums, threads = {}, {}
    for forum_id in targets:
        forums[forum_id] = discord.get(f'/channels/{forum_id}')
        threads.update({t['id']: t for t in active if t.get('parent_id') == forum_id})
        archived = discord.get(f'/channels/{forum_id}/threads/archived/public', limit=50)
        threads.update({t['id']: t for t in archived['threads']})
    channel_names.update({t['id']: t['name'] for t in threads.values()})

    repo = github.query('''query($owner: String!, $name: String!, $label: String!) {
      repository(owner: $owner, name: $name) {
        id
        discussionCategories(first: 25) { nodes { id slug } }
        label(name: $label) { id }
      }
    }''', owner=owner, name=name, label=label)['repository']
    categories = {c['slug']: c['id'] for c in repo['discussionCategories']['nodes']}
    for forum_id, target in targets.items():
        if target != 'issues' and target not in categories:
            raise SyncError(f'Discussion category {target!r} for forum {forum_id} not found')
    label_id = (repo.get('label') or {}).get('id')
    if not label_id:
        raise SyncError(f'Label {label!r} not found')
    mirrors = load_mirrors(github, owner, name, label)
    print(f'{len(threads)} forum thread(s), {len(mirrors)} already on GitHub')

    for thread_id in sorted(threads, key=int):
        if budget <= 1:
            print('Write budget used up; the rest waits for the next run.')
            break
        thread = threads[thread_id]
        forum = forums[thread['parent_id']]
        target = targets[thread['parent_id']]
        links = {'thread': f'https://discord.com/channels/{guild_id}/{thread_id}',
                 'forum_name': forum.get('name', 'forum')}
        tag_names = {t['id']: t['name'] for t in forum.get('available_tags', [])}
        applied = thread.get('applied_tags', [])
        mirror = mirrors.get(thread_id)

        if mirror is None:
            starter = discord.message(thread_id, thread_id)
            if starter and (starter['author'].get('bot') or starter.get('type') not in MESSAGE_TYPES):
                continue
            kind = 'issue' if target == 'issues' else 'discussion'
            body = post_body(thread, starter, [tag_names[t] for t in applied if t in tag_names],
                             links, channel_names, thread_id)
            where = 'issue' if kind == 'issue' else f'Discussion ({target})'
            fields = ({'labels': [label_id]} if kind == 'issue' else {'category': categories[target]})
            data = github.mutate(f'Create {where} for "{thread["name"]}" ({thread_id})',
                                 MUTATIONS[kind]['create'], repo=repo['id'],
                                 title=thread['name'][:256], body=body, **fields)
            budget -= 1
            if data is None:
                continue
            created = data['createIssue']['issue'] if kind == 'issue' else data['createDiscussion']['discussion']
            mirror = {**created, 'kind': kind, 'last': int(thread_id)}
            print(f'  -> {mirror["url"]}')
            if kind == 'discussion':
                try:
                    github.mutate('  label', '''mutation($id: ID!, $labels: [ID!]!) {
                      addLabelsToLabelable(input: {labelableId: $id, labelIds: $labels}) { clientMutationId }
                    }''', id=mirror['id'], labels=[label_id])
                except SyncError as exc:
                    print(f'  label not added: {exc}')
                budget -= 1
            elif triage:
                try:
                    github.post(f'  triage #{mirror["number"]}',
                                f'/repos/{owner}/{name}/actions/workflows/{triage}/dispatches',
                                {'ref': triage_ref, 'inputs': {'issue_number': str(mirror['number']),
                                                               'dry_run': 'false'}})
                except SyncError as exc:
                    print(f'  triage not started: {exc}')
                budget -= 1

        kind = mirror['kind']
        last = mirror['last']
        if int(thread.get('last_message_id') or 0) > last:
            new = [m for m in discord.messages_after(thread_id, last) if int(m['id']) > last]
            by_id = {m['id']: m for m in new}
            try:
                for message in new:
                    if budget <= 1:
                        break
                    if not message['author'].get('bot') and message.get('type') in MESSAGE_TYPES:
                        github.mutate(f'Comment on {kind} #{mirror["number"]} from message {message["id"]}',
                                      MUTATIONS[kind]['comment'], id=mirror['id'],
                                      body=comment_body(message, guild_id, thread_id, channel_names, by_id))
                        budget -= 1
                    last = int(message['id'])
            finally:
                if last != mirror['last']:
                    body = BODY_MARKER.sub(f'<!-- discord-sync thread={thread_id} last={last} -->',
                                           mirror['body'])
                    github.mutate(f'Record progress on {kind} #{mirror["number"]}',
                                  MUTATIONS[kind]['update'], id=mirror['id'], body=body)
                    budget -= 1

        solved = {i for i, n in tag_names.items() if n.lower() == 'solved'}
        if solved & set(applied) and not mirror.get('closed'):
            github.mutate(f'Close {kind} #{mirror["number"]}', MUTATIONS[kind]['close'], id=mirror['id'])
            budget -= 1
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except SyncError as exc:
        print(f'error: {exc}', file=sys.stderr)
        sys.exit(1)
