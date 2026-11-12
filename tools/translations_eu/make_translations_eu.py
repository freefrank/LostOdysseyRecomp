# Generates settings/translations_eu.h from the Spanish, French, German and Italian JSON.
# Usage: python -I make_translations_eu.py <folder with ui_*.json> <output.h>
import json, sys, pathlib

d = pathlib.Path(sys.argv[1])
keys = json.load(open(d / "ui_strings.json", encoding="utf-8"))
tables = [json.load(open(d / f"ui_{code}.json", encoding="utf-8")) for code in ("es", "fr", "de", "it")]
for code, table in zip(("es", "fr", "de", "it"), tables):
    missing = [k for k in keys if k not in table]
    assert not missing, (code, missing[:5])

def lit(s):
    out = s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n").replace("\t", "\\t")
    return 'L"' + out + '"'

lines = [
    "#pragma once",
    "",
    "namespace settings",
    "{",
    "// Spanish, French, German and Italian settings text, keyed by the English",
    "// text passed to Translate. Generated from one translation per language.",
    "struct EuropeanText",
    "{",
    "    const wchar_t *key, *spanish, *french, *german, *italian;",
    "};",
    "inline constexpr EuropeanText EuropeanTexts[] = {",
]
for k in keys:
    values = [t[k] for t in tables]
    if all(v == k for v in values):
        continue  # names and symbols that read the same everywhere
    lines.append("    {" + lit(k) + ",")
    lines.append("     " + ", ".join(lit(v) for v in values[:2]) + ",")
    lines.append("     " + ", ".join(lit(v) for v in values[2:]) + "},")
lines += ["};", "} // namespace settings", ""]
pathlib.Path(sys.argv[2]).write_text("\n".join(lines), encoding="utf-8", newline="\n")
print(len(keys), "strings")
