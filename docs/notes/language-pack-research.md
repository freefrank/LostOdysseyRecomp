# Language Packs (#369): Design and Feasibility for Export, Import and New Languages

[简体中文](language-pack-research.zh-CN.md)

2026-10-09. Sources: static reading of the code and recompiled PPC on main `ae84cf7e`. Offline parsing of `LO.fpi`/`*.fpd` on the Asia (int/jpn/kor/chi/sch) and USA/Europe (int/jpn/deu/fra/spa/ita) discs. One existing debug run log (Asia disc + 3 DLCs, with `NtReadFile` lines). The game was not run. Evidence labels: "Code" = confirmed by reading the code, "Offline" = offline parsing or prototype validation, "Log" = run log, "Inferred" = not verified.

Goal: modders can export the game text to editable files, translate it, and import it back into the game. The translation then appears as a new language in the settings. The first target language is Brazilian Portuguese.

## Conclusions

- Feasible, and no game code needs to change. The game reads archive members using the records in the in-memory `LO.fpi` (offset, stored length, decoded size). The host rewrites the target records when `LO.fpi` is read in, then appends a "virtual tail" to the archive. The game can then read new content of any length. Validated offline: `menu_int.dat` entry ID 9104 was replaced with a longer Portuguese string. After patching, the index was parsed again, the content was retrieved and decoded the way the game does it, the result was correct, and the other records were unchanged.
- Text merging happens at runtime. Export and import share one C++ parser. A language pack carries only translations, not the original text. For players who have DLCs installed, some text files are effectively supplied by the DLC version. Merging at runtime by key works whether or not DLCs are installed. A pre-generated binary cannot do this.
- The base is English (INT). Both versions have it, and the files are byte-for-byte identical. The INT font already contains all the characters Brazilian Portuguese needs. The same Brazilian Portuguese pack works on both versions, and the first new language needs no new glyphs.
- A new language does not register a new language code with the game. The game still treats the current language as INT. The text comes from the language pack, and the settings show the pack's name.
- Most of the work is parsing and rebuilding each text format. Six formats have been reduced to a Python prototype that parses and rebuilds with no content change, byte-for-byte identical. `name_data.xmb` and the Millennium Dream (UE3 package inline text) still need reverse engineering, and are scheduled later.

## 1. Archive read path and overlay

