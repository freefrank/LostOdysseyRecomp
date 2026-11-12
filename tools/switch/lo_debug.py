#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
# distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# Adapted from SwitchWakerHD's tools/switch/wwhd_debug.py (github.com/centollOS/SwitchWakerHD, commit
# cfd44c0a26182361820ac59859c1d94243ad8ea0, MPL-2.0) for LostOdysseyRecomp: a Console class that
# tools/scenario.py --switch drives, XInput button names, PNG or PPM screenshots, env.txt editing, deploy waiting for
# a new session, and this port's file layout.
"""Client of the Switch debug server (docs/SWITCH.md, "Debug server").

The console runs the server when env.txt has LO_DEBUG_SERVER=1 (or settings.ini debug_server=1), from the next
start; its stderr.log and network log say '[debug] server listening on <ip>:6543'.
The console's address: --host, else LO_SWITCH_HOST, else the first line of out/switch-host.txt.

  lo_debug.py info                        build, session, frame, map, mode, clocks, memory, battery, address
  lo_debug.py log [--all] [--save F] [--grep RE] [--seconds N]
                                          the log as it is written (Ctrl-C ends it)
  lo_debug.py logtext [OUT]               the log text the console keeps (the running session's last 1-2 MiB)
  lo_debug.py shot [OUT.png] [--ppm]      screenshot of the next frame
  lo_debug.py press A [B ...] [ms]        also hold, release, stick L|R x y [ms], input <test input line>
  lo_debug.py get REMOTE [LOCAL] | put LOCAL REMOTE | ls [REMOTE] | rm REMOTE | mkdir REMOTE
  lo_debug.py env [KEY=VALUE ...] [--unset KEY ...] [--restart]
                                          show or change env.txt (read at the next start)
  lo_debug.py logs | lastlog [LOCAL] [--current]
                                          session logs (state/logs); the previous session's log
  lo_debug.py crashes [--fetch DIR]       crash.log files and Atmosphere's crash reports
  lo_debug.py deploy [NRO] [--no-restart] [--force]
                                          upload the NRO, check its CRC32, restart, wait for the new session
  lo_debug.py restart [--force] | wait [--seconds N] | ping | help
  lo_debug.py raw COMMAND ARGS...         any command, its reply printed

Remote paths are relative to sdmc:/switch/LostOdysseyRecomp unless they start with / (the SD card's root).
Writes (put, rm, mkdir) only go inside that folder.
"""
from __future__ import annotations

import argparse
import os
import re
import socket
import sys
import threading
import time
import zlib
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
DEFAULT_PORT = 6543
ENV_HOST = "LO_SWITCH_HOST"
ENV_PORT = "LO_DEBUG_PORT"
HOST_FILE = REPO / "out" / "switch-host.txt"
NRO_CANDIDATES = [REPO / "out/switch/LostOdysseyRecomp.nro", REPO / "out/build/switch/LostOdysseyRecomp/LostOdysseyRecomp.nro"]
REMOTE_NRO = "LostOdysseyRecomp.nro"
REMOTE_LOGS = "state/logs"
SESSION_LOG = re.compile(r"runtime-(\d+)\.log")
MENU_SWITCH = "LO_DEBUG_SERVER=1 in sdmc:/switch/LostOdysseyRecomp/env.txt (or the settings menu's Debug section), then start the game"


class ServerError(Exception):
    pass


def default_host() -> str | None:
    if os.environ.get(ENV_HOST):
        return os.environ[ENV_HOST]
    if HOST_FILE.exists():
        lines = HOST_FILE.read_text().strip().splitlines()
        if lines:
            return lines[0].strip()
    return None


def quote(arg: str) -> str:
    return f'"{arg}"' if (" " in arg or not arg) else arg


