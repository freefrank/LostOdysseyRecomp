# Built-in cheats and hold-LT speed control

Source implementation for issue #25, 2026-09-24. Not a release or whole-game
acceptance claim. Enter **F1 / LB+RB → Cheats**; LB/RB switches the three main
tabs. On the Cheats page, select the first row and use Left/Right to change
category. A/Enter opens paged selectors; Left/Right skips six entries. B/Esc
backs out of a selector or confirmation before closing F1.

## Controls and supported actions

- Quick tools: independent hold-LT speed toggle, 2/3/4/6/8× rate, memory-edit
  permission, +100,000 gold, set gold to 9,999,999, heal the current party.
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

The CT's learned-skill rows contain disagreements (including Mack offsets) and
unknown `N/A`/`?` labels. Only the **161 named rows whose relative offset and
name agree across all nine characters** are enabled. Excluded rows are not
silently corrected, and "learn known skills" does not mean every game skill.
Generate a local exclusion report and verify the catalog without modifying it:

```sh
python tools/cheats/import_ct_catalog.py /path/to/LostOdysseyRecomp_v1.4.CT \
  --output LostOdysseyRecomp/debug/cheat_data.h --check \
  --report out/cheats/excluded-skills.json
```

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
press/release hysteresis and requires release after enabling, a menu/focus
transition or device change. Releasing LT, unplugging, losing focus, minimizing,
opening F1/settings, or shutdown stops acceleration. LT+RT does not boost, and
requesting the retail editor suspends speed. A 250 ms input lease also returns
the clock to 1× if event pumping stalls. No additional thread is created and
no guest trigger/button state is consumed or modified.

The multiplier controls **guest time**, not guaranteed measured gameplay speed
in every subsystem. Existing engine clamps, performance and media can limit it.
Audio is **not time-stretched**. High multipliers, voice/FMVs, event timing,
combat and save/reload behavior require real-game checks. Do not describe this
as a verified pitch-preserving or whole-game synchronized fast-forward system.

## Verification

Standalone fixtures do not require copyrighted inputs or GPU SDKs:

```sh
cmake -S tools/tests/cheats -B out/cheats -DCMAKE_BUILD_TYPE=Release
cmake --build out/cheats --config Release
ctest --test-dir out/cheats -C Release --output-on-failure
```

Locally executed in Linux with Clang ASan/UBSan and GCC: 3,982 core/clock checks,
5,435 UI checks including 5,000 navigation steps, and 61 checks compiling the
actual runtime adapter against synthetic SDL/allocator services. The UI fixture
produces 14 category/picker/confirmation images. Local visual checks used the
unchanged production rasterizer with a **temporary local glyph fixture**, not
the production Unifont payload; no font replacement is committed. The existing
overlay-controller extraction fixture also passed after adding the new header
dependency. This is not a full runtime build, physical controller test, real
Windows/Linux gameplay test or real save/load acceptance of the PlayData binding.

The new GitHub workflow builds offline core/UI fixtures against the repository's
actual headers and font data on Windows/Linux. Its outcome must be checked on
the resulting commit; this document does not claim it has run or passed.

Before release: verify root binding on an isolated save, an individual item and
character edit, a batch operation, scene/load cancellation, LT 2×/higher/release,
menu/focus/hotplug, and voice/cutscene behavior. Verify the experimental retail
editor and equipment separately. Keep issue #25 open until that acceptance.
