"""List the functions that must keep non-volatile registers in ctx (non_volatile_in_context).

Generate with an empty non_volatile_in_context first, then run this: it writes the list into
LostOdysseyRecompLib/config/LostOdysseyRecomp.toml, and generating again applies it. Requires Python 3.11+.

A function goes on the list when, on some path from its entry, it reads a non-volatile register or CTR before
writing it (compiler __finally blocks read their parent's registers; hand-written codecs pass values in r14-r31 and
v14-v127), when it calls such a function, when it calls setjmp, or when a host hook reads its registers.
Stores below the stack pointer (prologue saves of the caller's value) don't count as reads; anything else does,
because listing too many functions only costs a little speed, and missing one corrupts the game.
"""
import argparse
import bisect
import glob
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / "LostOdysseyRecompLib/config/LostOdysseyRecomp.toml"
PPC = ROOT / "LostOdysseyRecompLib/ppc"
# Return addresses host hooks test before reading the caller's non-volatile registers.
HOOK_CALL_SITES = {
    0x82AD616C: "gpu/cinematic_bars.cpp",
    0x8238AD64: "patches/battle_script_timer.cpp",
    0x8237D308: "debug/asset_failure.cpp",
    0x823CEC48: "patches/occlusion_history.cpp",
    0x823CEAD0: "patches/occlusion_history.cpp",
}
REG = r"(?:r(?:1[4-9]|2\d|3[01])|f(?:1[4-9]|2\d|3[01])|v(?:1[4-9]|2\d|3[01]|6[4-9]|[7-9]\d|1[01]\d|12[0-7])|ctr)"
TOKEN = re.compile(rf"(?<![\w.])({REG})(?=\.|\b)")
ASSIGN = re.compile(rf"^({REG})\.\w+(?:\[[^\]]*\])?\s*=(?!=)\s*(.*)$")
STORE = re.compile(rf"^(?:simde)?_mm\w*_store\w*\(\(?[^,]*?\b({REG})\.\w+[^,]*,\s*(.*)$")
STACK_SAVE = re.compile(r"PPC_STORE_U(32|64)\(ctx\.r1\.u32 \+ -\d+, (r|f)\d+\.\w+\);")
NOP = re.compile(r"(r\d+)\.u64 = \1\.u64;$")
LABEL = re.compile(r"^loc_([0-9A-F]+):")
GOTO = re.compile(r"goto loc_([0-9A-F]+);")
FUNCTION = re.compile(r"PPC_FUNC_IMPL\(__imp__(\w+)\) \{\n(.*?)\n\}\n", re.S)
CALL = re.compile(r"^\s*(sub_[0-9A-F]+)\(ctx, base\);", re.M)


def entry_live(body):
    blocks, labels = [], {}
    depth = 0
    current = {"use": set(), "def": set(), "succ": [], "fall": True}

    def close():
        nonlocal current
        blocks.append(current)
        current = {"use": set(), "def": set(), "succ": [], "fall": True}

    for line in body.split("\n"):
        s = line.strip()
        if not s or s.startswith("//") or s.endswith("{};"):
            continue
        if s.startswith("if (") and s.endswith("{"):
            depth += 1
        elif s == "}":
            depth = max(0, depth - 1)
        if m := LABEL.match(s):
            close()
            labels[m.group(1)] = len(blocks)
            continue
        if STACK_SAVE.match(s) or NOP.match(s) or ("ctx.r1.u32" in s and "_mm_store" in s):
            continue
        a = ASSIGN.match(s)
        st = None if a else STORE.match(s)
        uses = TOKEN.findall((a or st).group(2)) if (a or st) else TOKEN.findall(s)
        for use in uses:
            if use not in current["def"]:
                current["use"].add(use)
        if a or st:
            current["def"].add((a or st).group(1))
        if g := GOTO.search(s):
            current["succ"].append(g.group(1))
            if s.startswith("goto"):
                current["fall"] = False
            close()
        elif s == "return;":
            # A conditional tail call: "if (c) {", its call, "return;", "}" must not end the path after the "}".
            current["fall"] = depth > 0
            close()
        elif s.startswith("if (") and s.endswith("return;"):
            close()
    close()
    succ = []
    for i, block in enumerate(blocks):
        targets = [labels[t] for t in block["succ"] if t in labels]
        if block["fall"] and i + 1 < len(blocks):
            targets.append(i + 1)
        succ.append(targets)
    live = [set() for _ in blocks]
    changed = True
    while changed:
        changed = False
        for i in range(len(blocks) - 1, -1, -1):
            out = set().union(*(live[j] for j in succ[i])) if succ[i] else set()
            value = blocks[i]["use"] | (out - blocks[i]["def"])
            if value != live[i]:
                live[i] = value
                changed = True
    return live[0] if live else set()


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--extra-site", action="append", default=[], help="another hook return address (hex)")
    args = parser.parse_args()
    bodies = {}
    for path in sorted(glob.glob(str(PPC / "ppc_recomp.*.cpp"))):
        for m in FUNCTION.finditer(Path(path).read_text(encoding="utf-8")):
            if not m.group(1).startswith("__"):
                bodies[m.group(1)] = m.group(2)
    sharing = {name for name, body in bodies.items() if entry_live(body)}
    callers = {name for name, body in bodies.items() if sharing.intersection(CALL.findall(body))}
    jumps = {name for name, body in bodies.items() if "setjmp(" in body}
    starts = sorted(int(name[4:], 16) for name in bodies if name.startswith("sub_"))
    sites = dict(HOOK_CALL_SITES)
    sites.update({int(a, 16): "--extra-site" for a in args.extra_site})
    hooked = {f"sub_{starts[bisect.bisect_right(starts, a) - 1]:X}" for a in sites}
    chosen = sharing | callers | jumps | hooked
    print(f"{len(bodies)} functions: {len(sharing)} share registers, {len(callers)} call those, "
          f"{len(jumps)} setjmp, {len(hooked)} hook callers: {len(chosen)} keep ctx")
    addresses = sorted(int(name[4:], 16) for name in chosen if name.startswith("sub_"))
    block = "non_volatile_in_context = [\n" + "".join(
        "    " + ", ".join(f"0x{a:08X}" for a in addresses[i:i + 8]) + ",\n" for i in range(0, len(addresses), 8)) + "]\n"
    text = CONFIG.read_text(encoding="utf-8")
    text, count = re.subn(r"non_volatile_in_context = \[.*?\]\n", block, text, flags=re.S)
    if not count:
        sys.exit("non_volatile_in_context not found in the config")
    CONFIG.write_text(text, encoding="utf-8")


if __name__ == "__main__":
    main()