| Item | Content | Basis |
|---|---|---|
| Index load | At startup, `sub_82854400` reads `LO.FPI`: first the 64-byte header, then the rest in one read (used sectors ≤ 512, that is ≤ 1 MiB). It then stays resident in memory | Code, Log |
| Record | 24 bytes. Low 21 bits of `+8` are the sector number (byte offset = sector × 2048, limit 4 GiB). The high byte is the disc/layer number. `0x40` in `+14` = CPX compressed. `+16` is the stored length. `+20` is the CPX decoded size | Code, Offline (all records on the 4 discs of both versions checked) |
| Open member | `sub_828551E8` checks the overlay index (DLC) first, then the base index. `sub_828548A0` → `sub_82853B28` match by name and fill in the file info | Code |
| Read | OVERLAPPED asynchronous read. Reads a 0x20-byte probe first, then the whole member or segments by CPX block. The offset can be any byte, and the read does not go past the member. There is no check that the offset must not exceed the FPD size | Code, Log |
| Host side | `OpenFileHandle`, `NtReadFile`, `NtReadFileScatter` and `NtQueryInformationFile` in `kernel/io/file_system.cpp`. DLC root `XDC*:\` also goes through `OpenFileHandle` | Code |
| DLC | 3 DLCs override `namedata.bin`, `itemexplaindata.bin`, and `dc*_scrw.jmd` (for each language). They are searched before the base index. The DLCs mount at about 9.4 s, while `system.ini`, `coalesced.int`, `menu_int.dat`, and the name and description tables were already read by 6.0–7.4 s | Log |

**Overlay design** (host side, `file_system.cpp`):

1. Before the game opens `LO.FPI` (about 6.0 s), load the language pack and compute `{member path → new bytes}`.
2. When `lo.fpi` or `XDC*:\*.fpi` is opened, generate a patched index image. When `NtReadFile` reads `[0, used)`, it reads from this image. For each replaced record: keep the high byte of `+8`, change the low bits to the tail sector number. Fill in the new values of `+16` and `+20`. The `0x40` bit of `+14` matches the payload.
3. New content is always wrapped in CPX storage blocks (`0xFF`, not compressed), so no compressor needs to be written. Retail data already contains many such blocks (disc1 has 13,048), so the game's decoder must support them. The third byte of the CPX header is reserved space for in-place decompression (in units of 2048 bytes). The game decompresses the compressed stream at `buffer + decompressed size + reserved − stored size` (`sub_8284E6B8`, `sub_828571B8`). A stored-block stream is `16 + 8 × block count` bytes larger than the output, so the reserve must cover at least this. Otherwise the output of one block overwrites the block header of the next. (psvita measurement, 2026-10-09: the DLC version of the name table had reserve 0, and after rebuilding, 1,366 names were misaligned.)
4. The virtual tail starts at `RoundUp(real size, 2048)`. Each payload is zero-padded to a 2048 alignment. Reads that land in the tail are served from memory. `NtQueryInformationFile` reports the extended size. The tail must stay within 4 GiB (the largest, `xenon_snd.fpd`, is 1.48 GB, so there is plenty of room).
5. The patch is reapplied on every open of `LO.FPI`, including after a disc change. The loc members on the four discs have the same content at different offsets, so each disc is patched separately.

A "fake DLC overlay package" alone is not enough. DLCs mount too late, so the text read at startup would not be covered.

This layer is a general "replace archive member by path" mechanism, and it can later serve other purposes (for example, retiming subtitles, or replacing `AssetKind::File` as is).

## 2. Text families

Sizes are counted on disc1 for int / jpn / deu. "Characters" excludes control codes.

| Family | Format | Key | Size | Parse → rebuild | Phase |
|---|---|---|---|---|---|
| `loc/<l>/menu/menu_<l>.dat` | `DAT$` relocatable block, UTF-16BE string pool plus pointer relocation table. The `MStringDescData` segment is keyed by text ID (for example 9104) | Text ID. Other segments use (segment, index) | 1366 / 1710 / 1357 entries | Byte-for-byte identical (9 languages). Long-text edit test passed (offline) | P0 |
| `loc/<l>/battle/text/*data.bin` (names, item/magic/skill/recipe/command descriptions) | BE string library: ID → index mapping, plus (offset, length) table, UTF-16BE | Index | 3335 / 3334 / 3327 entries | Byte-for-byte identical (offline) | P0 |
| `loc/<l>/battle/text/*.jmd`, `scr/mes/<l>/*_scrw.jmd` (field dialogue) | JMD: offset table plus UTF-16BE. Control codes are in the private use area (`E0xx`/`E1xx`). Script bytecode is in the language-independent `scr/scr/*_scrw.bin`, referenced by index | (file, index) | Field dialogue: 287 files, 27,364 entries, about 1.76 million character units | Byte-for-byte identical. Needs handling of out-of-order tables and junk slots in `gk0_1_scrw.jmd` (offline) | P0 |
| `event/subtitle/<text>/[<voice>/]*.sub` | UTF-8 text, each line `start frame,end frame,text` | (file, line) | 188 files, 3462 lines | Plain text | P0 |
| `loc/<l>/staffroll/roll_<l>.bin` | UTF-16LE line script, with layout codes such as `$c4 $p85.0` | Line number | About 1638 lines | Plain text | P0 |
| `rpgame/localization/coalesced.<l>` (in `LO.fpd`) | UE3 Coalesced: 13 INI files, mostly editor content | ini/section/key | RPGame about 3.9k characters | Byte-for-byte identical (offline) | P0 |
| `loc/<l>/field/name_data.xmb` | `xmlbin` binary XML. Nodes reference strings in a string pool by offset. Place names, shop names, Millennium Dream titles, etc. | Attribute ID | Pool 2337 entries | String pool parsed. Node encoding not reverse engineered | P3 |
| Millennium Dream `event/1000/<l>/dr_##_<l>.xxx` etc. | UE3 packages. Text is inline in the serialized properties of `InterpTrackLO_FontRect` | (package, export, index) | 36 packages, about 7,900 strings, about 470,000 characters | Needs a UE3 package rewriter | P3 |
| Text in images | `nowloading/*.dds`, `rpmenures*_<l>.xxx`, `tutorial_##_<l>.xxx`, `staffroll_<l>.xxx` | — | — | Handled by texture mods | Out of scope |

Note: the text that `sub_8230BA20` looks up by ID comes from the `MStringDescData` segment of `menu_<l>.dat`, not from `rpmenures*.xxx` (those packages contain only textures).

Control codes are exported as tokens with parameters, for example `{E001}` or `{E10D:0012}`. Icon codes such as `$500$` are kept as they are. Example: `Pick it up?{E001}{E10C}Yes{E001}No{E002}`.

## 3. Language selection and the two versions

| Item | Content | Basis |
|---|---|---|
| ID → code | The host hands over a numeric ID through `XGetLanguage`/`ExGetXConfigSetting`. At game startup, the code is looked up in the registry filled from `loc/system.ini`, giving a 3-letter code, which is written into `GLanguage` (0x83238A28). This happens only once | Code |
| Use of the code | All 17 reads of `GLanguage` build paths or package names (`loc/<code>/`, `_<code>` packages, `Coalesced.<code>`, `subtitle/<code>/`). No logic based on numeric ID or on specific language branches was found | Code (none found in scope; does not prove absence) |
| Text and voice | Separate: `TextFolders` and `SoundFolders`. The in-game voice option chooses from `SoundFolders` | Code |
| Subtitles | First look for `subtitle/<text>/<voice>/`. If absent, fall back to `subtitle/<text>/`. The INT base needs only `subtitle/int/` | Code |
| Asia version | Text int/jpn/kor/chi/sch. Voice int/jpn/kor | Offline |
| USA/Europe version | Text int/jpn/deu/fra/spa/ita. Voice int/jpn/deu/fra/ita (Spanish text with English voice) | Offline |
| INT across versions | `loc/int/`: 568 members, `subtitle/int/`: 752, `*_int.xxx`: 244. File counts and lengths are all the same, and the font pack bytes are the same | Offline |

Neither version contains Portuguese. The porting layer treats ID 9 as SCH (keeping the old number from the static exe table), but on Xbox 360, 9 is Portuguese. So **9 must not be passed to the game as Portuguese**.

## 4. Fonts

- The format is UE3 UFont. The character table (UV rectangles and page number) is stored twice in the package. The code point mapping `CharRemap` is u16→u16, with no per-glyph spacing adjustment. Glyph pages are DXT5, single mip, LZO, Xenos tiling (offline).
- INT's 9 common fonts (Maru23, Meiyo18, LocTit1, etc.), 3 Millennium Dream fonts (Maiandra), and Title all contain the 33 characters Brazilian Portuguese needs (ã õ ç á à â é ê í ó ô ú ü and capitals, ª º “ ” « » – …). The glyphs were checked by decoding the pages (offline), and they are intact.
- Gaps: the end-credits StaffRoll font has no `–` (replaced by `-`). The engine default font Gothic11 has no accented letters. The retail German, French, Italian and Spanish versions are the same in this respect. Player-visible text should not go through it (Inferred).
- INT and the German/French/Italian/Spanish fonts have the same character set (199 characters). Compared with Latin-1, they lack `¤¥¦§¬¯±µ¶·¸¼½¾Ð×Þð÷þÿ`. Translations must avoid these.
- The porting layer's own settings menu reads fonts according to the UI language, and is not affected by the game language pack.

To support languages that need new glyphs later (Polish, Russian, etc.), the recommended approach is "patch the font pack at host load time". The language pack carries only the new glyphs and metrics. When `rpfonts*_<base>.xxx` is read, the character table is extended in memory and pages are appended. It uses the Font kind already reserved in `mod_api.h`. Estimated 1–2 weeks, medium risk. Changing only the pixels or host fallback rendering is not recommended.

## 5. Language pack format (proposal)

Export:

```text
LostOdysseyRecomp.exe --export-assets <dir> --export-kinds text
<dir>/text/<lang>/<成员路径>.json      {"<key>": "<原文>"}，UTF-8
<dir>/text/index.csv                    成员路径、语言、来源（基础或某个 DLC）、条数
```

Language pack (mod folder, works with MO2 and manual install):

```text
mods/<id>/language.ini                    id=pt-BR, name=Português (Brasil), base=int
mods/<id>/text/<成员路径>.json           {"<key>": "<译文>"}，只放翻译过的条目
```

- With `language.ini`: adds one new selectable language, based on `base`.
- Without `language.ini`: overrides the text of an existing language with the same name (for example to fix an English typo).
- A missing key falls back to the original text. An extra key logs a warning.
- Author tool `lo_mod.py language-check`: checks against the export whether keys match, whether control-code tokens are paired, and whether each character is in the `CharRemap` of the base font. (Not done, deferred.)

Settings: `settings.ini` keeps `game_language=<base's ID>` and adds a new text key `game_language_pack=<id>`. The language list becomes generated at runtime. In `ConfigureGameLanguages`, language packs whose base exists in the current version are appended (mod initialization happens before this step). `restart::Required`, the settings menu, and the first-run wizard also need changes. `quit_text_hook` for 9104/9114/9120 must yield to the translation in the language pack.

Implementation of P2 (2026-10-09): `mod_api` scans each mod folder for `language.ini` (in all mod modes, because it only takes effect when the player selects it, so there are no load-order conflicts). `id` is converted to lowercase. For duplicate `id`s, the first one in folder-name order is kept. `main.cpp` registers the language packs in `settings::LanguagePacks` (`config.cpp` does not depend on `modding/`, and tests compile it directly). The list in the settings menu and the first-run wizard is "this version's languages plus language packs whose base is in this version". At startup, `GameLanguagePack()` returns the id only when the language pack exists and its base equals `GameLanguage()`, and passes it to `text_overlay::SelectLanguagePack`. When a language pack is selected, its files replace the translations of the same file by other mods as a whole (per file, not merged by key).

## 6. Phases

| Phase | Content | Acceptance | Estimate |
|---|---|---|---|
| P0 export | C++ parsers: menu `DAT$`, battle string library, JMD, `.sub`, end credits, Coalesced. `--export-kinds text`. Option added to the MO2 export tool | Both versions × all languages × 4 discs × DLC members: parse → rebuild with no content change, all byte-for-byte identical (no game run needed) | 2–3 days |
| P1 import | Archive overlay (base plus DLC index patch, storage block wrapping, virtual tail). Merge by key. Override existing languages | First test on psvita with a longer text in `menu_int.dat`, one string library and one `_scrw.jmd`, to confirm no fixed-size buffers. Then test with DLCs installed | 3–4 days |
| P2 new language | `language.ini`, language lists in settings and first run, `game_language_pack`, quit-menu text yields, `language-check`, document "Making a language pack" | On psvita, select a test language pack. Menus, dialogue, battle and subtitles show the translation. With no language pack installed, behavior is unchanged | 2 days |
| P3 remaining text | `name_data.xmb` node encoding and rebuild. Millennium Dream UE3 package rewrite | Parse → rebuild byte-for-byte identical. Shown in game | 3–5 days |
| Later | Font glyph additions (for languages that need new glyphs) | As needed | 1–2 weeks |

P0 does not depend on any of the decisions below, so it can start first.

## 7. Decisions needed (with recommendations)

1. Base: INT. Reason as above: both versions share it, and its fonts are complete.
2. Merge location: at runtime. Export and import share the C++ parser, and the language pack carries only translations.
3. New language: do not register a new game language code. The game still sees the base.
4. Font glyph additions: deferred. Brazilian Portuguese does not need them.
5. Millennium Dream and `name_data`: P3. But the language pack directory structure reserves room for these two categories now.

## 8. Risks and open questions

- Whether longer text will hit fixed-size buffers or memory pools. The game allocates by the decoded size in `+20`, and no fixed pool was found, but this has not been measured. This is the only issue that could overturn the whole plan. The first thing P1 does is test it.
- Where automatic line wrapping is implemented has not been found. Portuguese is 20–30% longer than English, so fixed-width menu boxes may overflow. Translators must use manual line breaks such as `{E001}`.
- How the game displays characters missing from the font (placeholder boxes or skipping) is not confirmed. `language-check` will block them before import.
- Order when several DLCs override the same path. The export must record whether each member comes from the base or from which DLC.
- The exact meaning of JMD control codes was inferred from the parameter distribution. The export keeps the tokens as they are, so no understanding of their meaning is needed.
- With Japanese voice selected, the INT text subtitles use the version timed for the English dub (as the retail English version does). A language pack that wants separate timing for Japanese voice would need overlay support for new members, which is outside v1 scope.

Prototype scripts (Python, not in the repository): FPI/FPD/CPX reading, the overlay prototype, `DAT$`/string library/JMD/Coalesced parsing and rebuilding, the `xmlbin` string pool, and font character table and coverage statistics. They serve as a reference for the P0 C++ implementation.
