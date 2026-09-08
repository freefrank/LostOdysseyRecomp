"""Read-only source adapters and transactional game import (stdlib only)."""
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

MAGIC = b'MICROSOFT*XBOX*MEDIA'
SECTOR = 2048
SUPPORTED = {
    1: '40c7dbb12cca03921d52cf4177a0f700cc4ae94ab730ab594940e2bdffd8ecf2',
    2: 'a42a46b211b22e923ccaed7a313e59083adbdbfbbc62d04cb7c6848fd9717ec7',
    3: '0d7965a11fb9d102856e26c7fb462b888cb57a1fedc474378b9ae2cceccf6570',
    4: '893914d1bf334f06508b5d54fa20004ee642a53ffcc6137500c10655c3831916',
}
EU_SUPPORTED = {
    1: '175ae53d109d480a83bebbd186e7b6871f7b03ce80af69ab388db2f747640de3',
    2: '1d8a78379349e4583957d34148d5dbf8a091955edb6c6877bc24bdc9c7b87541',
    3: 'dd323967d7f4b99b48c00aa6a15a643c96df525e539669e9be49a876513b86f6',
    4: '9204ba8b91836853ae1e9f0dc49090abd5e63709935599c5551c23b28ecf48d4',
}

# Hashes authenticate audited encrypted XEX files. Execution metadata explains
# a near match, but cannot establish compatible guest code by itself. MD5
# tables are intentionally optional so another independently audited identity
# can be added without weakening the preferred SHA256 match.
SUPPORTED_MD5 = {}
EU_SUPPORTED_MD5 = {}
EDITIONS = {
    'asia': {
        'label': 'Europe / Asia', 'version': 4, 'base': 4,
        'media': {1: '39F7D748', 2: '0EF8CEA8', 3: '309E3386', 4: '7B21A91D'},
        'sha256': SUPPORTED, 'md5': SUPPORTED_MD5,
    },
    'usa-europe': {
        'label': 'USA / Europe', 'version': 3, 'base': 3,
        'media': {1: '368DE6DD', 2: '1888BE4E', 3: '6DD59D08', 4: '0C0E80B5'},
        'sha256': EU_SUPPORTED, 'md5': EU_SUPPORTED_MD5,
    },
}
MAX_DISCOVERY_DEPTH = 8
MAX_DISCOVERY_ENTRIES = 10000
MAX_CANDIDATES = 256


class ImportError(ValueError):
    pass


class Cancelled(ImportError):
    pass


@dataclass(frozen=True)
class Disc:
    path: Path
    format: str
    files: int
    bytes: int
    info: dict


@dataclass(frozen=True)
class Scan:
    discs: tuple
    rejected: tuple


@dataclass(frozen=True)
class Sources:
    """Explicit (path, format) candidates from the automatic content picker."""
    candidates: tuple
    reviewed: tuple = ()


def component(name):
    if (not name or name in ('.', '..') or name[-1] in ' .'
            or any(ord(c) < 32 or c in '<>:"/\\|?*' for c in name)
            or re.fullmatch(r'(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\..*)?', name, re.I)):
        raise ImportError(f'Unsafe file name: {name!r}')
    return name


def linked(path):
    return path.is_symlink() or bool(getattr(path.lstat(), 'st_file_attributes', 0) & 0x400)


@dataclass
class Entry:
    name: str
    offset: int
    size: int


