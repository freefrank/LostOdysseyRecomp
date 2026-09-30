"""Run native generated PPC language routines without a game/profile/renderer.
Generated game code stays in the ignored output directory; no game bytes are copied
into this test. Requires local generated PPC sources, Python, and clang++.
"""
from pathlib import Path
import argparse, re, subprocess

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', default='clang++')
parser.add_argument('--output', type=Path, default=root / 'out/guest-language-test')
parser.add_argument('--baseline', default='c1e6953', help='Revision before the voice-menu fix')
args = parser.parse_args()
functions = {
    '23': ['82481BE8', '82481C58', '82481CD0', '82481E78', '82481EE8', '82481F40', '82482038'],
    '2': ['822D03D8'], '4': ['8230BAC0'],
}
names = [name for group in functions.values() for name in group]
source = ['#include "ppc_context.h"\n']
source += [f'PPC_EXTERN_FUNC(sub_{name});\n' for name in names]
source += ['PPC_EXTERN_FUNC(__savegprlr_28);\nPPC_EXTERN_FUNC(__savegprlr_29);\nPPC_EXTERN_FUNC(__restgprlr_28);\nPPC_EXTERN_FUNC(__restgprlr_29);\nPPC_EXTERN_FUNC(sub_82BE19E8);\n']
for shard, group in functions.items():
    text = (root / f'LostOdysseyRecompLib/ppc/ppc_recomp.{shard}.cpp').read_text()
    for name in group:
        match = re.search(r'PPC_FUNC_IMPL\(__imp__sub_' + name + r'\) \{.*?\n\}', text, re.S)
        if not match:
            raise SystemExit(f'Missing generated function {name}')
        # Preserve instruction bodies verbatim; only give the native lookup a
        # separate name so the real call sites pass through the host policy.
        symbol = 'native_lookup' if name == '82481BE8' else 'sub_' + name
        source.append(match[0].replace('PPC_FUNC_IMPL(__imp__sub_' + name + ')', 'PPC_FUNC(' + symbol + ')') + '\n')
source.append('#include <algorithm>\n#include <settings/language_selection.h>\n')
current_menu = (root / 'LostOdysseyRecomp/settings/menu.cpp').read_text()
baseline_menu = subprocess.check_output(['git', 'show', args.baseline + ':LostOdysseyRecomp/settings/menu.cpp'], cwd=root, text=True)
for namespace, menu in [('baseline_menu', baseline_menu), ('current_menu', current_menu)]:
    source.append('namespace ' + namespace + ' {\nnamespace language = settings::language;\n')
    for function in ['VoiceCount', 'VoiceLanguage']:
        match = re.search(r'uint32_t ' + function + r'\([^)]*\)\s*\{.*?\n\}', menu, re.S)
        if not match:
            raise SystemExit(f'Missing production menu helper {function}')
        source.append(match[0] + '\n')
    source.append('}\n')
source.append((root / 'tools/tests/guest_language_test.cpp').read_text())
args.output.mkdir(parents=True, exist_ok=True)
cpp = args.output / 'native_fixture.cpp'
cpp.write_text(''.join(source))
exe = args.output / ('guest_language_test.exe' if __import__('os').name == 'nt' else 'guest_language_test')
subprocess.run([args.compiler, '-std=c++20', '-O0', '-Wno-ignored-attributes', '-msse4.1',
    '-I' + str(root / 'LostOdysseyRecompLib/ppc'),
    '-I' + str(root / 'LostOdysseyRecomp'),
    '-I' + str(root / 'tools/XenonRecomp/thirdparty/simde'), str(cpp), '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)

