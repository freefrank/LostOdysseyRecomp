# Built-in cheats and LT speed control

> **Topic reference.** The implementation and measurements below retain their stated version and scope. Recheck current delivery and acceptance in [STATUS](../STATUS.md) and [ROADMAP](../ROADMAP.md).

This note began with the source implementation for issue #25 on 2026-09-24 and
now covers the later Hold/Toggle speed mode. It does not claim whole-game
acceptance. Enter **F1 / LB+RB → Cheats**; LB/RB switches the three main tabs.
On the Cheats page, select the first row and use Left/Right to change category.
A/Enter opens paged selectors; Left/Right skips six entries. B/Esc backs out of
a selector or confirmation before closing F1.

## Controls and supported actions

- Quick tools: independent fast-forward switch, Hold/Toggle speed mode, 2/3/4/6/8×
  rate, memory-edit permission, +100,000 gold, set gold to 9,999,999, heal the
  current party. Speed mode defaults to Hold each process. Select **Speed mode**
  and press Left/Right or A/Enter to change it. Hold accelerates while LT is held;
  Toggle switches acceleration with each new LT press.
- Characters: choose one of nine characters, restore out-of-battle HP/MP,
  set EXP progress to 0–99, learn one catalogued skill or the reviewed subset.
  EXP here is the CT's progress field, not a level or total-XP setter.
- Inventory: individual or batch quantities 1/10/50/99 for 41 listed item and
  119 listed material fields. Reserved gaps are untouched. Sort the in-game
  inventory once to refresh, as directed by the CT.
- Equipment: named selectors for 63 weapon, 236 ring and 107 accessory IDs;
  only existing accessory slots can be changed. Experimental: compatibility
  between an equipment ID and a particular character is not inferred from the
  CT. This edits equipped fields, not ownership/unlock inventory tables.
- Party: assign/swap an existing initialized character, front/back row, and
  an experimental field-character byte. The latter is not an immediate pawn
  replacement; the CT says to keep its override enabled before loading a save.
  This implementation only writes the current data value once and does not
  promise the CT's continuous pre-load override behavior.
- Developer: experimental retail EDIT MENU flag, accessed by LT+RT after
  closing F1. Exit the retail editor before switching the option off. This is
  not the prototype debug menu and does not implement the CT's five native
  instruction patches. Actual editor reachability still needs game validation.

