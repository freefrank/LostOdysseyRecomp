"""Run the actual guest-I/O regressions with a process deadline and hang stacks."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys


def capture_stacks(pid, output):
    gdb = shutil.which("gdb")
    lldb = shutil.which("lldb")
    if not lldb and os.name == "nt":
        candidate = Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "LLVM/bin/lldb.exe"
        if candidate.is_file():
            lldb = str(candidate)
    if gdb:
        command = [gdb, "-q", "-nx", "-batch", "-p", str(pid),
                   "-ex", "set pagination off", "-ex", "thread apply all bt full", "-ex", "detach"]
    elif lldb:
        command = [lldb, "--batch", "-p", str(pid), "-o", "thread backtrace all", "-o", "detach"]
    else:
        output.write_text("No gdb/lldb available to capture the timed-out process.\n", encoding="utf-8")
        return
    with output.open("w", encoding="utf-8") as log:
        try:
            subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, timeout=15, check=False)
        except (OSError, subprocess.TimeoutExpired) as error:
            log.write(f"\nDebugger failed: {error}\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--out", required=True, type=Path, help="new evidence directory")
    parser.add_argument("--mode", choices=("all", "io-lifetime", "io-invalid-handle", "io-diagnostics"), default="all")
    parser.add_argument("--diagnostics", action="store_true", help="also enable recording for the selected mode")
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    output = args.out.resolve()
    output.mkdir(parents=True, exist_ok=False)
    results = []
    modes = ("io-lifetime", "io-invalid-handle", "io-diagnostics") if args.mode == "all" else (args.mode,)
    for mode in modes:
        # Each child has its own process-initialized diagnostic switch and data.
        env = dict(os.environ)
        env.pop("LO_IO_DIAGNOSTICS", None)
        diagnostics = mode == "io-diagnostics" or args.diagnostics
        if diagnostics:
            env["LO_IO_DIAGNOSTICS"] = "1"
        command = [str(executable), mode, str(output / mode)]
        result = {"mode": mode, "command": command, "diagnostics": diagnostics}
        print("RUN", subprocess.list2cmdline(command), flush=True)
        with (output / f"{mode}.log").open("wb") as log:
            process = subprocess.Popen(command, env=env, cwd=output, stdout=log, stderr=subprocess.STDOUT)
            try:
                result["returncode"] = process.wait(timeout=30)
                result["status"] = "PASS" if result["returncode"] == 0 else "FAIL"
            except subprocess.TimeoutExpired:
                result["status"] = "TIMEOUT"
                capture_stacks(process.pid, output / f"{mode}.stacks.txt")
                process.kill()
                result["returncode"] = process.wait(timeout=5)
        results.append(result)
        print(mode, result["status"], result["returncode"], flush=True)
        (output / "results.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
    return 0 if all(result["status"] == "PASS" for result in results) else 1


if __name__ == "__main__":
    sys.exit(main())
