# Debug character teleport: reverse-engineering basis and validation boundaries (2026-09-04)

[简体中文](debug-teleport.zh-CN.md)

> **Historical checkpoint.** Versions, Issues, processes and to-dos below are the records as of that date. For current delivery and acceptance status, see [project status](../STATUS.md) and [roadmap](../ROADMAP.md).

> 2026-09-05 sync: this page keeps the round-by-round investigation records. Related new code still has local uncommitted parts. For the release scope, see [current status](../STATUS.md). Historical PIDs and in-test states do not mean the processes are still running.

## Status

`debug/teleport.cpp` implements native movement on the game thread. The background validation results for teleport, return, resuming walking, menu refusal and POI landing are in the second half of this page. Foreground interaction with the F1 window and compatibility with all maps are not yet validated. The PIDs and dynamic object addresses in this page are only investigation evidence from the time and cannot be reused across processes.

## Getting the player object

The native registry `intAActorexecLocalPlayerControllers` corresponds to `8250E158`. The generated code and the read-only decompilation `out/teleport-player.txt` confirm:

```text
engine = *(0x83315FB4)
players = *(engine + 0x2B8), count = *(engine + 0x2BC)
localPlayer = players[0]
controller = *(localPlayer + 0x40)
pawn = *(controller + 0x204)
```

`822A6020` (the PlayerController GetViewTarget path) uses `controller+0x204` as the fallback character target. See `out/teleport-target.txt`. On-map evidence also confirmed that Pawn `+0x210` points back to the same controller. The backend re-reads this chain on the game thread every time and verifies the two-way relationship. It does not cache a dynamic Pawn pointer for the next frame.

On-map values at that time, in map Wasteland: controller `0x4BED200`, pawn `0x3DA9000`, Pawn vtable `0x8201E4C8`. During battle, controller `+0x204` is zero, and the controller object is also different.

## Coordinates and native movement

The registry string `intAActorexecSetLocation` (`0x8219FC3C`) maps to `82470DA8`. It calls `822FA548`:

| Register | Value |
|---|---|
| r3 | `*(0x83318744)`, world |
| r4 | Actor / Pawn |
| r5 | Pointer to an FVector in guest memory, three big-endian floats |
| r6, r7, r8 | 0, the same as execSetLocation |
| Return r3 | Non-zero means success |

`822FA548` reads and updates the Actor's `+0xF8/+0xFC/+0x100`. These offsets were not guessed from common UE3 offsets. On-map value: `(-2868.7700, -3599.7004, 175.2687)`. Decompilation evidence is in `out/teleport-native.txt` and `out/teleport-move.txt`.

Calling this native function keeps collision checks, movement of attached objects, and component and touch updates. The backend does not write Actor.Location directly, and does not force no-collision. The destination may be refused or adjusted. Status and logs follow the native return and the actual coordinates.

The backend passes the FVector through guest heap memory allocated once for the purpose. It saves and restores the full PPCContext before and after the call. After the native call returns, it re-checks the current player identity, to handle scene changes triggered by touch callbacks.

## World / Level identity

The generated code for `822FA548` does get another object through world `+0x50`, then accesses its `+0x3C/+0x40` array data and count, and reads the world properties of the first Actor in the array. The backend uses this **Level context pointer**, together with world and pawn, to identify scene changes.

Calling it `PersistentLevel` fits this UE3 object structure. However, this round did not independently confirm the field name using a reflection field table. The implementation depends only on the observed pointer identity, not on the name. It cannot be claimed that all story changes within the same world/level/pawn will be detected this way.

## Game thread and disabling conditions

The two initial hooks (`82388BE0` / `82384D78`) never received a call on the live map. Further reading of the native registration string confirmed that `82384CC0` is actually `intAbhHUDexecBattleUpdate`. So `82384D78` is the battle HUD path, and the static battle call chain cannot be called a common game tick. The earlier draft conclusion on this point has been withdrawn.

Corrected entry `82290B60`: in PID 21524, `*(0x83315FB4)` engine=`0x350E600`, vtable=`0x8200D128`, and vtable `+0x108` actually points to this function. The function receives f1, calls `8231EE68` (world update, map-switch handling, and the global counter increment), and after returning checks the deferred map URL. Evidence: `out/teleport-engine-tick3.txt` / `out/teleport-engine-tick4.txt`. The hook saves the this pointer before the call and checks that it equals the current global engine. The teleport Tick runs only after the native return.

