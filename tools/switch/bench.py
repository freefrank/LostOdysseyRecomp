#!/usr/bin/env python3
"""Benchmark runs on the Switch, from tools/scenarios/*.toml (docs/SWITCH-BENCHMARKS.md).

The console reads its LO_* switches from env.txt; a run is one game start with a
benchmark env.txt (LO_BENCH): it presses the scenario's buttons, waits for its
scene, settles, measures a fixed number of frames, logs one "bench result" line,
takes a screenshot and quits. This script writes that env.txt, collects the
results over the network log (LO_NETLOG) or from copied runtime logs, and turns
them into a report and an A/B comparison. With the game's debug server on
(docs/SWITCH.md, "Debug server"), --debug-server does all of it: it uploads
env.txt and the save slot, restarts the game for each run, reads the results
from the server's log stream and puts the console's own env.txt back.

    python3 tools/switch/bench.py env disc1-ship --out env.txt     # then copy env.txt to the SD card
    python3 tools/switch/bench.py watch 192.168.1.87 --runs 3      # start the game 3 times; results land in out/bench/
    python3 tools/switch/bench.py env disc1-ship --debug-server 192.168.1.87 --runs 3   # no hands
    python3 tools/switch/bench.py parse runtime-*.log --json out/bench/manual.json
    python3 tools/switch/bench.py report out/bench/<run>/results.json --md
    python3 tools/switch/bench.py compare baseline.json candidate.json

Per-console overrides go in a scenario's [switch] table (all optional):

    [switch]
    buttons = "s@20s,d@30s,a@40s"   # LO_AUTO_BUTTONS: swap stamps, or seconds with an "s"
    start_draws = 700               # the scene: the first frame with at least this many draws
    start_swap = 0                  # and not before this swap
    stick = [0, 28000]              # left stick held from the scene to the end of the measurement
    settle = 150                    # frames between reaching the scene and measuring
    frames = 600                    # frames measured
    timeout = 900                   # seconds before a run that never reaches its scene gives up
"""
from __future__ import annotations

import argparse
import ftplib
import io
import json
import re
import socket
import statistics
import sys
import time
import tomllib
from datetime import datetime
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
SCENARIOS = REPO_ROOT / "tools/scenarios"
RELEASE_ENV = REPO_ROOT / "tools/switch/release-env.txt"
# What a run renders when neither its base env.txt nor --set says (the console's Graphics menu would decide).
# FSR stays off: the Switch's presets turn it on, and it adds GPU time at the output's size.
PINNED_RENDERING = (("LO_SWITCH_RENDER_HEIGHT", "540"), ("LO_DYNAMIC_RESOLUTION", "0"), ("LO_SWITCH_UPSCALER", "bilinear"))
DEFAULT_OUT = REPO_ROOT / "out/bench"
DATA_ROOT = "sdmc:/switch/LostOdysseyRecomp"
NETLOG_PORT = 28780

RESULT_RE = re.compile(r"bench result: (.*)$")
DONE_RE = re.compile(r"bench done: name=(\S+) status=(\S+)")
ENV_RE = re.compile(r"bench env:(.*)$")
BUILD_RE = re.compile(r"build: source=(\S+) revision=(\S+)")
PERF_RE = re.compile(r"\[ *([0-9.]+) t[0-9a-f]+\] \[note\]\s+perf: (.*)$")
HITCH_RE = re.compile(r"\[note\]\s+hitch: (.*)$")
OPTION_RE = re.compile(r"\[note\]\s+option: (.*)$")
# The fields of an option line that describe it rather than name an option.
OPTION_META = {"frame", "source", "applies", "env", "saved", "env_wins", "overrides_env", "built"}
SHOT_RE = re.compile(r"bench screenshot at swap \d+ -> (\S+) \((ok|failed)\)")
INPUT_RE = re.compile(r"auto input: '(.)' at swap (\d+), ([0-9.]+) s")

# The figures a report and a comparison show, and which way is better.
METRICS = [
    ("fps", "FPS", "higher", 2),
    ("ms_p50", "frame ms p50", "lower", 2),
    ("ms_p95", "frame ms p95", "lower", 2),
    ("ms_p99", "frame ms p99", "lower", 2),
    ("ms_max", "frame ms max", "lower", 1),
    ("low1_fps", "1% low FPS", "higher", 2),
    ("late_pct", "late frames %", "lower", 1),
    ("hitches", "hitches", "lower", 0),
    ("gpu_ms", "GPU busy ms/frame", "lower", 2),
    ("gpu_pct", "GPU busy %", None, 1),
    ("cp_idle_ms", "render thread idle ms/frame", None, 2),
    ("pace_ms", "pacing sleep ms/frame", None, 2),
    ("present_ms", "present ms/frame", "lower", 2),
    ("draws", "draws/frame", None, 0),
    ("gpu_draws", "submitted draws/frame", None, 0),
    ("pipeline_binds", "pipeline binds/frame", "lower", 1),
    ("state_binds", "state binds/frame", "lower", 1),
    ("descriptor_writes", "descriptor writes/frame", "lower", 1),
    ("barriers_full", "full barriers/frame", "lower", 1),
    ("uploads", "uploads/frame", "lower", 2),
    ("compiles", "shader compiles", "lower", 0),
    ("cpu_total_pct", "CPU, all threads (% of one core)", None, 0),
]