class Conn:
    """One connection: commands and their replies, in order."""

    def __init__(self, host: str, port: int, timeout: float = 10.0):
        self.sock = socket.create_connection((host, port), timeout=timeout)
        self.buf = b""

    def close(self) -> None:
        self.sock.close()

    def _fill(self) -> None:
        data = self.sock.recv(1 << 16)
        if not data:
            raise ConnectionError("the console closed the connection")
        self.buf += data

    def _line(self) -> str:
        while b"\n" not in self.buf:
            self._fill()
        line, self.buf = self.buf.split(b"\n", 1)
        return line.decode()

    def _exact(self, n: int, out=None) -> bytes:
        """n bytes: returned, or written to the file object out"""
        chunks = []
        while n:
            if not self.buf:
                self._fill()
            take = self.buf[:n]
            self.buf = self.buf[len(take):]
            n -= len(take)
            if out:
                out.write(take)
            else:
                chunks.append(take)
        return b"".join(chunks)

    def header(self) -> tuple[bool, int]:
        status, size = self._line().split(" ", 1)
        return status == "ok", int(size)

    def command(self, *args, payload: bytes | None = None, out=None, timeout: float | None = None) -> bytes:
        """Sends a command; its reply (bytes, or written to out). Raises ServerError on 'err'."""
        if timeout is not None:
            self.sock.settimeout(timeout)
        self.sock.sendall((" ".join(quote(str(a)) for a in args) + "\n").encode())
        if payload is not None:
            self.sock.sendall(payload)
        ok, size = self.header()
        if not ok:
            raise ServerError(self._exact(size).decode(errors="replace"))
        return self._exact(size, out)


class LogStream(threading.Thread):
    """The 'log' stream on a connection of its own, appended to a file (and to lines, for callers that wait)."""

    def __init__(self, host: str, port: int, path: Path, everything: bool = True):
        super().__init__(daemon=True)
        self.conn = Conn(host, port)
        self.conn.sock.sendall(b"log all\n" if everything else b"log\n")
        ok, _ = self.conn.header()
        if not ok:
            raise ServerError("the server refused the log stream")
        self.path = path
        self.closed = threading.Event()  # the console closed the stream (it quit, restarted or crashed)
        self._stop = threading.Event()
        self.bytes = 0

    def run(self) -> None:
        self.conn.sock.settimeout(0.5)
        with open(self.path, "ab") as f:
            if self.conn.buf:
                f.write(self.conn.buf)
                self.bytes += len(self.conn.buf)
                self.conn.buf = b""
            while not self._stop.is_set():
                try:
                    data = self.conn.sock.recv(1 << 16)
                except socket.timeout:
                    continue
                except OSError:
                    break
                if not data:
                    break
                f.write(data)
                f.flush()
                self.bytes += len(data)
        self.closed.set()

    def stop(self) -> None:
        self._stop.set()
        self.join(timeout=3)
        try:
            self.conn.close()
        except OSError:
            pass


