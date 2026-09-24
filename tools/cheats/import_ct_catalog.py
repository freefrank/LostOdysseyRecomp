#!/usr/bin/env python3
"""Extract catalog facts only; never evaluate a Cheat Engine script.

The CT stays local. Unknown/discordant skill rows are excluded, not repaired
from assumptions. --check verifies the committed catalog without changing it.
"""
from pathlib import Path
import argparse
import hashlib
import xml.etree.ElementTree as E
import json

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('table',type=Path)
parser.add_argument('--output',type=Path,required=True)
parser.add_argument('--check',action='store_true')
parser.add_argument('--report',type=Path)
args=parser.parse_args()
src=args.table
expected='f2ec19455cf5f313374874ea1b0611a8fd3ca1b44ae1129375d9c336d7dc2c6e'
if hashlib.sha256(src.read_bytes()).hexdigest()!=expected:
    raise SystemExit('CT source differs from the reviewed v1.4 table; review the schema first')
r=E.parse(src).getroot()
desc=lambda e:e.findtext('Description','').strip('"')
entries=r.findall('.//CheatEntry')
find=lambda name:next(e for e in entries if desc(e)==name)
off=lambda e:int(e.findtext('Address').split('+')[1],16)
def fields(group):
 return [(off(e),desc(e)) for e in find(group).findall('./CheatEntries/CheatEntry') if e.findtext('Address')]
def choices(name):
 return [(int(k,16),v.strip()) for ln in find(name).findtext('DropDownList','').splitlines() if ':' in ln for k,v in [ln.split(':',1)]]
items=fields('Items (sort once to refresh)'); mats=fields('Materials (sort once to refresh)')
skills=fields('Learned Skills')
chars=['Kaim','Seth','Ming','Sarah','Jansen','Cooke','Mack','Sed','Tolten']
# Cross-check every character's learned table rather than extrapolating gaps.
consistent = dict(skills)
for n,name in enumerate(chars):
 char=find(name)
 learn=next(e for e in char.findall('./CheatEntries/CheatEntry') if desc(e)=='Learned Skills')
 actual={(off(e)-n*0x37e4,desc(e)) for e in learn.findall('./CheatEntries/CheatEntry')}
 consistent={k:v for k,v in consistent.items() if (k,v) in actual}
 for stem,expected in [('HP(Out of Battle)',0x16c),('MP(Out of Battle)',0x188),('EXP (Out of 100)',0x84)]:
  e=next(e for e in char.findall('./CheatEntries/CheatEntry') if stem in desc(e))
  assert off(e)==expected+n*0x37e4 and e.findtext('CustomType')=='Float Big Endian'
for group in ['Items (sort once to refresh)','Materials (sort once to refresh)']:
 for e in find(group).findall('./CheatEntries/CheatEntry'): assert e.findtext('CustomType')=='Float Big Endian'
skipped=[(k,v) for k,v in skills if k not in consistent or v=='N/A' or '?' in v]
skills=[(k,v) for k,v in skills if k in consistent and v!='N/A' and '?' not in v]
if args.report:
 args.report.parent.mkdir(parents=True,exist_ok=True)
 args.report.write_text(json.dumps(skipped,indent=2),encoding='utf-8')
arrays={'Items':items,'Materials':mats,'LearnedSkills':skills,'Weapons':choices('Kaim Weapon'),'Rings':choices("Kaim's Ring Slot"),'Accessories':choices('Accessory 1')}
out=['#pragma once','#include <cstdint>','#include <span>','',
 '// Catalog facts transcribed from dokkoriax\'s LostOdysseyRecomp_v1.4.CT.',
 '// SHA-256: '+hashlib.sha256(src.read_bytes()).hexdigest(),
 '// No CE scripts, native AOB patches or copyrighted game binaries are embedded.',
 '// Offsets are relative to CT LO_BASE; item/character scalar values are BE float.',
 'namespace debug_menu::cheats::data {','struct Entry { uint32_t value; const wchar_t* name; };',
 'inline constexpr uint32_t CharacterStride = 0x37E4, DataSize = 0x275C0;',
 'inline constexpr const wchar_t* Characters[] = {'+', '.join('L"'+c+'"' for c in chars)+'};']
for name,vals in arrays.items():
 assert len(vals)==len({v for v,_ in vals}),name
 out+=['inline constexpr Entry '+name+'[] = {']
 out += ['    {0x%X, L%s},'%(v,json.dumps(n,ensure_ascii=False)) for v,n in vals]
 out+=['};']
out+=['} // namespace debug_menu::cheats::data','']
generated='\n'.join(out)
if args.check:
 if not args.output.exists() or args.output.read_text(encoding='utf-8')!=generated:
  raise SystemExit('Generated catalog differs from the committed file')
else:
 args.output.parent.mkdir(parents=True,exist_ok=True)
 args.output.write_text(generated,encoding='utf-8')
print({k:len(v) for k,v in arrays.items()},'sha',hashlib.sha256(src.read_bytes()).hexdigest())