# ---------------------------------------------------------------- scenarios

def find_scenario(name: str) -> Path:
    path = Path(name)
    if path.is_file():
        return path
    candidate = SCENARIOS / f"{name}.toml"
    if candidate.is_file():
        return candidate
    sys.exit(f"bench: no scenario '{name}' (looked for {path} and {candidate})")


def bench_settings(path: Path) -> dict:
    """A scenario's benchmark settings: its own fields, then its [switch] table."""
    with open(path, "rb") as f:
        s = tomllib.load(f)
    steps = s.get("step", [])
    first = steps[0] if steps else {}
    # The scenario's hold time, at the console's 30 FPS, is what it measures.
    hold = float(first.get("hold_seconds", 30))
    settings = {
        "name": path.stem,
        "description": s.get("description", ""),
        "save": s.get("save"),
        "buttons": s.get("buttons", ""),
        "start_draws": int(first.get("wait_draws", 0)),
        "start_swap": 0,
        "stick": first.get("stick"),
        "settle": 150,
        "frames": max(300, int(hold * 30)),
        "timeout": 900,
        "pulse": int(s.get("pulse", 6)),
    }
    if not settings["buttons"] and s.get("timeline"):
        # Seconds since launch: the console form of a PC timeline (loads are slower there; retune in [switch]).
        settings["buttons"] = ",".join(f"{entry.strip()}s" for entry in s["timeline"].split(",") if entry.strip())
    settings.update(s.get("switch", {}))
    return settings


