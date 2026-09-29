"""Derive bounded SDK tail metadata without exporting the generated program.

Read only the two canonical ordinary draw entries. Emit program points,
argument-register mappings and dirty-mask register ownership, not source
bodies or assets. No build or game is launched by this analyzer.
"""
from pathlib import Path
import argparse
import json
import re

ENTRIES = {'823C6860': 12, '827B56B0': 74}


def inspect(root):
    result = {}
    for address, unit in ENTRIES.items():
        text = (root / f'LostOdysseyRecompLib/ppc/ppc_recomp.{unit}.cpp').read_text(encoding='utf-8')
        match = re.search(r'PPC_FUNC_IMPL\(__imp__sub_' + address + r'\) \{(.*?)\n\}', text, re.S)
        if not match:
            raise ValueError('Missing canonical entry ' + address)
        pc = int(address, 16)
        instructions = []
        for line in match[1].splitlines():
            label = re.fullmatch(r'loc_([0-9A-F]+):', line)
            if label:
                pc = int(label[1], 16)
            asm = re.fullmatch(r'\s*// ([a-z.][a-z0-9.]*)\s*(.*)', line)
            if not asm:
                continue
            op, args = asm[1], asm[2].strip()
            instructions.append((pc, op, args))
            pc += 4
        calls = []
        for i, (pc, op, args) in enumerate(instructions):
            if op != 'bl' or args.lower() not in {'0x823c6e18', '0x823c2200', '0x823c1bd8'}:
                continue
            before = []
            for at, kind, value in instructions[max(0, i - 6):i]:
                if kind in {'mr', 'ld', 'lwz', 'li', 'lis', 'addi', 'rldicr', 'clrldi', 'rlwinm'}:
                    before.append({'pc': f'{at:08X}', 'operation': kind, 'operands': value})
            after = [{'pc': f'{at:08X}', 'operation': kind, 'operands': value}
                     for at, kind, value in instructions[i + 1:i + 5]]
            calls.append({'pc': f'{pc:08X}', 'callee': args,
                          'argument_setup': before, 'continuation': after})
        captures = [{'pc': f'{pc:08X}', 'saved': args.split(',')[0], 'input': args.split(',')[1]}
                    for pc, op, args in instructions[:55]
                    if op == 'mr' and re.fullmatch(r'r(?:1[4-9]|2[0-9]|3[01]),r(?:[3-9]|10)', args)]
        masks = [{'pc': f'{pc:08X}', 'register': args.split(',')[0],
                  'offset': int(re.search(r',(-?\d+)\(', args)[1])}
                 for pc, op, args in instructions
                 if op == 'ld' and re.fullmatch(r'r\d+,(?:0|8|16|24|32|40)\(r31\)', args)]
        epilogues = [{'pc': f'{pc:08X}', 'frame_bytes': int(args.rsplit(',', 1)[1])}
                     for pc, op, args in instructions
                     if op == 'addi' and re.fullmatch(r'r1,r1,\d+', args)]
        result[address] = {'unit': unit, 'instruction_count': len(instructions),
                           'argument_captures': captures, 'dirty_loads': masks,
                           'preparation_and_state_boundaries': calls, 'epilogues': epilogues}
    return {'scope': 'derived_metadata_only_not_generated_program_or_runtime_validation',
            'entries': result}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path.cwd())
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(inspect(args.root), indent=2) + '\n', encoding='utf-8')
