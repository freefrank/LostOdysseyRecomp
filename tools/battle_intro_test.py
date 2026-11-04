#!/usr/bin/env python3
"""In-game test rig for the battle intro: one formation from the field to the field again.

  python tools/battle_intro_test.py setup <run_dir> [--save-from DIR] [--exe PATH]
  python tools/battle_intro_test.py list  <run_dir>
  python tools/battle_intro_test.py run   <run_dir> --formations 3,10 [--hold 3] [--prompt-after 60] [--timeout 420]
  python tools/battle_intro_test.py run   <run_dir> --walk [--skip-mode press] [--press-at 3000] [--real] [--shots] [--runs 3]

setup stages an isolated run folder: a copy of the save and profile folders, a
small settings.ini, launch.json. Every game run uses that folder as its working
directory (a build folder is a portable layout, so settings, saves, profile and
cache follow the working directory), with LO_SAVE_DIR, LO_PROFILE_DIR and the XDG
variables pointing inside it, and under bwrap with the player's data and config
folders read-only. The rig refuses to start when another game process runs, and
it fails a run when a checksum of the player's files changes.

run starts a fresh game per formation (Continue loads the newest save), requests
the formation, presses A where a prompt needs it, requests the debug victory and
waits for the field. LO_TRACE_BATTLE_INTRO=1 is set unless --no-trace is given.
The result is <run_dir>/results/<label>-<formation>.json and the log is copied to
<run_dir>/results/<label>-<formation>.log. Exit code 0: every run returned to the
field and the isolation check passed.

--skip-mode off|press|always writes battle_intro_skip=0, 1 or 2 (Debug Menu -> Skip
battle animation) into the isolated settings.ini. --skip is --skip-mode press and
--skip-off is --skip-mode off. Without them the key is absent, which is the default
Off. --press-at MS presses the game's cancel button (--press-button, B by default)
once, MS milliseconds of wall time after phase 0 starts, through LO_TEST_INPUT_FILE;
--press-at-request MS times that press from the battle request instead. The result
lists what the guest saw of it. LO_TRACE_INPUT=1 is set with the trace, so the result
counts the skip latch lines. --walk starts no formation: it sets LO_DEBUG_ENCOUNTER_EVERY_STEP=1 and walks, so the game's own
draw picks the formation from the area's table. --real answers the command menu
with A and asks for the debug victory only after --real-turns turns. --shots takes
screenshots at the first command (PNG next to the result file).
"""
import argparse
import hashlib
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from recipe_tour import A_BUTTON, FORMATION, MAP, PHASE, Game  # noqa: E402

REPO = Path(__file__).resolve().parent.parent
REAL_DATA = Path.home() / ".local/share/lost-odyssey-recomp"
REAL_CONFIG = Path.home() / ".config/lost-odyssey-recomp"
GAME_NAME = "LostOdysseyRecomp"
LOG_TIME = re.compile(r"^\[\s*([\d.]+) ")
TRACE = re.compile(r"battle intro: swap=(\d+) t=(\d+) phase=(\d+) (\w+) ?(.*)$")
SETTINGS = {"width": 1280, "height": 720, "window_mode": 0, "graphics_backend": 1, "game_language": 1, "ui_language": 0,
            "frame_rate": 60, "automatic_updates": 0, "discord_status": 0, "ask_compact_game_data": 0, "diagnostic_log": 1}


class IsolationError(Exception):
    pass


class Timeout(Exception):
    pass


