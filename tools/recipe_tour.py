#!/usr/bin/env python3
"""Automated tours that record pipeline recipes (pipelines.bin) for the shipped corpus.

  python tools/recipe_tour.py list    <run_dir>
  python tools/recipe_tour.py battles <run_dir> [--formations all|cover|3,10-20] [--hold 45] [--budget S] [--disc N]
  python tools/recipe_tour.py stages  <run_dir> --stages names.txt [--formation 3]
  python tools/recipe_tour.py maps    <run_dir> --maps maps.json [--discs 1,2,3,4] [--budget S]

<run_dir> is a staged game folder (settings.ini, a save whose Continue loads a
field save, profile/, shaders/ with the matching pack) plus launch.json:
  {"exe": ..., "game": ".../disc1", "path": "win|z|posix", "prefix": ["umu-run"],
   "env": {"K": "V" or "glob:/pattern"}, "interrupt": [...], "kill": [...]}
("interrupt" asks the game to quit so it flushes the recipe file, "kill" cleans up;
both default to signalling the launched process.)
Recipes go to <run_dir>/shader-cache. Each command resumes from its result file.

list     logs the formation table (LO_DEBUG_BATTLE_FILE "list") into formations.json.
battles  starts each formation through the walking-encounter path, presses A on
         every command prompt, requests the debug Victory after --hold seconds
         and goes on from the field; defeat, a forced victory or a stall restarts
         the game. "cover" picks formations until every enemy config/parameter
         pair and every fixed battle stage has been seen once. Formations whose
         resources are not on the mounted disc fail ("fatal"); --disc N
         (diagnostic build) retries only those on disc N.
stages   fights one weak formation on each listed battle stage (the random
         encounter stages that no formation names).
maps     needs the diagnostic map jump / disc request / package probe build
         (LO_DIAG_MAPJUMP/DISC/PROBE_COMMAND_FILE, not in main): per disc, probe
         the maps, jump to each, turn the camera and dwell.
"""
import argparse
import json
import os
import re
import signal
import struct
import subprocess
import sys
import time
from pathlib import Path

PHASE = re.compile(r"battle debug: core 0x832ca0e8 phase (\d+) -> (\d+)")
MAP = re.compile(r"current map available=true id=(\d+) name=.* package=(\S+)")
FORMATION = re.compile(r"battle tour: formation (\d+)(.*?)(?: slots:(.*))?$")
A_BUTTON = "1000"