The build through this entry passed `out/teleport-engine-hook-build.log`. The main agent later confirmed on-site that the hook runs: in a 3.602 second log, this=`0x351E600`, caller=`0x822902A0`, and the command file began to get responses. This evidence confirms that the actual engine path was entered. The specific teleport result is validated separately.

- `0x832CB6B4` belongs to the state fields of bhHUD / battlecore (bhHUD+0x15D4, battlecore+0x15CC). It cannot be called a universal map type: on the walkable Hypocenter scene it is 0, and on Wasteland after battle it is 10. Currently only the observed 0 and 10 are allowed, combined with Pawn/controller and PlayerWalking / PlayerIdling checks. The battle path uses 2. This field alone cannot prove the player is controllable.
- Disabled when the player chain or the Pawn.Controller relationship is invalid, or when the character's pending-destroy flag appears.
- If world/level/pawn changes, or the tick has not updated for more than 1 second, the request is cancelled and the recorded position is invalidated.
- The UI only reads snapshots and submits requests. It does not read or write guest objects from the window thread.

The menu A/B check confirmed that the bhHUD state and `PlayerWalking` can both stay unchanged. So the backend also checks the native input-disable flag (see below). The bhHUD value or the existence of a Pawn cannot be equated with accepting input. Story, pause and other maps still need live regression.

`intAfcControllerexecSetPlayerControl` → `829EB0D8` → controller vtable `+0x49C`, with two bool parameters. On PID 40964's map controller, this virtual function is `82A1C0C8`: if bEnable is true it selects `PlayerWalking`, otherwise it selects `PlayerNoControl` and resets motion. Evidence: `out/teleport-control.txt`, `out/teleport-control-flags.txt`.

The backend additionally allows only the state names exactly `PlayerWalking` or `PlayerIdling`. The state-name getter at `822A95C8` provides the FName along the path `controller+0x18 → frame+0x1C → state+0x2C`. `822A9668` provides the name table `*(0x833690D0)`, the count `*(0x833690D4)`, and the UTF-16BE entry text at `+16`. Reads check the array bounds, and the numeric suffix must be zero. There is no fixed dynamic FName number, and no state-switch function is called.

PID 40964 on-site: controller `0x3096E00`, state `0x18D1D98`, FName `(0x32A5,0)`. The corresponding entry in table `0x33E0000` actually read `PlayerWalking`. These addresses are only evidence. This gate explicitly refuses `PlayerNoControl`. Later, PID 19040 recorded available=true at 45.053 seconds. After standing still, a read-only script read `PlayerIdling`. Walking-only caused the availability to change and repeatedly cleared the recorded coordinates, so the implementation was expanded to the union of the two confirmed states. The game menu can still keep `PlayerWalking`, so the control-state list cannot replace the menu input-disable flag described below.

## Background integration validation entry

Set `LO_TELEPORT_COMMAND_FILE` to a text file in a separate test directory. Serial numbers must increase strictly:

```text
1 status
2 save
3 offset 50 0 0
4 restore
5 absolute -2868.77 -3599.70 175.27
```

Each file holds only one command. Write the next command only after the log has finished. Commands use the same UI request interface and do not inject system keyboard or mouse input. `status` outputs whether it is available, the current position, and whether a recorded position exists. `native result` outputs the target and the actual coordinates. The coordinates above are only old on-site examples. They are not default destinations for all maps.

The target after adding the input and offset is limited to finite values, with each axis's absolute value not exceeding `MaxTeleportCoordinate=1000000`. This is a human-made debug guard, to keep very large input from breaking collision calculations. It is not an engine world range obtained by reverse engineering.

Minimum regression: map record → small displacement → restore → restart and reload. Refusal in battle and in the game menu. After a map change, old records cannot be restored. Invalid and non-finite coordinates are rejected. A target inside a wall is handled by native collision. After finishing, the person who actually ran the test appends the directory, screenshots and log results.


## Game menu input-disable state and native evidence

Read-only snapshots of the menu close and open on PID 29764 are saved as `out/teleport-menu-baseline.json` and `out/teleport-menu-open.json`, and the diff is `out/teleport-menu-diff.txt`. The menu does not change PlayerWalking, and bhHUD state stays 0. So the two checks above alone cannot block teleporting from inside the menu.

