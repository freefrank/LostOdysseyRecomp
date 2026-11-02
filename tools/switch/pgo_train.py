#!/usr/bin/env python3
"""Train the profile-guided Switch build on the console (docs/SWITCH-PGO.md). From SwitchWakerHD's tools/switch/pgo_train.py.

The console runs a LO_SWITCH_PGO=GEN build. For each scene this starts the game, gets there, waits until the draw count says
the scene is on screen, zeroes the counters, lets it run and writes them ("pgo dump": libgcov adds them to the .gcda files in
sdmc:/switch/LostOdysseyRecomp/pgo/). Menus and loading are left out on purpose: code the training never reached is
compiled as usual (-fprofile-partial-training).

  pgo_train.py clean                          removes the .gcda files from the console
  pgo_train.py scene battle|ship [--dwell S]  one scene; adds to what is on the console
  pgo_train.py fetch [--out out/pgo]          copies the .gcda files here

env.txt is put back after a scene. To check a profile on a place it never saw, fetch after one scene, build USE from it
and judge it on the other scene. The instrumented game is slow: never judge speed on it.
"""
import argparse
import re
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import lo_debug  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
REMOTE = "pgo"
WALK = [(0, 1), (1, 0), (0, -1), (-1, 0)]


def wait_for_draws(con, least, seconds):
    end = time.time() + seconds
    draws = 0
    while time.time() < end:
        match = re.search(r" draws=(\d+)", con.info().get("last", ""))
        draws = int(match[1]) if match else 0
        if draws >= least:
            return True
        time.sleep(3.0)
    print(f"the scene did not arrive: {draws} draws, {least} wanted")
    return False


def walk(con, step):
    x, y = WALK[step % len(WALK)]
    con.command("stick", "L", x, y, 4000, timeout=20.0)


def attack(con):
    con.command("press", "A", timeout=20.0)


def battle(con, scale, restart):
    """A new game's first battle: New Game, B to close the settings it opens, then START and BACK to skip the movie."""
    restart({})
    time.sleep(58 * scale)
    for button, wait in (("START", 10), ("A", 6), ("B", 40), ("START", 4), ("BACK", 0)):
        con.command("press", button, timeout=20.0)
        time.sleep(wait * scale)
    return 1000, lambda step: attack(con)


def ship(con, scale, restart):
    """The merchant ship, from the console's save (save/user02)."""
    restart({})
    time.sleep(58 * scale)
    for button in ("START", "DOWN", "A", "A", "A", "A"):
        con.command("press", button, timeout=20.0)
        time.sleep(4 * scale)

    return 600, lambda step: walk(con, step)


SCENES = {"battle": battle, "ship": ship}


def cmd_scene(args):
    con = lo_debug.console(args)
    if "pgo:" not in con.text("pgo", "status"):
        sys.exit("this build has no 'pgo' command: it must be a LO_SWITCH_PGO=GEN build")
    original = con.get_bytes("env.txt")

    def restart(updates):
        lines = lo_debug.edit_env(original.decode("utf-8", errors="replace").splitlines(), updates)
        con.put_bytes(("\n".join(lines) + "\n").encode(), "env.txt", quiet=True)
        con.restart(force=True)

    try:
        if args.here:
            least = 1
            during = (lambda step: attack(con)) if args.scene == "battle" else (lambda step: walk(con, step))
        else:
            least, during = SCENES[args.scene](con, args.scale, restart)
        if not wait_for_draws(con, least, args.arrive):
            sys.exit(f"{args.scene}: not trained")
        con.text("pgo", "reset")
        end, step = time.time() + args.dwell, 0
        while time.time() < end:
            if during:
                during(step)
            step += 1
            time.sleep(5.0)
        print(con.text("pgo", "dump", timeout=300.0))
        print(con.info().get("last", "")[:120])
    finally:
        con.put_bytes(original, "env.txt", quiet=True)


def cmd_fetch(args):
    con = lo_debug.console(args)
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    total = count = 0
    for kind, _, name in con.ls(REMOTE):
        if kind == "f" and name.endswith(".gcda"):
            data = con.get_bytes(f"{REMOTE}/{name}")
            (out / name).write_bytes(data)
            total += len(data)
            count += 1
    print(f"fetched {count} .gcda files ({total / 1e6:.1f} MB) to {out}")


def cmd_clean(args):
    con = lo_debug.console(args)
    names = [name for kind, _, name in con.ls(REMOTE) if kind == "f" and name.endswith(".gcda")]
    for name in names:
        con.text("rm", f"{REMOTE}/{name}")
    print(f"removed {len(names)} .gcda files")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--host", default=lo_debug.default_host())
    parser.add_argument("--port", type=int, default=lo_debug.DEFAULT_PORT)
    sub = parser.add_subparsers(dest="command", required=True)
    scene = sub.add_parser("scene")
    scene.add_argument("scene", choices=SCENES)
    scene.add_argument("--dwell", type=float, default=180.0, help="seconds of training in the scene")
    scene.add_argument("--arrive", type=float, default=600.0, help="seconds to wait for the scene")
    scene.add_argument("--here", action="store_true", help="the game is already in the scene: train where it stands")
    scene.add_argument("--scale", type=float, default=1.0, help="stretches the start's timings")
    scene.set_defaults(run=cmd_scene)
    fetch = sub.add_parser("fetch")
    fetch.add_argument("--out", default=str(REPO / "out/pgo"))
    fetch.set_defaults(run=cmd_fetch)
    sub.add_parser("clean").set_defaults(run=cmd_clean)
    args = parser.parse_args()
    args.run(args)


if __name__ == "__main__":
    main()
