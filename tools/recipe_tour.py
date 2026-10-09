#!/usr/bin/env python3
"""Automated tours that record pipeline recipes for the shipped corpus.

Notes: docs/notes/pipeline-first-use-stalls.md, section 语料巡游.

  python tools/recipe_tour.py list    <run_dir>
  python tools/recipe_tour.py battles <run_dir> [--formations all|3,10-20] [--part K/N] [--disc N]
  python tools/recipe_tour.py stages  <run_dir> --stages names.txt [--formation 3]
  python tools/recipe_tour.py maps    <run_dir> --maps maps.json [--discs 1,2,3,4]

<run_dir> holds a staged game (settings.ini, a save that Continue loads into the
field, profile/, shaders/ with the matching pack) and launch.json:
  {"exe": ..., "game": ".../disc1", "path": "win|z|posix", "prefix": ["umu-run"],
   "env": {"K": "V" or "glob:/pattern"}, "interrupt": [cmd], "kill": [cmd],
   "waiters": [cmd], "mine": "substring"}
"interrupt" asks the game to quit so it writes its recipe file (default: SIGINT /
taskkill to the launched process). On a shared host "waiters" lists the users of
the game lock; a chunk exits at once while a line without "mine" waits. Recipes
land in <run_dir>/shader-cache; every command resumes from its result file and
stops starting new work after --budget seconds (at most 15 min).

list     dumps the formation table (LO_DEBUG_BATTLE_FILE "list") to formations.json.
battles  starts formations through the walking-encounter path, presses A at every
         prompt, asks for the debug Victory after --hold seconds and goes on from
         the field; defeat, a forced victory or a stall restarts the game. Order:
         a greedy cover of every enemy model and fixed stage, then every enemy
         parameter variant, then the rest. A formation is skipped once all its
         enemy models were fought (also on peer hosts: peer-*.jsonl copies of
         their battles.jsonl) unless --no-skip.
stages   fights --formation once on every listed battle stage (battle map
         package), for the area stages no formation names.
maps     needs a diagnostic build with the LO_DIAG_MAPJUMP/DISC/PROBE command
         files (not in main): per disc, probe the maps, jump to each, turn the
         camera and dwell. --disc N on battles uses the same disc request.
"""
import argparse
import json
import os
import re
import signal
import subprocess
import sys
import time
from pathlib import Path

PHASE = re.compile(r"battle debug: core 0x832ca0e8 phase (\d+) -> (\d+)")
MAP = re.compile(r"current map available=true id=(\d+) name=.* package=(\S+)")
FORMATION = re.compile(r"battle tour: formation (\d+)(.*?)(?: slots:(.*))?$")
A_BUTTON = "1000"
ENDED = ("victory", "forced", "defeat", "ended")


