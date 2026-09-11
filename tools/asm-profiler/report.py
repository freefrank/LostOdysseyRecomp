"""Offline instruction hotspot report for LoAsmProfiler captures."""
import argparse
from collections import Counter
import hashlib
import html
import json
from pathlib import Path
import re
from functools import lru_cache


@lru_cache(maxsize=16)
def source_lines(path):
    return path.read_text(encoding="utf-8").splitlines()


def number(value):
    return int(value, 0) if isinstance(value, str) else int(value)


def guest_context(source, line, root):
    """Show emitted PPC comment, never infer an exact guest PC from line counts."""
    if not root or not source or not line:
        return ""
    name = source.replace("\\", "/").rsplit("/", 1)[-1]
    if not re.fullmatch(r"ppc_recomp\.[\w.-]+\.cpp", name):
        return ""
    path = root / name
    if not path.is_file():
        return ""
    lines = source_lines(path)
    index = int(line) - 1
    if not 0 <= index < len(lines):
        return ""
    for text in reversed(lines[max(0, index - 24):index + 1]):
        if "PPC_FUNC_IMPL" in text or text.strip() == "}":
            break
        if text.strip().startswith("// "):
            return text.strip()[3:] + " (source-line context; guest PC unverified)"
    return ""


def analyze(capture, decoder, tid=None, source_root=None):
    if capture.get("schema_version") != 1:
        raise ValueError("unsupported capture schema_version")
    samples = capture["samples"]
    if not isinstance(samples, list):
        raise ValueError("samples must be an array")
    selected = [s for s in samples if tid is None or number(s["tid"]) == tid]
    counts = Counter()
    representatives = {}
    functions = Counter()
    threads = Counter()
    for sample in selected:
        ip = number(sample["ip"])
        base = number(sample.get("module_base", 0))
        module = sample.get("module", "") or "<unresolved>"
        # Retain distinct code bytes, including changed/unreadable code snapshots.
        key = (module, base, ip, sample.get("bytes", ""))
        counts[key] += 1
        representatives[key] = sample
        functions[(module, sample.get("symbol") or "<unresolved>")] += 1
        threads[number(sample["tid"])] += 1
    rows = []
    for key, count in counts.most_common():
        sample = representatives[key]
        module, base, ip, code = key
        raw = bytes.fromhex(code)
        instructions = list(decoder.disasm(raw, ip, count=6))
        rows.append(dict(count=count, percent=100 * count / len(selected),
                         tid_note="all selected threads", module=module,
                         ip=f"0x{ip:x}", rva=f"0x{ip-base:x}" if base else "unknown",
                         symbol=sample.get("symbol") or "<unresolved>",
                         displacement=sample.get("displacement", 0),
                         source=sample.get("source", ""), line=sample.get("line", 0),
                         guest_context=guest_context(sample.get("source", ""), sample.get("line", 0), source_root),
                         assembly=[f"0x{i.address:x}: {i.mnemonic} {i.op_str}".rstrip() for i in instructions],
                         bytes=code))
    return dict(schema_version=1, selected_samples=len(selected), total_samples=len(samples),
                selected_tid=tid, threads=dict(threads),
                functions=[dict(module=m, symbol=s, count=n, percent=100*n/len(selected))
                           for (m, s), n in functions.most_common()], hotspots=rows)


