# Scenarios

Scripted in-game runs for `tools/scenario.py`:

```bash
python3 tools/scenario.py list tools/scenarios/*.toml
python3 tools/scenario.py run tools/scenarios/*.toml            # all
python3 tools/scenario.py run tools/scenarios/numara.toml       # one
```

## Regression run

`tools/regression.py` runs every scenario here and compares each one with a local
baseline (pass/fail, median FPS, median GPU time, final screenshot):

```bash
python3 tools/regression.py run                     # all scenarios, about 40 minutes
python3 tools/regression.py run --only numara attract
python3 tools/regression.py run --accept            # make a passing run the baseline
python3 tools/regression.py accept out/regression/runs/<timestamp>
```

The report is `out/regression/runs/<timestamp>/report.md`. A scenario fails when it
fails its own checks, loses more than 5% FPS, or its final screenshot (reduced to
160x90) differs from the baseline by more than `max_image_diff` (default 40, mean
absolute difference) or changes brightness by more than 30. GPU time more than 20%
above the baseline is a warning (capped scenes vary about 15% between runs). The baseline stays in `out/regression/baseline` and
is never committed; accept a new one after an intended visual or performance change.

## Single scenarios

Each run gets its own folder under `out/scenarios/<timestamp>/<name>/` (runtime log,
stdout, screenshots, `result.json`) and never touches the game's own saves, settings
or logs. The shader cache next to the binary is shared.

## File format

```toml
description = "What the run covers"
save = "user17"      # slot folder from --saves (default out/drive-city/save); omit for no save
timeout = 150        # seconds; reaching it is the normal end of a run
buttons = "s@300,a@900"   # swap-stamped presses (LO_AUTO_BUTTONS): s a b x y, u/d/l/r d-pad
timeline = "s@10,a@30"    # the same letters at seconds since launch; use this when the
                          # frame rate differs from 30 FPS (swap counts then drift)
pulse = 6            # polls per stamped press (default 6)

[[step]]             # event-driven steps, in order
wait_draws = 700     # wait until a frame reaches this many draws (scene loaded)
wait_seconds = 0     # and/or this many seconds since launch
press = "a"          # buttons: a b x y start back lb rb up down left right, joined with +
stick = [0, 28000]   # left stick x, y
polls = 600          # how long the press/stick is held (input polls)
hold_seconds = 20    # time before the next step
screenshot = true

[settings]           # settings.ini overrides; runs start from frame_rate=30,
antialiasing = 3     # internal_resolution=0, antialiasing=0, upscaler=0,
                     # window_mode=0, variable_refresh_rate=0

[env]                # extra environment variables for the game
LO_SCREENSHOT_PRESENTED = "1"   # screenshots of the presented image, not the guest frame

[checks]
min_draws = 700      # the scene was reached
compare_image = true # regression.py: compare the final screenshot with the baseline
max_image_diff = 40  # regression.py: allowed mean difference (0-255)
min_fps = 25         # median of the second half of the run
game_time_ratio = [0.95, 1.05]  # game seconds per real second (needs
                     # LO_FRAME_TIMING_SUMMARY=1 in [env]); catches game logic tied to the frame rate
max_errors = 0       # [error] log lines allowed
```

On the Switch, `tools/switch/bench.py` runs the same files as benchmarks (see
[docs/SWITCH-BENCHMARKS.md](../../docs/SWITCH-BENCHMARKS.md)). A `[switch]` table
overrides what differs on the console: `buttons` (frame stamps, or seconds as `a@30s`),
`start_draws`, `start_swap`, `stick`, `settle`, `frames` and `timeout`.
With the console's debug server on, `scenario.py run --switch <ip>` and
`regression.py run --switch <ip>` run them as on PC, live, over the network (the
`[switch]` table's `buttons` apply there too; see "Debug server" in
[docs/SWITCH.md](../../docs/SWITCH.md)).

A run fails on a crash report, an early exit, a guest bug check, a disc failure, a
shader compile failure, error lines above `max_errors`, unfinished steps, or a missed
draw/FPS target.

## Saves

Saves are personal and never committed. These scenarios expect Xenia saves converted
with `tools/import_xenia_saves.py` (folder `userN` is in-game slot N-1):

| Slot folder | Location |
| :--- | :--- |
| user15 | Disc 1, ship cells after the jail sequence |
| user16 | Disc 2 start, Crimson Forest shrine |
| user17 | Disc 4 world map, Northern Shore of Ipsilon |
| user18 | Seeker of the Deep! DLC dungeon (needs the DLC imported) |

`new-game-battle` needs no save: it starts a New Game, records the opening movie and
plays one attack in the first battle.
