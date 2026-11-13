#!/usr/bin/env python3
"""Game-free importer/catalog regressions; no CE scripts or game files needed."""
import importlib.util
from pathlib import Path
import re
import sys
import unittest
from unittest.mock import patch
import xml.etree.ElementTree as E

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / 'tools/cheats/import_ct_catalog.py'
spec = importlib.util.spec_from_file_location('ct_catalog', SCRIPT)
catalog = importlib.util.module_from_spec(spec)
spec.loader.exec_module(catalog)
RECOVERED = {0xEA8,0xEB8,0xEC8,0xED8,0xEE8,0xEF8,0xF08,0xF18,0xF58,0xF68,0xF98}


def fixture(rows=((0xB58,'Turn Tail'),(0xEA8,'Mental Stability 1'))):
    root = E.Element('CheatTable')
    entries = E.SubElement(root,'CheatEntries')
    for n,name in enumerate(catalog.CHARACTERS):
        char = E.SubElement(entries,'CheatEntry')
        E.SubElement(char,'Description').text='"'+name+'"'
        groups = E.SubElement(char,'CheatEntries')
        for stem,value in [('HP(Out of Battle)',0x16C),('MP(Out of Battle)',0x188),('EXP (Out of 100)',0x84)]:
            scalar = E.SubElement(groups,'CheatEntry')
            E.SubElement(scalar,'Description').text='"'+name+' '+stem+'"'
            E.SubElement(scalar,'Address').text=f'[LO_BASE]+{value+n*catalog.CHARACTER_STRIDE:X}'
        learned = E.SubElement(groups,'CheatEntry')
        E.SubElement(learned,'Description').text='"Learned Skills"'
        skills = E.SubElement(learned,'CheatEntries')
        for value,label in rows:
            skill=E.SubElement(skills,'CheatEntry')
            E.SubElement(skill,'Description').text='"'+label+'"'
            E.SubElement(skill,'Address').text=f'[LO_BASE]+{value+n*catalog.CHARACTER_STRIDE:X}'
            E.SubElement(skill,'VariableType').text='Byte'
    return root


def row(root,character,index):
    char=catalog.find(root,catalog.CHARACTERS[character])
    return char.findall('./CheatEntries/CheatEntry/CheatEntries/CheatEntry')[index]


def repaired_fixture():
    rows=tuple((right,name) for name,(_,right) in catalog.MACK_SKILL_CORRECTIONS.items())
    root=fixture(rows)
    for index,(name,(wrong,_)) in enumerate(catalog.MACK_SKILL_CORRECTIONS.items()):
        row(root,6,index).find('Address').text=f'[LO_BASE]+{wrong+6*catalog.CHARACTER_STRIDE:X}'
    return root


def reference_fixture():
    root=fixture(tuple((right,name) for name,(_,right) in catalog.MACK_SKILL_CORRECTIONS.items()))
    for entry in root.findall('.//CheatEntry'):
        address=entry.find('Address')
        if address is not None: address.text=f'{0x16F911000+catalog.offset(entry):X}'
    gold=E.SubElement(root.find('CheatEntries'),'CheatEntry')
    E.SubElement(gold,'Description').text='"Gold"'
    E.SubElement(gold,'Address').text='16F911044'
    return root