def real_files():
    """Checksums of the player's files that a run must not change."""
    roots = [REAL_DATA / "save", REAL_DATA / "profile", REAL_CONFIG]
    result = {}
    for root in roots:
        for path in sorted(p for p in root.rglob("*") if p.is_file() and "captures" not in p.parts):
            result[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
    return result


def inside(path, parent):
    return path == parent or parent in path.parents


def check_run_dir(run):
    if inside(run, REAL_DATA) or inside(run, REAL_CONFIG):
        raise IsolationError(f"{run} is inside the player's folders")


def other_game_processes():
    found = []
    for entry in Path("/proc").iterdir():
        if entry.name.isdigit():
            try:
                if os.readlink(entry / "exe").rsplit("/", 1)[-1].startswith(GAME_NAME):
                    found.append(int(entry.name))
            except OSError:
                pass
    return found


SKIP_MODES = {"off": 0, "press": 1, "always": 2}


def write_settings(run, frame_rate, skip_mode=None, extra=()):
    values = dict(SETTINGS, frame_rate=frame_rate)
    values.update(item.split("=", 1) for item in extra)
    if skip_mode is not None:
        values["battle_intro_skip"] = SKIP_MODES[skip_mode]
    (run / "settings.ini").write_text("".join(f"{k}={v}\n" for k, v in values.items()))


def setup(run, args):
    check_run_dir(run)
    run.mkdir(parents=True, exist_ok=True)
    source = Path(args.save_from).expanduser() if args.save_from else REAL_DATA / "save"
    if not (run / "save").exists():
        # A backup is one user folder; the real folder holds all of them.
        shutil.copytree(source, run / "save" if any(source.glob("user*")) else run / "save" / "user00")
    if not (run / "profile").exists():
        shutil.copytree(REAL_DATA / "profile", run / "profile")
    (run / "shaders").mkdir(exist_ok=True)
    pack = run / "shaders" / "portable_vk.lospv"
    if not pack.is_symlink():
        pack.symlink_to(REAL_DATA / "shaders" / "portable_vk.lospv")
    write_settings(run, 60)
    env = {"LO_SAVE_DIR": str(run / "save"), "LO_PROFILE_DIR": str(run / "profile"), "LO_SHADER_PACK_PATH": str(pack),
           "XDG_CONFIG_HOME": str(run / "xdg-config"), "XDG_DATA_HOME": str(run / "xdg-data"),
           "XDG_STATE_HOME": str(run / "xdg-state")}
    bwrap = shutil.which("bwrap")
    if not bwrap:
        raise IsolationError("bwrap is not installed; the rig needs it to hold the player's folders read-only")
    prefix = [bwrap, "--dev-bind", "/", "/", "--ro-bind", str(REAL_DATA), str(REAL_DATA),
              "--ro-bind", str(REAL_CONFIG), str(REAL_CONFIG), "--die-with-parent", "--"]
    launch = {"exe": str(Path(args.exe).resolve()), "game": str(REAL_DATA / "game" / "disc1"), "path": "posix",
              "prefix": prefix, "env": env}
    (run / "launch.json").write_text(json.dumps(launch, indent=1))
    print(f"run folder ready: {run}; saves from {source}: {sorted(p.name for p in (run / 'save').iterdir())}")


class IsolatedGame(Game):
    """recipe_tour's Game, but it stops the game by its own PID and checks where the game runs."""

    def __init__(self, run, trace, extra_env=None):
        super().__init__(run)
        if trace:
            self.config["env"]["LO_TRACE_BATTLE_INTRO"] = "1"
        self.config["env"].update(extra_env or {})
        self.game_pid = None

    def find_game_pid(self):
        """bwrap forks; the game is a descendant of the process that the rig started."""
        pending = [self.proc.pid]
        while pending:
            pid = pending.pop()
            try:
                if pid != self.proc.pid and os.readlink(f"/proc/{pid}/exe").rsplit("/", 1)[-1].startswith(GAME_NAME):
                    return pid
                pending += [int(c) for c in Path(f"/proc/{pid}/task/{pid}/children").read_text().split()]
            except OSError:
                pass
        return None

    def verify_isolation(self):
        for _ in range(40):
            self.game_pid = self.find_game_pid()
            if self.game_pid:
                break
            time.sleep(0.25)
        if not self.game_pid:
            raise IsolationError("cannot find the game process under bwrap")
        cwd = Path(os.readlink(f"/proc/{self.game_pid}/cwd"))
        if cwd != self.run:
            raise IsolationError(f"game working directory is {cwd}, not {self.run}")
        mounts = Path(f"/proc/{self.game_pid}/mounts").read_text()
        for folder in (REAL_DATA, REAL_CONFIG):
            if not re.search(rf"^\S+ {re.escape(str(folder))} \S+ ro[, ]", mounts, re.M):
                raise IsolationError(f"{folder} is not read-only for the game")

    def open_real_paths(self):
        """Files of the player's folders that the game holds open, except the game data and the shader pack."""
        hits = set()
        if not self.game_pid:
            return []
        try:
            for fd in Path(f"/proc/{self.game_pid}/fd").iterdir():
                target = Path(os.readlink(fd))
                if inside(target, REAL_CONFIG) or (inside(target, REAL_DATA) and not inside(target, REAL_DATA / "game")
                                                   and not inside(target, REAL_DATA / "shaders")):
                    hits.add(str(target))
        except OSError:
            pass
        return sorted(hits)

    def stop(self):
        if self.proc and self.proc.poll() is None:
            pid = self.find_game_pid()
            if pid:
                os.kill(pid, signal.SIGINT)
            for _ in range(40):
                if self.proc.poll() is not None:
                    break
                time.sleep(0.5)
            if self.proc.poll() is None and pid and self.find_game_pid() == pid:
                os.kill(pid, signal.SIGKILL)
                time.sleep(1)
            if self.proc.poll() is None:
                self.proc.kill()
        self.proc = None


def check_deadline(deadline):
    if time.time() > deadline:
        raise Timeout()


def fight(game, fid, args, deadline, shot=None):
    """Drives one battle; the phase times come from the live log lines, in wall seconds."""
    hold, prompt_after = args.hold, args.prompt_after
    status, phases, fatal = "nodraw", [], False
    began = time.time()
    drawn = command = ended = victory = field = None
    nudged = False
    shots = list(args.shot_delays) if shot else []
    hold_shot = None
    commands_entered = presses = 0
    target_shot_at = None
    action_shot = False
    next_press = next_walk = 0
    cancel_at = cancel_sent = cancel_serial = None
    walk_step = 0
    walk_dirs = ("0 20000", "20000 0", "0 -20000", "-20000 0")
    if fid is not None:
        game.command("battle.txt", f"battle {fid}")
    moved = began
    while True:
        # Lines after this battle's field return belong to the next battle (--chain).
        batch, game.carry = getattr(game, "carry", []) + game.lines(), []
        for index, line in enumerate(batch):
            if fid is not None and f"battle tour: formation {fid} drawn" in line:
                drawn = time.time()
            elif "battle request: formation=" in line and not drawn:
                drawn = time.time()
                stamp = LOG_TIME.match(line)
                if args.press_at_request is not None and stamp and getattr(game, "started_at", None):
                    cancel_at = game.started_at + float(stamp.group(1)) + args.press_at_request / 1000.0
            elif "fatal resource lookup" in line or "dirty disc error" in line:
                fatal = True
            elif ended and MAP.search(line) and not field:
                field = {"wall": time.time(), "line": MAP.search(line).group(0)}
                game.carry = batch[index + 1:]
                break
            m = PHASE.search(line)
            if m and args.press_at is not None and not phases and int(m.group(2)) == 0:
                # The log time of the line, not the time the rig read it: the log reaches
                # the rig up to a second or more late.
                stamp = LOG_TIME.match(line)
                if stamp and getattr(game, "started_at", None):
                    cancel_at = game.started_at + float(stamp.group(1)) + args.press_at / 1000.0
            if m:
                phase = int(m.group(2))
                moved = time.time()
                phases.append({"phase": phase, "wall_s": round(moved - began, 3)})
                command = command or (moved if phase == 2 else None)
                if phase == 2:
                    commands_entered += 1
                if phase in (11, 12, 13) and not ended:
                    ended = moved
                    status = {11: "victory", 12: "defeat", 13: "ended"}[phase]
        now = time.time()
        current = phases[-1]["phase"] if phases else None
        if fatal or game.proc.poll() is not None:
            status = "fatal" if fatal else "dead"
            break
        if field:
            # Confirm that the field stays up: no new battle and the game still runs.
            time.sleep(5)
            if game.proc.poll() is not None:
                status, field = "dead", None
            break
        limit = max(90, prompt_after + 30)
        no_start = args.walk_timeout if fid is None else 90
        if now > deadline or (not phases and now - began > no_start) or (phases and now - moved > limit and not ended):
            status = "timeout" if now > deadline else "stall" if phases else ("nobattle" if drawn else "nodraw")
            break
        # The scripted skip press, timed from the first phase 0 line or from the request.
        if args.press_at is not None and cancel_sent is None and phases and phases[0]["phase"] == 0:
            cancel_at = cancel_at or began + phases[0]["wall_s"] + args.press_at / 1000.0
        if args.press_at_request is not None and cancel_sent is None and drawn:
            cancel_at = cancel_at or drawn + args.press_at_request / 1000.0
        if cancel_at and cancel_sent is None and now >= cancel_at:
            game.command("input.txt", f"{args.press_button} 0 0 {args.press_ticks}")
            delay = args.press_at if args.press_at is not None else args.press_at_request
            cancel_sent = round(now - cancel_at + delay / 1000.0, 3)
            cancel_serial = game.serial
        if fid is None and not drawn and now >= next_walk:
            # Walking encounter: keep the stick moving, turning so that a wall does not stop the steps.
            game.command("input.txt", f"0 {walk_dirs[walk_step % 4]} 150")
            walk_step += 1
            next_walk = now + 2.5
        if fid is not None and not drawn and not nudged and now - began > 15:
            game.command("input.txt", "0 0 20000 60")  # walk a step in case the update needs movement
            nudged = True
        # Screenshots: first command, and every few seconds in a phase 1 hold.
        if shot and args.intro_shots and phases:
            if now - phases[0]["wall_s"] - began >= args.intro_shots[0]:
                shot(f"p0_plus_{args.intro_shots.pop(0)}s")
        if shot:
            if command and shots and now - command >= shots[0]:
                shot(f"p2_plus_{shots.pop(0)}s")
            if current == 1 and now - moved > 1 and (hold_shot is None or now - hold_shot > args.hold_shot_every):
                shot(f"p1_hold_{now - moved:.0f}s")
                hold_shot = now
        ready = command and now - command > (max(args.shot_delays) + 0.5 if shot else 0) + args.first_press_delay
        if args.real:
            # Real input: A answers the command menu. The debug victory comes only
            # after the turns that the test wants, so that it cannot hide a bad turn.
            if ready and not ended and current in (2, 3) and now >= next_press:
                game.command("input.txt", f"{A_BUTTON} 0 0 6")
                presses += 1
                next_press = now + (args.target_look if shot and presses == 1 else 0.7)
                if shot and presses == 1:
                    target_shot_at = now + args.target_look - 0.3
            if shot and target_shot_at and now >= target_shot_at:
                shot("target_select")
                target_shot_at = None
            if shot and current == 8 and not action_shot and now - moved > args.action_look:
                shot("first_action")
                action_shot = True
            turns_done = commands_entered > args.real_turns
            if turns_done and not ended and (not victory or now - victory > 10):
                game.command("battle.txt", "victory")
                victory = now
        elif command and not ended and now - command > hold and (not victory or now - victory > 10):
            game.command("battle.txt", "victory")
            victory = now
        # A answers results and messages; before the first command only when the
        # intro waits on something, so that it does not disturb the measurement.
        if ended or (phases and not command and now - moved > prompt_after):
            game.command("input.txt", f"{A_BUTTON} 0 0 6")
        # A short poll while a timed press waits, so that it is not late.
        time.sleep(2 if ended else 0.01 if cancel_sent is None and (drawn or phases) and
                   (args.press_at is not None or args.press_at_request is not None) else 0.1)
    return dict(status=status, phases=phases, drawn=bool(drawn), victory_requested=bool(victory),
                returned_to_field=bool(field), field_line=field["line"] if field else None,
                commands_entered=commands_entered, wall_s=round(time.time() - began, 1),
                press_sent_after_phase0_s=cancel_sent, press_serial=cancel_serial)


def press_timing(text, serial):
    """Log time of the first guest read of the press pulse, of the request and of phase 2 (ms)."""
    def stamp(pattern):
        for line in text.splitlines():
            if re.search(pattern, line):
                m = LOG_TIME.match(line)
                return float(m.group(1)) * 1000 if m else None
        return None
    request = stamp(r"battle request: formation=\d+ caller=0x829e3220")
    pulse = stamp(rf"test input returned: serial={serial} .* active=true") if serial else None
    phase2 = stamp(r"battle intro: swap=\d+ t=\d+ phase=2 ")
    rel = lambda v: round(v - request) if v is not None and request is not None else None
    return dict(press_after_request_ms=rel(pulse), phase2_after_request_ms=rel(phase2))


def parse_trace(text):
    """Trace lines give each phase change a swap number and active game time; lists the script's natives."""
    sequence, events, sleeps, waits = [], {}, [], []
    start_class = actor = None
    sleep_hits = dict(phase_actor=0, other=0)
    snapshots, skipped, holds, battle_id = [], 0, [], None
    window, spawn_t = [], None
    matinees = set()
    slot = None
    for line in text.splitlines():
        m = TRACE.search(line)
        if not m:
            continue
        swap, t, phase, kind, rest = int(m.group(1)), int(m.group(2)), int(m.group(3)), m.group(4), m.group(5)
        fields = dict(re.findall(r"(\w+)=(\S+)", rest))
        if kind in ("skip_gate", "skip_window", "skip_press", "skip_sound_press", "skip_sound_sweep",
                    "skip_sound_summary", "skip_press_stored", "skip_press_dropped", "skip_early_end", "skip_early_picked",
                    "request"):
            window.append(dict(fields, event=kind, game_ms=t, swap=swap, phase=phase,
                               after_spawn_ms=t - spawn_t if spawn_t is not None else None))
        if kind == "snapshot":
            snapshots.append(fields)
        elif kind == "skip_sleep":
            skipped += 1
        elif kind == "ai_hold":
            holds.append(dict(fields, game_ms=t, swap=swap))
        elif kind == "state" and fields.get("battle_id") not in (None, "-1") and battle_id is None:
            battle_id = int(fields["battle_id"])
        if not sequence or sequence[-1]["phase"] != phase:
            sequence.append(dict(phase=phase, game_ms=t, swap=swap))
        events[kind] = events.get(kind, 0) + 1
        if kind == "spawn" and fields.get("slot") == "1":
            spawn_t = t
            start_class, actor, slot = fields.get("class"), fields.get("actor"), fields.get("stored_slot")
            events["spawn_slot1"] = dict(manager=fields.get("manager"), actor=actor, stored_slot=slot,
                                         result=fields.get("result"), battle_id=fields.get("battle_id"), phase=phase)
        elif kind == "sleep":
            if fields.get("phase_actor") == "true":
                sleep_hits["phase_actor"] += 1
                sleeps.append(dict(phase=phase, actor=fields.get("actor"), slot=fields.get("slot"),
                                   seconds=float(fields.get("seconds", "nan")), latent_op=fields.get("latent_op"),
                                   game_ms=t, swap=swap, caller=fields.get("caller")))
            else:
                sleep_hits["other"] += 1
        elif kind == "wait":
            waits.append(dict(phase=phase, type=fields.get("type"), game_ms=t, swap=swap))
        elif kind == "state":
            matinees.add(fields.get("matinees"))
    return dict(start_class=start_class, sequence=sequence, sleeps=sleeps, sleep_hits=sleep_hits, waits=waits,
                events=events, matinee_counts=sorted(m for m in matinees if m is not None), snapshots=snapshots,
                skipped_sleeps=skipped, ai_holds=holds, battle_id=battle_id, window=window)


def first_time(sequence, phase, key):
    return next((p[key] for p in sequence if p["phase"] == phase), None)


def one_run(run, fid, args, table, attempt=0):
    results = run / "results"
    results.mkdir(exist_ok=True)
    stem = f"{args.label}-{'walk' if fid is None else fid}" + (f"-r{attempt}" if args.runs > 1 else "")
    env = dict(item.split("=", 1) for item in args.env)
    if fid is None:
        env["LO_DEBUG_ENCOUNTER_EVERY_STEP"] = "1"
    shot_path = results / f"{stem}-shot.ppm"
    if args.shots:
        env.update(LO_SCREENSHOT_REQUEST=str(run / "shot.txt"), LO_SCREENSHOT_PATH=str(shot_path))
    (run / "shot.txt").unlink(missing_ok=True)
    if not args.no_trace:
        env.setdefault("LO_TRACE_INPUT", "1")
    game = IsolatedGame(run, not args.no_trace, env)
    shot_log = []
    chain = []

    def shot(name, count=1):
        game.serial += 1
        tmp = run / "shot.txt.tmp"
        tmp.write_text(f"{game.serial} {count}\n")
        os.replace(tmp, run / "shot.txt")
        shot_log.append(dict(name=name, wall=round(time.time() - started, 2)))
    started = time.time()
    deadline = started + args.timeout
    result = dict(formation=fid, start_class=table.get(str(fid), {}).get("start_class"), battle_status="noboot",
                  returned_to_field=False)
    try:
        before = real_files()
        others = other_game_processes()
        if others:
            raise IsolationError(f"another game process runs ({others}); it was not touched")
        game.started_at = time.time()
        game.start()
        game.verify_isolation()
        booted = game.wait_for(lambda l: MAP.search(l).group(0) if MAP.search(l) else None, min(args.timeout, 300))
        if not booted.startswith("current map"):
            raise Timeout()
        result.update(boot_s=round(time.time() - started, 1), field_line_before=booted)
        if args.shots and args.burst:
            # Every frame for a while: the request comes about 1.5 s after the map line.
            time.sleep(args.burst[0])
            shot("burst", int(args.burst[1]))
            time.sleep(max(0.0, 3 - args.burst[0]))
        elif args.press_at_request is None:
            time.sleep(3)
        # --press-at-request: the request can come about 1.5 s after the map line, so
        # the rig reads the log at once.
        check_deadline(deadline)
        live = fight(game, fid, args, deadline, shot if args.shots else None)
        result.update(battle_status=live.pop("status"), returned_to_field=live["returned_to_field"], live=live,
                      same_map_after=live["field_line"] == booted)
        # A grinding session: more walking encounters in the same process.
        for _ in range(args.chain - 1):
            if not live["returned_to_field"]:
                break
            live = fight(game, fid, args, deadline, None)
            chain.append(dict(live))
            result.update(battle_status=live.pop("status"), returned_to_field=live["returned_to_field"])
    except Timeout:
        result["timed_out"] = True
    finally:
        held = game.open_real_paths()
        game.stop()
        log = getattr(game, "log", None)
    text = log.read_text("utf-8", "replace") if log and log.exists() else ""
    pngs = []
    for ppm in sorted(results.glob(f"{stem}-shot*.ppm")):
        png = ppm.with_suffix(".png")
        subprocess.run([sys.executable, str(REPO / "tools/ppm2png.py"), str(ppm), str(png)], check=True)
        ppm.unlink()
        pngs.append(str(png))
    if log and log.exists():
        shutil.copy(log, results / f"{stem}.log")
    trace = parse_trace(text)
    sequence = trace["sequence"]
    t0, t2 = first_time(sequence, 0, "game_ms"), first_time(sequence, 2, "game_ms")
    w = (result.get("live") or {}).get("phases", [])
    w0, w2 = first_time(w, 0, "wall_s"), first_time(w, 2, "wall_s")
    changed = real_files() != before
    # The game's own draw, not a tour request: the formation comes from the walking update.
    requests = re.findall(r"battle request: formation=(\d+) caller=(\S+) walking=(\w+)", text)
    toured = "battle tour: formation" in text and " drawn" in text
    # The scripted press as the guest saw it: test input lines while the pulse is active.
    press_mask = int(args.press_button, 16)
    press_serial = (result.get("live") or {}).get("press_serial")
    pulse = [dict(buttons=int(b, 16), raw=int(r, 16))
             for serial, b, r in re.findall(r"test input returned: serial=(\d+) .* active=true buttons=(0x[0-9a-f]+) "
                                            r"before_menu=(0x[0-9a-f]+)", text) if press_serial and int(serial) == press_serial]
    # The first battle only: in a walk, the next draw can start before the game stops.
    window = trace["window"]
    gates = [i for i, e in enumerate(window) if e["event"] == "skip_gate"]
    window = window[:gates[1]] if len(gates) > 1 else window
    press_events = [e for e in window if e["event"] == "skip_press"]
    phase2 = first_time(sequence, 2, "game_ms")
    stored = [e for e in window if e["event"] == "skip_press_stored"]
    press_info = dict(
        sent_after_phase0_s=(result.get("live") or {}).get("press_sent_after_phase0_s"),
        **press_timing(text, press_serial),
        stored=len(stored),
        stored_after_request_ms=int(stored[0]["since_request_ms"]) if stored else None,
        dropped=sum(1 for e in window if e["event"] == "skip_press_dropped"),
        early_end=[e.get("reason") for e in window if e["event"] == "skip_early_end"],
        request_to_spawn_ms=next((int(e["since_request_ms"]) for e in window if e["event"] == "skip_gate"
                                  and e.get("early") == "true"), None),
        guest_saw_cancel=any(p["buttons"] & press_mask for p in pulse) if pulse else None,
        pulse_reads=len(pulse),
        armed=sum(1 for e in window if e["event"] == "skip_window" and e.get("visible") == "true"),
        fired=len(press_events),
        press_after_spawn_ms=int(press_events[0]["after_spawn_ms"]) if press_events else None,
        press_to_phase2_ms=phase2 - press_events[0]["game_ms"] if press_events and phase2 is not None else None,
        source=press_events[0].get("source") if press_events else None,
        latch_events=len(re.findall(r"skip latch: ", text)),
        sounds=[{k: v for k, v in e.items() if k not in ("swap", "after_spawn_ms")} for e in window
                if e["event"].startswith("skip_sound")],
        window=[{k: e.get(k) for k in ("event", "visible", "reason", "after_spawn_ms", "phase", "random", "option",
                                       "source", "running_sleep", "seconds_left", "early", "stored",
                                       "since_request_ms", "class", "game_ms") if e.get(k) is not None}
                for e in window])
    if fid is None and requests:
        result["formation"] = int(requests[0][0])
    result.update(
        env={k: v for k, v in env.items() if k.startswith("LO_BATTLE") or k.startswith("LO_DEBUG")},
        battle_intro_skip=SKIP_MODES[args.skip_mode] if args.skip_mode else None,
        requests=[dict(formation=int(f), caller=c, walking=w == "true") for f, c, w in requests],
        real_walking_draw=fid is None and not toured and any(w == "true" for _, _, w in requests),
        screenshots=pngs, shot_log=shot_log, chain=chain, press=press_info,
        start_class=trace["start_class"] or result["start_class"], trace=trace,
        phase_sequence=[p["phase"] for p in w] or [p["phase"] for p in sequence],
        phase0_to_phase2_ms=t2 - t0 if t0 is not None and t2 is not None else None,
        phase0_to_phase2_wall_s=round(w2 - w0, 3) if w0 is not None and w2 is not None else None,
        battle_ended=result["battle_status"] in ("victory", "defeat", "ended"),
        isolation=dict(real_files_unchanged=not changed, real_files_checked=len(before), real_paths_held_open=held),
        log=str(results / f"{stem}.log"))
    (results / f"{stem}.json").write_text(json.dumps(result, indent=1))
    brief = {k: result.get(k) for k in ("formation", "start_class", "battle_status", "phase_sequence", "phase0_to_phase2_ms",
                                        "phase0_to_phase2_wall_s", "battle_ended", "returned_to_field", "same_map_after",
                                        "real_walking_draw", "isolation", "boot_s")}
    brief.update(label=stem, skipped_sleeps=trace["skipped_sleeps"],
                 press={k: v for k, v in press_info.items() if k != "window"},
                 gate=next(({k: e.get(k) for k in ("option", "random", "battle_id")} for e in window
                            if e["event"] == "skip_gate"), None))
    print(json.dumps(brief), flush=True)
    if changed:
        raise IsolationError("the player's files changed during the run")
    return result["returned_to_field"]


def list_formations(run, args):
    game = IsolatedGame(run, False)
    lines, total = [], None
    try:
        if other_game_processes():
            raise IsolationError("another game process runs; it was not touched")
        before = real_files()
        game.start()
        game.verify_isolation()
        if game.wait_for(lambda l: "ok" if MAP.search(l) else None, min(args.timeout, 300)) != "ok":
            sys.exit("boot failed")
        time.sleep(3)
        game.command("battle.txt", "list")
        deadline = time.time() + 30
        while time.time() < deadline and (total is None or len(lines) < total):
            for line in game.lines():
                if "battle tour: formations=" in line:
                    total = int(line.split("formations=")[1].split()[0])
                elif FORMATION.search(line) and FORMATION.search(line).group(2).startswith(" +"):
                    lines.append(line)
            time.sleep(0.5)
    finally:
        game.stop()
    table = {}
    for line in lines:
        m = FORMATION.search(line)
        fields = dict(re.findall(r"\+(\d+)[=:](\S+)", m.group(2)))
        words = (m.group(3) or "").split()
        table[m.group(1)] = dict(stage=fields.get("0", ""), ai=fields.get("60", ""), start_class=fields.get("72", ""),
                                 end_class=fields.get("84", ""), plus24=fields.get("24", "0"),
                                 enemies=int(fields.get("152", "0"), 16),
                                 models=[words[k * 8 + 1] for k in range(len(words) // 8)])
    (run / "formations.json").write_text(json.dumps(table, indent=0))
    classes = {}
    for row in table.values():
        classes.setdefault(row["start_class"], []).append(row)
    print(f"{len(table)} of {total} formations; classes: { {k: len(v) for k, v in classes.items()} }")
    if real_files() != before:
        raise IsolationError("the player's files changed during the run")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("command", choices=["setup", "list", "run"])
    ap.add_argument("run", type=Path)
    ap.add_argument("--save-from", help="setup: a save folder; default the player's folder (read only)")
    ap.add_argument("--exe", default=str(REPO / "out/build/linux-clang/LostOdysseyRecomp/LostOdysseyRecomp"))
    ap.add_argument("--formations", default="3", help="run: formation ids, like 3,10")
    ap.add_argument("--hold", type=float, default=3, help="run: seconds at the first command before the victory")
    ap.add_argument("--prompt-after", type=float, default=60,
                    help="run: before the first command, press A after this many seconds without a phase change")
    ap.add_argument("--timeout", type=float, default=420, help="hard limit for one run, in seconds")
    ap.add_argument("--frame-rate", type=int, default=60, help="run: frame rate setting")
    ap.add_argument("--label", default="trace", help="run: name prefix of the result files")
    ap.add_argument("--no-trace", action="store_true", help="run: leave LO_TRACE_BATTLE_INTRO unset")
    ap.add_argument("--skip-mode", choices=sorted(SKIP_MODES), default=None,
                    help="run: write battle_intro_skip (off 0, press 1, always 2)")
    ap.add_argument("--skip", action="store_true", help="run: --skip-mode press")
    ap.add_argument("--press-at", type=float, default=None,
                    help="run: press the cancel button this many ms (wall) after phase 0 starts")
    ap.add_argument("--press-at-request", type=float, default=None,
                    help="run: press the cancel button this many ms (wall) after the battle request")
    ap.add_argument("--burst", type=lambda v: [float(x) for x in v.split(",")], default=None,
                    help="run: with --shots, SECONDS,FRAMES: every frame for FRAMES frames, SECONDS after the map line")
    ap.add_argument("--press-button", default="2000", help="run: XInput mask (hex) for --press-at; 2000 is B")
    ap.add_argument("--press-ticks", type=int, default=6, help="run: engine ticks that the --press-at button is held")
    ap.add_argument("--skip-off", action="store_true", help="run: write battle_intro_skip=0 explicitly")
    ap.add_argument("--walk", action="store_true", help="run: a real walking encounter (the game's own draw)")
    ap.add_argument("--walk-timeout", type=float, default=90, help="run: seconds of walking before a run counts as no battle")
    ap.add_argument("--real", action="store_true", help="run: drive the command menu with A")
    ap.add_argument("--real-turns", type=int, default=2, help="run: command phases to play before the debug victory")
    ap.add_argument("--first-press-delay", type=float, default=0, help="run: seconds at the first command before the first A")
    ap.add_argument("--shots", action="store_true", help="run: screenshots at the first command and in a phase 1 hold")
    ap.add_argument("--shot-delays", type=lambda v: [float(x) for x in v.split(",")], default=[1.0, 4.0],
                    help="run: seconds after the first command for the screenshots")
    ap.add_argument("--intro-shots", type=lambda v: [float(x) for x in v.split(",")], default=[],
                    help="run: seconds after phase 0 starts for screenshots of the intro")
    ap.add_argument("--hold-shot-every", type=float, default=4, help="run: seconds between screenshots in a phase 1 hold")
    ap.add_argument("--target-look", type=float, default=2.0, help="run: with --real --shots, seconds on the target menu")
    ap.add_argument("--action-look", type=float, default=3.0, help="run: with --real --shots, seconds into phase 8 for a screenshot")
    ap.add_argument("--chain", type=int, default=1, help="run: with --walk, battles to play in one game process")
    ap.add_argument("--runs", type=int, default=1, help="run: repeats of each formation")
    ap.add_argument("--setting", action="append", default=[], help="run: extra settings.ini line KEY=VALUE")
    ap.add_argument("--env", action="append", default=[],
                    help="run: extra game environment KEY=VALUE, like LO_SCREENSHOT_PRESENTED=1")
    args = ap.parse_args()
    if args.press_at is not None and args.press_at_request is not None:
        ap.error("use one of --press-at and --press-at-request")
    if args.skip or args.skip_off:
        if args.skip_mode or (args.skip and args.skip_off):
            ap.error("use one of --skip, --skip-off and --skip-mode")
        args.skip_mode = "press" if args.skip else "off"
    run = args.run.resolve()
    # A SIGTERM must also stop the game.
    signal.signal(signal.SIGTERM, lambda *_: sys.exit("terminated"))
    try:
        check_run_dir(run)
        if args.command == "setup":
            setup(run, args)
            return 0
        if not (run / "launch.json").exists():
            raise IsolationError(f"{run} has no launch.json; run setup first")
        if args.command == "list":
            list_formations(run, args)
            return 0
        write_settings(run, args.frame_rate, args.skip_mode, args.setting)
        table = json.loads((run / "formations.json").read_text()) if (run / "formations.json").exists() else {}
        ok = True
        targets = [None] if args.walk else [int(x) for x in args.formations.split(",")]
        for fid in targets:
            for attempt in range(1, args.runs + 1):
                ok = one_run(run, fid, args, table, attempt) and ok
        return 0 if ok else 1
    except IsolationError as error:
        print(f"ISOLATION FAILURE: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
