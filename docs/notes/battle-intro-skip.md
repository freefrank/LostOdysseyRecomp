# Battle intro skip

Debug Menu (F1 or LB+RB) → Cheats → **Battle** → **Skip battle animation**:

| Value | Key `battle_intro_skip` | Behavior |
|---|---|---|
| Off (default) | 0 or missing | Retail. No hook changes guest state. |
| Press to skip | 1 | In a random walking battle, a prompt "(cancel button): Skip" shows from the battle request until the first command menu. A press of the game's cancel button skips the intro. |
| Always | 2 | Every random walking battle skips its intro, with no prompt and no press. |

Boss, story, scripted, world map, retry and debug battles keep the retail intro and show no prompt. The value saves at once, like the other Debug Menu values. The game thread reads it when a battle is requested and when the intro actor spawns.

Code: `patches/battle_intro_skip.cpp` (hooks), `patches/battle_intro_skip.h` (pure decisions), `patches/battle_intro_gate.h` (prompt API), `hid/skip_latch.h` (input latch), `debug/battle_intro_skip_setting.*` (option), `settings/title_entry.cpp` and `settings/menu_render.cpp` (prompt drawing).

## What the intro is

The intro is battle phase 0. It is not a Matinee. The battle core is at `0x832CA0E8`: phase at `+0x38`, phase request at `+144`, mode at `+0x15CC` (2 = battle running).

- `sub_8238F198` (SetMotion tick) spawns the phase script actor with `sub_82390408`. For phase 0 the class name is formation `+72` (formation table `0x83264978 + 128`, 160-byte rows). The actor pointer is at `0x832CB560`, its slot at `0x832CB550 + 32` (1 = intro).
- The class is `bsSMpha_Start` (335 rows), `bsSMpha_StartNoVoice` (58) or `bsSMpha_Start_OPCrowd` (6). The data is the same on all four discs.
- The UnrealScript state `PHASE_ON` makes the pawns visible (`Hide(false)`), plays the entry motions, camera shots, footsteps and voices, and ends with `SwitchBattleUI(1)`, `ChangeBattlePhase(1)`, `stop`. `ChangeBattlePhase` (`sub_82AE1020`) writes the phase request, and the dispatcher `sub_8238E3D8` moves to phase 1, then 2 (first command menu).
- The script holds time only with `Sleep` (native `sub_82470968`, latent time at `actor + 236`, latent op 384). Its `Wait` calls (`sub_82AE2AF0`) hold on resource or state conditions (types 19-22 wait for pawn loaders and motion packages).

Forcing phase 1 from phase 0 is not safe: pawns stay hidden and the script keeps running. `OPCrowd` waits for a matinee and is never skipped.

## How the skip works

The skip lets the script run every statement. It only removes the waiting:

1. **Sleeps.** After the press (or the Always start), each intro `Sleep` ends on the next tick (`actor + 236 = 0`). The running Sleep ends at once when its latent op is 384 and no Wait is active.
2. **Loading.** The remaining time was resource loading at a rate per frame (about 190 frames at any frame rate). While the skip is active, the resource queue `sub_82385300` ticks 8 times per frame, and `sub_82322360` calls the engine's own flush `sub_8237CC40`. All Waits stay. Frames can hold for up to about 150 ms during this.
3. **Sounds.** All phase 0 sounds go through the battle sound manager entry `sub_82B1ACA8` (manager `0x832CC0FC`). Music is a stream that starts before phase 0 and does not pass this entry. The hook records the intro sound handles. After the press it returns -1 for new phase 0 sounds (every caller accepts -1). Playing sounds of kind 1 or 4 fade with `sub_82B62048(sys, handle, 2, 0, 0.25)`; sounds not started yet get state 3. A hard stop with `sub_82B1A518` follows 0.5 s of guest time later. `sub_82B62048` does not check the handle generation, so the hook checks the slot state, generation, status and release bit just before each call. Kind 3 (music) is never touched.

Phase 2 then starts with the retail state: all pawns visible, the camera cuts to the command camera, and the battle UI opens on its own trigger (the first phase 2 tick in `sub_82388BE0`). The encounter effect (purple, about 1.2-2.1 s after the request) can show over the first menu for a short time after an early press.

## Random battles only