def env_lines(settings: dict, args) -> list[str]:
    name, frames = settings["name"], args.frames or settings["frames"]
    if args.fps:
        # A run at another frame rate target is another benchmark: its name says so, and it measures as long.
        name = f"{name}-{args.fps}fps"
        frames = args.frames or max(300, settings["frames"] * args.fps // 30)
    lines = [
        f"# Benchmark run: {name} ({settings['description']})",
        f"# Written by tools/switch/bench.py on {datetime.now():%Y-%m-%d %H:%M}. Restore the normal env.txt afterwards.",
        f"LO_BENCH={name}",
        f"LO_BENCH_FRAMES={frames}",
        f"LO_BENCH_SETTLE={settings['settle']}",
        f"LO_BENCH_TIMEOUT={settings['timeout']}",
    ]
    if settings["start_draws"]:
        lines.append(f"LO_BENCH_START_DRAWS={settings['start_draws']}")
    if settings["start_swap"]:
        lines.append(f"LO_BENCH_START_SWAP={settings['start_swap']}")
    if settings["stick"]:
        x, y = settings["stick"]
        lines.append(f"LO_BENCH_STICK={int(x)},{int(y)}")
    if not args.manual and settings["buttons"]:
        lines.append(f"LO_AUTO_BUTTONS={settings['buttons']}")
        lines.append(f"LO_AUTO_PULSE={settings['pulse']}")
    if not args.no_shot:
        lines.append("LO_BENCH_SHOT=1")
        lines.append(f"LO_SCREENSHOT_PATH=bench-{name}.ppm")
    if not args.no_exit:
        lines.append("LO_BENCH_EXIT=1")
    if args.fps:
        lines.append(f"LO_FPS={args.fps}")
    for setting in args.set:
        # An A/B switch for this run (bench.py compare warns when two results' LO_* settings differ).
        if "=" not in setting:
            sys.exit(f"bench: --set {setting}: KEY=VALUE expected")
        lines.append(setting)
    if settings["save"]:
        # The run's own save folder: the player's slots are never read or written.
        lines.append(f"LO_SAVE_DIR={DATA_ROOT}/bench/save")
    lines.append(f"LO_NETLOG={args.netlog_port}")
    # Scripted presses are not input to the system: without this a long run could put the console to sleep.
    lines.append("LO_SWITCH_AUTO_SLEEP=0")
    return lines


def cmd_env(args) -> None:
    settings = bench_settings(find_scenario(args.scenario))
    if args.debug_server and args.ftp:
        sys.exit("bench: --debug-server or --ftp, not both")
    con = original = None
    if args.debug_server:
        con = debug_console(args.debug_server)
        original = read_original_env(con)
        # The game must keep running for the next restart, and its server must start with the bench env.txt.
        args.no_exit = True
    base = Path(args.base).read_text(encoding="utf-8").splitlines() if args.base else []
    bench = env_lines(settings, args)
    if con:
        bench += debug_env_lines(original)
    import env_comments  # next to this file
    # The release env.txt leaves the scene height and dynamic resolution to the Graphics menu (settings.ini):
    # pin them so a run measures the same frames on every console, unless the base file or --set chooses.
    chosen = {line.split("=", 1)[0].strip() for line in base + bench if env_comments.is_setting(line)}
    for key, value in PINNED_RENDERING:
        if key not in chosen:
            bench.append(f"{key}={value}")
    keys = {line.split("=", 1)[0].strip() for line in bench if env_comments.is_setting(line)}
    # The bench lines win over the same keys in the base file (their comments go with them); every setting
    # keeps a comment that says what it does.
    kept = env_comments.without(base, keys)
    text = "\n".join(env_comments.commented(kept + [""] + bench)) + "\n"
    if args.out == "-":
        sys.stdout.write(text)
    else:
        Path(args.out).write_text(text, encoding="utf-8")
        print(f"wrote {args.out} for '{settings['name']}'{f' at {args.fps} FPS' if args.fps else ''}: "
              f"{'uploading it to the console' if con else f'copy it to {DATA_ROOT}/env.txt'}")
    if settings["save"] and not con:
        print(f"the run loads save slot {settings['save']}: copy that folder (out/drive-city/save/{settings['save']}) to "
              f"{DATA_ROOT}/bench/save/{settings['save']}", file=sys.stderr)
    if args.ftp:
        upload(args, text, settings)
    if con:
        debug_server_runs(args, con, original, text, settings)


def upload(args, text: str, settings: dict) -> None:
    """Puts env.txt (and the scenario's save slot) on the card through an FTP server on the console (ftpd, sys-ftpd)."""
    host, _, port = args.ftp.partition(":")
    root = "/switch/LostOdysseyRecomp"
    with ftplib.FTP() as ftp:
        ftp.connect(host, int(port or 5000), timeout=15)
        ftp.login(args.ftp_user or "", args.ftp_password or "")
        ftp.storbinary(f"STOR {root}/env.txt", io.BytesIO(text.encode()))
        print(f"uploaded env.txt to {host}:{root}/env.txt")
        if settings["save"] and args.saves:
            source = Path(args.saves) / settings["save"]
            if not source.is_dir():
                sys.exit(f"bench: save folder {source} not found")
            target = f"{root}/bench/save/{settings['save']}"
            for part in ("bench", "bench/save", f"bench/save/{settings['save']}"):
                try:
                    ftp.mkd(f"{root}/{part}")
                except ftplib.error_perm:
                    pass  # already there
            for file in sorted(source.iterdir()):
                if file.is_file():
                    with open(file, "rb") as f:
                        ftp.storbinary(f"STOR {target}/{file.name}", f)
            print(f"uploaded save slot {settings['save']} to {target}")


# ---------------------------------------------------------------- debug server

def debug_console(address: str):
    """The game's debug server at IP[:PORT] (tools/switch/lo_debug.py), checked to be this game's."""
    import lo_debug  # next to this file
    host, _, port = address.partition(":")
    con = lo_debug.Console(host, int(port or lo_debug.DEFAULT_PORT))
    try:
        con.check_game()
    except (OSError, lo_debug.ServerError) as e:
        sys.exit(f"bench: no debug server at {host}:{con.port}: {e}")
    return con


def read_original_env(con) -> bytes | None:
    """The console's env.txt as it is (None: there is none), to put back after the runs."""
    import lo_debug
    try:
        return con.get_bytes("env.txt")
    except lo_debug.ServerError:
        return None


def debug_env_lines(original: bytes | None) -> list[str]:
    """The debug server on for every run, with the console's own restart and port switches."""
    lines = ["LO_DEBUG_SERVER=1"]
    for line in (original or b"").decode("utf-8", "replace").splitlines():
        key = line.split("=", 1)[0].strip()
        if key in ("LO_DEBUG_RESTART", "LO_DEBUG_PORT") and not line.lstrip().startswith("#"):
            lines.append(line.strip())
    return lines


def restart_game(con, state: dict) -> None:
    """Starts the game again and waits for its new session; asks the player to when restart is off (title takeover)."""
    import lo_debug
    if not state.get("by_hand"):
        try:
            con.restart()
            return
        except lo_debug.ServerError as e:
            print(f"restart refused: {e}")
            state["by_hand"] = True
    before = con.session()
    print("close the game on the console (HOME, then X) and start it again; waiting...", flush=True)
    if not con.wait(3600, other_than=before):
        raise SystemExit("bench: the game did not start again within an hour")


def read_run(con, raw, seconds: float, shot: bool = False) -> list[str]:
    """The running session's log from its start (the server keeps it) until its 'bench done' line, and with shot
    until its screenshot is on the card: a restart before that loses it (the PPM is written after a GPU readback)."""
    import lo_debug
    conn = lo_debug.Conn(con.host, con.port)
    try:
        conn.sock.sendall(b"log all\n")
        ok, _ = conn.header()
        if not ok:
            raise SystemExit("bench: the debug server refused the log stream")
        conn.sock.settimeout(1.0)
        session, pending, end = [], conn.buf, time.time() + seconds
        conn.buf = b""
        while time.time() < end:
            *lines, pending = pending.split(b"\n")
            for line in lines:
                raw.write(line + b"\n")
                text = line.decode("utf-8", "replace")
                session.append(text)
                if any(key in text for key in ("bench:", "bench done", "auto input:", "build:")):
                    print(text, flush=True)
                if (m := DONE_RE.search(text)) and not (shot and m.group(2) == "ok"):
                    raw.flush()
                    return session
                if m:
                    end = min(end, time.time() + 60)  # the screenshot line follows within seconds
                if SHOT_RE.search(text):
                    raw.flush()
                    return session
            try:
                data = conn.sock.recv(65536)
            except socket.timeout:
                continue
            if not data:
                print("the log stream closed before the result (the game quit or crashed: lo_debug.py lastlog)")
                break
            pending += data
        else:
            print(f"no result within {seconds:.0f} s")
        raw.flush()
        return session
    finally:
        conn.close()


def debug_server_runs(args, con, original: bytes | None, text: str, settings: dict) -> None:
    """Uploads the run's env.txt and save slot, restarts the game for each run and collects the results."""
    import lo_debug
    con.put_bytes(text.encode(), "env.txt", quiet=True)
    print(f"uploaded env.txt to {con.host}:{con.port}")
    try:
        if settings["save"]:
            source = Path(args.saves) / settings["save"]
            if not source.is_dir():
                sys.exit(f"bench: save folder {source} not found")
            target = f"bench/save/{settings['save']}"
            for part in ("bench", "bench/save", target):
                con.command("mkdir", part)
            for file in sorted(source.iterdir()):
                if file.is_file():
                    con.put_bytes(file.read_bytes(), f"{target}/{file.name}", quiet=True)
            print(f"uploaded save slot {settings['save']} to {target}")
        out = Path(args.results_out) / datetime.now().strftime("%Y%m%d-%H%M%S")
        out.mkdir(parents=True, exist_ok=True)
        frames = args.frames or settings["frames"]
        seconds = settings["timeout"] + settings["settle"] / 15 + frames / 15 + 120
        runs: list[dict] = []
        state: dict = {}
        wanted = args.runs + args.warmup
        with open(out / "log.txt", "ab") as raw:
            for attempt in range(1, wanted + 1):
                print(f"--- run {attempt} of {wanted} ({args.warmup} warm-up) {time.strftime('%H:%M:%S')}")
                restart_game(con, state)
                session = read_run(con, raw, seconds, shot=not args.no_shot)
                found = parse_lines(session, f"debug server run {attempt}")
                if not found:
                    print(f"run {attempt}: no result")
                    continue
                run = found[-1]
                fetch_shot(con, session, out / f"run{attempt}.png", run)
                if attempt <= args.warmup:
                    run["warmup"] = True  # shader caches and the background pipeline build warm up here
                    print("(warm-up run, not counted)")
                runs.append(run)
                print_run(run)
                time.sleep(2)  # the log writer's last lines reach the card before the next restart
        finish(runs, out)
    finally:
        if args.keep_env:
            print("left the benchmark env.txt on the console (--keep-env)")
        else:
            restore_env(con, original)


def ppm_to_png(ppm: bytes) -> bytes:
    """A binary PPM (P6, 8 bits) as an uncompressed-filter PNG."""
    import struct
    import zlib
    fields, offset = [], 0
    while len(fields) < 4:  # magic, width, height, maximum; '#' comments allowed
        while ppm[offset:offset + 1].isspace():
            offset += 1
        if ppm[offset:offset + 1] == b"#":
            offset = ppm.index(b"\n", offset) + 1
            continue
        start = offset
        while not ppm[offset:offset + 1].isspace():
            offset += 1
        fields.append(ppm[start:offset])
    offset += 1
    width, height = int(fields[1]), int(fields[2])
    pixels = ppm[offset:offset + width * height * 3]
    rows = b"".join(b"\x00" + pixels[y * width * 3:(y + 1) * width * 3] for y in range(height))
    chunk = lambda kind, data: (struct.pack(">I", len(data)) + kind + data +
                                struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(rows, 6)) + chunk(b"IEND", b""))


