"""Automatic source discovery and grouped disc/DLC import, without UI dependencies."""
from dataclasses import dataclass
import os
from pathlib import Path
import struct

import import_dlc
import import_game
from import_game import Cancelled, ImportError, linked


@dataclass(frozen=True)
class Scan:
    discs: tuple = ()
    packages: tuple = ()
    rejected: tuple = ()


@dataclass(frozen=True)
class Result:
    discs: tuple
    dlc: dict | None
    remaining: Scan
    destination: str
    warning: str = ''
    error: str = ''
    cancelled: bool = False


def _cancel(cancelled):
    if cancelled():
        raise Cancelled('Source check cancelled')


def discover(paths, cancelled=lambda: False):
    """Probe headers/layouts; leave identity and content validation to each adapter.

    Unknown explicitly selected files are tried as images once. Nested unnamed
    images use the standard stripped/XGD2/XGD3 descriptor locations as hints;
    .iso files can also use the image adapter's arbitrary-padding search. A hint
    never authenticates a disc, and discovery never hashes an entire image.
    """
    if isinstance(paths, (str, os.PathLike)):
        paths = (paths,)
    pending = [(Path(p).absolute(), 0, True, False) for p in paths]
    seen, discs, packages, rejected = set(), {}, set(), []
    entries = 0
    while pending:
        _cancel(cancelled)
        path, depth, direct, inside_disc = pending.pop()
        key = str(path).casefold()
        if key in seen:
            continue
        seen.add(key)
        if linked(path):
            if direct:
                rejected.append((path, 'Select a source without links or junctions'))
            continue
        try:
            if path.is_dir():
                children = sorted(path.iterdir(), key=lambda p: p.name.casefold())
                entries += len(children)
                if entries > import_game.MAX_DISCOVERY_ENTRIES:
                    raise ImportError('Too many source entries; select a closer folder')
                names = {p.name.casefold(): p for p in children}
                if not inside_disc and 'default.xex' in names:
                    discs[path] = 'Folder'
                    inside_disc = True
                if not inside_disc and 'data0000' in names:
                    discs[path] = 'GOD'
                    continue
                if depth < import_game.MAX_DISCOVERY_DEPTH:
                    pending.extend((p, depth + 1, False, inside_disc) for p in reversed(children))
            elif path.is_file():
                with path.open('rb') as stream:
                    header = stream.read(0x400)
                    if header[:4] in import_dlc.MAGICS:
                        if len(header) < 0x3AD:
                            raise ImportError('Truncated Xbox 360 content header')
                        content_type = struct.unpack_from('>I', header, 0x344)[0]
                        volume = struct.unpack_from('>I', header, 0x3A9)[0]
                        if volume == 0 and content_type == 2:
                            packages.add(path)
                        elif volume == 1 and content_type in (0x4000, 0x7000):
                            title = struct.unpack_from('>I', header, 0x360)[0]
                            if title != 0x4D5307FA:
                                raise ImportError(f'Wrong GOD game: Title ID {title:08X} (expected 4D5307FA)')
                            data = path.with_name(path.name + '.data')
                            if not data.is_dir() or linked(data):
                                raise ImportError('GOD header needs its matching .data folder')
                            discs[data] = 'GOD'
                        else:
                            raise ImportError(f'Unsupported Xbox 360 content type {content_type:#x}, volume {volume}; '
                                              'select game discs or Marketplace STFS DLC')
                    elif not inside_disc:
                        if header[:4] == b'XEX2':
                            discs[path.parent] = 'Folder'
                        elif direct or path.suffix.lower() == '.iso':
                            discs[path] = 'ISO'
                        else:
                            for offset in (0x10000, 0x2090000, 0xFDA0000):
                                _cancel(cancelled)
                                stream.seek(offset)
                                vd = stream.read(2048)
                                if vd[:20] == import_game.MAGIC and vd[0x7EC:] == import_game.MAGIC:
                                    discs[path] = 'ISO'
                                    break
            elif direct:
                rejected.append((path, 'Source does not exist or is not a regular file/folder'))
        except Cancelled:
            raise
        except OSError as error:
            rejected.append((path, str(error)))
        except ImportError as error:
            # A bounded directory scan must not silently claim complete coverage.
            if path.is_dir():
                raise
            rejected.append((path, str(error)))
        if len(discs) + len(packages) > import_game.MAX_CANDIDATES:
            raise ImportError('Too many content candidates; select a closer folder')
    ordered = lambda path: str(path).casefold()
    return tuple(sorted(discs.items(), key=lambda item: ordered(item[0]))), tuple(sorted(packages, key=ordered)), rejected


def scan(paths, cancelled=lambda: False):
    candidates, packages, rejected = discover(paths, cancelled)
    discs, accepted = (), ()
    if candidates:
        result = import_game.scan(import_game.Sources(candidates), cancelled=cancelled)
        discs = result.discs
        rejected.extend(result.rejected)
    if packages:
        try:
            result = import_dlc.scan(packages, cancelled)
            accepted = result.packages
            rejected.extend(result.rejected)
        except Cancelled:
            raise
        except (ImportError, OSError, UnicodeError) as error:
            rejected.append((packages[0], str(error)))
    _cancel(cancelled)
    if not discs and not accepted:
        details = '; '.join(f'{p.name}: {error}' for p, error in rejected[:4])
        raise ImportError('No supported Lost Odyssey discs or DLC found.' + (' ' + details if details else ''))
    return Scan(discs, accepted, tuple(rejected))


def install(selection, destination, progress=lambda done, total, label: None,
            cancelled=lambda: False, save_path=lambda destination: ''):
    """Keep each existing category transaction intact and report partial completion.

    Discs commit as one group, then path configuration is saved, then DLC commits
    as one group. A later failure leaves completed discs usable; remaining omits
    those discs so the same Import button can retry only unfinished DLC.
    """
    discs, dlc, warning = (), None, ''
    remaining = selection
    target = str(destination)
    try:
        target = str(import_dlc.game_root(destination))
        if selection.discs:
            source = import_game.Sources(tuple((d.path, d.format) for d in selection.discs), selection.discs)
            discs = tuple(import_game.install(source, target, progress, cancelled))
            remaining = Scan(packages=selection.packages)
            try:
                warning = save_path(target)
            except OSError as error:
                warning = f'Game data was imported, but game-path.txt could not be updated: {error}'
        if selection.packages:
            dlc = import_dlc.install(import_dlc.Scan(selection.packages, ()), target, progress, cancelled)
        remaining = Scan()
        return Result(discs, dlc, remaining, target, warning)
    except Exception as error:
        return Result(discs, dlc, remaining, target, warning, str(error), isinstance(error, Cancelled))