Update, 2026-10-07 (merged to `main`, not yet in a release; latest release
v0.8.44): the fast-forward switch, Speed mode and rate are no longer reset every
process. PR [#269](https://github.com/freefrank/LostOdysseyRecomp/pull/269)
(merged 2026-10-07T03:01:14Z as `35e22a28`; Issue #104 closed) saves them to
`settings.ini` as `fast_forward`, `fast_forward_mode` and `fast_forward_rate` when
they are changed and restores them before the first trigger poll. The defaults
stay off, Hold and 2×, invalid values fall back to them, and the in-memory
turn-off at shutdown is not written. Memory edits and the other cheats are not
saved and still start OFF every process. Checked with `LoSaveAnywhereConfigTest`,
`LoDebugMenuInteractionTest` and `LoDebugOverlayTest`, and on psvita (Proton
D3D12): set on, Toggle, 4× through F1, restart, and the log shows
`debug menu: fast-forward on (toggle, 4x)`; the in-game quit path was not run.
The "defaults to Hold each process" sentence above and the speed part of the next
paragraph describe the earlier behavior.

Both speed and memory edits start OFF every process. Speed does not require
memory-edit permission. Enabling edits and destructive/batch actions require a
confirmation with **Cancel selected by default**. Turning edits off cancels a
pending request and the editor override; it does not undo values already
written or remove them from saved games. Back up saves before using these tools.

A memory action queues at most one request. Close F1 to let the guest game
thread execute it. The status distinguishes Pending, Applied, Invalid and
Cancelled. It never reports a queued write as an executed one.

## CT provenance and schema

Catalog facts come from the supplied `LostOdysseyRecomp_v1.4.CT` by issue author
**dokkoriax**, SHA-256
`f2ec19455cf5f313374874ea1b0611a8fd3ca1b44ae1129375d9c336d7dc2c6e`.
Only names, IDs, types and relative offsets are extracted. No CE Lua or native
AOB patch code runs or ships, and no game images or generated PPC are included.

`tools/cheats/import_ct_catalog.py` verifies that exact input and cross-checks
all nine character layouts. The character stride is `0x37E4`. HP/MP, their
maxima, EXP progress, and item/material quantities are **big-endian float32**;
gold and party IDs are big-endian u32; equipped IDs are big-endian u16;
learned flags are bytes. Learned writes OR `0x80`, retaining other bits.

The converted CT contains eleven named Mack rows at incorrect offsets. Exact
corrections are recorded and checked against the original Xenia CT described
below. All **172 supported offsets now agree across all nine characters**.
Unknown `N/A`/`?` rows and any other disagreements remain excluded. The two
CT IDs named Ultimate Hit remain separate and are labelled by ID; 172 offsets
cover the 171 distinct names reported in issue #755, not 172 proven distinct
retail abilities. "Learn known skills" still touches only catalogued bytes.
Generate an exclusion/correction report and verify the catalog:

```sh
python tools/cheats/import_ct_catalog.py /path/to/LostOdysseyRecomp_v1.4.CT \
  --output LostOdysseyRecomp/debug/cheat_data.h --check \
  --report out/cheats/excluded-skills.json
```


### Missing skills (#755): source evidence and remaining acceptance

The original source is WAZAAAAA's
[LostOdyssey-xenia-canary-eeb96f9(2024-07-15).CT](https://fearlessrevolution.com/viewtopic.php?p=362871#p362871),
SHA-256 `50c3f4dbbb5005760a40f2354dcf202d7bfcdaac13ec27c50ca1e4d10e9f7572`.
The converted [v1.4 table](https://opencheattables.com/viewtopic.php?t=2144)
retains the previously recorded input hash. Neither table is executed or
included in the repository.

In the original CT, Gold is at `0x16F911044`, anchoring its equivalent LO_BASE
at `0x16F911000`. Subtracting that base gives the same HP, MP and EXP offsets
for each of the nine characters as the converted table. Every supported
learned byte was compared by character, original name, type and address:
**172 × 9 = 1,548 explicit reference rows**. The original Mack rows, rather
than a presumed regular sequence or a majority vote, establish these fixes:

| Skill | Converted Mack relative offset | Verified relative offset |
| --- | --- | --- |
| Mental Stability 1 | 0xFA8 | 0xEA8 |
| Auto Shield | 0xFB8 | 0xEB8 |
| Auto Barrier | 0xFC8 | 0xEC8 |
| Auto HP Recovery | 0xFD8 | 0xED8 |
| Auto MP Recovery | 0xFE8 | 0xEE8 |
| Reduce Casting Time 1 | 0xFF8 | 0xEF8 |
| Reduce Casting Time 2 | 0x1008 | 0xF08 |
| MP Conservation | 0x1018 | 0xF18 |
| Double Back | 0x1058 | 0xF58 |
| Counter Up | 0x1068 | 0xF68 |
| Angel Heart | 0x1098 | 0xF98 |

These offsets are relative to the start of a character record (Kaim's
catalogued offsets); add Mack's existing `6 * 0x37E4` for LO_BASE offsets.
For example, Mental Stability 1 is explicitly at original
`0x16F926E00`, equivalent to LO_BASE+`0x15E00`; the converted table incorrectly
uses LO_BASE+`0x15F00`, aliasing Resourceful. The importer applies only these
eleven named, exact old/new pairs and rejects an unexpected input.

The old all-character intersection discarded those eleven skills globally.
It counted two different fields named Ultimate Hit (`0xD08`, ID `0x1C`, and
`0x1888`, ID `0xD4`), explaining the apparent 161/171 versus eleven missing
names. Both IDs also appear in the original CT's equipped-skill dropdown.
They remain supported to preserve existing behavior and now have distinct
labels. Whether they are equivalent, unused, or separate in the retail game
still requires original game tables or a live check; neither is removed or
renamed to a different ability on an assumption.

Display spellings are normalized to the
[171-name issue list](https://github.com/freefrank/LostOdysseyRecomp/issues/755#issuecomment-6521853695):
Double-Cast, Cut Down, Drain Shower, Special Accessory, Composite Magic,
Elements Resistance Up, Slot +3/+5/+10, and removal of CT-only exclamation
marks. Raw source names are compared before display normalization.

To repeat the full two-source check, supply both local files:

~~~sh
python tools/cheats/import_ct_catalog.py /path/to/LostOdysseyRecomp_v1.4.CT \
  --reference-table '/path/to/LostOdyssey-xenia-canary-eeb96f9(2024-07-15).CT' \
  --output LostOdysseyRecomp/debug/cheat_data.h --check \
  --report out/cheats/excluded-skills.json
~~~

Both hashes are verified before their input is used. The normal regeneration
also retains the pinned v1.4 hash and the reviewed correction allowlist.

Game-free regressions are included in tools/tests/review_regressions:
LoCheatsSkillTest compiles the production Plan/Commit code and checks all nine
characters, every individual skill, bulk writes, all 256 existing flag-byte
values, adjacent/unrelated bytes, invalid indices, short spans, uninitialized
targets and scene cancellation. LoCheatCatalogTest checks the importer on
synthetic XML and compares the complete catalog with the issue list.
LoDebugMenuInteractionTest traverses the actual picker for all nine characters.

These checks establish source correspondence and bounded byte operations,
not gameplay or save compatibility. Real-game acceptance remains: on a backed-up
save, learn Counter Up and Double Back individually, try Learn known skills for
each initialized character, inspect the game's own skill list, and save/reload.
The public repository has no generated PPC sources (only a placeholder) or
original game executables/assets, so a fresh XenonRecomp run and a full game
build/gameplay check require a supported local game dump.

## Runtime binding and write boundary

UI and window threads publish requests and read by-value snapshots. The existing
`sub_82290B60` engine hook calls `cheats::Tick` on the guest thread. Writes are
planned in full and bounds-checked before any byte is changed. Root/world/level
or availability changes invalidate the pending request. The only accepted field
HUD states are the existing teleport implementation's observed 0 and 10, not a
claim that all game modes have been enumerated. Battle HP is not touched.

The adapter calls the same guest getter as the CT's Gold reader:
`GEngine = BE32(0x83315FB4)` → `BE32(GEngine)` vtable → function at `+0x160`
→ returned PlayData. Gold is `PlayData+0x4C`, so CT `LO_BASE = PlayData+8`.
The caller's complete PPC register context is saved/restored around the guest
call. Vtable bytes must lie in the loaded XEX image; the function address must
lie in the generated guest-code range. The complete returned data span must
belong to a live allocation. Unique valid party IDs, initialized party members,
and coherent typed HP/MP/EXP/inventory fields are checked before enabling edits.
A failed check disables editing; no heap/AOB scan or unchecked fallback is used.

### Static binding evidence

The published v0.6.15 Windows CI artifact `10828374206` from release run
`36044604844`, source `6eef30d257f2e14ce30a546217574a0dc74fad69`, was downloaded
and inspected without execution. Its `LostOdysseyRecomp.exe` is 97,367,552 bytes,
SHA-256 `cc092b18ef6361e49f1bf20d4ea9f4208ee21ab91e25522f1b15e99f0537213b`.
The CT's complete Gold AOB has exactly one match at file offset `0x65D930`
(native VA `0x14065E530`). The preceding instructions at `0x14065E464` load
GEngine through guest `0x83315FB4`, obtain its virtual function at `+0x160`,
call through the guest function table, and preserve the returned pointer for
the Gold load at `+0x4C`. This replaces the earlier **unverified** idea of
reading PlayData through BattleCore+0x20. The implementation does not scan or
patch the executable at runtime. Static call-path verification is not a live
save/load or whole-game acceptance test.

The experimental editor uses guest byte `0x831EAD88`. It remembers/restores the
old byte only while the current byte still equals its override; it does not
clobber a newer game-authored value when switched off. Context changes discard
the request. Per-tick flag writes are not asserted equivalent to patching every
native reader; keep this feature experimental pending actual EDIT MENU tests.

## Speed implementation and limits

One continuous, piecewise-scaled clock feeds the existing shared path used by
`mftb` and `KeTimeStampBundle`. Changing rates never multiplies the accumulated
epoch or jumps backwards. Render pacing, host UI deadlines and I/O waits keep
unscaled time; `GetActiveGameTimeMs` remains the pause-aware host helper.

The existing SDL window thread samples already-open controllers. LT has
press/release hysteresis. In Hold mode, releasing LT returns to 1×. In Toggle
mode, each new LT press switches acceleration on or off; releasing LT alone does
not stop it. LT+RT does not boost or switch the toggle; RT temporarily suppresses
an active boost. Enabling speed, changing mode, unplugging, losing focus,
minimizing, opening F1/settings, pausing or shutdown stops acceleration. After
these stops, LT must be released before another press can boost. Requesting the
retail editor suspends speed. A 250 ms input lease also returns the clock to 1×
if event pumping stalls. No additional thread is created and no guest
trigger/button state is consumed or modified.

The multiplier controls **guest time**, not guaranteed measured gameplay speed
in every subsystem. Existing engine clamps, performance and media can limit it.
Audio is **not time-stretched**. High multipliers, voice/FMVs, event timing,
combat and save/reload behavior require real-game checks. Do not describe this
as a verified pitch-preserving or whole-game synchronized fast-forward system.

## Verification

The standalone cheat fixtures (`tools/tests/cheats`: core, UI and runtime adapter) were removed from the tree in the 2026-10 test pruning; recover them from git history if needed. They did not require copyrighted inputs or GPU SDKs. Recorded results:

Locally executed in Linux with Clang ASan/UBSan and GCC: 3,982 core/clock checks,
5,435 UI checks including 5,000 navigation steps, and 61 checks compiling the
actual runtime adapter against synthetic SDL/allocator services. The UI fixture
produces 14 category/picker/confirmation images. Local visual checks used the
unchanged production rasterizer with a **temporary local glyph fixture**, not
the production Unifont payload; no font replacement is committed. The existing
overlay-controller extraction fixture also passed after adding the new header
dependency. This is not a full runtime build, physical controller test, real
Windows/Linux gameplay test or real save/load acceptance of the PlayData binding.

For the Hold/Toggle change, the Windows MSVC standalone `cheats_core` and
`cheats_ui` fixtures passed. Their synthetic English and Chinese UI images were
visually checked. This does not establish in-game controller behavior or
cross-platform acceptance.

For the initial Cheats tab, GitHub Actions
[run `36064213269`](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/36064213269)
passed Windows and Linux offline fixture jobs on 2026-09-24, using the
repository's headers and font data. That run predates the Hold/Toggle change
and does not validate it.

For in-game acceptance, verify root binding on an isolated save, an individual
item and character edit, a batch operation, scene/load cancellation, LT
Hold/Toggle at 2×/higher rates, stop/re-arm on menu/focus/hotplug, and
voice/cutscene behavior. Verify the experimental retail editor and equipment
separately. [Issue #25](https://github.com/freefrank/LostOdysseyRecomp/issues/25)
was closed on 2026-09-24 after the initial implementation; that closure does
not establish in-game acceptance of this later speed-mode change.
