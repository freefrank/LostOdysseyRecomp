"""Compile production reconfiguration and resolve lookup with CPU resource stand-ins."""
from pathlib import Path
import sys

root, output = map(Path, sys.argv[1:])
source = (root / 'LostOdysseyRecomp/gpu/renderer.cpp').read_text(encoding='utf-8')
template = Path(__file__).with_name('fixture.cpp.in').read_text(encoding='utf-8')
for method in ('ApplyInternalResolution', 'FindResolved', 'ResolvedSlot',
               'InvalidateFgResolve', 'ResolveFormatMatches'):
    # Find the definition, not a call or declaration, and copy its whole body.
    import re
    matches = list(re.finditer(r'^            [^\n;]+\b' + method + r'\([^;\n]*\)\n            \{', source, re.M))
    if len(matches) != 1:
        raise RuntimeError(f'{method}: expected one production definition')
    begin = matches[0].start()
    pos = source.index('{', matches[0].start())
    depth = 1
    end = pos + 1
    while depth:
        if source[end] == '{': depth += 1
        elif source[end] == '}': depth -= 1
        end += 1
    template = template.replace('/* ' + method + ' */', source[begin:end])
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(template, encoding='utf-8')
