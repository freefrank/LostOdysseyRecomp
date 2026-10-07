"""Post GitHub pushes to main and published releases to Discord through the sync bot.

Reads the Actions event from GITHUB_EVENT_PATH. Pushes go to DISCORD_COMMITS_CHANNEL as one
embed listing the commits; releases go to DISCORD_RELEASES_CHANNEL (an announcement channel)
and are crossposted to following servers. Nothing in a commit message or release note can
ping anyone. A manual run takes RELEASE_TAG to (re)post an existing release; DRY_RUN=true
only prints the message.
"""
import json
import os
import sys

from discord_sync import DISCORD_API, GITHUB_API, SyncError, env, http_json

NO_PINGS = {'parse': []}
MAX_COMMITS = 10


def clip(text, limit):
    text = (text or '').strip()
    return text if len(text) <= limit else text[:limit - 1].rstrip() + '…'


def push_message(event):
    commits = [c for c in event.get('commits') or [] if c.get('distinct', True)]
    if not commits:
        return None
    branch = event['ref'].rsplit('/', 1)[-1]
    lines = []
    for commit in commits[-MAX_COMMITS:]:
        title = clip(commit['message'].splitlines()[0], 90).replace('[', '(').replace(']', ')')
        author = (commit.get('author') or {}).get('name', '')
        lines.append(f'[`{commit["id"][:7]}`]({commit["url"]}) {title} - {author}')
    if len(commits) > MAX_COMMITS:
        lines.append(f'…and {len(commits) - MAX_COMMITS} earlier')
    return {
        'allowed_mentions': NO_PINGS,
        'embeds': [{
            'title': f'{len(commits)} new commit{"s" if len(commits) != 1 else ""} on {branch}',
            'url': event.get('compare') or event['repository']['html_url'],
            'description': '\n'.join(lines),
            'color': 0x5865F2,
        }],
    }


def release_message(release, repo_name):
    name = release.get('name') or release['tag_name']
    notes = clip(release.get('body'), 3500)
    return {
        'allowed_mentions': NO_PINGS,
        'content': f'**{repo_name} {name}** is out: <{release["html_url"]}>',
        'embeds': [{
            'title': f'{repo_name} {name}',
            'url': release['html_url'],
            'description': notes or 'See the release page for downloads.',
            'color': 0xE0A030,
        }],
    }


def main():
    token = os.environ.get('DISCORD_BOT_TOKEN', '').strip()
    if not token:
        print('DISCORD_BOT_TOKEN is not set; nothing to post.')
        return 0
    dry_run = env('DRY_RUN', 'false').lower() == 'true'
    with open(env('GITHUB_EVENT_PATH'), encoding='utf-8') as stream:
        event = json.load(stream)
    event_name = env('GITHUB_EVENT_NAME')
    repo_name = env('GITHUB_REPOSITORY').split('/', 1)[1]
    tag = os.environ.get('RELEASE_TAG', '').strip()

    if event_name == 'push':
        channel, message = env('DISCORD_COMMITS_CHANNEL'), push_message(event)
    elif event_name == 'release' or tag:
        release = event.get('release') if event_name == 'release' else http_json(
            f'{GITHUB_API}/repos/{env("GITHUB_REPOSITORY")}/releases/tags/{tag}',
            {'Authorization': 'Bearer ' + env('GITHUB_TOKEN')})
        if release.get('draft'):
            print('Draft release; nothing to post.')
            return 0
        channel, message = env('DISCORD_RELEASES_CHANNEL'), release_message(release, repo_name)
    else:
        print(f'Nothing to post for {event_name}.')
        return 0
    if message is None:
        print('No new commits; nothing to post.')
        return 0

    print(json.dumps(message, ensure_ascii=False, indent=2))
    if dry_run:
        return 0
    headers = {'Authorization': 'Bot ' + token}
    posted = http_json(f'{DISCORD_API}/channels/{channel}/messages', headers, 'POST', message)
    print(f'Posted message {posted["id"]} to channel {channel}')
    if channel == os.environ.get('DISCORD_RELEASES_CHANNEL'):
        try:  # publish to servers following the announcement channel
            http_json(f'{DISCORD_API}/channels/{channel}/messages/{posted["id"]}/crosspost', headers, 'POST')
        except SyncError as exc:
            print(f'Crosspost failed: {exc}')
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except SyncError as exc:
        print(f'error: {exc}', file=sys.stderr)
        sys.exit(1)
