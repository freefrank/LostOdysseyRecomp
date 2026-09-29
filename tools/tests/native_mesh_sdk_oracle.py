"""Build a bounded SDK oracle from canonical generated sources, without exporting them.

Only the two ordinary draw entries and the dirty writers, including stream coherency are selected.
Save/restore helper shims preserve their nonvolatile registers; unsupported SDK
allocation/shader/recording helpers fail immediately rather than pretending to run.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import re

SELECTED = {"823C6860", "827B56B0", "823C1BD8", "823C6CB8", "823C78E0", "823C7A40"}
ENTRIES = {"823C6860", "827B56B0"}


def generate(root: Path, output: Path) -> None:
    bodies: dict[str, str] = {}
    for number in (12, 74):
        source = root / f"LostOdysseyRecompLib/ppc/ppc_recomp.{number}.cpp"
        text = source.read_text(encoding="utf-8")
        for match in re.finditer(r"PPC_FUNC_IMPL\(__imp__sub_([0-9A-Fa-f]{8})\) \{.*?\n\}", text, re.S):
            if match[1].upper() in SELECTED:
                bodies[match[1].upper()] = match[0]
    if bodies.keys() != SELECTED:
        raise RuntimeError(f"Missing canonical SDK entries: {SELECTED - bodies.keys()}")
    calls = {name for body in bodies.values()
             for name in re.findall(r"\b([A-Za-z_]\w*)\(ctx, base\)", body)}
    source = ['#include "ppc/ppc_recomp_shared.h"', '#include <array>', '#include <vector>',
              '#include <cstdio>', '#include <cstdlib>',
              'struct SavedFrame { unsigned first; uint64_t link; std::array<uint64_t,32> registers{}; };',
              'static std::vector<SavedFrame> frames;',
              'static void Save(PPCContext& ctx, unsigned first) {',
              '  SavedFrame frame; frame.first=first; frame.link=ctx.r12.u64;']
    source += [f'  if(first<={i}) frame.registers[{i}]=ctx.r{i}.u64;' for i in range(14,32)]
    source += ['  frames.push_back(frame);', '}',
               'static void Restore(PPCContext& ctx, unsigned first) {',
               '  if(frames.empty() || frames.back().first!=first) std::abort();',
               '  const auto frame=frames.back(); frames.pop_back(); ctx.lr=frame.link;']
    source += [f'  if(first<={i}) ctx.r{i}.u64=frame.registers[{i}];' for i in range(14,32)]
    source += ['}', 'uint64_t oracle_sdk_helper_calls=0;']
    for name in sorted(calls):
        if re.fullmatch(r"__savegprlr_\d+", name):
            first = int(name.rsplit('_',1)[1])
            source.append(f'PPC_FUNC({name}) {{ Save(ctx,{first}); }}')
        elif re.fullmatch(r"__restgprlr_\d+", name):
            first = int(name.rsplit('_',1)[1])
            source.append(f'PPC_FUNC({name}) {{ Restore(ctx,{first}); }}')
        elif re.fullmatch(r"sub_[0-9A-Fa-f]{8}", name):
            address = name[4:].upper()
            if address in SELECTED:
                source.append(f'extern "C" PPC_FUNC(__imp__sub_{address});')
                if address not in ENTRIES:
                    source.append(f'PPC_FUNC(sub_{address}) {{ ++oracle_sdk_helper_calls; __imp__sub_{address}(ctx,base); }}')
            else:
                source.append(f'PPC_FUNC({name}) {{ std::fputs("Unexpected SDK helper {name}\\n", stderr); std::abort(); }}')
        else:
            raise RuntimeError(f"Unmodeled generated helper call {name}")
    source += ['bool NativeAutoFan(PPCRegister&,PPCRegister&,PPCRegister&,PPCRegister&,PPCRegister&) { return false; }']
    source += [bodies[address] for address in sorted(bodies)]
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text('\n'.join(source)+'\n',encoding='utf-8')
    print('Generated bounded SDK draw/dirty-writer oracle; no generated program exported.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path.cwd())
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    generate(args.root.resolve(),args.output.resolve())
