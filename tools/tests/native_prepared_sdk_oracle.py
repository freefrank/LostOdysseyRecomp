"""Generate a local-only continuation oracle, without exporting guest source.

Actual SDK draw prefixes/tails and dirty writers are canonical. Shader and
complex derived-state selection are opaque effect models, NOT validated here.
"""
from pathlib import Path
import argparse
import re
from native_mesh_sdk_oracle import generate

PRELUDE = r'''
#include "ppc/ppc_recomp_shared.h"
#include <cstdint>
#include <cstring>
#include <cstdlib>
extern bool NativePreparedIndexed(PPCRegister&, PPCRegister&, PPCRegister&, PPCRegister&, PPCRegister&, PPCRegister&, PPCRegister&, PPCRegister&);
extern bool NativePreparedAuto(PPCRegister&, PPCRegister&, PPCRegister&, PPCRegister&, PPCRegister&, PPCRegister&, PPCRegister&, PPCRegister&);
uint32_t oracle_preparation_model = 0;
uint32_t oracle_preparation_calls = 0;
static uint32_t SwapWord(uint32_t v) {
    return (v >> 24) | ((v >> 8) & 0xff00u) | ((v << 8) & 0xff0000u) | (v << 24);
}
static uint32_t ReadWord(const uint8_t* base, uint32_t at) {
    uint32_t value; std::memcpy(&value, base + at, 4); return SwapWord(value);
}
static void WriteWord(uint8_t* base, uint32_t at, uint32_t value) {
    value = SwapWord(value); std::memcpy(base + at, &value, 4);
}
static void PreparationEffect(PPCContext& ctx, uint8_t* base, bool derived) {
    if (!oracle_preparation_model) std::abort();
    ++oracle_preparation_calls;
    const uint32_t device = ctx.r3.u32;
    const auto cursor = ReadWord(base, device + 48);
    // Observable ordered prefix marker outside the dirty groups consumed later.
    WriteWord(base, cursor + 4, derived ? 0x2186u : 0x2185u);
    WriteWord(base, cursor + 8, derived ? 0xdecaf002u : 0xdecaf001u);
    WriteWord(base, device + 48, cursor + 8);
    // Already-consumed ALU banks become dirty for the NEXT call. The remaining
    // original tail must not clear these flags; neither may the native tail.
    WriteWord(base, device, 0x80000000u); WriteWord(base, device + 4, 0);
    WriteWord(base, device + 8, 0x40000000u); WriteWord(base, device + 12, 0);
    auto remaining = ctx.r4.u64;
    if (derived) remaining &= ~(uint64_t(1) << 2);
    else if (oracle_preparation_model != 2) remaining &= ~(uint64_t(15) << 17);
    ctx.r3.u64 = remaining;
}
'''


def build(root: Path, output: Path) -> None:
    generate(root, output)
    text = output.read_text(encoding='utf-8')
    for address, derived in [('823C6E18', 'false'), ('823C2200', 'true')]:
        pattern = r'PPC_FUNC\(sub_' + address + r'\) \{[^\n]+\}'
        text, count = re.subn(pattern,
            f'PPC_FUNC(sub_{address}) {{ PreparationEffect(ctx, base, {derived}); }}', text)
        if count != 1:
            raise ValueError('Expected one opaque helper stub: ' + address)
    for name in ('NativePreparedIndexed', 'NativePreparedAuto'):
        if f'if ({name}(' not in text:
            raise ValueError('Regenerate PPC with the prepared-tail configuration: ' + name)
    output.write_text(PRELUDE + text, encoding='utf-8')
    print('Canonical SDK prefix/tail selected; shader/derived helpers are explicit opaque effect models.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path.cwd())
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    build(args.root.resolve(), args.output.resolve())
