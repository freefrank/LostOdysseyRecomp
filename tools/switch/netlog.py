#!/usr/bin/env python3
"""Receive the Switch build's live log (LO_NETLOG=1 in env.txt, see docs/SWITCH.md).

    python tools/switch/netlog.py 192.168.1.87 [--port 28780] [--out netlog.txt] [--grep TEXT]

The console listens and streams the runtime log and stderr, starting with the
newest megabyte. This keeps reconnecting, so it can run before the game starts
and across restarts. Everything is appended to --out; lines that contain
--grep (or all lines without it) are also printed.
"""
import argparse
import socket
import sys
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("host", help="the console's IP address")
    parser.add_argument("--port", type=int, default=28780)
    parser.add_argument("--out", default="netlog.txt")
    parser.add_argument("--grep", action="append", default=[], help="print only lines containing this (repeatable)")
    args = parser.parse_args()

    with open(args.out, "ab") as out:
        while True:
            try:
                with socket.create_connection((args.host, args.port), timeout=5) as conn:
                    conn.settimeout(None)
                    banner = f"--- connected {time.strftime('%Y-%m-%d %H:%M:%S')} ---\n".encode()
                    out.write(banner)
                    out.flush()
                    pending = b""
                    while True:
                        data = conn.recv(65536)
                        if not data:
                            break
                        out.write(data)
                        out.flush()
                        pending += data
                        *lines, pending = pending.split(b"\n")
                        for line in lines:
                            text = line.decode("utf-8", "replace")
                            if not args.grep or any(g in text for g in args.grep):
                                print(text, flush=True)
            except OSError:
                pass
            time.sleep(2)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        sys.exit(0)
