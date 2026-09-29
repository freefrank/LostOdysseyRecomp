"""Incremental receipts for the native graphics probe (no Windows dependency)."""
from __future__ import annotations
from collections import deque
from pathlib import Path
import argparse
import os
import re

_COMPLETED = re.compile(rb"(?:frame|present) timing completed=(\d+)")
_HEARTBEAT = re.compile(
    rb"heartbeat: swap #(\d+)[^\r\n]*?, (\d+) draws/frame[^\r\n]*?last file '([^']+)'")
_FRONTEND = re.compile(rb"native frontend: swap=(\d+) mode=mesh\b[^\r\n]*")
_COUNTER = re.compile(rb"([a-z_]+)=(\d+)")
_SCENE_RECEIPT = re.compile(
    rb"native probe scene: swap=(\d+) observation=(\d+) available=(true|false|0|1) "
    rb"map_id=(\d+) package='([^']*)'")
_SCENE = b"open 'game:\\xenon_scr.fpd'"


class NativeProbeLog:
    """Read each byte once and latch scene load before verbose logs evict it.

    A poll stops at the size observed when opening the file, even if rendering
    keeps appending. Incomplete lines are deferred to the next poll. No logger
    state is inferred from only the last megabyte of a high-volume trace.
    """
    def __init__(self, path: Path):
        self.path = path
        self.offset = 0
        self.identity: tuple[int, int] | None = None
        self.pending = b""
        self.completed = 0
        self.scene_loaded = False
        self.scene: dict | None = None
        self.scene_changes = 0
        self.heartbeats: deque[tuple[int, int, str]] = deque(maxlen=2)
        self.frontend: dict[str, int] | None = None
        self.diagnostics: str | None = None
        self.generation = 0

    def _reset(self) -> None:
        self.offset = 0
        self.pending = b""
        self.completed = 0
        self.scene_loaded = False
        self.scene: dict | None = None
        self.scene_changes = 0
        self.heartbeats.clear()
        self.frontend = None
        self.diagnostics = None
        self.generation += 1

    def _line(self, line: bytes) -> None:
        if match := _COMPLETED.search(line):
            self.completed = int(match[1])
        if _SCENE in line.lower():
            self.scene_loaded = True
        if match := _SCENE_RECEIPT.search(line):
            current = {"swap": int(match[1]), "observation": int(match[2]),
                       "available": match[3] in (b'true', b'1'), "map_id": int(match[4]),
                       "package": match[5].decode('utf-8', errors='replace')}
            if self.scene is not None and (
                    not current['available'] or not self.scene['available'] or
                    current['observation'] <= self.scene['observation'] or
                    current['map_id'] != self.scene['map_id'] or
                    current['package'] != self.scene['package']):
                self.scene_changes += 1
            self.scene = current
        if match := _HEARTBEAT.search(line):
            self.heartbeats.append((int(match[1]), int(match[2]),
                                    match[3].decode('ascii', errors='replace')))
        if match := _FRONTEND.search(line):
            self.frontend = {key.decode('ascii'): int(value)
                             for key, value in _COUNTER.findall(match[0])}
        if b'native frontend diagnostics:' in line:
            self.diagnostics = line.decode('utf-8', errors='replace')

    def poll(self) -> None:
        try:
            with self.path.open('rb') as stream:
                stat = os.fstat(stream.fileno())
                identity = (stat.st_dev, stat.st_ino)
                if (self.identity is not None and identity != self.identity) or stat.st_size < self.offset:
                    self._reset()
                self.identity = identity
                stream.seek(self.offset)
                remaining = stat.st_size - self.offset
                while remaining:
                    chunk = stream.read(min(remaining, 131072))
                    if not chunk:
                        break
                    self.offset += len(chunk)
                    remaining -= len(chunk)
                    lines = (self.pending + chunk).split(b'\n')
                    self.pending = lines.pop()
                    for line in lines:
                        self._line(line)
                    # A malformed, unbounded line must not grow probe memory.
                    # All supported receipts are below this bounded suffix.
                    self.pending = self.pending[-16384:]
        except FileNotFoundError:
            return

    def uhra_heartbeat(self) -> tuple[int, int, str] | None:
        if len(self.heartbeats) == 2 and all(draws >= 800 and name == 'xenon_scr.fpd'
                                           for _, draws, name in self.heartbeats):
            return self.heartbeats[-1]
        return None

    def fresh_scene(self) -> bool:
        # A receipt can precede its own completed-present line by one swap.
        return bool(self.scene and self.scene['available'] and self.scene['observation'] > 0
                    and self.scene['package'] and -1 <= self.completed - self.scene['swap'] <= 240)

    def ready(self, scene: str, warmup_frame: int, require_heartbeat: bool) -> bool:
        self.poll()  # Always observe early load markers, even before warm-up.
        if self.completed < warmup_frame:
            return False
        if scene == 'title':
            return True
        if not self.fresh_scene():
            return False
        beat = self.uhra_heartbeat()
        return not require_heartbeat or (beat is not None and beat[0] >= warmup_frame - 120)