class Game:
    def __init__(self, run):
        self.run = run
        self.config = json.loads((run / "launch.json").read_text())
        self.proc = None
        self.index = len(list(run.glob("logs/runtime-*.log")))
        self.serial = int(time.time() * 1000) % 1_000_000_000

    def path(self, p):
        style = self.config.get("path", "win" if os.name == "nt" else "posix")
        s = str(p)
        return s.replace("/", "\\") if style == "win" else "Z:" + s.replace("/", "\\") if style == "z" else s

    def start(self):
        self.stop()
        self.index += 1
        r, p = self.run, self.path
        (r / "logs").mkdir(exist_ok=True)
        (r / "shader-cache").mkdir(exist_ok=True)
        for name in ("input.txt", "battle.txt", "mapjump.txt", "disc.txt", "probe.txt"):
            (r / name).unlink(missing_ok=True)
        self.log = r / "logs" / f"runtime-{self.index:02d}.log"
        env = {k: v for k, v in os.environ.items() if not k.startswith("LO_")}
        env.update(LO_AUDIO_MUTE="1", LO_BACKGROUND="1", LO_SHADER_PACK_DOWNLOAD="0", LO_TRACE_MAP_INFO="1",
                   LO_PIPELINE_MISS_LOG="1", LO_DEBUG_LOG="1", LO_LOG_FILE=p(self.log), LO_SHADER_CACHE_DIR=p(r / "shader-cache"),
                   LO_AUTO_BUTTONS="s@120,a@240,a@360,a@480,a@700,a@900", LO_AUTO_PULSE="6",
                   LO_TEST_INPUT_FILE=p(r / "input.txt"), LO_TEST_INPUT_TICKS="1",
                   LO_DEBUG_BATTLE_FILE=p(r / "battle.txt"), LO_DIAG_MAPJUMP_COMMAND_FILE=p(r / "mapjump.txt"),
                   LO_DIAG_DISC_COMMAND_FILE=p(r / "disc.txt"), LO_DIAG_PROBE_COMMAND_FILE=p(r / "probe.txt"))
        for k, v in self.config.get("env", {}).items():
            if v.startswith("glob:"):
                hits = sorted(Path(v[5:]).parent.glob(Path(v[5:]).name))
                v = str(hits[0]) if hits else ""
            env[k] = v
        cmd = self.config.get("prefix", []) + [self.config["exe"], "--game", p(self.config["game"]), "--quiet-kernel"]
        out = (r / "logs" / f"stdout-{self.index:02d}.log").open("wb")
        # A background shell starts us with SIGINT ignored; the game needs the default to quit cleanly.
        reset = None if os.name == "nt" else (lambda: signal.signal(signal.SIGINT, signal.SIG_DFL))
        self.proc = subprocess.Popen(cmd, cwd=r, env=env, stdin=subprocess.DEVNULL, stdout=out, stderr=out,
                                     preexec_fn=reset)
        self.offset = 0

    def stop(self):
        """Ask the game to quit (its exit path writes the recipe file), then kill what is left."""
        if self.proc and self.proc.poll() is None:
            if self.config.get("interrupt"):
                subprocess.run(self.config["interrupt"], capture_output=True)
            elif os.name == "nt":
                subprocess.run(["taskkill", "/PID", str(self.proc.pid)], capture_output=True)
            else:
                self.proc.send_signal(signal.SIGINT)
            for _ in range(40):
                if self.proc.poll() is not None:
                    break
                time.sleep(0.5)
            if self.proc.poll() is None:
                self.proc.kill()
        if self.proc and self.config.get("kill"):
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

    def boot(self, disc=1):
        self.start()
        if self.wait_for(lambda l: "ok" if MAP.search(l) else None, 240) != "ok":
            return False
        time.sleep(3)
        if disc == 1:
            return True
        self.command("disc.txt", str(disc))  # diagnostic build: the original disc swap request
        ok = self.wait_for(lambda l: "ok" if f"diag disc completed disc={disc}" in l else None, 90) == "ok"
        time.sleep(3)
        return ok


def lock_wanted(config):
    """True when another job waits for a shared host's game lock (launch.json "waiters"/"mine")."""
    if not config.get("waiters"):
        return False
    lines = subprocess.run(config["waiters"], capture_output=True, text=True).stdout.splitlines()
    return any(config.get("mine", "\0") not in line for line in lines)


def start_chunk(run, todo, what):
    print(f"{len(todo)} {what} to visit", flush=True)
    if not todo:
        sys.exit(3)
    game = Game(run)
    if lock_wanted(game.config):
        print("another job waits for the game lock; giving it back", flush=True)
        sys.exit(0)
    return game, time.time()


def append(path, result):
    with path.open("a") as f:
        f.write(json.dumps(result) + "\n")
    print(json.dumps(result), flush=True)