class Console:
    """A console's debug server: one command per connection, so callers on several threads do not mix replies."""

    def __init__(self, host: str, port: int = DEFAULT_PORT):
        if not host:
            raise SystemExit(f"the console's address: --host IP, {ENV_HOST}=IP, or {HOST_FILE.relative_to(REPO)}")
        self.host, self.port = host, port

    def connect(self, timeout: float = 10.0) -> Conn:
        try:
            return Conn(self.host, self.port, timeout)
        except OSError as e:
            raise ConnectionError(f"cannot reach {self.host}:{self.port}: {e} (is the game running with its debug "
                                  f"server on? {MENU_SWITCH})") from e

    def command(self, *args, payload: bytes | None = None, out=None, timeout: float = 30.0) -> bytes:
        c = self.connect()
        try:
            return c.command(*args, payload=payload, out=out, timeout=timeout)
        finally:
            c.close()

    def text(self, *args, timeout: float = 30.0) -> str:
        return self.command(*args, timeout=timeout).decode(errors="replace")

    def info(self) -> dict[str, str]:
        """info's lines as a dict: the first word is the key."""
        fields = {}
        for line in self.text("info", timeout=10.0).splitlines():
            key, _, rest = line.partition(" ")
            fields[key] = rest
        return fields

    def session(self) -> str | None:
        """The running session (it changes when the game starts again); None when the server does not answer."""
        try:
            return self.info().get("session", "").split(" ")[0] or None
        except (OSError, ServerError, ValueError):
            return None

    def check_game(self) -> None:
        """Refuses to go on when another game's debug server answers on this port (SwitchWakerHD uses 6543 too)."""
        info = self.info()
        if "build" not in info or "session" not in info:
            first = next(iter(info.items()), ("", ""))
            raise SystemExit(f"the console runs another program's debug server, not LostOdysseyRecomp's "
                             f"({' '.join(first).strip()}): nothing sent")

    def put_bytes(self, data: bytes, remote: str, quiet: bool = False) -> None:
        t0 = time.time()
        reply = self.command("put", remote, len(data), payload=data, timeout=max(60.0, len(data) / 2e5)).decode()
        want = f"crc32 {zlib.crc32(data) & 0xFFFFFFFF:08x} size {len(data)}"
        if reply != want:
            raise ServerError(f"upload check failed: the console says '{reply}', expected '{want}'")
        dt = max(time.time() - t0, 1e-3)
        if not quiet:
            print(f"-> {remote}: {len(data) / 1048576:.1f} MiB in {dt:.1f} s ({len(data) / 1048576 / dt:.1f} MiB/s), crc32 ok")

    def get_bytes(self, remote: str, timeout: float = 120.0) -> bytes:
        return self.command("get", remote, timeout=timeout)

    def ls(self, path: str = "") -> list[tuple[str, int | None, str]]:
        entries = []
        for line in self.text("ls", *([path] if path else [])).splitlines():
            kind, size, name = line[0], line[2:14].strip(), line[15:]
            entries.append((kind, int(size) if size.isdigit() else None, name))
        return entries

    def shot(self, ppm: bool = False) -> bytes:
        return self.command("shot", *(["ppm"] if ppm else []), timeout=60.0)

    def wait(self, seconds: float, other_than: str | None = None) -> str | None:
        """Waits until the server answers (with a session other than other_than); that session, or None."""
        end = time.time() + seconds
        while time.time() < end:
            session = self.session()
            if session and session != other_than:
                return session
            time.sleep(1.0)
        return None

    def restart(self, force: bool = False, seconds: float = 180.0) -> str:
        """Restarts the game and waits for its new session (the server starts again with it)."""
        before = self.session()
        print(self.text("restart", *(["force"] if force else []), timeout=10.0))
        print("waiting for the game to start again...", flush=True)
        session = self.wait(seconds, other_than=before)
        if not session:
            raise SystemExit(f"the server did not come back within {seconds:.0f} s (crashed at start? the previous "
                             "session's log: lastlog)")
        return session

    def read_env(self) -> list[str]:
        try:
            return self.get_bytes("env.txt").decode("utf-8", errors="replace").splitlines()
        except ServerError:
            return []


def edit_env(lines: list[str], updates: dict[str, str | None]) -> list[str]:
    """env.txt with KEY=VALUE lines replaced (None: removed, with the comment above it); new keys at the end, each
    under a comment that says what it does (env_comments.py)."""
    import env_comments  # next to this file
    removed = {k for k, v in updates.items() if v is None}
    out, done = [], set()
    for line in env_comments.without(lines, removed):
        key = line.split("=", 1)[0].strip() if env_comments.is_setting(line) else None
        if key in updates:
            if key not in done:
                out.append(f"{key}={updates[key]}")
            done.add(key)
            continue
        out.append(line)
    added = [f"{k}={v}" for k, v in updates.items() if v is not None and k not in done and k not in removed]
    return out + env_comments.commented(added)


# ---------------------------------------------------------------- commands

def console(args) -> Console:
    return Console(args.host, args.port)


def show(text: str) -> None:
    if text:
        print(text, end="" if text.endswith("\n") else "\n")


def cmd_log(args) -> None:
    c = console(args).connect()
    c.sock.sendall(b"log all\n" if args.all else b"log\n")
    ok, _ = c.header()
    if not ok:
        sys.exit("the server refused the log stream")
    c.sock.settimeout(1.0)
    pattern = re.compile(args.grep) if args.grep else None
    save = open(args.save, "a", encoding="utf-8") if args.save else None
    end = time.time() + args.seconds if args.seconds else None
    pending = c.buf.decode(errors="replace")
    c.buf = b""
    try:
        while end is None or time.time() < end:
            lines = pending.split("\n")
            pending = lines.pop()
            for line in lines:
                if pattern and not pattern.search(line):
                    continue
                print(line, flush=True)
                if save:
                    save.write(line + "\n")
                    save.flush()
            try:
                data = c.sock.recv(1 << 16)
            except socket.timeout:
                continue
            if not data:
                print("[debug client] the console closed the stream", file=sys.stderr)
                break
            pending += data.decode(errors="replace")
    except KeyboardInterrupt:
        pass
    finally:
        c.close()
        if save:
            save.close()


