"""Read Xbox 360 STFS DLC and publish an isolated, verified content directory.

Format references (no ReXGlue dependency):
https://github.com/rexglue/rexglue-sdk/blob/c94f5ebdcb3c9d1a460ca48e04f9758448f8d518/include/rex/filesystem/devices/stfs_xbox.h
https://github.com/rexglue/rexglue-sdk/blob/c94f5ebdcb3c9d1a460ca48e04f9758448f8d518/src/filesystem/devices/stfs_container_device.cpp
The reader checks structure and hashes, not Microsoft RSA signatures or ownership.
"""
from contextlib import ExitStack
from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import tempfile

from import_game import Cancelled, ImportError, component, linked

TITLE_ID = '4D5307FA'
MAGICS = (b'CON ', b'LIVE', b'PIRS')
BLOCK = 4096
FANOUT = 170
END = 0xffffff
MAX_BLOCKS = FANOUT ** 3
MAX_FILES = 100000
SIDECARS = {'.lo-content', '.lo-dlc.json', '.lo-dlc-header'}


def _cancel(check):
    if check():
        raise Cancelled('DLC import cancelled; source files were kept')


def _digest(stream, cancelled):
    stream.seek(0)
    digest = hashlib.sha256()
    while data := stream.read(1024 * 1024):
        _cancel(cancelled)
        digest.update(data)
    return digest.hexdigest()


@dataclass(frozen=True)
class Entry:
    path: str
    size: int
    blocks: tuple
    directory: bool = False


@dataclass(frozen=True)
class Package:
    path: Path
    info: dict
    files: int
    bytes: int


@dataclass(frozen=True)
class Scan:
    packages: tuple
    rejected: tuple