def parse_formations(lines):
    table = {}
    for line in lines:
        m = FORMATION.search(line)
        if not m or not m.group(2).startswith(" +"):
            continue
        fields = dict(re.findall(r"\+(\d+)[=:](\S+)", m.group(2)))
        words = (m.group(3) or "").split()
        count = int(fields.get("152", "0"), 16)
        # 32-byte slots: slot id, enemy config (model), enemy parameter id.
        slots = [[int(words[k * 8 + 1], 16), int(words[k * 8 + 2], 16)] for k in range(min(count, len(words) // 8))]
        table[int(m.group(1))] = dict(stage=fields.get("0", ""), ai=fields.get("60", ""), slots=slots)
    return table


def priority(table):
    """Greedy cover of every enemy model and fixed stage, then of every model/parameter pair, then the rest."""
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
                if phase in (11, 12, 13) and not t["end"]:  # victory, defeat or escape, scripted end
                    t["end"] = time.time()
                    r["status"] = {12: "defeat", 13: "ended"}.get(phase, "forced" if t["victory_asked"] else "victory")

    nudged = False
    while time.time() - began < 45 and not t["first"] and not fatal and game.proc.poll() is None:
        scan()
        if not t["drawn"] and not nudged and time.time() - began > 15:
            game.command("input.txt", "0 0 20000 60")  # walk a step in case the update needs movement
            nudged = True
        time.sleep(0.5)
    if not t["first"]:
        r["status"] = "fatal" if fatal else "nobattle" if t["drawn"] else "nodraw"
    while t["first"]:
        scan()
        if t["field"] or (r["status"] in ("defeat", "ended") and time.time() - t["end"] > 15):
            break
        # Scripted battles can wait for input the harness never gives.
        stalled = time.time() - t["moved"] > 45 and not t["end"] or time.time() - began > 150
        if fatal or game.proc.poll() is not None or stalled:
            r["status"] = "fatal" if fatal else "dead" if game.proc.poll() is not None else "stall"
            break
        if t["command"] and not t["end"] and time.time() - t["command"] > hold and (
                not t["victory_asked"] or time.time() - t["victory_asked"] > 10):
            game.command("battle.txt", "victory")
            t["victory_asked"] = time.time()
        # A answers commands, results and messages; before the first command only
        # when the intro waits on something (no phase change for 20 s).
        if t["command"] or t["end"] or time.time() - t["moved"] > 20:
            game.command("input.txt", f"{A_BUTTON} 0 0 6")
        time.sleep(2)
    r["wall_s"] = round(time.time() - began, 1)
    r["phases"] = r["phases"][:60]
    # Back in the field after a natural victory or an escape: carry on in this process.
    return r, not (t["field"] and r["status"] in ("victory", "defeat", "ended"))


def fought_models(run):
    models = set()
    for path in [run / "battles.jsonl", *sorted(run.glob("peer-*.jsonl"))]:
        for line in path.read_text().splitlines() if path.exists() else []:
            r = json.loads(line)
            if r["status"] in ENDED or 2 in r["phases"]:
                models |= {c for c, _ in r["slots"]}
    return models


def battles(run, args):
    table = {int(k): v for k, v in json.loads((run / "formations.json").read_text()).items()}
    out = run / "battles.jsonl"
    tried = {}
    for line in out.read_text().splitlines() if out.exists() else []:
        r = json.loads(line)
        tried.setdefault(r["formation"], []).append(r)
    # Done unless its resources were missing on every disc tried so far.
    done = {f for f, rs in tried.items() if any(r["status"] != "fatal" or r.get("disc", 1) == args.disc for r in rs)}
    order = priority(table) if args.formations == "all" else [
        i for part in args.formations.split(",") for i in range(int(part.split("-")[0]), int(part.split("-")[-1]) + 1)]
    k, n = (int(x) for x in args.part.split("/"))
    todo = [i for j, i in enumerate(order) if j % n == k and i not in done and (args.disc == 1 or i in tried)]

    def seen(fid):
        return not args.no_skip and {c for c, _ in table[fid]["slots"]} <= fought_models(run)
    todo = [i for i in todo if not seen(i)]
    game, started = start_chunk(run, todo, f"formations (disc {args.disc})")
    booted = False
    try:
        for fid in todo:
            if time.time() - started > args.budget:
                break
            if seen(fid):  # fought on a peer host meanwhile
                continue
            if not booted and not game.boot(args.disc):
                sys.exit("boot failed")
            r, restart = fight(game, fid, args.hold)
            r.update(disc=args.disc, stage=table[fid]["stage"], slots=table[fid]["slots"])
            append(out, r)
            booted = not restart
    finally:
        game.stop()


def stages(run, args):
    out = run / "stages.jsonl"
    done = {json.loads(l)["stage"] for l in out.read_text().splitlines()} if out.exists() else set()
    todo = [n for n in Path(args.stages).read_text().split() if n not in done]
    game, started = start_chunk(run, todo, "stages")
    booted = False
    try:
        for name in todo:
            if time.time() - started > args.budget:
                break
            if not booted and not game.boot():
                sys.exit("boot failed")
            r, restart = fight(game, args.formation, args.hold, name)
            r["stage"] = name
            append(out, r)
            booted = not restart
    finally:
        game.stop()


def list_formations(run):
    game = Game(run)
    lines, total = [], None
    try:
        if not game.boot():
            sys.exit("boot failed")
        game.command("battle.txt", "list")
        deadline = time.time() + 30
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
    print(f"{len(table)} of {total} formations")


def maps(run, args):
    tour = json.loads(Path(args.maps).read_text())
    names, ids = tour["visited"], {k: v["id"] for k, v in tour["maps"].items()}
    out = run / "maps.json"
    done = json.loads(out.read_text()) if out.exists() else {}
    discs = [int(d) for d in args.discs.split(",")]
    pending = {m for m in names if m not in done and not m.startswith(("xxx_", "z0g_"))}
    probed = [run / f"found-{d}.json" for d in discs]
    if all(f.exists() for f in probed):  # maps on no disc are never reachable
        pending &= set().union(*(json.loads(f.read_text()) for f in probed))
    game, started = start_chunk(run, sorted(pending), "maps")
    try:
        for disc in discs:
            found_file = run / f"found-{disc}.json"
            if time.time() - started > args.budget:
                break
            if found_file.exists() and not set(json.loads(found_file.read_text())) & pending:
                continue
            if not game.boot(disc):
                continue
            game.command("probe.txt", " ".join(sorted(ids)))
            found = set()

            def probe(line):
                if "diag probe package=" in line and line.rstrip().endswith("found=1"):
                    found.add(line.split("package=")[1].split()[0])
                return "ok" if "diag probe done" in line else None
            if game.wait_for(probe, 90) != "ok":
                continue
            found_file.write_text(json.dumps(sorted(found)))
            for name in sorted(found & pending):
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
                pending.discard(name)
                out.write_text(json.dumps(done, indent=0))
                print(f"{name}: {result}", flush=True)
                if result != "ok":
                    break  # the next chunk reboots this disc
    finally:
        game.stop()


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("command", choices=["list", "battles", "stages", "maps"])
    ap.add_argument("run", type=Path)
    ap.add_argument("--formations", default="all", help="battles: all (coverage order) or a list like 3,10-20")
    ap.add_argument("--part", default="0/1", help="battles: K/N takes every Nth formation from the K-th (split hosts)")
    ap.add_argument("--no-skip", action="store_true", help="battles: also fight formations whose models were fought")
    ap.add_argument("--disc", type=int, default=1, help="battles: retry formations missing on disc 1 on this disc")
    ap.add_argument("--hold", type=float, default=45, help="seconds after the first command before the Victory")
    ap.add_argument("--budget", type=float, default=900, help="stop starting new work after this many seconds")
    ap.add_argument("--stages", help="stages: file with battle map package names")
    ap.add_argument("--formation", type=int, default=3, help="stages: formation to fight (3: two weak enemies)")
    ap.add_argument("--maps", help="maps: map list JSON (maps: name -> {id}, visited: [names])")
    ap.add_argument("--discs", default="1,2,3,4", help="maps: discs to visit")
    args = ap.parse_args()
    args.budget = min(args.budget, 900)
    run = args.run.resolve()
    {"list": lambda: list_formations(run), "battles": lambda: battles(run, args),
     "stages": lambda: stages(run, args), "maps": lambda: maps(run, args)}[args.command]()


if __name__ == "__main__":
    main()