def render(result, capture, top):
    esc = lambda value: html.escape(str(value), quote=True)
    rows = []
    for row in result["hotspots"][:top]:
        assembly = "\n".join(row["assembly"]) or "<bytes unavailable or undecodable>"
        location = f'{row["source"]}:{row["line"]}' if row["source"] else "No source line"
        rows.append(f'<tr><td>{row["count"]}<br>{row["percent"]:.2f}%</td>'
                    f'<td>{esc(row["module"])}<br>{esc(row["symbol"])} + {esc(row["displacement"])}'
                    f'<br>RVA {esc(row["rva"])}<br>{esc(location)}<br>{esc(row["guest_context"])}</td>'
                    f'<td><pre>{esc(assembly)}</pre></td></tr>')
    function_rows = "".join(f'<tr><td>{r["count"]}</td><td>{r["percent"]:.2f}%</td>'
                            f'<td>{esc(r["module"])} — {esc(r["symbol"])}</td></tr>'
                            for r in result["functions"][:top])
    metadata = {k:v for k,v in capture.items() if k != "samples"}
    cpu = {number(t["tid"]): t.get("observed_cpu_seconds", "unknown")
           for t in capture.get("thread_cpu_times", [])}
    thread_rows = "".join(f'<tr><td>{esc(tid)}</td><td>{count}</td><td>{esc(cpu.get(number(tid), "unknown"))}</td></tr>'
                          for tid, count in sorted(result["threads"].items(), key=lambda item: item[1], reverse=True))
    return f'''<!doctype html><html lang="en"><meta charset="utf-8">
<title>Lost Odyssey assembly profile</title>
<style>body{{font:15px system-ui;margin:32px;background:#101820;color:#e4edf5}}table{{border-collapse:collapse;width:100%}}td,th{{padding:12px;text-align:left;border-bottom:1px solid #384653;vertical-align:top}}pre{{white-space:pre-wrap;overflow-wrap:anywhere}}input{{padding:10px;width:50%;margin:16px 0}}small{{color:#abc}}</style>
<h1>Lost Odyssey assembly profile</h1>
<p>{result["selected_samples"]} selected / {result["total_samples"]} captured samples. Thread filter: {esc(result["selected_tid"])}</p>
<p>Wall-clock thread snapshots, including sleeping/waiting threads. Percentages are sample shares, not CPU utilization, instruction latency, cycles, or recoverable frame time. Suspension perturbs execution. No call-stack/inclusive attribution or GPU instruction timing.</p>
<p>The first disassembled instruction is the sampled RIP. Following instructions are context only. Optimized PDB source lines and PPC comments are approximate context, not exact guest instruction timing. Unresolved samples remain in the denominator.</p>
<details><summary>Capture identity and counters</summary><pre>{esc(json.dumps(metadata, indent=2, ensure_ascii=False))}</pre></details>
<h2>Threads</h2><p>CPU seconds are OS thread-time deltas between first and last observations; they do not attribute CPU time to individual RIP samples. Use --tid to regenerate a report for a selected thread.</p>
<table><tr><th>Thread ID</th><th>Selected samples</th><th>Observed CPU seconds</th></tr>{thread_rows}</table>
<h2>Functions (self samples)</h2><table><tr><th>Samples</th><th>Share</th><th>Function</th></tr>{function_rows}</table>
<h2>Instruction hotspots (top {top})</h2><input id="search" placeholder="Filter module, symbol, assembly or source">
<table id="hotspots"><thead><tr><th>Samples / share</th><th>Location</th><th>x64 assembly (Intel)</th></tr></thead><tbody>{''.join(rows)}</tbody></table>
<script>document.getElementById('search').addEventListener('input',e=>{{const q=e.target.value.toLowerCase();document.querySelectorAll('#hotspots tbody tr').forEach(r=>r.hidden=!r.textContent.toLowerCase().includes(q));}});</script></html>'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("--output", type=Path, required=True, help="HTML output; JSON summary written beside it")
    parser.add_argument("--tid", type=int)
    parser.add_argument("--top", type=int, default=200)
    parser.add_argument("--source-root", type=Path, help="Matching build's generated ppc directory (optional)")
    args = parser.parse_args()
    if args.top <= 0:
        parser.error("--top must be positive")
    try:
        from capstone import Cs, CS_ARCH_X86, CS_MODE_64
        raw = args.capture.read_bytes()
        capture = json.loads(raw)
        result = analyze(capture, Cs(CS_ARCH_X86, CS_MODE_64), args.tid, args.source_root)
        result["capture_sha256"] = hashlib.sha256(raw).hexdigest()
        result["source_context_verified"] = False
        summary = args.output.with_suffix(".json")
        if args.capture.resolve() in (args.output.resolve(), summary.resolve()) or summary == args.output:
            raise ValueError("output HTML and summary JSON must be distinct from capture")
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(render(result, capture, args.top), encoding="utf-8")
        summary.write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
        print(f"{result['selected_samples']} samples; report: {args.output}")
    except (OSError, ValueError, KeyError, TypeError, ImportError) as exc:
        parser.exit(1, f"error: {exc}\n")


if __name__ == "__main__":
    main()