The key change: bit `0x20` of controller `+0x5C8` goes from 0 to 1, and the byte at controller `+0x639` goes from 0 to 1. The native `82A18080` does write the bool parameter into this bit. On entering the disabled-control branch, it saves Pawn `+0x54` into controller `+0x638`, changes the Pawn's physics state through a native virtual call, and records the mode in `+0x639`. On leaving, it restores the saved physics state and clears the mode. `82A151D8` queries this bit and mode, and `8235FCA0` skips the corresponding control branch when the query is true or the mode is non-zero. The full read-only decompilation is `out/teleport-menu-control.txt`.

The current implementation combines a conservative gate: if `controller+0x5C8 & 0x20` or `byte(controller+0x639)!=0`, the request is refused. It is judged together with the Walking/Idling list and the two-way Pawn relationship. The main agent is re-testing in `out/teleport-control-verified/`. The static and menu-diff evidence in this section is not the same as the integrated build having passed the menu-refusal test.

## Completed background game validation

Integrated build directory `out/teleport-control-verified/`, with runtime and test copy identical. It used a hidden SDL window, in-process input and the teleport command file. No desktop keyboard or mouse, and no window activation.

- At 63.901 seconds, `(896,1008,41.921627)` was recorded. At 65.899 seconds, the native teleport to `(1046,1008,41.921627)` returned success. At 87.096 seconds X/Y were still kept, and Z was adjusted by ground physics to 44.07163, not overwritten by the old position.
- At 89.107 seconds, return to the recorded point. The native actual coordinates matched the record exactly. Screenshots before.png, offset.png, restored.png.
- After opening the game menu, `game menu/control suspended` appeared. At 96.638 seconds, the offset request had accepted=false. After closing the menu, the old record was invalid, and restore was refused.
- Out-of-range coordinate 1000001 was refused. At 137.442 seconds, absolute coordinates `(896,1060,50)` teleported successfully.
- After that, normal walking with game input worked. At 143.442 seconds the coordinates became `(1274.8342,1086.3708,46.09128)`, screenshot walk-after.png, which shows that physics and movement continue to run.

The F1 menu code build passed. Following the user's convention in this round, the foreground menu was not opened. Test commands used the same request interface. Battle and tutorial boundary tests are in the next section. Cross-area map changes have not been validated.

### Battle/tutorial boundary and final build

In the same process, after teleport recovery it normally walked through the wreckage, obtained the ring, and reached the next encounter. During the ring tutorial at 301.051 seconds, available=false. At 400.270 seconds the position was recorded again on the map. At 404.835 seconds it entered the battle HUD state. At 429.471 seconds offset was refused, at 431.470 seconds restore was refused, and at 433.468 seconds status confirmed available=false and bookmark=false. Evidence: battle-disabled.png and run.log. No battle character was changed by teleport.

The measured backend binary SHA256 is `B5772F7F22E337938A2FE808A776D0816548D1F0A0196ED60FE621681F983D9D`. The only final change was to change the initial value of the coordinate input box from 0 to empty, to avoid teleporting to the origin when no coordinates are entered. The final build `out/teleport-final-build.log` passed, SHA256 `46D132782BA1E6615DC2995114A35FFE3F06760FEE94D06169E746E8328D2761`. The F1 interface was not opened in the foreground. Background commands were tested through the same request path. Not yet validated: all game maps, arbitrary targets inside walls, and all event states.

## Current map POI list (2026-09-04)

A POI dropdown list, landing coordinate and distance preview, and "Teleport to this POI" were added to the F1 teleport area. It only lists Levels already loaded in the current world. It does not provide cross-map loading. It refreshes every second, and when teleporting, it re-confirms that the Actor is still in the loaded list. The menu thread only reads snapshots and submits non-reused POI numbers.

The data basis comes from background read-only enumeration: UWorld+0x44/+0x48 is the loaded Level array. For each object, the Class (UObject+0x34) name is Level, and its Actor array is at +0x3C/+0x40. An Actor's Outer (+0x28) points back to the corresponding Level, and its FName is at +0x2C/+0x30. Hypocenter actually loads the Entry and eight Levels: u11_0_mapw/scrw/camw/sndw/colw/navw/lvdw. Enumerating only world+0x50 would miss map POIs.

