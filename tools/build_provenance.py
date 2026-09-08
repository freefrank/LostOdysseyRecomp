"""Record successful link identity and enforce release source provenance."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

PATCHES = {'thirdparty/plume': 'tools/patches/plume-lostodyssey.patch',
           'tools/XenonRecomp': 'tools/patches/XenonRecomp-lostodyssey.patch'}

def sha(path):
    with Path(path).open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()

def git(root, *args, env=None):
    return subprocess.check_output(['git', '-C', str(root), *args], env=env, stderr=subprocess.PIPE)

def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(',', ':')).encode()).hexdigest()

def tracked_tree(path, base, patch=None):
    # A private index: never stage or change the user's real index/worktree.
    with tempfile.TemporaryDirectory(prefix='lo-provenance-') as temporary:
        env = dict(os.environ, GIT_INDEX_FILE=str(Path(temporary) / 'index'))
        git(path, 'read-tree', base, env=env)
        if patch:
            git(path, 'apply', '--cached', '--whitespace=nowarn', str(patch), env=env)
        else:
            git(path, 'add', '-u', '--', '.', env=env)
        return git(path, 'write-tree', env=env).decode().strip()

def source_state(root):
    root = Path(root).resolve()
    status = git(root, 'status', '--porcelain=v1', '-z', '--untracked-files=all', '--ignore-submodules=all')
    result = {'commit': git(root, 'rev-parse', 'HEAD').decode().strip(),
              'root_status': status.decode('utf-8', 'replace').split('\0')[:-1], 'submodules': []}
    def visit(parent, prefix=''):
        indexed_links = {item.split(b'\t', 1)[1]: item.split()[1].decode()
                         for item in git(parent, 'ls-files', '--stage', '-z').split(b'\0')
                         if item.startswith(b'160000 ')}
        head_links = {item.split(b'\t', 1)[1]: item.split()[2].decode()
                      for item in git(parent, 'ls-tree', '-r', '-z', 'HEAD').split(b'\0')
                      if item.startswith(b'160000 ')}
        for raw_path in sorted(head_links.keys() | indexed_links.keys()):
            relative = raw_path.decode('utf-8')
            name = prefix + relative
            path = parent / relative
            pinned = head_links.get(raw_path)
            indexed_link = indexed_links.get(raw_path)
            entry = {'path': name, 'pinned_commit': pinned, 'indexed_commit': indexed_link, 'dirty': True}
            result['submodules'].append(entry)
            if not (path / '.git').exists():
                entry['reason'] = 'submodule not initialized'
                continue
            entry['commit'] = git(path, 'rev-parse', 'HEAD').decode().strip()
            sub_status = git(path, 'status', '--porcelain=v1', '-z', '--untracked-files=all', '--ignore-submodules=all')
            entry['status_sha256'] = hashlib.sha256(sub_status).hexdigest()
            entry['tracked_diff_sha256'] = hashlib.sha256(git(path, 'diff', '--binary', 'HEAD', '--ignore-submodules=all')).hexdigest()
            entry['indexed_diff_sha256'] = hashlib.sha256(git(path, 'diff', '--cached', '--binary', 'HEAD', '--ignore-submodules=all')).hexdigest()
            untracked = git(path, 'ls-files', '--others', '--exclude-standard', '-z').split(b'\0')
            entry['untracked'] = {p.decode('utf-8'): sha(path / p.decode('utf-8')) for p in untracked if p and (path / p.decode('utf-8')).is_file()}
            if indexed_link != pinned:
                entry['reason'] = 'staged gitlink differs from HEAD'
            elif entry['commit'] != pinned:
                entry['reason'] = 'submodule commit differs from gitlink'
            elif name in PATCHES:
                patch = root / PATCHES[name]
                entry['patch'] = PATCHES[name]
                entry['patch_sha256'] = sha(patch)
                try:
                    expected = tracked_tree(path, pinned, patch)
                    actual = tracked_tree(path, pinned)
                    indexed = git(path, 'write-tree').decode().strip()
                    base_tree = git(path, 'rev-parse', pinned + '^{tree}').decode().strip()
                    untracked = git(path, 'ls-files', '--others', '--exclude-standard', '-z')
                    entry.update(expected_tree=expected, actual_tree=actual)
                    entry['dirty'] = actual != expected or indexed not in (base_tree, expected) or bool(untracked)
                    entry['reason'] = 'unknown changes' if entry['dirty'] else 'exact tracked build patch'
                except subprocess.CalledProcessError:
                    entry['reason'] = 'tracked patch does not apply to pinned source'
            else:
                entry['dirty'] = bool(sub_status)
                entry['reason'] = 'unknown changes' if sub_status else 'pinned clean submodule'
            visit(path, name + '/')
    visit(root)
    result['dirty'] = bool(status) or any(x['dirty'] for x in result['submodules'])
    # Include patch/source contents, not just the names emitted by git status.
    result['tracked_diff_sha256'] = hashlib.sha256(git(root, 'diff', '--binary', 'HEAD', '--ignore-submodules=all')).hexdigest()
    result['indexed_diff_sha256'] = hashlib.sha256(git(root, 'diff', '--cached', '--binary', 'HEAD', '--ignore-submodules=all')).hexdigest()
    untracked = git(root, 'ls-files', '--others', '--exclude-standard', '-z').split(b'\0')
    result['untracked'] = {p.decode('utf-8'): sha(root / p.decode('utf-8')) for p in untracked if p and (root / p.decode('utf-8')).is_file()}
    result['identity'] = digest(result)
    return result

def read_stamp(binary, version):
    binary = Path(binary)
    stamp_path = binary.with_suffix(binary.suffix + '.build.json')
    if not stamp_path.is_file():
        raise ValueError(f'Missing successful-link provenance: {stamp_path}; rebuild this target.')
    stamp = json.loads(stamp_path.read_text(encoding='utf-8'))
    if stamp.get('source_version') != version or stamp.get('binary_sha256') != sha(binary):
        raise ValueError(f'Linked version/binary does not match provenance: {binary.name}')
    return stamp

def validate_link_source(captured, version, current):
    if captured['source_version'] != version or captured['source']['identity'] != current['identity']:
        raise ValueError('Source version/state changed during build; link provenance rejected.')

def validate_staged_binaries(binaries, stamps):
    for binary, stamp in zip(binaries, stamps, strict=True):
        if sha(binary) != stamp['binary_sha256']:
            raise ValueError(f'Binary changed during packaging: {Path(binary).name}')

def validate_formal(root, version, linked_version, state, stamps):
    if not version:
        return ''
    if not re.fullmatch(r'v[0-9]+\.[0-9]+(?:\.[0-9]+)?', version):
        raise ValueError('Version must have the form v0.1 or v0.1.0.')
    components = [int(x) for x in version[1:].split('.')]
    components += [0] * (3 - len(components))
    normalized = '.'.join(map(str, components))
    if normalized != linked_version:
        raise ValueError('Requested release version differs from linked source version.')
    if state['dirty'] or any(s['source']['dirty'] for s in stamps):
        raise ValueError('Versioned releases require a clean source checkout/build (except exact tracked build patches).')
    tag_commit = git(root, 'rev-parse', '--verify', f'refs/tags/{version}^{{commit}}').decode().strip()
    if tag_commit != state['commit'] or any(s['source']['commit'] != tag_commit for s in stamps):
        raise ValueError('Release tag, packaging HEAD and linked build commit must match.')
    return 'v' + normalized

def atomic_json(path, value):
    path = Path(path); path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + '.tmp')
    temporary.write_text(json.dumps(value, indent=2), encoding='utf-8')
    temporary.replace(path)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=['capture', 'linked'])
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--version', required=True)
    parser.add_argument('--binary', type=Path)
    args = parser.parse_args()
    if args.mode == 'capture':
        atomic_json(args.source, {'source_version': args.version, 'source': source_state(args.root)})
    else:
        captured = json.loads(args.source.read_text(encoding='utf-8'))
        validate_link_source(captured, args.version, source_state(args.root))
        captured.update(binary=args.binary.name, binary_sha256=sha(args.binary))
        atomic_json(args.binary.with_suffix(args.binary.suffix+'.build.json'), captured)

if __name__ == '__main__':
    main()
