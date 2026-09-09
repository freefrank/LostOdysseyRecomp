"""Validate native shader-log fixture output; no GPU or game process."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import zipfile


def records(path):
    data = path.read_bytes()
    assert data.endswith(b"\n"), path
    result = [json.loads(line) for line in data.decode("utf-8").splitlines()]
    assert [e["sequence"] for e in result] == list(range(1, len(result) + 1)), path
    assert all(e["schema"] == 1 and e["session"] == "4242" for e in result)
    assert [e["elapsed_seconds"] for e in result] == sorted(e["elapsed_seconds"] for e in result)
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", required=True, type=Path)
    parser.add_argument("--dxc", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--verify-only", action="store_true", help="inspect retained outputs without repeating native checks")
    parser.add_argument("--custom-output", type=Path, help="use a separately rerun custom-path case")
    args = parser.parse_args()
    root = args.output.resolve()
    if args.verify_only:
        assert root.is_dir(), "verification requires retained outputs"
    else:
        assert not root.exists(), "use a new evidence directory"
        root.mkdir(parents=True)
    env = os.environ.copy()
    env.pop("LO_SHADER_LOG_FILE", None)
    env["LO_DXC_PATH"] = str(args.dxc.resolve())
    outputs = {}
    for mode in (() if args.verify_only else ("normal", "return", "quick-exit", "disabled", "alias", "custom")):
        directory = root / mode
        child_env = env.copy()
        if mode == "disabled":
            child_env["LO_SHADER_LOG_FILE"] = "0"
        elif mode == "alias":
            child_env["LO_SHADER_LOG_FILE"] = str(directory / "logs" / "runtime-4242.log")
        elif mode == "custom":
            child_env["LO_SHADER_LOG_FILE"] = str(directory / "custom" / "诊断.jsonl")
        command = [str(args.exe.resolve()), str(directory)]
        if mode != "normal":
            command.append(mode)
        run = subprocess.run(command, env=child_env, capture_output=True, timeout=60, creationflags=0x08000000)
        (root / f"{mode}.stdout.txt").write_bytes(run.stdout)
        (root / f"{mode}.stderr.txt").write_bytes(run.stderr)
        assert run.returncode == 0, (mode, run.returncode, run.stderr.decode("utf-8", errors="replace"))
        outputs[mode] = {"returncode": run.returncode}

    normal = root / "normal"
    events = records(normal / "snapshot.jsonl")
    threaded = [e for e in events if e["category"] == "thread-record"]
    assert len(threaded) == 1024
    assert len({e["detail"] for e in threaded}) == 1024
    for snapshot in normal.glob("concurrent-*.jsonl"):
        prefix = records(snapshot)
        assert events[:len(prefix)] == prefix
    identities = [e for e in events if e["category"] == "identity"]
    assert {e["hash_namespace"] for e in identities} == {"renderer-byte-fnv1a64", "command-processor-word-fnv1a64"}
    assert 'escaped="\\\r\n\t中文' in identities[0]["detail"]
    malformed = next(e for e in events if e["category"] == "malformed-utf8")
    assert malformed["detail"].endswith("\u00ff\u00c0\u00af\u00ed\u00a0\u0080")
    failed = next(e for e in events if e["category"] == "compile-rejected")
    assert failed["hash_namespace"] == "hlsl-source-sha256"
    source = b"float4 main() : SV_Target { this_is_not_hlsl }"
    assert f"source={hashlib.sha256(source).hexdigest()}" in failed["detail"]
    assert "error:" in failed["detail"] and "this_is_not_hlsl" in failed["detail"]
    assert any(e["category"] == "compile-success" for e in events)
    runtime = (normal / "logs/runtime-4242.log").read_text(encoding="utf-8")
    timestamp = re.search(r"\[\s*([\d.]+) t[0-9a-f]+\].*shader event " + str(failed["sequence"]), runtime)
    assert timestamp and abs(float(timestamp.group(1)) - failed["elapsed_seconds"]) < 0.01
    assert "last diagnostic" not in runtime and "complete fallback diagnostic" in runtime
    archive = next((normal / "captures").glob("render-shader-log.zip"))
    with zipfile.ZipFile(archive) as z:
        assert z.testzip() is None
        assert z.read("shader.jsonl") == (normal / "snapshot.jsonl").read_bytes()
        assert b"status=included" in z.read("shader-log-status.txt")
        assert "runtime.log" in z.namelist()
    with zipfile.ZipFile(normal / "captures/render-no-shader-log.zip") as z:
        assert z.testzip() is None
        assert "shader.jsonl" not in z.namelist()
        assert b"status=unavailable" in z.read("shader-log-status.txt")
    for mode in ("return", "quick-exit"):
        exit_events = records(root / mode / "logs/shader-4242.jsonl")
        assert exit_events[-1]["category"] == "exit-flush"
    custom = records(args.custom_output if args.custom_output else root / "custom/custom/诊断.jsonl")
    assert custom[-1]["category"] == "exit-flush"
    for mode in ("disabled", "alias"):
        assert not list((root / mode).rglob("*.jsonl"))
        contents = (root / mode / "logs/runtime-4242.log").read_text(encoding="utf-8")
        assert "last buffered record" in contents and '"schema":' not in contents
    assert "aliases runtime" in (root / "alias/logs/runtime-4242.log").read_text(encoding="utf-8")
    result = {"passed": True, "events": len(events), "concurrent_events": len(threaded),
              "configured_dxcompiler_sha256": hashlib.sha256(args.dxc.read_bytes()).hexdigest(), "modes": outputs,
              "verify_only": args.verify_only, "custom_output": str(args.custom_output) if args.custom_output else None}
    (root / "result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result))


if __name__ == "__main__":
    main()