class Stfs:
    """A bounded, read-only STFS reader; data blocks are streamed during extraction."""

    def __init__(self, path, stack, cancelled=lambda: False):
        self.path = Path(path).absolute()
        self.cancelled = cancelled
        if not self.path.is_file() or linked(self.path):
            raise ImportError('DLC source must be a regular file, without links')
        self.stream = stack.enter_context(self.path.open('rb'))
        self.length = os.fstat(self.stream.fileno()).st_size
        header = self.read(0, 0x971a)
        if header[:4] not in MAGICS:
            raise ImportError('Not an Xbox 360 STFS package (CON, LIVE or PIRS)')
        header_size = int.from_bytes(header[0x340:0x344], 'big')
        if not 0x971a <= header_size <= 1024 * 1024:
            raise ImportError('Invalid STFS header size')
        self.header = self.read(0, header_size)
        self.base = (header_size + BLOCK - 1) & ~(BLOCK - 1)
        title = header[0x360:0x364].hex().upper()
        if title != TITLE_ID:
            raise ImportError(f'Wrong game: Title ID {title} (expected {TITLE_ID})')
        if int.from_bytes(header[0x344:0x348], 'big') != 2:
            raise ImportError('This package is not Marketplace DLC (content type 2)')
        if int.from_bytes(header[0x3a9:0x3ad], 'big') != 0:
            raise ImportError('DLC import supports STFS volumes only, not SVOD')
        if int.from_bytes(header[0x39d:0x3a1], 'big') > 1:
            raise ImportError('Multi-file content volumes are not supported')
        volume = header[0x379:0x39d]
        if volume[0] != 0x24 or volume[1] != 0:
            raise ImportError('Unsupported STFS volume descriptor')
        self.copies = 1 if volume[2] & 1 else 2
        self.root_copy = int(bool(volume[2] & 2)) if self.copies == 2 else 0
        self.total_blocks = int.from_bytes(volume[0x1c:0x20], 'big')
        self.table_count = int.from_bytes(volume[3:5], 'little')
        self.table_start = int.from_bytes(volume[5:8], 'little')
        self.top_digest = volume[8:28]
        if not 0 < self.total_blocks <= MAX_BLOCKS or not 0 < self.table_count <= 1563:
            raise ImportError('Invalid or excessive STFS block/table count')
        if self.data_offset(self.total_blocks - 1) + BLOCK > self.length:
            raise ImportError('Truncated STFS data blocks')
        self.tables = {}
        self.claimed = set()
        self.top_level = 2 if self.total_blocks > FANOUT ** 2 else int(self.total_blocks > FANOUT)
        content_id = header[0x32c:0x340].hex().upper()
        if content_id == '0' * 40:
            raise ImportError('DLC content ID is empty')
        display_names = [self.text(header[0x411 + i * 256:0x511 + i * 256]) for i in range(9)]
        display_name = next((name for name in display_names if name), content_id)
        encoded = display_name.encode('utf-16-be')[:254]
        if encoded and 0xd800 <= int.from_bytes(encoded[-2:], 'big') <= 0xdbff:
            encoded = encoded[:-2]
        display_name = encoded.decode('utf-16-be')
        license_mask = 0
        for index in range(16):
            _, bits, flags = struct.unpack_from('>QII', header, 0x22c + 16 * index)
            if flags:
                license_mask |= bits
        self.info = dict(schema=1, title_id=TITLE_ID, content_type=2,
                         content_id=content_id, display_name=display_name,
                         license_mask=license_mask, format=header[:4].decode('ascii').strip(),
                         source_sha256=_digest(self.stream, cancelled))
        self.entries = self._entries()

    @staticmethod
    def text(data):
        value = data.decode('utf-16-be').split('\0', 1)[0]
        # UI control characters are not package names; keep legitimate Unicode.
        return ''.join(c for c in value if ord(c) >= 32).strip()

    def read(self, offset, size):
        _cancel(self.cancelled)
        if offset < 0 or size < 0 or offset + size > self.length:
            raise ImportError('Truncated STFS package or out-of-range block')
        self.stream.seek(offset)
        data = self.stream.read(size)
        if len(data) != size:
            raise ImportError('STFS source changed or was truncated')
        return data

    def data_offset(self, number):
        if not 0 <= number < self.total_blocks:
            raise ImportError('STFS block is outside the volume')
        overhead = 0
        for span in (FANOUT, FANOUT ** 2, FANOUT ** 3):
            overhead += (number // span + 1) * self.copies
            if number < span:
                break
        return self.base + (number + overhead) * BLOCK

    def table_offset(self, number, level):
        first_span = FANOUT + self.copies
        second_span = FANOUT ** 2 + (FANOUT + 1) * self.copies
        if level == 2:
            physical = second_span
        elif level == 1:
            physical = first_span if number < FANOUT ** 2 else (number // FANOUT ** 2) * second_span + self.copies
        elif number < FANOUT:
            physical = 0
        else:
            physical = (number // FANOUT) * first_span + (number // FANOUT ** 2 + 1) * self.copies
            if number >= FANOUT ** 2:
                physical += self.copies
        return self.base + physical * BLOCK

    def table(self, number, level):
        offset = self.table_offset(number, level)
        if (offset, level) in self.tables:
            return self.tables[(offset, level)]
        if level == self.top_level:
            active, expected = self.root_copy, self.top_digest
        else:
            parent = self.table(number, level + 1)
            index = (number // (FANOUT ** (level + 1))) % FANOUT
            record = parent[index * 24:(index + 1) * 24]
            active = int(bool(record[20] & 0x40)) if self.copies == 2 else 0
            expected = record[:20]
        data = self.read(offset + active * BLOCK, BLOCK)
        if hashlib.sha1(data).digest() != expected:
            raise ImportError('STFS hash table checksum mismatch')
        self.tables[(offset, level)] = data
        return data

    def record(self, number):
        self.data_offset(number)
        table = self.table(number, 0)
        return table[(number % FANOUT) * 24:(number % FANOUT + 1) * 24]

    def block(self, number):
        record = self.record(number)
        if not record[20] & 0x80:
            raise ImportError('STFS chain references an unallocated block')
        data = self.read(self.data_offset(number), BLOCK)
        if hashlib.sha1(data).digest() != record[:20]:
            raise ImportError('STFS data block checksum mismatch')
        return data

    def chain(self, start, count):
        if not 0 <= count <= self.total_blocks:
            raise ImportError('Invalid STFS chain length')
        result = []
        number = start
        for _ in range(count):
            _cancel(self.cancelled)
            if number in self.claimed:
                raise ImportError('Cyclic or overlapping STFS block chain')
            record = self.record(number)
            if not record[20] & 0x80:
                raise ImportError('STFS chain references an unallocated block')
            self.claimed.add(number)
            result.append(number)
            number = int.from_bytes(record[21:24], 'big')
        if count and number != END:
            raise ImportError('STFS chain exceeds declared file length')
        return tuple(result)

    def _entries(self):
        directory_blocks = self.chain(self.table_start, self.table_count)
        raw_entries = {}
        for table_index, block in enumerate(directory_blocks):
            data = self.block(block)
            for index in range(64):
                record = data[index * 64:(index + 1) * 64]
                length = record[40] & 63
                if not record[0] and not length:
                    continue
                if not 0 < length <= 40:
                    raise ImportError('Invalid STFS file name length')
                name = component(record[:length].decode('utf-8'))
                if name.casefold().startswith('.lo-'):
                    raise ImportError('DLC payload uses a reserved metadata name')
                raw_entries[table_index * 64 + index] = (name, bool(record[40] & 128),
                    int.from_bytes(record[50:52], 'big'), int.from_bytes(record[52:56], 'big'),
                    int.from_bytes(record[47:50], 'little'), int.from_bytes(record[41:44], 'little'),
                    int.from_bytes(record[44:47], 'little'))
        if not raw_entries or len(raw_entries) > MAX_FILES:
            raise ImportError('DLC package is empty or contains too many entries')
        paths = {}
        def path_for(index, active=()):
            if index in paths:
                return paths[index]
            if index in active or len(active) >= 64:
                raise ImportError('Cyclic or excessively deep STFS directory')
            name, _, parent, *_ = raw_entries[index]
            if parent == 0xffff:
                value = name
            else:
                if parent not in raw_entries or not raw_entries[parent][1]:
                    raise ImportError('STFS file has an invalid parent directory')
                value = path_for(parent, active + (index,)) + '/' + name
            paths[index] = value
            return value
        names, entries = set(), []
        for index, (_, directory, _, size, start, valid, allocated) in raw_entries.items():
            path = path_for(index)
            if path.casefold() in names:
                raise ImportError('Duplicate STFS file path')
            names.add(path.casefold())
            count = (size + BLOCK - 1) // BLOCK
            if directory:
                if size:
                    raise ImportError('STFS directory has a nonzero file length')
                blocks = ()
            else:
                if valid != count or allocated < count or allocated > self.total_blocks:
                    raise ImportError('STFS file length and block counts disagree')
                # Allocated-but-unused trailing blocks are valid. Validate their
                # chain as well, but extract only the logical file length.
                blocks = self.chain(start, allocated) if allocated else ()
            entries.append(Entry(path, size, blocks, directory))
        if not any(not entry.directory for entry in entries):
            raise ImportError('DLC package contains no files')
        return tuple(entries)

    def summary(self):
        return Package(self.path, dict(self.info), sum(not e.directory for e in self.entries),
                       sum(e.size for e in self.entries))


def discover(paths, cancelled=lambda: False):
    if isinstance(paths, (str, os.PathLike)):
        paths = (paths,)
    pending = [(Path(path).absolute(), 0, True) for path in paths]
    found, visited = set(), 0
    while pending:
        _cancel(cancelled)
        path, depth, direct = pending.pop()
        if linked(path):
            if direct:
                raise ImportError('Links are not supported as DLC sources')
            continue
        if path.is_dir():
            children = sorted(path.iterdir(), key=lambda p: p.name.casefold())
            visited += len(children)
            if visited > 10000:
                raise ImportError('Too many files to scan; select a closer DLC folder')
            if depth < 8:
                pending.extend((child, depth + 1, False) for child in reversed(children))
        elif path.is_file():
            with path.open('rb') as stream:
                magic = stream.read(4)
            if direct or magic in MAGICS:
                found.add(path)
            if len(found) > 256:
                raise ImportError('Too many DLC packages; select a closer folder')
        elif direct:
            raise ImportError(f'DLC source does not exist: {path}')
    return sorted(found, key=lambda path: str(path).casefold())


def scan(paths, cancelled=lambda: False):
    packages, rejected, identities = [], [], {}
    for path in discover(paths, cancelled):
        try:
            with ExitStack() as stack:
                package = Stfs(path, stack, cancelled).summary()
            identity = package.info['content_id']
            if identity in identities:
                if identities[identity] != package.info['source_sha256']:
                    raise ImportError('Different source packages have the same DLC content ID')
                continue
            identities[identity] = package.info['source_sha256']
            packages.append(package)
        except Cancelled:
            raise
        except (ImportError, OSError, UnicodeError) as error:
            rejected.append((path, str(error)))
    if not packages:
        details = '; '.join(f'{path.name}: {error}' for path, error in rejected[:3])
        raise ImportError('No supported Lost Odyssey STFS DLC packages found.' + (' ' + details if details else ''))
    return Scan(tuple(packages), tuple(rejected))


def game_root(path):
    """Match the runtime's disc-set root rule; reject destination reparse points."""
    path = Path(path).absolute()
    for ancestor in (path, *path.parents):
        if ancestor.exists() and linked(ancestor):
            raise ImportError('DLC destination must not contain links or junctions')
    if re.fullmatch(r'disc[1-4]', path.name, re.I):
        xex = next((child for child in path.iterdir() if child.name.casefold() == 'default.xex'), None) if path.is_dir() else None
        if xex and xex.is_file() and not linked(xex):
            path = path.parent
    return path.resolve()


def content_record(info):
    data = bytearray(308)
    struct.pack_into('>II', data, 0, 1, 2)
    name = info['display_name'].encode('utf-16-be')[:254]
    # Avoid truncating a UTF-16 surrogate pair at the fixed-width boundary.
    if name and 0xd800 <= int.from_bytes(name[-2:], 'big') <= 0xdbff:
        name = name[:-2]
    data[8:8 + len(name)] = name
    identity = info['content_id'].encode('ascii')
    data[264:264 + len(identity)] = identity
    return bytes(data)


def _existing_matches(target, source, cancelled):
    """A duplicate is a no-op only if every installed payload still matches."""
    expected = source.info
    if linked(target) or not target.is_dir():
        return False
    for name in SIDECARS:
        path = target / name
        if not path.is_file() or linked(path):
            return False
    try:
        manifest = json.loads((target / '.lo-dlc.json').read_text(encoding='utf-8'))
        for field in ('schema', 'title_id', 'content_id', 'source_sha256', 'display_name', 'license_mask'):
            if manifest.get(field) != expected[field]:
                return False
        if (target / '.lo-content').read_bytes() != content_record(expected):
            return False
        files = manifest.get('files')
        if not isinstance(files, list) or not 0 < len(files) <= MAX_FILES:
            return False
        source_files = {entry.path: entry for entry in source.entries if not entry.directory}
        if len(files) != len(source_files):
            return False
        expected_names = set(SIDECARS)
        for entry in files:
            _cancel(cancelled)
            parts = entry['path'].split('/')
            if not parts or any(component(part).casefold().startswith('.lo-') for part in parts):
                return False
            path = target.joinpath(*parts)
            original = source_files.get(entry['path'])
            if original is None or original.size != entry['size']:
                return False
            for ancestor in (path, *path.parents):
                if ancestor == target:
                    break
                if linked(ancestor):
                    return False
            if not path.is_file() or path.stat().st_size != entry['size']:
                return False
            with path.open('rb') as stream:
                if _digest(stream, cancelled) != entry['sha256']:
                    return False
            digest, remaining = hashlib.sha256(), original.size
            for block in original.blocks:
                data = source.block(block)
                take = min(remaining, BLOCK)
                digest.update(data[:take])
                remaining -= take
            if digest.hexdigest() != entry['sha256']:
                return False
            key = entry['path'].casefold()
            if key in expected_names:
                return False
            expected_names.add(key)
        actual_names = set()
        for base, dirs, names in os.walk(target, followlinks=False):
            for name in dirs + names:
                if linked(Path(base) / name):
                    return False
            actual_names.update((Path(base) / name).relative_to(target).as_posix().casefold() for name in names)
        return actual_names == expected_names
    except Cancelled:
        raise
    except (OSError, ValueError, TypeError, KeyError, UnicodeError):
        return False


def install(selection, destination, progress=lambda done, total, label: None,
            cancelled=lambda: False):
    """Import a reviewed selection atomically, without replacing existing content."""
    inspected = selection if isinstance(selection, Scan) else scan(selection, cancelled)
    root = game_root(destination)
    dlc_root = root / 'dlc'
    root.mkdir(parents=True, exist_ok=True)
    if dlc_root.exists() and (linked(dlc_root) or not dlc_root.is_dir()):
        raise ImportError('DLC destination is not a regular directory')
    dlc_root.mkdir(exist_ok=True)
    with ExitStack() as stack:
        _cancel(cancelled)
        lock_path = root / '.import.lock'
        try:
            lock = lock_path.open('x')
        except FileExistsError:
            raise ImportError('Another importer owns this destination (.import.lock)') from None
        def unlock():
            lock.close()
            lock_path.unlink()
        stack.callback(unlock)
        sources, unchanged = [], []
        for reviewed in inspected.packages:
            _cancel(cancelled)
            resolved = reviewed.path.resolve()
            if resolved.is_relative_to(dlc_root):
                raise ImportError('Choose the original DLC package outside the installed DLC folder')
            source = Stfs(reviewed.path, stack, cancelled)
            if source.info != reviewed.info:
                raise ImportError('DLC source changed since review; check the source again')
            identity = source.info['content_id']
            target = dlc_root / identity
            case_matches = [p for p in dlc_root.iterdir() if p.name.casefold() == identity.casefold()]
            if case_matches:
                if len(case_matches) != 1 or case_matches[0].name != identity or not _existing_matches(target, source, cancelled):
                    raise ImportError(f'DLC {identity} already exists with different or damaged content; existing files were kept')
                if (target / '.lo-dlc-header').read_bytes() != source.header:
                    raise ImportError(f'DLC {identity} has different header metadata; existing files were kept')
                unchanged.append(identity)
            else:
                sources.append(source)
        total = sum(sum(entry.size for entry in source.entries) for source in sources)
        if shutil.disk_usage(root).free < total + 16 * 1024 ** 2:
            raise ImportError('Not enough free space for the selected DLC')
        done = 0
        with tempfile.TemporaryDirectory(prefix='.dlc-import-', dir=root) as staging_name:
            staging = Path(staging_name)
            for source in sources:
                identity = source.info['content_id']
                target = staging / identity
                target.mkdir()
                manifest = {key: source.info[key] for key in
                            ('schema', 'title_id', 'content_id', 'source_sha256', 'display_name', 'license_mask')}
                manifest['files'] = []
                for entry in source.entries:
                    _cancel(cancelled)
                    output = target / entry.path
                    if entry.directory:
                        output.mkdir(parents=True, exist_ok=True)
                        continue
                    output.parent.mkdir(parents=True, exist_ok=True)
                    digest = hashlib.sha256()
                    remaining = entry.size
                    with output.open('xb') as stream:
                        for block in entry.blocks:
                            data = source.block(block)
                            take = min(remaining, BLOCK)
                            if take:
                                stream.write(data[:take])
                                digest.update(data[:take])
                                remaining -= take
                                done += take
                                progress(done, total, entry.path)
                        stream.flush()
                        os.fsync(stream.fileno())
                    if remaining:
                        raise ImportError('DLC file chain is shorter than its declared size')
                    manifest['files'].append(dict(path=entry.path, size=entry.size, sha256=digest.hexdigest()))
                if _digest(source.stream, cancelled) != source.info['source_sha256']:
                    raise ImportError('DLC source changed during extraction; import rolled back')
                (target / '.lo-content').write_bytes(content_record(source.info))
                (target / '.lo-dlc-header').write_bytes(source.header)
                (target / '.lo-dlc.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
            _cancel(cancelled)
            published = []
            try:
                for source in sources:
                    identity = source.info['content_id']
                    target = dlc_root / identity
                    if target.exists():
                        raise ImportError('DLC destination changed during import; existing files were kept')
                    (staging / identity).rename(target)
                    published.append(identity)
            except Exception:
                for identity in reversed(published):
                    (dlc_root / identity).rename(staging / identity)
                raise
    progress(total, total, 'DLC import complete')
    return dict(imported=[source.info['content_id'] for source in sources], unchanged=unchanged,
                destination=str(dlc_root))


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('sources', type=Path, nargs='+')
    parser.add_argument('--destination', type=Path)
    args = parser.parse_args()
    reviewed = scan(args.sources)
    if args.destination:
        print(json.dumps(install(reviewed, args.destination), indent=2))
    else:
        print(json.dumps(dict(packages=[dict(path=str(p.path), files=p.files, bytes=p.bytes, **p.info)
                                       for p in reviewed.packages],
                              rejected=[(str(p), error) for p, error in reviewed.rejected]), ensure_ascii=False, indent=2))