def fetch_shot(con, session: list[str], target: Path, run: dict) -> None:
    """Copies the run's screenshot into the results as a PNG and deletes it from the card."""
    shots = [m for line in session if (m := SHOT_RE.search(line))]
    if not shots:
        return
    path, status = shots[-1].group(1), shots[-1].group(2)
    if status != "ok":
        print(f"the console could not save the screenshot ({path})")
        return
    # A relative path is the game's working directory's: config/ on the console (main.cpp), not the app folder.
    if not path.startswith("/") and not path.startswith("sdmc:"):
        path = f"config/{path}"
    elif path.startswith("sdmc:/switch/LostOdysseyRecomp/"):
        path = path[len("sdmc:/switch/LostOdysseyRecomp/"):]
    try:
        target.write_bytes(ppm_to_png(con.get_bytes(path)))
        con.command("rm", path)
        run["screenshot"] = str(target)
        print(f"screenshot: {target}")
    except Exception as error:  # the result counts without its screenshot
        print(f"could not fetch the screenshot {path}: {error}")


def restore_env(con, original: bytes | None) -> None:
    import lo_debug
    try:
        if original is None:
            con.command("rm", "env.txt")
        else:
            con.put_bytes(original, "env.txt", quiet=True)
        print("put the console's own env.txt back" if original is not None else "removed the benchmark env.txt "
              "(the console had none)")
    except (OSError, lo_debug.ServerError) as e:
        print(f"bench: could not restore env.txt ({e}); put it back by hand: {DATA_ROOT}/env.txt", file=sys.stderr)
        return
    # Not waited for: the console's own env.txt may leave the debug server off.
    try:
        print(con.text("restart", timeout=10.0))
    except (OSError, lo_debug.ServerError) as e:
        print(f"not restarted ({e}): start the game again to use its own env.txt")