Recognised classes: fgGimmickSavePoint (save), fnMapJumpPoint (exit), PlayerStart (entry/landing), fgGimmickAttackPoint (mechanism), fgGimmickTouchPoint and fiItem (pickup/touch). Dynamic hint fpNoticeActor, lights, volumes, and arbitrary dummy points are not counted as POIs. The Entry's spare PlayerStart at Z=-14924 was excluded. Zero-coordinate placeholders, objects marked for deletion, and invalid coordinates are also excluded.

Save markers use the effect/ground origin, for example (896,1008,-54.846), which cannot be used directly as the player's landing point. The backend prefers matching a nearby fdDummyPoint. Exits prefer matching a nearby PlayerStart, for example the Hypocenter exit corresponds to (9982.069,-3972.467,61.457). Other markers are moved up by 100 game units as candidates. If the native SetLocation refuses, it checks up to 8 candidates each at radius 128 and 256, keeps collision, and does not force clipping through walls. If all fail, it reports the refusal. The upward shift and search radii are debug landing policy, not engine fields inferred from the data.

The data is not a complete collection list. Areas not yet loaded, props not yet spawned by scripts, and unrecognised categories will not appear. The generic numbers are debug labels and do not pass as localised item names. Reaching a POI can still trigger encounters, story or map changes.

The first round, out/poi-verified, listed 20 points. Teleport to the save POI succeeded. The mechanism marker was refused by collision, and the character stayed in its original position, which led to the nearby landing search.

### POI integrated build background validation

Directory `out/poi-landing-verified/`, using an independent save/profile copy, a hidden window and in-process commands. No desktop input or window activation. The `run.log` and screenshots keep the following evidence:

- At 38.653 seconds, the 20 POIs in Hypocenter were listed. At 133.858 seconds, it teleported to the mechanism point. The original coordinate was refused by collision, and a landing point was found within radius 128 at `(6211.241,150.47495,121.66301)`. Screenshot `interaction-poi.png` shows the native A Ram prompt.
- At 149.857 seconds, return to the save POI succeeded. Screenshot `save-poi.png` shows the A Save prompt. The native landing used the anchor read at execution time, not the old coordinate from the first list.
- At 167.291 seconds the game menu was opened, which triggered control disable. At 175.017 seconds the POI request was refused. At 185.416 seconds the list was empty. After closing the menu, at 205.018 seconds the old number 33 was still refused. At 213.019 seconds the new list had 20 points, with numbers starting from 41, not reusing 21–40.
- At 228.220 seconds, teleport to near the exit succeeded, target `(9982.069,-3972.4673,61.456505)`. Native collision adjusted the actual Z to 76.17852. Screenshot `exit-poi.png` shows the exit passage and the standing character. "Near the exit" was not equated with completing a map change.

The final build `out/poi-final-build.log` succeeded, SHA256 `2D9A5696DDD48B77593792FF61D2BBBD89F566F319940D0E26D5D488BD492BD5`. Compared with the measured version, it only increases the menu window height, to avoid clipping the bottom help text. The backend is the same. The F1 window was not opened in the foreground. Background commands used the same request interface. The whole game map and all types of landing points have not been validated. This list only guarantees the currently loaded and recognised candidate points. Actual teleporting is still decided by native collision and control state.

## Foreground menu regression (2026-09-05)

A normal application window style was added, so the debug panel can be switched independently from the taskbar. The display label refreshes only when the text changes, to avoid a brief blank caused by repeated WM_SETTEXT. `LO_DEBUG_MENU_OPEN=1` is an optional startup open entry. Normal startup still does not open it.

Gorge independent save copy in `out/map-info-current-01/window.log`: actual clicks on "Remember current position", X +100, and "Return to recorded position". The native result moved to X=-3369.0159 and returned to X=-3469.0159. The three-axis coordinates on return were exactly the same as the record. Foreground button enabled/disabled state, status and coordinates were checked with screenshots.

`out/map-info-hypocenter-01/final.log` uses the final build, which changes the label to refresh on demand. In Hypocenter, "Teleport to this POI" was clicked to the exit area. After native collision, the character stood at X=9982.069, Y=-3972.4673, and the interface showed "Teleport complete". The earlier background boundary tests remain valid.

Closing F1 from inside the panel was verified this round. Sending F1 to the game window by automation did not produce an SDL keyboard log, so this automatic input is not counted as passed. Startup opening and mouse buttons were verified. The original SDL shortcut definitions were not changed.
