# Extracts the English text passed to settings::Translate (settings menu).
# Usage: python -I extract_ui_strings.py <LostOdysseyRecomp folder> <output.json>
import json, re, sys, pathlib

root = pathlib.Path(sys.argv[1])
files = ["settings/menu.cpp", "settings/menu_render.cpp", "settings/title_entry.cpp",
         "settings/first_run.cpp", "install/compaction_prompt.cpp", "gpu/video.cpp",
         "settings/translations.h"]

def literals(text, i):
    """Reads adjacent L"..." literals from i; returns (text, end) or (None, i)."""
    out, found = [], False
    while True:
        m = re.compile(r'\s*(?://[^\n]*\n\s*)*L"').match(text, i)
        if not m:
            break
        j = m.end()
        buf = []
        while text[j] != '"':
            if text[j] == '\\':
                nxt = text[j + 1]
                buf.append({'n': '\n', 't': '\t', '"': '"', '\\': '\\'}.get(nxt, '\\' + nxt))
                j += 2
            else:
                buf.append(text[j]); j += 1
        out.append(''.join(buf)); i = j + 1; found = True
    return (''.join(out), i) if found else (None, i)

def skip_arg(text, i):
    depth = 0
    while i < len(text):
        c = text[i]
        if c in '([{': depth += 1
        elif c in ')]}':
            if depth == 0: return i
            depth -= 1
        elif c == ',' and depth == 0: return i + 1
        elif c == '"':
            i += 1
            while text[i] != '"': i += 2 if text[i] == '\\' else 1
        i += 1
    return i

keys = {}
for name in files:
    text = (root / name).read_text(encoding="utf-8")
    if name.endswith("translations.h"):
        for m in re.finditer(r'\n        \{', text):
            s, _ = literals(text, m.end())
            if s is not None: keys.setdefault(s, name)
        continue
    for m in re.finditer(r'\b(Tr|tr|Translate)\(', text):
        i = m.end()
        if m.group(1) == 'Translate':
            i = skip_arg(text, i)
        s, _ = literals(text, i)
        if s is not None: keys.setdefault(s, name)
    # Tab names and other arrays handed to Translate by index.
    for m in re.finditer(r'enTabs\[\]\s*=\s*\{', text):
        i = m.end()
        while True:
            s, i2 = literals(text, i)
            if s is None: break
            keys.setdefault(s, name)
            i = i2 + 1 if text[i2:i2 + 1] == ',' else i2
            if text[i:i + 2].strip().startswith('}'): break

keys = {k: v for k, v in keys.items() if k.strip() and re.search('[A-Za-z]', k)}
json.dump(sorted(keys), open(sys.argv[2], "w", encoding="utf-8", newline="\n"), ensure_ascii=False, indent=0)
print(len(keys), "strings")
