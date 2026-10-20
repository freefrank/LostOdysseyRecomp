# Issue #74: After Saving Anywhere During a Party Split, Character Switching Is Lost (Investigation of 2026-09-29)

[简体中文](issue-74-save-anywhere-party-split.zh-CN.md)

> **Investigation record.** Conclusions and open items apply only to the dates, versions and captures in the text. For current problem, delivery and acceptance status, see [project status](../STATUS.md) and the [roadmap](../ROADMAP.md).

[Issue #74](https://github.com/freefrank/LostOdysseyRecomp/issues/74) reports that during a party split in The Great Ancient Ruins (Astral Square, map 244), saving with Save Anywhere made the "RB Change Character" prompt disappear in the lower right after loading. RB then did nothing. In this round, the `user01` save attached to the report and the USA/Europe data were used to reproduce the problem in an isolated directory, and reverse engineering was done. The Save Anywhere behavior was not changed. Only two always-visible warning lines were added below the F1 menu switch. The Chinese and English layouts were checked with offline rendered screenshots from `LoDebugOverlayTest`. On main, this test already reports `concurrent menu actions closed overlay` in a later step. Removing this change gives the same result, so the result is not related to the warning text.

## Conclusion

The character switching function is turned on by a one-time story script. The on state exists only at runtime and is not stored in the save. Save Anywhere only opens the Save entry of the native System menu (see the [Save Anywhere note](save-anywhere.md)). The saved content is the same as a native save. So the problem is "saving at a moment the original game does not allow", not a corrupted save. After loading, the story flags and party members are still correct. At the exit, Cooke says to wait for the captain's group, but RB switching does not come back.

Recovery paths that were ruled out:

- Leaving Astral Square for the neighboring Legacy of the Eastern Tribe and walking back does not bring the prompt back. RB still does nothing.
- The `CmdDrawState` / `CmdEnableState` bits of rpPlayData (`PlayData+0x31F7C..0x31F8B`, read and written through the vtable `+0x1C4..+0x1D8` by native interfaces such as `SetCmdDrawState`) are indeed outside the section that EasySave copies into the save (`+0x4C..+0x30BF8`). But setting all 64 bits to 1 does not make the prompt appear, so the switch is not here.
- The current level has almost no UE Kismet logic. The heap has only 13 `SeqEvent_Touch` instances, and `fsSeqAct_DrawControl`, `fsSeqAct_KeyControl` and `fsSeqAct_CheckButtonInfo` exist only as default objects. The switching logic should be in the game's own event script system.
- `rpBaseController.XPad_RB` (native `828173B0`) is called only by native C++ with a newly created FFrame when RB is pressed. The caller is not an UnrealScript function, so the switch cannot be found from the script side.

## Confirmed addresses

| Item | Value |
|---|---|
| rpPlayData vtable | `0x820108FC`, obtained from the GEngine vtable `+0x160`, same path as the cheat feature |
| EasySave implementation | vtable `+0x220` → `82906E18`, copies five data sections starting at `+0x4C` |
| MaskMember | vtable `+0x1EC` → `82908308`, at `PlayData+0x64`, saved |
| ChangeCtrlChara | vtable `+0x1DC` → `829087B0`, writes `PlayData+0x58/+0x5C`, saved |
| Prompt text | "Change Character" is in the same localized FString table as "Pick Up Firewood" and "Enter" |
| UFunction reflection | UField `SuperField +0x3C`, `Next +0x40`; UStruct `Children +0x4C`, `PropertiesSize +0x50`, script bytecode `+0x54/+0x58`; UProperty `Offset +0x68` |

The member mask at `PlayData+0x64` cannot be used to tell whether the party is split. The report's save has `0x596`, and a normal save at the native Numara Palace save point also has `0x58C`.

## Next direction

Either stop Save Anywhere from overwriting the native Save disable during a party split, or rebuild the switching state after loading. Both need a runtime switch to be found first. What is missing is a live "can switch" state. This requires a save from before the split, and then a memory comparison between the state after entering the split and the state after loading. The temporary probes of this round (FName search, reflection field traversal, object/string/pointer scanning, and recording the callers of `XPad_RB`) are kept in the ignored `out/issue74/issue74-probe.patch`. The source has been restored.

## Static location update, 2026-09-30

The "live state that can switch" that the previous section says is missing is the runtime conclusion of 2026-09-29. On this day, the switch and the group-change action were statically located in the event script as candidate positions. This narrows the scope of the later comparison, but it does not prove why the runtime state is not restored after loading.

### Asset and decoding evidence

| Item | Static evidence |
|---|---|
| Map mapping | `MapID 244` in `worldinfo.xmb` → `ev5_0_scrw` |
| Script virtual path | `bin\xenon\scr\scr\ev5_0_scrw.bin` |
| Assets on disc | `D:/Mihoyo/LostOdysseyRecomp-windows-x64/game_us/disc1..4`: all four discs have the same asset |
| FPD record | `xenon_scr.fpd` offset `0xA35800`, stored `0x3588`; CPX decoded `36828 (0x8FDC)` |
| Script SHA-256 | `2e6c74bde83062fba5173482f65d6fe619e2e0ab84a8ee86a521e54935941561` |
| Reviewable outputs | `out/issue74/script-locations.json`, `out/issue74/party-switch-locations.json`, `out/issue74/decoded-scripts/ev5_0_scrw.bin` (all ignored, local only) |

The script container is little-endian. Each record is, in order, `id:u32`, `entrycount:u32`, `entries:u16[]`, `constcount:u32`, `constants:u32[]`, `codebytes:u32`, `code:u8[]`. There are 148 records. The codebase of record 0 is `0x14D0`. The header of record 1 is `0x365C`, with constantbase `0x3672` and count `63`, codebase `0x3772`, and codebytes `0x362`. The PCs in the table below are relative positions within the codebase of the matching record. `fileoff` is the offset in the decoded file, not the FPD offset.

### Reviewable script locations

| Location | Static content | Current interpretation |
|---|---|---|
| record 0, PC `0x1C65` / file `0x3135` | `03 34 10 6E 84` | `const[0x46E] = 1`, that is, write `ref 0x1034 = 1` |
| record 0, PC `0x1D5B` / file `0x322B` | `03 34 10 A1 84` | `const[0x4A1] = 0`, that is, clear `ref 0x1034` |
| record 1, entry 1, PC `0x04` / file `0x3776` | Control prompt entry. PC `0x07` compares `ref 0x1034 == 1`, PC `0x0F` compares `ref 0x1036 == 0`, then `F1 0B` checks the controller state | Candidate display condition for the "Change Character" prompt |
| record 1, entry 1, PC `0x36` / file `0x37A8` | `F0 6D 00 05 80 06 80` | Create group 2, key 6, textID 24 |
| record 1, entry 1, PC `0x40` / file `0x37B2` | Create group 2, key 9, textID 24. Which of the two is used is decided by `ref 0x0B07` | Two prompt variants. textID 24 is confirmed as `Change Character` by `name_data.xml` in `xenon_loc.fpd` |
| record 1, entry 1, PC `0x47` / file `0x37B9`; PC `0x52` / file `0x37C4` | `F0 6E 01`; `F0 6E 00` | Show / hide group 2 |
| record 1, entry 4, PC `0x58` / file `0x37CA` | Entry of the group-change action. PC `0x82` / file `0x37F4` writes `ref 0x1035 = 1`, PC `0x19E` / file `0x3910` writes `ref 0x1035 = 0` | Candidate group-change state, using `pc_010a0` and `pc_000a0` respectively |
| record 1, entry 4, PC `0x2BF` / `0x30F` | Two places that rearrange the two groups of members | Candidate member update for the group-change action |

The native `core27` handler `828472F8` calls member interfaces such as `PlayData` vtable `+0x1EC/+0x1F0`. Opcode `ED` (`82A4E2C8`) switches the character model with a 32-byte string. The mapping from characters to person names is not complete, so specific characters should not be guessed from this.

### Runtime boundary

The script VM dispatcher is `829FDB40` (`context+0x494` is the codebase, `+0x69C` is the PC). The handler table constructor is `829FF7C8`. The `F0 6D` and `F0 6E` handlers are `82A5FF28` and `82A5F650`. `F0 6D` goes through `823607A0` → `82360800`, whose `r4`, `r5` and `r6` are the group, key and textID. This shows that `2` is a group, not a fixed slot. The `ref 0x1034/0x1035/0x1036` values are read and written by the VM accessors `8229DDE8` / `8229E280`. They map to the VM manager pointed to by `*(u32*)0x831F1B60`: when `scriptCtx+0 >= 0`, the offsets are `+0xF88/+0xF8C/+0xF90`. This shows they are fields of the event VM object. Whether this object has another save serialization path has not been verified. So the earlier statement "not stored in the save" still needs runtime or serialization evidence.

The dialogue resource with the same name is `bin\xenon\scr\mes\int\ev5_0_scrw.jmd` (FPD offset `0x32B000`, length `0xE68`, decoded `11168` bytes). Zero-based row 10 (`0x7C6`) describes Seth telling Kaim and Cooke to walk down while the other group walks up. Row 20 (`0xE4C`) has the prompt where Cooke waits for the captain's group. It can serve as a cross-check that the scene script has the same origin.

This round covered only static unpacking and location. The runtime was not changed, nothing was built, the restoring of a save was not verified in practice, and no game files were copied into docs. The next runtime probe should directly observe `+0xF88/+0xF8C/+0xF90` at the pointer target of `0x831F1B60`, and record the record 1 entry dispatch. The broad `XPad_RB` callback search should no longer be the main path for locating the problem.

## Runtime validation and restore button, 2026-09-30

The static location from the previous section was validated at runtime. The probe read `*(u32*)0x831F1B60`, which gives the VM manager `0x832C386C`. This is in the XEX image data segment, not on the heap. After the reporter's `user01` save was loaded, `+0xF88` (`ref 0x1034`), `+0xF8C` (`ref 0x1035`) and `+0xF90` (`ref 0x1036`) were all 0. Only after writing `+0xF88` to 1, "RB Change Character" appeared in the lower right immediately. Pressing RB switched to the other group where Seth is. `ref 0x1035` became 1. Pressing again switched back to Kaim, and `ref 0x1035` returned to 0. This confirms that `ref 0x1034` is the switch, and that it is not restored after loading.

As a control, all 64 `CmdDrawState` / `CmdEnableState` bits of rpPlayData were set to 1, and then RB was pressed. `XPad_RB` was indeed read, but the character did not change and the prompt did not appear. So this group of bits is not related to switching.

A "Force-enable RB character switch" button was added to the F1 general page, next to Save Anywhere. The UI only sets one atomic request. The engine tick (`82290B60` hook) writes `ref 0x1034` to 1 on the game thread and records the original value. End-to-end validation was done by triggering the same `RequestPartySwitch()` through a temporary file. The log showed `party switch armed (was 0)`, the prompt came back, and pressing RB switched groups successfully. The temporary trigger was then deleted. The button click itself is covered by `LoDebugMenuInteractionTest`. The layout was checked in Chinese and English with offline screenshots from `LoDebugOverlayTest`. Both tests passed.

Scope not yet covered: validation was done only in Astral Square. Whether `ref 0x1034`, a global script variable, is read by later story scenes after being written in an area that does not offer switching has not been verified. At the end of the split, the script clears it at record 0 PC `0x1D5B`.

### Disable Save Anywhere during a party split

`Apply` in `save_anywhere.cpp` no longer overwrites the native Save enable bit when `ref 0x1034 == 1`, so the game's own permission is kept. Player save test (Save Anywhere on): after loading, the flag is 0 and the System menu Save is available. Using the same button path to set the flag to 1, then leaving the menu and entering System again, Save was greyed out. This only prevents new saves during a split. Old saves that have already lost the switch still need the button above to recover. The behavior of the native save point during a party split was not tested separately in this round.
