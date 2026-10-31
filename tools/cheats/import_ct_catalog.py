#!/usr/bin/env python3
"""Extract catalog facts only; never evaluate a Cheat Engine script.

The CT stays local. Unknown/discordant rows stay excluded unless an exact,
reviewed correction is listed below. --check never changes the output.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import xml.etree.ElementTree as E

SOURCE_SHA256 = 'f2ec19455cf5f313374874ea1b0611a8fd3ca1b44ae1129375d9c336d7dc2c6e'
REFERENCE_SHA256 = '50c3f4dbbb5005760a40f2354dcf202d7bfcdaac13ec27c50ca1e4d10e9f7572'
CHARACTERS = ['Kaim', 'Seth', 'Ming', 'Sarah', 'Jansen', 'Cooke', 'Mack', 'Sed', 'Tolten']
CHARACTER_STRIDE, DATA_SIZE = 0x37E4, 0x275C0
# Exact Mack character-relative (v1.4 offset, verified Xenia-table offset).
# These are explicit rows in WAZAAAAA's eeb96f9 (2024-07-15) table, not inferred
# gap/stride repairs. See docs/notes/cheats.md; --reference-table rechecks them.
MACK_SKILL_CORRECTIONS = {
    'Mental Stability 1': (0xFA8, 0xEA8),
    'Auto Shield': (0xFB8, 0xEB8),
    'Auto Barrier': (0xFC8, 0xEC8),
    'Auto HP Recovery': (0xFD8, 0xED8),
    'Auto MP Recovery': (0xFE8, 0xEE8),
    'Reduce Casting Time 1': (0xFF8, 0xEF8),
    'Reduce Casting Time 2': (0x1008, 0xF08),
    'MP Conservation': (0x1018, 0xF18),
    'Double Back': (0x1058, 0xF58),
    'Counter Up': (0x1068, 0xF68),
    'Angel Heart': (0x1098, 0xF98),
}
# Display only: address/name comparison always uses the original CT spelling.
SKILL_NAMES = {
    'Double Cast': 'Double-Cast', 'Cut DOwn': 'Cut Down',
    'Drain SHower': 'Drain Shower', 'Special Accessories': 'Special Accessory',
    'Element Reistance Up': 'Elements Resistance Up',
}


def description(entry):
    return entry.findtext('Description', '').strip('"')


def find(root, name):
    return next(e for e in root.findall('.//CheatEntry') if description(e) == name)


def offset(entry):
    address = entry.findtext('Address', '')
    match = re.fullmatch(r'\[LO_BASE\]\+([0-9A-Fa-f]+)', address)
    if not match:
        raise ValueError(f'Unsupported catalog address: {address}')
    return int(match[1], 16)


def fields(root, group):
    return [(offset(e), description(e)) for e in find(root, group).findall('./CheatEntries/CheatEntry')
            if e.findtext('Address')]


def choices(root, name):
    return [(int(k, 16), v.strip())
            for line in find(root, name).findtext('DropDownList', '').splitlines() if ':' in line
            for k, v in [line.split(':', 1)]]


def skill_name(value, name):
    name = name.lstrip('! ').replace('Composit Magic', 'Composite Magic').replace('Slot + ', 'Slot +')
    name = SKILL_NAMES.get(name, name)
    # Two explicit CT IDs/offsets, not evidence of two distinct retail skills.
    if name == 'Ultimate Hit':
        name += {0xD08: ' (ID 0x1C)', 0x1888: ' (ID 0xD4)'}[value]
    return name


def skill_tables(root, read_offset=offset, repair_mack=False):
    tables, repaired = [], []
    for n, name in enumerate(CHARACTERS):
        char = find(root, name)
        learn = next(e for e in char.findall('./CheatEntries/CheatEntry') if description(e) == 'Learned Skills')
        rows = []
        for entry in learn.findall('./CheatEntries/CheatEntry'):
            value, label = read_offset(entry) - n * CHARACTER_STRIDE, description(entry)
            if not 0 <= value < CHARACTER_STRIDE or n * CHARACTER_STRIDE + value >= DATA_SIZE:
                raise ValueError(f'Skill outside character data: {name} {label}')
            if entry.findtext('VariableType') != 'Byte':
                raise ValueError(f'Skill flag is not a byte: {name} {label}')
            if repair_mack and name == 'Mack' and label in MACK_SKILL_CORRECTIONS:
                wrong, verified = MACK_SKILL_CORRECTIONS[label]
                if value != wrong:
                    raise ValueError(f'Reviewed Mack correction input differs: {label}')
                repaired.append({'character': name, 'source_name': label, 'old_offset': f'0x{wrong:X}',
                                 'offset': f'0x{verified:X}', 'reason': 'verified reference-table correction'})
                value = verified
            rows.append((value, label))
        named = [(v, label) for v, label in rows if label != 'N/A' and '?' not in label]
        if len(named) != len(set(named)):
            raise ValueError(f'Duplicate skill row: {name}')
        tables.append(rows)
    if repair_mack and len(repaired) != len(MACK_SKILL_CORRECTIONS):
        raise ValueError('Reviewed Mack correction rows are missing')
    return tables, repaired


def learned_skills(root, repair_mack=False):
    tables, report = skill_tables(root, repair_mack=repair_mack)
    skills = []
    for value, name in tables[0]:
        unknown = name == 'N/A' or '?' in name or not name.strip()
        disagree = [CHARACTERS[i] for i, table in enumerate(tables) if (value, name) not in table]
        if unknown or disagree:
            report.append({'offset': f'0x{value:X}', 'source_name': name,
                           'reason': 'unknown name' if unknown else 'character address/name disagreement',
                           'disagreeing_characters': disagree})
        else:
            skills.append((value, skill_name(value, name)))
    if len(skills) != len({value for value, _ in skills}):
        raise ValueError('Duplicate canonical skill offset')
    if len(skills) != len({name for _, name in skills}):
        raise ValueError('Ambiguous skill display name')
    return skills, report


def verify_reference(root, reference):
    base = int(find(reference, 'Gold').findtext('Address'), 16) - 0x44
    read_reference = lambda e: int(e.findtext('Address'), 16) - base
    reviewed, _ = skill_tables(root, repair_mack=True)
    original, _ = skill_tables(reference, read_offset=read_reference)
    verified = 0
    for n, name in enumerate(CHARACTERS):
        char = find(reference, name)
        # Independently anchor each character's layout, not just the Gold base.
        for stem, expected in [('HP(Out of Battle)',0x16C),('MP(Out of Battle)',0x188),('EXP (Out of 100)',0x84)]:
            entry = next(e for e in char.findall('./CheatEntries/CheatEntry') if stem in description(e))
            if read_reference(entry) != expected + n * CHARACTER_STRIDE:
                raise ValueError(f'Reference character layout differs: {name} {stem}')
        for value, label in reviewed[0]:
            if label == 'N/A' or '?' in label or not label.strip():
                continue
            if not all((value, label) in table for table in reviewed):
                continue
            if (value, label) not in original[n]:
                raise ValueError(f'Reference skill differs: {name} {label}')
            verified += 1
    return verified


def generate(root):
    skills, report = learned_skills(root, repair_mack=True)
    for n, name in enumerate(CHARACTERS):
        char = find(root, name)
        for stem, expected in [('HP(Out of Battle)',0x16C),('MP(Out of Battle)',0x188),('EXP (Out of 100)',0x84)]:
            entry = next(e for e in char.findall('./CheatEntries/CheatEntry') if stem in description(e))
            if offset(entry) != expected + n * CHARACTER_STRIDE or entry.findtext('CustomType') != 'Float Big Endian':
                raise ValueError(f'Unexpected scalar layout: {name} {stem}')
    for group in ['Items (sort once to refresh)', 'Materials (sort once to refresh)']:
        for entry in find(root, group).findall('./CheatEntries/CheatEntry'):
            if entry.findtext('CustomType') != 'Float Big Endian':
                raise ValueError(f'Unexpected quantity type: {description(entry)}')
    arrays = {
        'Items': fields(root, 'Items (sort once to refresh)'),
        'Materials': fields(root, 'Materials (sort once to refresh)'),
        'LearnedSkills': skills,
        'Weapons': choices(root, 'Kaim Weapon'),
        'Rings': choices(root, "Kaim's Ring Slot"),
        'Accessories': choices(root, 'Accessory 1'),
    }
    out = ['#pragma once', '#include <cstdint>', '#include <span>', '',
           "// Catalog facts transcribed from dokkoriax's LostOdysseyRecomp_v1.4.CT.",
           '// SHA-256: ' + SOURCE_SHA256,
           '// Reviewed Mack skill corrections: original Xenia eeb96f9 (2024-07-15) CT.',
           '// Reference SHA-256: ' + REFERENCE_SHA256,
           '// No CE scripts, native AOB patches or copyrighted game binaries are embedded.',
           '// Offsets are relative to CT LO_BASE; item/character scalar values are BE float.',
           'namespace debug_menu::cheats::data {',
           'struct Entry { uint32_t value; const wchar_t* name; };',
           'inline constexpr uint32_t CharacterStride = 0x37E4, DataSize = 0x275C0;',
           'inline constexpr const wchar_t* Characters[] = {' + ', '.join('L"' + c + '"' for c in CHARACTERS) + '};']
    for name, values in arrays.items():
        if len(values) != len({value for value, _ in values}):
            raise ValueError(f'Duplicate catalog value: {name}')
        out += ['inline constexpr Entry ' + name + '[] = {']
        out += [f'    {{0x{value:X}, L{json.dumps(label, ensure_ascii=False)}}},' for value, label in values]
        out += ['};']
    out += ['} // namespace debug_menu::cheats::data', '']
    return '\n'.join(out), report, {name: len(values) for name, values in arrays.items()}


def reviewed_table(path, digest):
    source = path.read_bytes()
    if hashlib.sha256(source).hexdigest() != digest:
        raise SystemExit('CT source differs from the reviewed table; review the schema first')
    return E.fromstring(source)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('table', type=Path)
    parser.add_argument('--reference-table', type=Path, help='Recheck all supported rows against the reviewed original Xenia CT')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--check', action='store_true')
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    root = reviewed_table(args.table, SOURCE_SHA256)
    if args.reference_table:
        print('Verified reference skill rows:', verify_reference(root, reviewed_table(args.reference_table, REFERENCE_SHA256)))
    generated, report, counts = generate(root)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2), encoding='utf-8')
    if args.check:
        if not args.output.exists() or args.output.read_text(encoding='utf-8') != generated:
            raise SystemExit('Generated catalog differs from the committed file')
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(generated, encoding='utf-8')
    print(counts, 'sha', SOURCE_SHA256)


if __name__ == '__main__':
    main()
