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


class ImportError(ValueError):
    pass


class Cancelled(ImportError):
    pass


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
    def __init__(self, path, stack):
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


def discover(path, depth=0):
    path = Path(path).absolute()
    if linked(path):
        raise ImportError('Select a source without symbolic links or junctions')
    if path.is_file():
        if path.suffix.lower() == '.xex':
            return [path.parent]
        if path.suffix.lower() == '.iso':
            return [path]
        if Path(str(path) + '.data').is_dir():
            return [Path(str(path) + '.data')]
        raise ImportError('Select an ISO, default.xex, GOD header, or game folder')
    children = list(path.iterdir())
    if any(p.name.lower() == 'default.xex' for p in children):
        return [path]
    if path.suffix.lower() == '.data' and any(p.name == 'Data0000' for p in children):
        return [path]
    if depth >= 5:
        return []
    sources = []
    for p in children:
        if p.is_dir() or p.suffix.lower() == '.iso':
            sources.extend(discover(p, depth + 1))
    return sources


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
                disc=data[offset + 18], discs=data[offset + 19])


def prepare(path, stack, validate=True):
    if linked(path):
        raise ImportError('Links are not supported as game sources')
    source = (Image(path, stack) if path.suffix.lower() in ('.iso', '.data') else Folder(path))
    entries = source.entries()
    xex = next((e for e in entries if e.name.casefold() == 'default.xex'), None)
    if xex is None or not 24 <= xex.size <= 32 * 1024**2:
        raise ImportError('Missing or invalid default.xex in game root')
    data = ((path / xex.name).read_bytes() if isinstance(source, Folder)
            else source.read(xex.offset, xex.size))
    info = execution(data)
    info['sha256'] = hashlib.sha256(data).hexdigest()
    if validate:
        if info['title'] != '4D5307FA':
            raise ImportError(f"Wrong game: Title ID {info['title']} (expected 4D5307FA)")
        if SUPPORTED.get(info['disc']) != info['sha256']:
            raise ImportError('This XEX version is not supported by this build')
        root_files = {e.name.casefold() for e in entries}
        required = {'lo.fpd', 'lo.fpi'} | {f'xenon_{name}.fpd' for name in
                    ('battle', 'chr', 'event', 'field', 'loc', 'mov', 'obj', 'scr', 'snd', 'sys', 'vfx', 'world')}
        if not required <= root_files:
            raise ImportError('Incomplete game folder: select the full disc, not only the XEX')
    return source, entries, info


def install(source_path, game_dir, progress=lambda done, total, label: None, cancelled=lambda: False):
    """Add discs without overwriting any existing disc; roll back this operation on error."""
    game_dir = Path(game_dir).resolve()
    sources = discover(source_path)
    if not sources:
        raise ImportError('No game discs found (searched up to five directory levels)')
    with ExitStack() as stack:
        discs = []
        for path in sources:
            if cancelled():
                raise Cancelled('Import cancelled')
            resolved = path.resolve()
            if game_dir == resolved or game_dir.is_relative_to(resolved) or resolved.is_relative_to(game_dir):
                raise ImportError('Source and destination must be separate folders')
            progress(0, 0, f'Checking {path.name}')
            discs.append(prepare(path, stack))
        numbers = [info['disc'] for _, _, info in discs]
        if len(numbers) != len(set(numbers)):
            raise ImportError('Multiple copies of the same disc selected')
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