class Image:
    def __init__(self, path, stack, cancelled=lambda: False):
        self.path = path
        self.files = []
        self.base = 0
        self.god = path.is_dir()
        if self.god:
            names = sorted(p for p in path.iterdir() if re.fullmatch(r'Data\d{4}', p.name))
            if not names or any(p.name != f'Data{i:04d}' for i, p in enumerate(names)):
                raise ImportError('Missing or non-contiguous GOD data chunks')
            for i, p in enumerate(names):
                if linked(p) or (i < len(names) - 1 and p.stat().st_size != 0xA290000):
                    raise ImportError(f'Invalid GOD chunk: {p.name}')
                self.files.append(stack.enter_context(p.open('rb')))
            self.limit = len(names) * 0xA1C4 * 4096
        else:
            self.files = [stack.enter_context(path.open('rb'))]
            self.limit = path.stat().st_size
            # Locate a sector-aligned descriptor; works for stripped and padded ISOs.
            # Only the prefix can contain the game partition descriptor.
            found = False
            for offset in range(0, min(self.limit, 512 * 1024**2), 1024**2):
                if cancelled():
                    raise Cancelled('Source check cancelled')
                self.files[0].seek(offset)
                chunk = self.files[0].read(1024**2 + SECTOR)
                pos = chunk.find(MAGIC)
                while pos >= 0:
                    absolute = offset + pos
                    if (absolute >= 0x10000 and absolute % SECTOR == 0
                            and chunk[pos + 0x7ec:pos + 0x800] == MAGIC):
                        self.base = absolute - 0x10000
                        self.limit -= self.base
                        found = True
                        break
                    pos = chunk.find(MAGIC, pos + 1)
                if found:
                    break
            if not found:
                raise ImportError('No XDVDFS game partition found in ISO')

    def read(self, offset, size):
        if offset < 0 or size < 0 or offset + size > self.limit:
            raise ImportError('Image file range is outside the source')
        result = bytearray()
        while size:
            if self.god:
                block, inner = divmod(offset, 4096)
                index, block = divmod(block, 0xA1C4)
                group, within = divmod(block, 204)
                physical = 4096 + (group + 1) * 4096 + block * 4096 + inner
                take = min(size, (204 - within) * 4096 - inner)
            else:
                index, physical, take = 0, self.base + offset, size
            f = self.files[index]
            f.seek(physical)
            data = f.read(take)
            if len(data) != take:
                raise ImportError('Truncated game image')
            result.extend(data)
            offset += take
            size -= take
        return bytes(result)

    def entries(self):
        vd = self.read(0x10000, SECTOR)
        if vd[:20] != MAGIC or vd[0x7ec:] != MAGIC:
            raise ImportError('Unsupported GOD/SVOD layout')
        root, size = struct.unpack_from('<II', vd, 20)
        pending = [('', root, size, 0)]
        dirs, names, entries = set(), set(), []
        while pending:
            prefix, sector, size, depth = pending.pop()
            if depth > 64 or size > 16 * 1024**2 or sector in dirs:
                raise ImportError('Invalid or recursive image directory')
            dirs.add(sector)
            if not size:
                continue
            data = self.read(sector * SECTOR, size)
            nodes, visited = [0], set()
            while nodes:
                offset = nodes.pop()
                if offset in visited or offset + 14 > len(data):
                    raise ImportError('Invalid image directory tree')
                visited.add(offset)
                left, right, start, length, attr, n = struct.unpack_from('<HHIIBB', data, offset)
                if left == right == 0xffff:
                    continue
                if offset + 14 + n > len(data):
                    raise ImportError('Truncated image file name')
                name = component(data[offset + 14:offset + 14 + n].decode('ascii'))
                relative = prefix + name
                if relative.casefold() in names or len(names) > 100000:
                    raise ImportError('Duplicate or excessive image entries')
                names.add(relative.casefold())
                for child in (left, right):
                    if child:
                        nodes.append(child * 4)
                if name.casefold() == '$systemupdate':
                    continue
                if attr & 0x10:
                    pending.append((relative + '/', start, length, depth + 1))
                else:
                    if start * SECTOR + length > self.limit:
                        raise ImportError('File extends beyond image')
                    entries.append(Entry(relative, start * SECTOR, length))
        return entries


class Folder:
    def __init__(self, path):
        self.path = path

    def entries(self):
        entries, names = [], set()
        for base, dirs, files in os.walk(self.path, followlinks=False):
            dirs[:] = [d for d in dirs if d.casefold() != '$systemupdate']
            for name in dirs + files:
                component(name)
                p = Path(base) / name
                if linked(p):
                    raise ImportError(f'Links are not supported: {p}')
                rel = p.relative_to(self.path).as_posix()
                if rel.casefold() in names:
                    raise ImportError(f'Duplicate file name: {rel}')
                names.add(rel.casefold())
                if name in files:
                    entries.append(Entry(rel, 0, p.stat().st_size))
        return entries