class Game:
    def __init__(self, run):
        self.run = run
        self.config = json.loads((run / "launch.json").read_text())
        self.proc = None
        self.offset = 0
        self.index = len(list((run / "logs").glob("runtime-*.log"))) if (run / "logs").exists() else 0
        self.serial = int(time.time() * 1000) % 1_000_000_000

    def path(self, p):
        style = self.config.get("path", "win" if os.name == "nt" else "posix")
        s = str(p)
        return s.replace("/", "\\") if style == "win" else "Z:" + s.replace("/", "\\") if style == "z" else s

    def start(self, extra=None):
        self.stop()
        self.index += 1
        (self.run / "logs").mkdir(exist_ok=True)
        for name in ("input.txt", "battle.txt", "mapjump.txt", "disc.txt", "probe.txt"):
            (self.run / name).unlink(missing_ok=True)
        self.log = self.run / "logs" / f"runtime-{self.index:02d}.log"
        r, p = self.run, self.path
        env = {k: v for k, v in os.environ.items() if not k.startswith("LO_")}
        env.update(LO_AUDIO_MUTE="1", LO_BACKGROUND="1", LO_SHADER_PACK_DOWNLOAD="0", LO_TRACE_MAP_INFO="1",
                   LO_PIPELINE_MISS_LOG="1", LO_LOG_FILE=p(self.log), LO_SHADER_CACHE_DIR=p(r / "shader-cache"),
                   LO_AUTO_BUTTONS="s@120,a@240,a@360,a@480,a@700,a@900", LO_AUTO_PULSE="6",
                   LO_TEST_INPUT_FILE=p(r / "input.txt"), LO_TEST_INPUT_TICKS="1",
                   LO_DEBUG_BATTLE_FILE=p(r / "battle.txt"), LO_DIAG_MAPJUMP_COMMAND_FILE=p(r / "mapjump.txt"),
                   LO_DIAG_DISC_COMMAND_FILE=p(r / "disc.txt"), LO_DIAG_PROBE_COMMAND_FILE=p(r / "probe.txt"))
        for k, v in {**self.config.get("env", {}), **(extra or {})}.items():
            if v.startswith("glob:"):
                hits = sorted(Path(v[5:]).parent.glob(Path(v[5:]).name))
                v = str(hits[0]) if hits else ""
            env[k] = v
        (r / "shader-cache").mkdir(exist_ok=True)
        cmd = self.config.get("prefix", []) + [self.config["exe"], "--game", p(self.config["game"]), "--quiet-kernel"]
        out = (r / "logs" / f"stdout-{self.index:02d}.log").open("wb")
        # A background shell starts us with SIGINT ignored; give the game its default so it can quit cleanly.
        reset = None if os.name == "nt" else (lambda: signal.signal(signal.SIGINT, signal.SIG_DFL))
        self.proc = subprocess.Popen(cmd, cwd=r, env=env, stdin=subprocess.DEVNULL, stdout=out, stderr=out,
                                     preexec_fn=reset)
        self.offset = 0

    def stop(self, flush=True):
        """Close request first (the exit path flushes the recipe file), then kill."""
        if self.proc and self.proc.poll() is None and flush:
            if self.config.get("interrupt"):  # e.g. a launcher (umu-run) between us and the game
                subprocess.run(self.config["interrupt"], capture_output=True)
            elif os.name == "nt":
                subprocess.run(["taskkill", "/PID", str(self.proc.pid)], capture_output=True)
            else:
                self.proc.send_signal(signal.SIGINT)
            for _ in range(40):
                if self.proc.poll() is not None:
                    break
                time.sleep(0.5)
        if self.proc and self.proc.poll() is None:
            self.proc.kill()
        if self.config.get("kill"):
            subprocess.run(self.config["kill"], capture_output=True)
            time.sleep(2)
        self.proc = None

    def command(self, name, text):
        self.serial += 1
        tmp = self.run / (name + ".tmp")
        tmp.write_text(f"{self.serial} {text}\n")
        os.replace(tmp, self.run / name)

    def lines(self):
        if not self.log.exists():
            return []
        with self.log.open("rb") as f:
            f.seek(self.offset)
            data = f.read()
        cut = data.rfind(b"\n") + 1
        self.offset += cut
        return data[:cut].decode("utf-8", "replace").splitlines()

    def wait_for(self, predicate, timeout):
        deadline = time.time() + timeout
        while time.time() < deadline:
            for line in self.lines():
                result = predicate(line)
                if result is not None:
                    return result
            if self.proc.poll() is not None:
                return "dead"
            time.sleep(0.25)
        return "timeout"

    def boot(self):
        self.start()
        if self.wait_for(lambda l: "ok" if MAP.search(l) else None, 240) != "ok":
            return False
        time.sleep(3)
        return True


def recipes(run):
    p = run / "shader-cache" / "pipelines.bin"
    if not p.exists():
        p = run / "shader-cache" / "pipelines_vk12_1.bin"
    return struct.unpack_from("<I", p.read_bytes(), 24)[0] if p.exists() and p.stat().st_size >= 48 else 0