def cmd_get(args) -> None:
    local = Path(args.local or Path(args.remote).name)
    con = console(args)
    c = con.connect()
    try:
        with open(local, "wb") as f:
            c.command("get", args.remote, out=f, timeout=120.0)
    except ServerError:
        local.unlink(missing_ok=True)
        raise
    finally:
        c.close()
    print(f"{args.remote} -> {local} ({local.stat().st_size} bytes)")


def cmd_shot(args) -> None:
    out = Path(args.out or time.strftime("shot_%Y%m%d_%H%M%S") + (".ppm" if args.ppm else ".png"))
    data = console(args).shot(ppm=args.ppm)
    out.write_bytes(data)
    print(f"{out} ({len(data)} bytes)")


def session_logs(con: Console) -> list[str]:
    """The runtime logs, oldest first (named by the session's start)."""
    names = [name for kind, _, name in con.ls(REMOTE_LOGS) if kind == "f" and SESSION_LOG.fullmatch(name)]
    return sorted(names, key=lambda n: int(SESSION_LOG.fullmatch(n).group(1)))


def cmd_lastlog(args) -> None:
    con = console(args)
    if args.current:
        out = Path(args.local or "current-session.log")
        out.write_bytes(con.command("logtext", timeout=30.0))
        print(f"{out}: the running session's log as the console keeps it (the last 1-2 MiB)")
        return
    logs = session_logs(con)
    # The newest one is the running session's, open for writing: the previous one is the last finished session.
    if len(logs) < 2:
        sys.exit(f"no finished session log in {REMOTE_LOGS}/ (the running session's: lastlog --current)")
    args.remote, args.local = f"{REMOTE_LOGS}/{logs[-2]}", args.local or logs[-2]
    cmd_get(args)


def cmd_crashes(args) -> None:
    con = console(args)
    found = []
    for kind, size, name in con.ls(""):
        if kind == "f" and name in ("crash.log", "hang.log"):
            found.append((name, size))
    for kind, size, name in con.ls(REMOTE_LOGS):
        if kind == "f" and re.fullmatch(r"(crash|hang)-\d+\.log", name):
            found.append((f"{REMOTE_LOGS}/{name}", size))
    try:
        for kind, size, name in con.ls("/atmosphere/crash_reports"):
            if kind == "f":
                found.append((f"/atmosphere/crash_reports/{name}", size))
    except ServerError:
        pass  # no reports folder
    for path, size in found:
        print(f"{size if size is not None else '-':>10}  {path}")
    if args.fetch:
        target = Path(args.fetch)
        target.mkdir(parents=True, exist_ok=True)
        for path, _ in found:
            local = target / path.strip("/").replace("/", "_")
            if not local.exists():
                local.write_bytes(con.get_bytes(path))
                print(f"-> {local}")


def cmd_env(args) -> None:
    con = console(args)
    lines = con.read_env()
    updates: dict[str, str | None] = {}
    for item in args.set:
        key, sep, value = item.partition("=")
        if not sep or not key:
            sys.exit(f"not KEY=VALUE: {item}")
        updates[key.strip()] = value.strip()
    for key in args.unset:
        updates[key] = None
    if updates:
        lines = edit_env(lines, updates)
        con.put_bytes(("\n".join(lines) + "\n").encode(), "env.txt", quiet=True)
        print("env.txt updated (read at the next start)")
    print("\n".join(lines))
    if args.restart:
        con.restart(force=args.force)


def nro_path(arg: str | None) -> Path:
    if arg:
        return Path(arg)
    for candidate in NRO_CANDIDATES:
        if candidate.exists():
            return candidate
    sys.exit("no NRO built (tools/switch/build-switch.sh); pass its path")