def frontend_delta(first: dict[str, int] | None, last: dict[str, int] | None) -> dict[str, int] | None:
    """Cumulative execution counters; their swap window is not the CPU window."""
    if first is None or last is None or last.get('swap', 0) <= first.get('swap', 0):
        return None
    counters = ('mesh_commands', 'native_draws', 'other_draws', 'predicated_skips', 'words', 'state_values')
    if any(key not in first or key not in last or last[key] < first[key] for key in counters):
        return None
    return {'start_swap': first['swap'], 'end_swap': last['swap'],
            **{key: last[key] - first[key] for key in counters}}


def frontend_coverage(delta: dict[str, int] | None) -> dict | None:
    if delta is None:
        return None
    swaps = delta['end_swap'] - delta['start_swap']
    total = delta['native_draws'] + delta['other_draws']
    return {'native_draws_per_receipt_swap': delta['native_draws'] / swaps,
            'native_draw_fraction': delta['native_draws'] / total if total else None,
            'backend_calls': total, 'receipt_swaps': swaps}


def scene_window(first: dict | None, last: dict | None, unchanged: bool,
                 last_completed: int) -> bool:
    # Proves matching observed map identity across receipts, not pixel equality
    # or a particular Uhra location. Screenshots/expected map ID are separate.
    return bool(first and last and unchanged and first['available'] and last['available']
                and first['observation'] > 0 and last['observation'] > first['observation']
                and last['swap'] > first['swap']
                and last['map_id'] == first['map_id'] and first['package']
                and last['package'] == first['package']
                and -1 <= last_completed - last['swap'] <= 240)


def probe_environment(args: argparse.Namespace, run: Path, inherited: dict[str, str]) -> dict[str, str]:
    env = {k: v for k, v in inherited.items()
           if not k.upper().startswith(("LO_", "VK_LAYER", "VK_INSTANCE_LAYERS"))}
    env.update(LO_NATIVE_COMMANDS="0" if args.mode == "off" else args.mode,
               LO_NATIVE_FRONTEND=args.native_frontend,
               LO_BACKGROUND="1", LO_AUDIO_MUTE="1", LO_DLSS_FG="0", LO_FG_PROVIDER="off",
               LO_FRAME_TIMING="1", LO_LOG_FILE=str(run / "runtime.log"),
               LO_SHADER_CACHE_DIR=str(run / "shader-cache"),
               LO_SCREENSHOT_REQUEST=str(run / "screenshot-request.txt"),
               LO_SCREENSHOT_PATH=str(run / "scene.ppm"))
    if args.scene == "uhra":
        env.update(LO_AUTO_BUTTONS="s@120,a@240,a@360,a@480,a@700,a@900",
                   LO_AUTO_PULSE="6", LO_NATIVE_PROBE_SCENE="1")
        # Cross-build comparisons must not move for a frame-count-dependent
        # interval. The previous route remains an explicit exploratory option.
        if getattr(args, 'movement', 'stationary') == 'fixed-swaps':
            env['LO_AUTO_STICK'] = "0,18000,1600,1900"
    if args.scene_stats:
        env["LO_GPU_STATS"] = "1"
    if args.render_timing:
        env["LO_RENDER_TIMING"] = "1"
    if args.frontend_stats:
        env["LO_NATIVE_FRONTEND_STATS"] = "1"
    return env