# ---------------------------------------------------------------- logs

def value(text: str):
    try:
        number = float(text)
        return int(number) if number.is_integer() and "." not in text else number
    except ValueError:
        return text


def parse_fields(text: str) -> dict:
    fields = {}
    for token in text.split():
        key, sep, val = token.partition("=")
        if sep:
            fields[key] = value(val)
    if isinstance(fields.get("threads"), str) and fields["threads"] != "-":
        # Display names are not unique (e.g. several Guest_82beca90 threads).
        # Preserve every sample; an ordinal is not an OS thread ID.
        fields["thread_samples"] = [
            {"name": name, "ordinal": i, "pct": float(pct)}
            for i, (name, _, pct) in enumerate(
                entry.rpartition(":") for entry in fields["threads"].split(","))]
        # Keep the old field for readers of existing JSON, without using it for ranking.
        fields["threads"] = {item["name"]: item["pct"] for item in fields["thread_samples"]}
    return fields


def parse_lines(lines, source: str) -> list[dict]:
    """Every bench result in a log, with the build it came from, its env and the perf lines it overlaps."""
    runs, build, perf, hitches, inputs, options = [], None, [], [], [], []
    current = None
    for line in lines:
        if m := BUILD_RE.search(line):
            build = m.group(2)
            perf, hitches, inputs, options = [], [], [], []
        elif m := OPTION_RE.search(line):
            options.append(parse_fields(m.group(1)))
        elif m := INPUT_RE.search(line):
            inputs.append({"button": m.group(1), "swap": int(m.group(2)), "seconds": float(m.group(3))})
        elif m := PERF_RE.search(line):
            perf.append({"t": float(m.group(1)), **parse_fields(m.group(2))})
        elif m := HITCH_RE.search(line):
            hitches.append(parse_fields(m.group(1)))
        elif m := RESULT_RE.search(line):
            current = {"source": source, "build": build, **parse_fields(m.group(1)),
                       "inputs": inputs, "perf": perf[-6:], "hitch_lines": hitches[-50:]}
            current["options"], current["option_changes"] = options_during(options, current)
            runs.append(current)
        elif (m := ENV_RE.search(line)) and current is not None:
            current["env"] = dict(token.split("=", 1) for token in m.group(1).split() if "=" in token)
    return runs


def options_during(lines: list[dict], run: dict) -> tuple[dict, list[dict]]:
    """The options (Debug page, env.txt; "option:" lines) in effect when the run started measuring, and the changes
    that took effect while it measured. A line's frame is the first presented frame that runs with its value."""
    start, end = run.get("start_swap"), run.get("end_swap")
    at_start, changes = {}, []
    for fields in lines:
        frame = fields.get("frame")
        if fields.get("applies") == "next_start" or not isinstance(frame, int):
            continue
        values = {k: v for k, v in fields.items() if k not in OPTION_META}
        if not isinstance(start, int) or frame <= start + 1:
            at_start.update(values)
        elif not isinstance(end, int) or frame <= end:
            changes.append({"frame": frame, **values})
    return at_start, changes


