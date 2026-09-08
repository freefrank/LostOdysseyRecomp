#!/usr/bin/env python3
"""Run the Windows production crash-handler fixture in isolated child processes."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import time


def run(exe: Path, root: Path) -> list[dict]:
    results = []
    cases = [
        "av-main", "av-main-locked", "av-worker-locked", "av-no-stderr", "av-full-stderr-pipe",
        "av-no-log", "av-no-sinks", "av-bad-context", "av-dump",
        "terminate", "uncaught", "abort", "terminate-worker-locked", "abort-worker-locked",
    ]
    for mode in cases:
        directory = root / (mode + "-日志-\u00b4-\u2032")
        directory.mkdir(parents=True, exist_ok=False)
        log = directory / "运行.log"
        no_log = mode in {"av-no-log", "av-no-sinks"}
        if not no_log:
            log.write_bytes(b"retained-prefix\n")
        env = os.environ.copy()
        env.pop("LO_CRASH_DUMP", None)
        if mode == "av-dump":
            env["LO_CRASH_DUMP"] = "r99,r3+bogus,FFFFFFFF*,DEADBEEF*,1000*,2000,r3+0x10"
        started = time.monotonic()
        # File redirection avoids a full pipe obscuring the independent FILE-lock test.
        with (directory / "stderr.log").open("wb") as stderr:
            child = subprocess.run([str(exe), mode, "-" if no_log else str(log)],
                                   stdout=subprocess.DEVNULL, stderr=stderr, env=env,
                                   creationflags=subprocess.CREATE_NO_WINDOW, timeout=30)
        stderr = (directory / "stderr.log").read_bytes().decode("utf-8", errors="strict")
        runtime = "" if no_log else log.read_bytes().decode("utf-8", errors="strict")
        report = stderr if no_log else runtime
        code = child.returncode & 0xFFFFFFFF
        # An uncaught main-thread MSVC C++ exception reaches the SEH filter
        # before terminate. MSVC's terminate handler is thread-local, so the
        # new worker's default terminate calls abort and reaches SIGABRT.
        expected = 0xE06D7363 if mode == "uncaught" else (
            0xE0000001 if mode == "terminate" else (
                0xE0000002 if mode in {"abort", "abort-worker-locked", "terminate-worker-locked"} else 0xC0000005))
        assert code == expected, (mode, hex(code), hex(expected))
        if mode != "av-no-sinks":
            assert "[crash] essential report complete\n" in report, (mode, "missing essential record")
            assert "host RIP=0x" in report and " RSP=0x" in report, mode
            assert re.search(r"module_base=0x[0-9A-F]+ module_rva=0x[0-9A-F]+", report), mode
            assert "module_name=" in report, mode
            assert " thread=" in report and " version=" in report, mode
            if mode.startswith("av-"):
                assert "ACCESS_VIOLATION code=0xC0000005" in report and "access=write address=0x" in report, mode
            elif mode == "uncaught":
                assert "CPP_EXCEPTION code=0xE06D7363" in report, mode
            elif mode == "terminate":
                assert "std::terminate code=0xE0000001" in report, mode
            else:
                assert "SIGABRT code=0xE0000002" in report, mode
            if mode == "av-bad-context":
                assert "guest context unavailable on this thread" in report, mode
            else:
                assert "guest r1=11112222 r3=33334444" in report, mode
            if "worker" in mode:
                main_thread = re.search(r"main-thread=(\d+)", report)[1]
                crash_thread = re.search(r" host=0x[0-9A-F]+ thread=(\d+)", report)[1]
                assert main_thread != crash_thread, mode
            if not no_log:
                assert report.startswith("retained-prefix\n"), mode
            if mode not in {"av-no-stderr", "av-full-stderr-pipe"}:
                assert "[crash] essential report complete\n" in stderr, (mode, "raw stderr fallback")
            if "optional host symbols begin" in report:
                assert report.index("essential report complete") < report.index("optional host symbols begin"), mode
            if mode == "av-dump":
                assert "malformed address" in report and "guest pointer unreadable" in report, mode
                assert "[00001000] -> 00002000" in report and "fixture-readable-guest" in report, mode
        else:
            assert "[crash]" not in stderr and not log.exists(), mode
        results.append({"case": mode, "exit_code": f"0x{code:08X}", "seconds": round(time.monotonic() - started, 3),
                        "runtime_bytes": len(runtime.encode()), "stderr_bytes": len(stderr.encode()), "passed": True})
    return results


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--output", type=Path, help="New evidence directory; omitted uses a disposable temporary directory")
    args = parser.parse_args()
    if os.name != "nt":
        parser.error("This fixture requires Windows")
    exe = args.exe.resolve(strict=True)
    if args.output:
        root = args.output.resolve()
        root.mkdir(parents=True, exist_ok=False)
        results = run(exe, root)
        (root / "results.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
    else:
        with tempfile.TemporaryDirectory(prefix="lo-crash-capture-") as temporary:
            results = run(exe, Path(temporary))
    print(f"PASS: {len(results)} actual child-process crash capture cases")


if __name__ == "__main__":
    main()