def discover(path, depth=MAX_DISCOVERY_DEPTH):
    """Find plausible disc roots without opening or trusting their contents."""
    selected = Path(path).absolute()
    if not selected.exists():
        raise ImportError(f'Source does not exist: {selected}')
    if linked(selected):
        raise ImportError('Select a source without symbolic links or junctions')
    if selected.is_file():
        if selected.suffix.lower() == '.xex':
            return [selected.parent]
        if selected.suffix.lower() == '.iso':
            return [selected]
        god = Path(str(selected) + '.data')
        if god.is_dir() and not linked(god):
            return [god]
        raise ImportError('Select an ISO, default.xex, GOD header, or game folder')
    if not selected.is_dir():
        raise ImportError('The selected source is not a file or folder')

    sources, pending, visited = [], [(selected, 0)], 0
    while pending:
        current, level = pending.pop()
        try:
            if linked(current):
                continue
            children = sorted(current.iterdir(), key=lambda p: p.name.casefold())
        except (OSError, PermissionError):
            continue
        visited += len(children)
        if visited > MAX_DISCOVERY_ENTRIES:
            raise ImportError(f'Source contains too many entries to search safely ({MAX_DISCOVERY_ENTRIES} limit)')
        names = {child.name.casefold() for child in children}
        if 'default.xex' in names:
            sources.append(current)
            continue
        if current.suffix.lower() == '.data' and 'data0000' in names:
            sources.append(current)
            continue
        if level >= depth:
            continue
        for child in reversed(children):
            try:
                if linked(child):
                    continue
                is_dir = child.is_dir()
            except OSError:
                continue
            if is_dir:
                pending.append((child, level + 1))
            elif child.suffix.lower() == '.iso':
                sources.append(child)
        if len(sources) > MAX_CANDIDATES:
            raise ImportError(f'Too many possible game sources ({MAX_CANDIDATES} limit); select a closer folder')
    return sorted(set(sources), key=lambda p: str(p).casefold())


def execution(data):
    if len(data) < 24 or data[:4] != b'XEX2':
        raise ImportError('Invalid default.xex')
    count = struct.unpack_from('>I', data, 20)[0]
    if count > 1024 or 24 + count * 8 > len(data):
        raise ImportError('Invalid XEX optional headers')
    headers = dict(struct.unpack_from('>II', data, 24 + i * 8) for i in range(count))
    offset = headers.get(0x40006, len(data))
    if offset + 24 > len(data):
        raise ImportError('Missing XEX execution information')
    media, version, base, title = struct.unpack_from('>IIII', data, offset)
    return dict(title=f'{title:08X}', media=f'{media:08X}', version=version,
                base=base, disc=data[offset + 18], discs=data[offset + 19])


def identify(info, sha256, md5):
    """Return (edition, evidence) only for an audited executable identity."""
    def metadata_matches(identity, disc):
        return (identity['media'].get(disc) == info['media']
                and info['version'] == identity['version'] and info['base'] == identity['base'])

    for edition, identity in EDITIONS.items():
        disc = info['disc']
        if identity['sha256'].get(disc) == sha256:
            if not metadata_matches(identity, disc):
                raise ImportError('XEX hash and execution metadata identify different disc builds')
            return edition, 'sha256'
        if identity['md5'].get(disc) == md5:
            if not metadata_matches(identity, disc):
                raise ImportError('XEX hash and execution metadata identify different disc builds')
            return edition, 'md5'
    return 'unknown', 'none'


def metadata_edition(info):
    for edition, identity in EDITIONS.items():
        if (identity['media'].get(info['disc']) == info['media']
                and info['version'] == identity['version'] and info['base'] == identity['base']):
            return edition
    return None