`RequestBattle` (`sub_828278A0`, hooked in `patches/encounter_defer.cpp`) records each accepted request. Only the walking draw (return address `0x829E3220`) counts as random. Script opcodes, the world map script, `BATTLEREBOOT` (retry), the opening battle and the battle tour are not random. The battle tour also uses the walking draw, so `debug/no_encounters.cpp` marks its formation. The world map controller turns the walking draw off (vtable `+1632` stub), so world map and sea battles never count. The spawn uses the record once, and only when the formation matches the battle ID.

Other exclusions: the `OPCrowd` class, an active matinee (`*(0x832CBFB0 + 24)`), and battle IDs -1, 255 and 256.

## Press to skip: window, press and latch

- **Window.** It opens at the accepted random request (formation class read from the table), before the encounter effect starts. It stays open into the intro and closes at the press or at phase 1. It also closes on a new request, a slot rewrite, a hold or drop by `encounter_defer`, or when no battle spawns within 8 s. A held random battle opens the window again at the spawn. A watchdog in the engine tick closes everything when the battle tick stops (soft reset).
- **Stored press.** A press before the intro actor spawns is stored and applied at the first phase 0 tick where the skip is allowed, the same point Always uses.
- **Latch** (`hid/skip_latch.h`, in `hid::GetState`). While the window is open, the first new press of the cancel button fires once. The bit is then hidden from the guest in every pad read and every source (pad, keyboard, touch, test input) until the button has been up for 250 ms. A press inside that time restarts the wait. The latch needs one up sample after it is armed, so a button held before the window does not fire. It never ends on a timer alone, focus loss, battle end or map load. Without the latch, B undoes the previous character's command in the first menu (with B/A, the press would select Attack).
- **Cancel button.** The game's own option `config + 4 & 0x02000000` (Confirmation button A/B or B/A). The game applies a change at once, without a restart, and so does the latch. The prompt icon follows it.
- **Prompt.** A host overlay in the title hint style (game font Maru23, face (196,197,194), black outline, no panel) at the bottom right of the 16:9 area. It uses the icon family of the active device (`hid::PromptFamily()`), or the bound key name with keyboard prompts (no prompt if the key is unbound). It fades in over 250 ms with no delay, hides at once, and changes opacity only. It is redrawn only when the size, language, family, cancel bit or key changes. Text: "Skip" in all UI languages.

Only keys bound to game input switch the prompts to the keyboard style (`hid::HandleKeyboardEvent`). Before this branch any key did, so a screenshot key changed a pad player's icons.

## Diagnostics and tests

- `LO_TRACE_BATTLE_INTRO=1` logs the intro natives, the gate, the window, the press and the sound actions. It only reads guest memory. Unset, each hook costs one cached bool.
- `LO_SKIP_LATCH_TEST=arm|once` arms the latch for input tests. `LO_TRACE_INPUT` logs latch events.
- `tools/battle_intro_test.py` runs the game with an isolated save copy (`bwrap`, the player's folders read-only, checksum check). It starts formations or walks to random encounters, presses at a set time (`--press-at`, `--press-at-request`), plays real turns (`--real`), takes screenshots and writes a JSON summary per battle.
- Unit tests: `LoBattleIntroSkipTest`, `LoSkipLatchTest`, the prompt checks in `tools/tests/settings_menu/raster_test.cpp`, the Debug Menu tests, and the key check in `LoHidTest`.
- Known issue, also on `main`: `LoHidTest` fails at "virtual non-PS controller retains regular prompts" when a real PlayStation pad is connected, because the test takes the real pad as the active device. It passes with the pad hidden: `SDL_GAMECONTROLLER_IGNORE_DEVICES=0x054C/0x0CE6`.

Results on Linux (AMD RX 580, Vulkan, 60 fps):

| Case | Result |
|---|---|
| Retail intro, random battles | 7.8-11.7 s to the first menu |
| Press during the intro | First menu 70-400 ms after the press |
| Press 50-200 ms after the request (stored) | First menu 1.0-1.5 s after the request |
| Always | First menu 0.66-1.0 s after the spawn |
| Soaks (Press, Always, Off; 34 + 40 battles) | All won and returned to the field; the guest never saw a skip press; no sound slot leak; music unchanged; RSS flat after the first battle |
| Boss 157, tour, scripted, Off | Retail intro, no prompt |

Not tested: Windows, macOS and Android; frame generation (the prompt fade is opacity only); fast-forward during the skip (at high rates the window can close before the player sees it); 30 fps with Press; the B/A option on real hardware; world map and sea battles; disc 4 maps and the disc 4 music formations (243-247, 282-284); a soft reset inside the early window.