def cmd_deploy(args) -> None:
    nro = nro_path(args.nro)
    if not nro.exists():
        sys.exit(f"{nro} does not exist")
    con = console(args)
    con.put_bytes(nro.read_bytes(), args.remote)
    if args.no_restart:
        return
    try:
        con.restart(force=args.force)
    except ServerError as e:
        sys.exit(f"uploaded, not restarted: {e}")
    show(con.text("info"))


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--host", default=default_host())
    p.add_argument("--port", type=int, default=int(os.environ.get(ENV_PORT, DEFAULT_PORT)))
    sub = p.add_subparsers(dest="cmd", required=True)

    simple = ("info", "ping", "help")
    for name in simple:
        sub.add_parser(name)
    with_rest = ("press", "hold", "release", "stick", "input")
    for name in with_rest:
        sub.add_parser(name).add_argument("rest", nargs="*")
    sub.add_parser("raw").add_argument("rest", nargs="+")
    sp = sub.add_parser("log")
    sp.add_argument("--all", action="store_true", help="start with the text the console kept")
    sp.add_argument("--save", help="also append the lines to this file")
    sp.add_argument("--grep", help="only lines matching this regular expression")
    sp.add_argument("--seconds", type=float, help="stop after this long")
    sub.add_parser("logtext").add_argument("out", nargs="?")
    sp = sub.add_parser("get")
    sp.add_argument("remote")
    sp.add_argument("local", nargs="?")
    sp = sub.add_parser("put")
    sp.add_argument("local")
    sp.add_argument("remote")
    for name in ("ls", "rm", "mkdir"):
        sub.add_parser(name).add_argument("remote", nargs="?" if name == "ls" else None, default="")
    sp = sub.add_parser("env")
    sp.add_argument("set", nargs="*", metavar="KEY=VALUE")
    sp.add_argument("--unset", action="append", default=[], metavar="KEY")
    sp.add_argument("--restart", action="store_true", help="restart the game afterwards")
    sp.add_argument("--force", action="store_true", help="restart also when the console looks like title takeover")
    sp = sub.add_parser("deploy")
    sp.add_argument("nro", nargs="?")
    sp.add_argument("--remote", default=REMOTE_NRO, help=f"where in the app folder (default {REMOTE_NRO})")
    sp.add_argument("--no-restart", action="store_true")
    sp.add_argument("--force", action="store_true", help="restart also when the console looks like title takeover")
    sp = sub.add_parser("shot")
    sp.add_argument("out", nargs="?")
    sp.add_argument("--ppm", action="store_true", help="the PPM as the game saved it (no PNG compression)")
    sub.add_parser("logs")
    sp = sub.add_parser("lastlog")
    sp.add_argument("local", nargs="?")
    sp.add_argument("--current", action="store_true", help="the running session's log (the text kept in memory)")
    sub.add_parser("crashes").add_argument("--fetch", metavar="DIR", help="download the files not yet in DIR")
    sp = sub.add_parser("restart")
    sp.add_argument("--force", action="store_true")
    sp.add_argument("--seconds", type=float, default=180)
    sub.add_parser("wait").add_argument("--seconds", type=float, default=120)

    args = p.parse_args()
    changing = {"put", "rm", "mkdir", "deploy", "restart", "press", "hold", "release", "stick", "input", "env"}
    try:
        con = console(args)
        if args.cmd in changing or (args.cmd == "raw" and args.rest[0] in changing):
            if args.cmd != "env" or args.set or args.unset or args.restart:
                con.check_game()
        if args.cmd in simple:
            show(con.text(args.cmd))
        elif args.cmd in with_rest:
            show(con.text(args.cmd, *args.rest, timeout=660.0))
        elif args.cmd == "raw":
            show(con.text(*args.rest, timeout=660.0))
        elif args.cmd in ("ls", "rm", "mkdir"):
            show(con.text(args.cmd, *([args.remote] if args.remote else [])))
        elif args.cmd == "put":
            con.put_bytes(Path(args.local).read_bytes(), args.remote)
        elif args.cmd == "logtext":
            data = con.command("logtext")
            if args.out:
                Path(args.out).write_bytes(data)
            else:
                sys.stdout.write(data.decode(errors="replace"))
        elif args.cmd == "logs":
            sizes = {name: size for _, size, name in con.ls(REMOTE_LOGS)}
            for name in session_logs(con):
                print(f"{sizes.get(name) or 0:>10}  {REMOTE_LOGS}/{name}")
        elif args.cmd == "restart":
            con.restart(force=args.force, seconds=args.seconds)
            show(con.text("info"))
        elif args.cmd == "wait":
            if not con.wait(args.seconds):
                sys.exit("no answer")
            print("ready")
        else:
            {"log": cmd_log, "get": cmd_get, "deploy": cmd_deploy, "shot": cmd_shot, "lastlog": cmd_lastlog,
             "crashes": cmd_crashes, "env": cmd_env}[args.cmd](args)
    except ServerError as e:
        sys.exit(f"console: {e}")
    except BrokenPipeError:  # (the output went to a pipe that closed: head, grep -m)
        os._exit(0)
    except (ConnectionError, socket.timeout) as e:
        sys.exit(f"connection: {e}")


if __name__ == "__main__":
    main()