def cmd_parse(args) -> None:
    runs = []
    for name in args.logs:
        with open(name, encoding="utf-8", errors="replace") as f:
            runs += parse_lines(f, name)
    if not runs:
        sys.exit("bench: no 'bench result' lines found")
    write_results(runs, Path(args.json) if args.json else None)
    print_report(runs)


def write_results(runs: list[dict], path: Path | None) -> None:
    if path:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps({"runs": runs}, indent=1), encoding="utf-8")
        print(f"wrote {path}")


def load_runs(paths) -> list[dict]:
    runs = []
    for path in paths:
        runs += json.loads(Path(path).read_text(encoding="utf-8"))["runs"]
    return runs


# ---------------------------------------------------------------- live capture

def cmd_watch(args) -> None:
    """Streams the network log of every game start until --runs results have come in."""
    out = Path(args.out) / datetime.now().strftime("%Y%m%d-%H%M%S")
    out.mkdir(parents=True, exist_ok=True)
    raw = open(out / "netlog.txt", "ab")
    runs: list[dict] = []
    seen: set[tuple] = set()
    wanted = args.runs + args.warmup
    print(f"waiting for {wanted} run(s) ({args.warmup} warm-up) from {args.host}:{args.port}; start the game on the "
          f"console each time (saving to {out})")
    while len(runs) < wanted:
        try:
            with socket.create_connection((args.host, args.port), timeout=5) as conn:
                conn.settimeout(None)
                conn.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
                print(f"--- connected {time.strftime('%H:%M:%S')}")
                session, pending = [], b""
                while len(runs) < wanted:
                    data = conn.recv(65536)
                    if not data:
                        break
                    raw.write(data)
                    raw.flush()
                    pending += data
                    *lines, pending = pending.split(b"\n")
                    for line in lines:
                        text = line.decode("utf-8", "replace")
                        session.append(text)
                        if any(key in text for key in ("bench:", "bench done", "auto input:", "build:", "option:")) or \
                                (args.verbose and ("perf:" in text or "hitch:" in text)):
                            print(text, flush=True)
                        if DONE_RE.search(text):
                            found = parse_lines(session, f"netlog run {len(runs) + 1}")
                            session = []
                            if not found:
                                continue
                            run = found[-1]
                            # A reconnect replays the newest megabyte of the log: the same result again.
                            key = (run.get("build"), run.get("name"), run.get("start_swap"), run.get("end_swap"),
                                   run.get("seconds"))
                            if key in seen:
                                continue
                            seen.add(key)
                            if len(runs) < args.warmup:
                                # Shader caches and the background pipeline build warm up here: kept, not counted.
                                run["warmup"] = True
                                print("(warm-up run, not counted)")
                            runs.append(run)
                            print_run(run)
        except OSError:
            pass
        if len(runs) < wanted:
            time.sleep(2)
    raw.close()
    finish(runs, out)


def finish(runs: list[dict], out: Path) -> None:
    if not runs:
        print("bench: no results")
        return
    write_results(runs, out / "results.json")
    print_report(runs)
    (out / "report.md").write_text(markdown_report(runs), encoding="utf-8")
    print(f"wrote {out / 'report.md'}")


# ---------------------------------------------------------------- reports

def fmt(x, digits=1) -> str:
    if x is None or x == "-":
        return "-"
    if isinstance(x, (int, float)):
        return f"{x:.{digits}f}"
    return str(x)


def print_run(run: dict) -> None:
    print(f"result {run.get('name')} [{run.get('status')}]: {fmt(run.get('fps'), 2)} FPS, p99 {fmt(run.get('ms_p99'))} ms, "
          f"GPU {fmt(run.get('gpu_ms'))} ms/frame, {fmt(run.get('draws'), 0)} draws, {run.get('mode', '?')} "
          f"GPU {fmt(run.get('gpu_mhz'), 0)} MHz", flush=True)
    for change in run.get("option_changes", []):
        print(f"  warning: an option changed while it measured: {change}", flush=True)


def group(runs: list[dict]) -> dict[str, list[dict]]:
    groups: dict[str, list[dict]] = {}
    for run in runs:
        if run.get("status") == "ok" and not run.get("warmup"):
            groups.setdefault(run["name"], []).append(run)
    return groups


def summary(runs: list[dict], key: str):
    values = [r[key] for r in runs if isinstance(r.get(key), (int, float))]
    if not values:
        return None
    return {"median": statistics.median(values), "min": min(values), "max": max(values), "n": len(values)}