def prepare(path, stack, validate=True, kind=None, cancelled=lambda: False):
    if linked(path):
        raise ImportError('Links are not supported as game sources')
    image = kind in ('ISO', 'GOD') if kind else path.suffix.lower() in ('.iso', '.data')
    source = Image(path, stack, cancelled) if image else Folder(path)
    entries = source.entries()
    xex = next((e for e in entries if e.name.casefold() == 'default.xex'), None)
    if xex is None or not 24 <= xex.size <= 32 * 1024**2:
        raise ImportError('Missing or invalid default.xex in game root')
    data = ((path / xex.name).read_bytes() if isinstance(source, Folder)
            else source.read(xex.offset, xex.size))
    info = execution(data)
    info['sha256'] = hashlib.sha256(data).hexdigest()
    info['md5'] = hashlib.md5(data).hexdigest()
    info['edition'], info['identity'] = identify(info, info['sha256'], info['md5'])
    info['metadata_edition'] = metadata_edition(info)
    if validate:
        if info['title'] != '4D5307FA':
            raise ImportError(f"Wrong game: Title ID {info['title']} (expected 4D5307FA)")
        if info['disc'] not in range(1, 5) or info['discs'] != 4:
            raise ImportError(f"Unsupported disc set: disc {info['disc']} of {info['discs']} (expected 1-4 of 4)")
        if info['edition'] == 'unknown':
            resembles = (f" Metadata resembles {EDITIONS[info['metadata_edition']]['label']}, "
                         'but metadata alone cannot prove compatible guest code.'
                         if info['metadata_edition'] else '')
            raise ImportError('Unrecognized Lost Odyssey XEX: '
                              f"Media ID {info['media']}, version {info['version']}, base {info['base']}; "
                              f"MD5 {info['md5']}; SHA256 {info['sha256']}.{resembles}")
        root_files = {e.name.casefold() for e in entries}
        required = {'lo.fpd', 'lo.fpi'} | {f'xenon_{name}.fpd' for name in
                    ('battle', 'chr', 'event', 'field', 'loc', 'mov', 'obj', 'scr', 'snd', 'sys', 'vfx', 'world')}
        if not required <= root_files:
            raise ImportError('Incomplete game folder: select the full disc, not only the XEX')
    return source, entries, info


def _load(path, stack, validate=True, cancelled=lambda: False, allow_empty=False):
    explicit = isinstance(path, Sources)
    candidates = list(path.candidates) if explicit else [(p, None) for p in discover(path)]
    if not candidates:
        raise ImportError(f'No game discs found (searched up to {MAX_DISCOVERY_DEPTH} directory levels)')
    selected = None if explicit else Path(path).absolute()
    direct = not explicit and (selected.is_file() or any(selected == p for p, _ in candidates))
    reviewed = {disc.path: disc.info for disc in path.reviewed} if explicit else {}
    discs, rejected = [], []
    for candidate, kind in candidates:
        if cancelled():
            raise Cancelled('Source check cancelled')
        try:
            source, entries, info = prepare(candidate, stack, validate, kind, cancelled)
            if reviewed and reviewed.get(candidate) != info:
                raise ImportError('The selected disc identity changed after review; check the source again')
            discs.append((source, entries, info))
        except Cancelled:
            raise
        except (ImportError, OSError, UnicodeError) as error:
            rejected.append((candidate, str(error)))
    if not discs:
        if allow_empty:
            return discs, rejected
        details = '; '.join(f'{candidate}: {error}' for candidate, error in rejected[:3])
        suffix = f' Details: {details}' if details else ''
        raise ImportError('No supported, complete Lost Odyssey discs were found.' + suffix)
    if (direct or reviewed) and rejected:
        candidate, error = rejected[0]
        raise ImportError(f'Cannot use selected source {candidate}: {error}')
    numbers = {}
    for source, _, info in discs:
        numbers.setdefault(info['disc'], []).append(source.path)
    duplicates = {number: paths for number, paths in numbers.items() if len(paths) > 1}
    if duplicates:
        number, paths = sorted(duplicates.items())[0]
        joined = ', '.join(str(path) for path in paths[:3])
        raise ImportError(f'Found multiple sources for Disc {number}: {joined}. Select a closer folder.')
    editions = {info['edition'] for _, _, info in discs}
    if len(editions) != 1:
        raise ImportError('Cannot mix Europe/Asia and USA/Europe discs in one installation')
    return discs, rejected


def scan(source_path, validate=True, cancelled=lambda: False):
    """Inspect a selection without copying and return UI-friendly disc details."""
    with ExitStack() as stack:
        discs, rejected = _load(source_path, stack, validate, cancelled,
                                allow_empty=isinstance(source_path, Sources))
        summaries = []
        for source, entries, info in sorted(discs, key=lambda item: item[2]['disc']):
            kind = 'GOD' if isinstance(source, Image) and source.god else ('ISO' if isinstance(source, Image) else 'Folder')
            summaries.append(Disc(source.path, kind, len(entries), sum(entry.size for entry in entries), dict(info)))
        return Scan(tuple(summaries), tuple((Path(path), error) for path, error in rejected))