def parse_formations(lines):
    table = {}
    for line in lines:
        m = FORMATION.search(line)
        if not m or not m.group(2).startswith(" +"):
            continue
        fields = dict(re.findall(r"\+(\d+)[=:](\S+)", m.group(2)))
        words = (m.group(3) or "").split()
        count = int(fields.get("152", "0"), 16)
        # 32-byte slots: slot id, enemy config (model), enemy parameters.
        slots = [[int(words[k * 8 + 1], 16), int(words[k * 8 + 2], 16)] for k in range(min(count, len(words) // 8))]
        table[int(m.group(1))] = dict(stage=fields.get("0", ""), ai=fields.get("60", ""),
                                      start=fields.get("72", ""), enemies=count, slots=slots)
    return table


def priority(table):
    """Formations in visiting order: a greedy cover of every enemy config (model)
    and fixed stage, then of every config/parameter pair, then the rest."""
    def models(f):
        return {("enemy", c) for c, _ in f["slots"]} | {("stage", f["stage"])}

    def pairs(f):
        return {("enemy", c, p) for c, p in f["slots"]} | {("stage", f["stage"])}
    order, left = [], set(table)
    for features in (models, pairs):
        wanted = set().union(*(features(f) for f in table.values()))
        seen = set().union(set(), *(features(table[i]) for i in order))
        while seen != wanted:
            best = max(left, key=lambda i: (len(features(table[i]) - seen), -i))
            order.append(best)
            left.discard(best)
            seen |= features(table[best])
    return order + sorted(left)


def cover(table):
    """The priority order up to the point where every config/parameter pair and stage was seen."""
    order = priority(table)
    feats = lambda i: {("enemy", c, p) for c, p in table[i]["slots"]} | {("stage", table[i]["stage"])}
    wanted, seen = set().union(*(feats(i) for i in table)), set()
    for n, i in enumerate(order):
        seen |= feats(i)
        if seen == wanted:
            return order[:n + 1]
    return order


def select(spec, table):
    if spec == "all":
        return priority(table)
    if spec == "cover":
        return cover(table)
    ids = []
    for part in spec.split(","):
        a, _, b = part.partition("-")
        ids += range(int(a), int(b or a) + 1)
    return ids


def fight(game, fid, hold, stage=""):
    """One formation from the field; returns (result, needs_restart)."""
    r = dict(formation=fid, status="nodraw", phases=[], added=0)
    began = time.time()
    game.command("battle.txt", f"battle {fid} {stage}".strip())
    t = dict(drawn=None, first=None, moved=None, command=None, victory_asked=None, end=None, field=None)
    fatal = False

    def scan():
        nonlocal fatal
        for line in game.lines():
            if f"battle tour: formation {fid} drawn" in line:
                t["drawn"] = time.time()
            elif "renderer: pipeline miss frame=" in line and " recipe=0 " in line:
                r["added"] += 1
            elif "fatal resource lookup" in line or "dirty disc error" in line:
                fatal = True
            elif t["end"] and MAP.search(line):
                t["field"] = time.time()
            m = PHASE.search(line)
            if m:
                phase = int(m.group(2))
                r["phases"].append(phase)
                t["first"] = t["first"] or time.time()
                t["moved"] = time.time()
                if phase == 2 and not t["command"]:
                    t["command"] = time.time()
                if phase in (11, 12) and not t["end"]:
                    t["end"] = time.time()
                    r["status"] = "defeat" if phase == 12 else "forced" if t["victory_asked"] else "victory"

    nudged = False
    while time.time() - began < 45 and not t["first"] and not fatal and game.proc.poll() is None:
        scan()
        if not t["drawn"] and not nudged and time.time() - began > 15:
            game.command("input.txt", "0 0 20000 60")  # walk a step if the update needs movement
            nudged = True
        time.sleep(0.5)
    if not t["first"]:
        r["status"] = "fatal" if fatal else "nobattle" if t["drawn"] else "nodraw"
    while t["first"]:
        scan()
        if t["field"] or (r["status"] == "defeat" and time.time() - t["end"] > 8):
            break
        # Scripted battles can wait on input the harness never gives.
        stalled = time.time() - t["moved"] > 60 and not t["end"] or time.time() - began > 240
        if fatal or game.proc.poll() is not None or stalled:
            r["status"] = "fatal" if fatal else "dead" if game.proc.poll() is not None else "stall"
            break
        if t["command"] and not t["end"] and time.time() - t["command"] > hold and (
                not t["victory_asked"] or time.time() - t["victory_asked"] > 10):
            game.command("battle.txt", "victory")
            t["victory_asked"] = time.time()
        if t["command"] or t["end"]:
            game.command("input.txt", f"{A_BUTTON} 0 0 6")
        time.sleep(2)
    r["intro_s"] = round(t["command"] - t["first"], 1) if t["command"] and t["first"] else None
    r["wall_s"] = round(time.time() - began, 1)
    r["phases"] = r["phases"][:60]
    # Back in the field after a natural victory or an escape-type loss: go on.
    return r, not (t["field"] and r["status"] in ("victory", "defeat"))


def stages(run, args):
    """One battle of --formation on every listed stage (battle map package)."""
    names = Path(args.stages).read_text().split()
    out = run / "stages.jsonl"
    done = {json.loads(l)["stage"] for l in out.read_text().splitlines()} if out.exists() else set()
    todo = [n for n in names if n not in done]
    print(f"{len(todo)} stages to visit", flush=True)
    if not todo:
        sys.exit(3)
    game, started, booted = Game(run), time.time(), False
    try:
        for name in todo:
            if time.time() - started > args.budget:
                break
            if not booted and not game.boot():
                sys.exit("boot failed")
            r, restart = fight(game, args.formation, args.hold, name)
            r["stage"] = name
            with out.open("a") as f:
                f.write(json.dumps(r) + "\n")
            print(json.dumps(r), flush=True)
            booted = not restart
    finally:
        game.stop()
    print(f"recipes in cache: {recipes(run)}", flush=True)


def battles(run, args):
    table = {int(k): v for k, v in json.loads((run / "formations.json").read_text()).items()}
    out = run / "battles.jsonl"
    results = [json.loads(l) for l in out.read_text().splitlines()] if out.exists() else []
    # A formation is done unless its resources were missing on every disc tried so far.
    tried = {}
    for r in results:
        tried.setdefault(r["formation"], []).append(r)
    done = {f for f, rs in tried.items() if any(r["status"] != "fatal" for r in rs) or
            any(r.get("disc", 1) == args.disc for r in rs)}
    k, n = (int(x) for x in args.part.split("/"))
    todo = [i for j, i in enumerate(select(args.formations, table)) if j % n == k and i not in done]
    if args.disc != 1:
        todo = [i for i in todo if i in tried]  # other discs only retry missing resources
    print(f"disc {args.disc}: {len(todo)} formations to visit", flush=True)
    if not todo:
        sys.exit(3)
    game, started, booted = Game(run), time.time(), False
    try:
        for fid in todo:
            if time.time() - started > args.budget:
                break
            if not booted and not (game.boot() and request_disc(game, args.disc)):
                sys.exit("boot failed")
            r, restart = fight(game, fid, args.hold)
            r.update(disc=args.disc, stage=table[fid]["stage"], slots=table[fid]["slots"])
            with out.open("a") as f:
                f.write(json.dumps(r) + "\n")
            print(json.dumps(r), flush=True)
            booted = not restart
    finally:
        game.stop()
    print(f"recipes in cache: {recipes(run)}", flush=True)


def list_formations(run):
    game = Game(run)
    try:
        if not game.boot():
            sys.exit("boot failed")
        game.command("battle.txt", "list")
        lines, total, deadline = [], None, time.time() + 30
        while time.time() < deadline and (total is None or len(parse_formations(lines)) < total):
            for line in game.lines():
                if "battle tour: formations=" in line:
                    total = int(line.split("formations=")[1].split()[0])
                elif "battle tour: formation " in line:
                    lines.append(line)
            time.sleep(0.5)
    finally:
        game.stop()
    table = parse_formations(lines)
    (run / "formations.json").write_text(json.dumps(table, indent=0))
    print(f"{len(table)} of {total} formations, cover {len(cover(table))}")


def request_disc(game, disc):
    """Diagnostic build only: the original disc swap request, then wait for it."""
    if disc == 1:
        return True
    time.sleep(3)
    game.command("disc.txt", str(disc))
    ok = game.wait_for(lambda l: "ok" if f"diag disc completed disc={disc}" in l else None, 90) == "ok"
    time.sleep(3)
    return ok


def maps(run, args):
    tour = json.loads(Path(args.maps).read_text())
    names, ids = tour["visited"], {k: v["id"] for k, v in tour["maps"].items()}
    out = run / "maps.json"
    done = json.loads(out.read_text()) if out.exists() else {}
    game, started = Game(run), time.time()
    try:
        for disc in [int(d) for d in args.discs.split(",")]:
            pending = [m for m in names if m not in done and not m.startswith(("xxx_", "z0g_"))]
            found_file = run / f"found-{disc}.json"
            if not pending or time.time() - started > args.budget:
                break
            if found_file.exists() and not set(json.loads(found_file.read_text())) & set(pending):
                continue
            if not game.boot():
                continue
            if not request_disc(game, disc):
                continue
            game.command("probe.txt", " ".join(sorted(ids)))
            found = set()

            def probed(line):
                if "diag probe package=" in line and line.rstrip().endswith("found=1"):
                    found.add(line.split("package=")[1].split()[0])
                return "ok" if "diag probe done" in line else None
            if game.wait_for(probed, 90) != "ok":
                continue
            found_file.write_text(json.dumps(sorted(found)))
            for name in [m for m in sorted(found) if m in pending]:
                if time.time() - started > args.budget or game.proc.poll() is not None:
                    break
                began = time.time()
                game.command("mapjump.txt", name)

                def arrived(line):
                    if "fatal resource lookup" in line or "dirty disc error" in line:
                        return "fatal"
                    if "diag mapjump rejected" in line:
                        return "rejected"
                    m = MAP.search(line)
                    return "ok" if m and int(m.group(1)) == ids[name] else None
                result = game.wait_for(arrived, 45)
                if result == "ok":
                    time.sleep(5)
                    game.command("input.txt", "0 0 0 240 0 0 32000 0")  # right stick: turn the camera
                    time.sleep(4)
                    game.command("input.txt", "0 0 0 240 0 0 -32000 0")
                    time.sleep(3)
                done[name] = dict(result=result, disc=disc, s=round(time.time() - began))
                out.write_text(json.dumps(done, indent=0))
                print(f"{name}: {result}", flush=True)
                if result != "ok":
                    break  # reboot this disc on the next chunk
    finally:
        game.stop()
    print(f"recipes in cache: {recipes(run)}", flush=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("command", choices=["list", "battles", "stages", "maps"])
    ap.add_argument("run", type=Path)
    ap.add_argument("--formations", default="cover", help="all (priority order), cover or a list like 3,10-20")
    ap.add_argument("--part", default="0/1", help="K/N: every Nth formation from the K-th, to split a tour between hosts")
    ap.add_argument("--hold", type=float, default=45, help="seconds after the first command before Victory")
    ap.add_argument("--budget", type=float, default=1200, help="stop starting new work after this many seconds")
    ap.add_argument("--maps", help="map list JSON (maps: name -> {id}, visited: [names])")
    ap.add_argument("--stages", help="stages: file with battle map package names")
    ap.add_argument("--formation", type=int, default=3, help="stages: formation to fight (default 3, two weak enemies)")
    ap.add_argument("--discs", default="1,2,3,4", help="maps: discs to visit")
    ap.add_argument("--disc", type=int, default=1, help="battles: request this disc after boot (diagnostic build)")
    args = ap.parse_args()
    run = args.run.resolve()
    {"list": lambda: list_formations(run), "battles": lambda: battles(run, args), "stages": lambda: stages(run, args),
     "maps": lambda: maps(run, args)}[args.command]()


if __name__ == "__main__":
    main()