def conditions(runs: list[dict]) -> str:
    first = runs[0]
    env = first.get("env", {})
    return (f"build {first.get('build')}, {first.get('mode', '?')}, CPU {fmt(first.get('cpu_mhz'), 0)} / "
            f"GPU {fmt(first.get('gpu_mhz'), 0)} / memory {fmt(first.get('mem_mhz'), 0)} MHz, scene {first.get('scene')}, "
            f"backend {env.get('LO_SWITCH_BACKEND', 'nvk')}, target {first.get('target_fps')} FPS, map {first.get('map')}")


def print_report(runs: list[dict]) -> None:
    failed = [r for r in runs if r.get("status") != "ok"]
    for run in failed:
        print(f"{run.get('name')}: status {run.get('status')} ({run.get('source')})")
    for name, items in group(runs).items():
        print(f"\n{name}: {len(items)} run(s); {conditions(items)}")
        for key, label, _, digits in METRICS:
            s = summary(items, key)
            if s:
                spread = f" (min {fmt(s['min'], digits)}, max {fmt(s['max'], digits)})" if s["n"] > 1 else ""
                print(f"  {label:34} {fmt(s['median'], digits):>9}{spread}")
        threads = thread_summary(items)
        if threads:
            print(f"  {'threads (% of one core)':30} {threads}")


def thread_summary(runs: list[dict]) -> str:
    names: dict[str, list[float]] = {}
    # Names that more than one thread has in any of the runs: ranked in every run, so that a run whose
    # line left out one of them (under 1%, or past the top 8) still adds to the same entries.
    shared = set()
    for run in runs:
        seen = set()
        for sample in run.get("thread_samples") or []:
            (shared if sample["name"] in seen else seen).add(sample["name"])
    for run in runs:
        if isinstance(run.get("thread_samples"), list):
            # Threads that share a name are ranked by their load within each run: the line has no thread ID.
            grouped = {}
            for sample in run["thread_samples"]:
                grouped.setdefault(sample["name"], []).append(sample["pct"])
            for name, values in grouped.items():
                for rank, pct in enumerate(sorted(values, reverse=True), 1):
                    label = f"{name} (load rank {rank})" if name in shared else name
                    names.setdefault(label, []).append(pct)
        elif isinstance(run.get("threads"), dict):
            for name, pct in run["threads"].items():
                names.setdefault(name, []).append(pct)
    ranked = sorted(names.items(), key=lambda item: -statistics.median(item[1]))[:5]
    return ", ".join(f"{name} {statistics.median(pcts):.0f}" for name, pcts in ranked)


def markdown_report(runs: list[dict]) -> str:
    out = []
    for name, items in group(runs).items():
        out.append(f"### {name}\n\n{len(items)} run(s); {conditions(items)}.\n")
        out.append("| Metric | Median | Min | Max |\n|---|---:|---:|---:|")
        for key, label, _, digits in METRICS:
            s = summary(items, key)
            if s:
                out.append(f"| {label} | {fmt(s['median'], digits)} | {fmt(s['min'], digits)} | {fmt(s['max'], digits)} |")
        threads = thread_summary(items)
        if threads:
            out.append(f"\nBusiest threads (median % of one core): {threads}.")
        out.append("")
    failed = [r for r in runs if r.get("status") != "ok"]
    if failed:
        out.append("Runs left out: " + ", ".join(f"{r.get('name')} ({r.get('status')})" for r in failed) + ".\n")
    return "\n".join(out)


def cmd_report(args) -> None:
    runs = load_runs(args.results)
    print_report(runs)
    if args.md:
        print("\n" + markdown_report(runs))


def cmd_compare(args) -> None:
    base, cand = group(load_runs([args.baseline])), group(load_runs([args.candidate]))
    lines = ["| Scenario | Metric | Baseline | Candidate | Change |", "|---|---|---:|---:|---:|"]
    for name in sorted(set(base) & set(cand)):
        b_runs, c_runs = base[name], cand[name]
        for warn in comparability(b_runs[0], c_runs[0]):
            print(f"warning: {name}: {warn}", file=sys.stderr)
        for key, label, better, digits in METRICS:
            b, c = summary(b_runs, key), summary(c_runs, key)
            if not b or not c:
                continue
            change = "-"
            if b["median"]:
                pct = 100.0 * (c["median"] - b["median"]) / abs(b["median"])
                mark = ""
                if better and abs(pct) >= args.threshold:
                    mark = " ✅" if (pct > 0) == (better == "higher") else " ❌"
                change = f"{pct:+.1f}%{mark}"
            lines.append(f"| {name} | {label} | {fmt(b['median'], digits)} | {fmt(c['median'], digits)} | {change} |")
    for name in sorted(set(base) ^ set(cand)):
        print(f"note: {name} is only in the {'baseline' if name in base else 'candidate'}", file=sys.stderr)
    print("\n".join(lines))