def install(source_path, game_dir, progress=lambda done, total, label: None, cancelled=lambda: False):
    """Add discs without overwriting any existing disc; roll back this operation on error."""
    game_dir = Path(game_dir).resolve()
    with ExitStack() as stack:
        if cancelled():
            raise Cancelled('Import cancelled')
        progress(0, 0, 'Checking selected source')
        discs, _ = _load(source_path, stack, cancelled=cancelled)
        for source, _, _ in discs:
            path = source.path
            resolved = path.resolve()
            if game_dir == resolved or game_dir.is_relative_to(resolved) or resolved.is_relative_to(game_dir):
                raise ImportError('Source and destination must be separate folders')
        numbers = [info['disc'] for _, _, info in discs]
        editions = {info['edition'] for _, _, info in discs}
        game_dir.mkdir(parents=True, exist_ok=True)
        lock_path = game_dir / '.import.lock'
        try:
            lock = lock_path.open('x')
        except FileExistsError:
            raise ImportError('Another import owns this destination. If an earlier import crashed, '
                              'close all importers before removing .import.lock.') from None
        def unlock():
            lock.close()
            lock_path.unlink()
        stack.callback(unlock)
        for n in numbers:
            if (game_dir / f'disc{n}').exists():
                raise ImportError(f'Disc {n} is already installed; existing files were kept')
        # Inspect the actual XEX, including installations made before edition
        # metadata existed. Do not trust an editable import-info.json label.
        for n in range(1, 5):
            existing = game_dir / f'disc{n}'
            if existing.exists():
                _, _, installed = prepare(existing, stack)
                if installed['disc'] != n or installed['edition'] not in editions:
                    raise ImportError('Cannot mix Europe/Asia and USA/Europe discs in one installation')
        total = sum(e.size for _, entries, _ in discs for e in entries)
        if shutil.disk_usage(game_dir).free < total + 64 * 1024**2:
            raise ImportError('Not enough free space for the selected discs')
        with tempfile.TemporaryDirectory(prefix='.import-', dir=game_dir) as staging:
            staging = Path(staging)
            done = 0
            for source, entries, info in sorted(discs, key=lambda d: d[2]['disc']):
                target = staging / f"disc{info['disc']}"
                target.mkdir()
                for entry in entries:
                    output = target / entry.name
                    output.parent.mkdir(parents=True, exist_ok=True)
                    with ExitStack() as files:
                        sink = files.enter_context(output.open('xb'))
                        input_file = (files.enter_context((source.path / entry.name).open('rb'))
                                      if isinstance(source, Folder) else None)
                        for offset in range(0, entry.size, 1024**2):
                            if cancelled():
                                raise Cancelled('Import cancelled; original files were kept')
                            size = min(1024**2, entry.size - offset)
                            data = input_file.read(size) if input_file else source.read(entry.offset + offset, size)
                            if len(data) != size:
                                raise ImportError('Source changed or was truncated during import')
                            sink.write(data)
                            done += size
                            progress(done, total, entry.name)
                (target / 'import-info.json').write_text(json.dumps(info, indent=2), encoding='utf-8')
                copied_xex = next(e.name for e in entries if e.name.casefold() == 'default.xex')
                if hashlib.sha256((target / copied_xex).read_bytes()).hexdigest() != info['sha256']:
                    raise ImportError('The source XEX changed during import; please retry')
            if cancelled():
                raise Cancelled('Import cancelled')
            published = []
            try:
                for number in sorted(numbers):
                    name = f'disc{number}'
                    (staging / name).rename(game_dir / name)
                    published.append(name)
            except Exception:
                for name in reversed(published):
                    (game_dir / name).rename(staging / name)
                raise
    progress(total, total, 'Import complete')
    return sorted(numbers)


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('--destination', type=Path)
    args = parser.parse_args()
    if args.destination:
        print(install(args.source, args.destination))
    else:
        with ExitStack() as stack:
            for source in discover(args.source):
                _, entries, info = prepare(source, stack, validate=False)
                print(json.dumps(dict(source=str(source), files=len(entries), **info)))