class ImporterTest(unittest.TestCase):
    def test_all_nine_explicit_rows(self):
        skills,report=catalog.learned_skills(fixture())
        self.assertEqual(skills,[(0xB58,'Turn Tail'),(0xEA8,'Mental Stability 1')])
        self.assertEqual(report,[])

    def test_unreviewed_conflict_stays_excluded(self):
        root=fixture()
        row(root,6,1).find('Address').text=f'[LO_BASE]+{0xFA8+6*catalog.CHARACTER_STRIDE:X}'
        skills,report=catalog.learned_skills(root)
        self.assertEqual(skills,[(0xB58,'Turn Tail')])
        self.assertEqual(report[0]['disagreeing_characters'],['Mack'])

    def test_reviewed_repairs_recover_all_eleven(self):
        skills,report=catalog.learned_skills(repaired_fixture(),repair_mack=True)
        self.assertEqual({value for value,_ in skills},RECOVERED)
        self.assertEqual(len(report),11)
        self.assertTrue(all(e['reason']=='verified reference-table correction' for e in report))

    def test_reviewed_repair_rejects_unexpected_input(self):
        root=repaired_fixture()
        row(root,6,0).find('Address').text=f'[LO_BASE]+{0xFA9+6*catalog.CHARACTER_STRIDE:X}'
        with self.assertRaisesRegex(ValueError,'correction input differs'):
            catalog.learned_skills(root,repair_mack=True)

    def test_missing_repair_rows_rejected(self):
        with self.assertRaisesRegex(ValueError,'correction rows are missing'):
            catalog.learned_skills(fixture(((0xB58,'Turn Tail'),)),repair_mack=True)

    def test_reference_verifies_every_character(self):
        self.assertEqual(catalog.verify_reference(repaired_fixture(),reference_fixture()),99)

    def test_reference_different_skill_rejected(self):
        reference=reference_fixture()
        row(reference,6,0).find('Address').text='16F926F00'
        with self.assertRaisesRegex(ValueError,'Reference skill differs'):
            catalog.verify_reference(repaired_fixture(),reference)

    def test_reference_wrong_character_anchor_rejected(self):
        reference=reference_fixture()
        char=catalog.find(reference,'Mack')
        char.find('./CheatEntries/CheatEntry/Address').text='16F9260C5'
        with self.assertRaisesRegex(ValueError,'Reference character layout differs'):
            catalog.verify_reference(repaired_fixture(),reference)

    def test_missing_character_row_stays_excluded(self):
        root=fixture(); char=catalog.find(root,'Tolten')
        rows=char.find('./CheatEntries/CheatEntry/CheatEntries'); rows.remove(rows[1])
        skills,report=catalog.learned_skills(root)
        self.assertEqual(skills,[(0xB58,'Turn Tail')])
        self.assertEqual(report[0]['disagreeing_characters'],['Tolten'])

    def test_unknowns_including_duplicate_unknowns_stay_excluded(self):
        root=fixture(((0xB58,'Turn Tail'),(0xF28,'?? Thievery Up'),(0x16F8,'N/A'),(0x16F8,'N/A')))
        skills,report=catalog.learned_skills(root)
        self.assertEqual(skills,[(0xB58,'Turn Tail')])
        self.assertTrue(all(e['reason']=='unknown name' for e in report))

    def test_duplicate_named_row_rejected(self):
        with self.assertRaisesRegex(ValueError,'Duplicate skill row'):
            catalog.learned_skills(fixture(((0xB58,'Turn Tail'),(0xB58,'Turn Tail'))))

    def test_duplicate_canonical_offset_rejected(self):
        with self.assertRaisesRegex(ValueError,'Duplicate canonical skill offset'):
            catalog.learned_skills(fixture(((0xB58,'Turn Tail'),(0xB58,'Steal'))))

    def test_duplicate_display_name_rejected(self):
        with self.assertRaisesRegex(ValueError,'Ambiguous skill display name'):
            catalog.learned_skills(fixture(((0xB58,'Turn Tail'),(0xB68,'Turn Tail'))))

    def test_only_byte_flags_accepted(self):
        root=fixture(); row(root,6,1).find('VariableType').text='4 Bytes'
        with self.assertRaisesRegex(ValueError,'not a byte'): catalog.learned_skills(root)

    def test_character_bounds(self):
        for value in (0x37E4,0xFFFFFFFF):
            with self.subTest(value=value),self.assertRaisesRegex(ValueError,'outside character data'):
                catalog.learned_skills(fixture(((value,'Turn Tail'),)))

    def test_address_expression_is_not_evaluated(self):
        for address in ('[LO_BASE]+-1','[LO_BASE]+EA8+100','other+EA8','[LO_BASE]+EA8;run()'):
            entry=E.Element('CheatEntry'); E.SubElement(entry,'Address').text=address
            with self.subTest(address=address),self.assertRaises(ValueError): catalog.offset(entry)

    def test_raw_name_conflict_not_hidden_by_alias(self):
        root=fixture(((0xD18,'Cut DOwn'),)); row(root,6,0).find('Description').text='"Cut Down"'
        self.assertEqual(catalog.learned_skills(root)[0],[])

    def test_both_ultimate_hit_ids_preserved_and_distinguishable(self):
        skills,_=catalog.learned_skills(fixture(((0xD08,'Ultimate Hit'),(0x1888,'Ultimate Hit'))))
        self.assertEqual([e[1] for e in skills],['Ultimate Hit (ID 0x1C)','Ultimate Hit (ID 0xD4)'])

    def test_unreviewed_source_rejected_before_output(self):
        with patch.object(sys,'argv',[str(SCRIPT),'changed.CT','--output','catalog.h']), \
                patch.object(Path,'read_bytes',return_value=b'<CheatTable/>'),patch.object(Path,'write_text') as write:
            with self.assertRaisesRegex(SystemExit,'source differs'): catalog.main()
            write.assert_not_called()

    def test_committed_catalog_against_issue_755(self):
        header=(ROOT/'LostOdysseyRecomp/debug/cheat_data.h').read_text(encoding='utf-8')
        section=header.split('LearnedSkills[] = {',1)[1].split('};',1)[0]
        skills=[(int(v,16),name) for v,name in re.findall(r'\{0x([0-9A-F]+), L"([^"]+)"\}',section)]
        expected=(Path(__file__).parent/'data/skill_names_issue_755.txt').read_text(encoding='utf-8').splitlines()
        self.assertEqual(len(expected),171); self.assertEqual(len(set(expected)),171)
        self.assertEqual(len(skills),172); self.assertEqual(len({s[0] for s in skills}),172)
        self.assertEqual(len({s[1] for s in skills}),172)
        self.assertEqual({re.sub(r' \(ID 0x[0-9A-F]+\)$','',s[1]) for s in skills},set(expected))
        self.assertTrue(RECOVERED <= {s[0] for s in skills})
        for value,name in skills:
            self.assertLess(value,catalog.CHARACTER_STRIDE)
            self.assertLess(value+8*catalog.CHARACTER_STRIDE,catalog.DATA_SIZE)
            self.assertNotIn('?',name)


if __name__=='__main__':
    unittest.main()