def comparability(a: dict, b: dict) -> list[str]:
    """Differences that make two runs measure different things."""
    warnings = []
    for key in ("mode", "scene", "target_fps", "gpu_mhz", "mem_mhz"):
        if a.get(key) != b.get(key):
            warnings.append(f"{key} differs ({a.get(key)} vs {b.get(key)})")
    oa, ob = a.get("options", {}), b.get("options", {})
    for key in sorted(set(oa) | set(ob)):
        if oa.get(key) != ob.get(key):
            warnings.append(f"option {key} differs ({oa.get(key)} vs {ob.get(key)})")
    for side, run in (("baseline", a), ("candidate", b)):
        if run.get("option_changes"):
            warnings.append(f"an option changed during the {side} run")
    ea, eb = a.get("env", {}), b.get("env", {})
    for key in sorted(set(ea) | set(eb)):
        if key.startswith("LO_BENCH") or key in ("LO_NETLOG", "LO_SCREENSHOT_PATH"):
            continue
        if ea.get(key) != eb.get(key):
            warnings.append(f"env {key} differs ({ea.get(key)} vs {eb.get(key)})")
    return warnings


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)

    p = sub.add_parser("env", help="write a benchmark env.txt for a scenario")
    p.add_argument("scenario", help="a name from tools/scenarios or a .toml path")
    p.add_argument("--out", default="env.txt", help="output file, - for stdout")
    p.add_argument("--base", default=str(RELEASE_ENV), help="env.txt the run starts from (default: the release one); '' for none")
    p.add_argument("--frames", type=int, help="frames measured (default: the scenario's hold time at the target)")
    p.add_argument("--fps", type=int, help="frame rate target (LO_FPS); 60 measures headroom a 30 FPS cap hides")
    p.add_argument("--manual", action="store_true", help="no scripted presses: load the save by hand, the run starts at the scene")
    p.add_argument("--set", action="append", default=[], metavar="KEY=VALUE",
                   help="an extra env.txt setting for this run, over the base's (repeatable): an A/B switch")
    p.add_argument("--no-shot", action="store_true", help="no screenshot after the measurement")
    p.add_argument("--no-exit", action="store_true", help="keep the game running after the result")
    p.add_argument("--netlog-port", type=int, default=NETLOG_PORT)
    p.add_argument("--ftp", help="upload to an FTP server on the console: HOST[:PORT] (default port 5000)")
    p.add_argument("--ftp-user")
    p.add_argument("--ftp-password")
    p.add_argument("--saves", default=str(REPO_ROOT / "out/drive-city/save"),
                   help="where the scenario save slots are (for --ftp and --debug-server)")
    p.add_argument("--debug-server", metavar="IP[:PORT]",
                   help="the game's debug server (default port 6543): upload, restart for each run, collect the results, "
                        "put env.txt back")
    p.add_argument("--runs", type=int, default=3, help="with --debug-server: results to count")
    p.add_argument("--warmup", type=int, default=1, help="with --debug-server: runs before those, kept but not counted")
    p.add_argument("--results-out", default=str(DEFAULT_OUT), help="with --debug-server: where the results go")
    p.add_argument("--keep-env", action="store_true", help="with --debug-server: leave the benchmark env.txt on the console")
    p.set_defaults(func=cmd_env)

    p = sub.add_parser("watch", help="collect results from the console's network log")
    p.add_argument("host", help="the console's IP address")
    p.add_argument("--port", type=int, default=NETLOG_PORT)
    p.add_argument("--runs", type=int, default=3, help="results to count (one per game start)")
    p.add_argument("--warmup", type=int, default=1, help="results before those, kept but not counted (default 1)")
    p.add_argument("--out", default=str(DEFAULT_OUT))
    p.add_argument("--verbose", action="store_true", help="also print perf and hitch lines")
    p.set_defaults(func=cmd_watch)

    p = sub.add_parser("parse", help="read results from runtime logs or netlog captures")
    p.add_argument("logs", nargs="+")
    p.add_argument("--json", help="write the results here")
    p.set_defaults(func=cmd_parse)

    p = sub.add_parser("report", help="summarise results (median of the runs per scenario)")
    p.add_argument("results", nargs="+")
    p.add_argument("--md", action="store_true", help="also print a Markdown table")
    p.set_defaults(func=cmd_report)

    p = sub.add_parser("compare", help="A/B: baseline results against candidate results")
    p.add_argument("baseline")
    p.add_argument("candidate")
    p.add_argument("--threshold", type=float, default=3.0, help="percent change marked as a win or a loss")
    p.set_defaults(func=cmd_compare)

    args = parser.parse_args()
    if getattr(args, "base", None) == "":
        args.base = None
    args.func(args)


if __name__ == "__main__":
    main()
