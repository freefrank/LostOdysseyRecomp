# Save anywhere: menu permissions and validation

[简体中文](save-anywhere.zh-CN.md)

> **Topic reference.** The implementation and measurements below apply only to the version and scope noted for each item. For current delivery and acceptance status, see [project status](../STATUS.md) and [roadmap](../ROADMAP.md).

2026-09-05, current Asian-version build. The feature was added and the runtime build passed. The new build switch, save and reload position readback passed. Regression of ordinary save points and permission recovery before and after entering camp passed. Real clicks on the desktop checkbox and its layout are not yet verified.

**Later limitation (2026-09-29):** [Issue #74 party split investigation](issue-74-save-anywhere-party-split.md) confirmed that turning on save anywhere in a party-split story segment, where the original game disables saving, and then reloading loses the RB party-switch permission, which exists only at runtime. `main` [follow-up commit `2e1c547`](https://github.com/freefrank/LostOdysseyRecomp/commit/2e1c5477447b5d3df361867a2c4395d03e99cdb4) adds a warning in the F1 menu. This commit came after the v0.7.20 release. The camp and ordinary save point validation below does not prove that every story node can be saved safely. Issue #74 is still open.

## Confirmed paths

- System menu Save item ID=34 (0x22). The first menu table entry is at 0x8326D690, and each entry is 0x1c long.
- 0x80000000 is the visible bit and 0x40000000 is the enabled bit. 82877110 queries these two bits, and 82876EA8 sets the visible/enabled parameters.
- 82878CB8 builds the System list. The widget is main menu object +0x238d4, and 828B6968 creates entries from the table.
- In 822E0E10, 822E7EA4 checks whether item 34 is selected. 822E7FF8 calls 8287C8F8, which calls 82860008 to enter the existing save screen.
- EasySave native 828F1A68 → rpPlayData vtable +0x220 → 82906E18 is a memory-state copy. It is not the menu check for whether saving is allowed. EasySaveFlag maps to field +0x3c, mask 0x40000000. That field was not modified.
- CheckSavePoint activates a save point. Do not fake this call or a save point pointer.

## Background prototype validation

Separate copy out/critical-section-camp-01, PID33632, a build with the critical-section fix. The desktop and the original user save/profile were not touched.
After opening the camp chest, System's Save is grey. fcController_0=0x03a43c00, mp_SavePoint(+0x86c)=0.
Changing only the first table entry from 0x80000000 to 0xc0000000, then backing out and re-entering System: shot47183 shows it selectable, shot48111 enters the save check, and shot48478 lists the slots.
Selecting empty slot 02: shot49648 shows saving, and shot50635 shows slot 02 complete, time 00:46, 675G, Kaim Lv10. The copy generated a second save.bin of 206000 bytes.
This shows that the normal flow can write from a non-save point. It does not yet prove that the position and story state are fully intact after a restart.
The prototype process stays on the save list. Input: last 25; shots: last 22. The enabled bit of the table entry changed by the test is still on.

## New implementation and pending validation

Added debug/save_anywhere.cpp/.h. A checkbox at the bottom of menu_window.cpp. Off by default, not persisted, and never starts a save automatically.
The UI only updates an atomic request. The guest menu thread in 822E0E10 applies the enabled bit. 82876EA8 records the original game permission. When turned off, the original enabled bit is restored. The original visibility and other bits are kept.
The test entry LO_SAVE_ANYWHERE_REQUEST points to a text file whose content is a non-zero serial and 0/1, for example `1 1`. It is read every 250 ms. A new serial runs once and uses the same setter as the UI call.
Build log out/save-anywhere-build.log = BUILD OK. The new EXE was run in an independent background copy; see the regression below.
Next step: copy the new build and the camp copy's save/profile to a new background directory, load from slot 02, and verify the camp position, walking, items and story. For the new build, test OFF → ON → save → OFF, verify that after turning it off, leaving the save point still greys out Save, and that saving at a save point still works. Also verify that menu table resets and map changes track the native permission.
All decompilation output is only in out/save-*.txt and is not in the public source.

## New build background regression

out/save-anywhere-01/SaveTest.exe PID24516: copy the camp save/profile, default off, Continue reads the latest slot 02.
- shot1009: back at the chest opened earlier on the left side of camp. shot1756 menu keeps Lv10/675G/00:46.
- shot2554: default OFF, Save grey.
- save-request `1 1`, then re-enter System. shot3208: Save enabled.
- Normal Save to empty slot 03. shot5098 shows 00:48/675G/Lv10, and a third save.bin was actually generated.
- save-request `2 0`, then back out and re-enter System. shot5781: Save is grey again.
- After returning to the scene, stick 24000 held for 90 polls. The coordinates changed from (2397.9229,214.01355,147.1508) to (2675.8682,701.6195,147.1508), so walking works after loading. 12000 input produced no movement and is not counted as failure evidence.

out/save-anywhere-reload-01/SaveReload.exe PID4428: a new process again. The three slots and profile were copied, and the default OFF Continue read the latest slot 03. shot942 returns to the chest opened. status sequence 1 confirms the saved position. No teleport was used to move the character.
Driven only by background file requests, sharing SetSaveAnywhereEnabled with the UI. This round did not open the desktop debug window, so the checkbox layout and real clicks were not checked visually.
Remaining: saving still works after the native save point switch, permission updates on map change, and more story state and item checks. This local regression must not be used to claim that every location and every story point is safe to save in.
PID4428 is kept for the next regression round. input was not requested, shots 1, teleport 1, and save-request was not created. PID24516 was stopped after the path check; the independent copy is kept. New build SHA256: 037229025402B00D12AF821AF9E4CC1A779EA75405598FABC33FB49E3CEA5BFB.

## Ordinary save point and camp entry permission regression

2026-09-05, out/save-point-regression-01/SavePointTest.exe PID35012, using the same 03722902 build and an independent save/profile from third-map-repro-01.
- shot967 is at the native Gorge save point, with the A Save prompt.
- shot1737: with the default OFF, System Save is available. ON → OFF, then re-enter System: shot2428 still available.
- Normal save to empty slot 02. shot3746 confirms the new save at 00:20 completed. The switch did not wrongly disable the native save.
- Walking right hit a wall first. shot5016 still shows A Save, so this does not count as leaving the save point. Then walking forward to the position where Healing Medicine is picked up, shot6615.
- Re-opening System: shot7332 Save is grey. This confirms that the permission updates when the character leaves the save point.
- Turning the switch on again. Ordinary walking triggered a soldier sequence: shot8381 the sequence, and shot11150 back at camp. No teleport. shot11958: the new menu Save is available. After turning it off and re-entering, shot12606 shows it grey again.
- This round did not rebuild the runtime or change game code. It only added the real regression and the index entries for this topic. All results match actual menu screenshots and are not replaced by log output.

PID35012 is kept in the camp System menu, with input 30, shots 13, save-request 4 = OFF. PID4428 is still by the chest opened at camp. Not committed or pushed.
Work returns to rendering next. The main save-anywhere background regression is covered. UI real clicks/layout and later map flows remain part of the overall testing. It cannot be inferred that saving in the middle of any game event is safe.
