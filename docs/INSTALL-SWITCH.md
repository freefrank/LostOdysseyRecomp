# Lost Odyssey Recomp for Nintendo Switch: installation

An **experimental** homebrew build of [Lost Odyssey Recomp](https://github.com/freefrank/LostOdysseyRecomp) for Nintendo Switch consoles running custom firmware. It runs the recompiled game natively on the console. You need your own Lost Odyssey discs; this download contains no game files.

> **Expect rough edges.** The game is playable, but the Switch's graphics chip is the limit. At stock clocks in handheld mode, battles and lighter areas run at 30 fps, and the heavier field areas at about 20–25 fps. Crashes and missing effects are possible. Please report problems with your logs (see [Reporting a problem](#reporting-a-problem)).

## Contents

1. [What you need](#what-you-need)
2. [Step 1: Extract your discs on a PC](#step-1-extract-your-discs-on-a-pc)
3. [Step 2: Copy everything to the SD card](#step-2-copy-everything-to-the-sd-card)
4. [Step 3: Start the game](#step-3-start-the-game)
5. [Settings](#settings)
6. [Updating](#updating)
7. [Troubleshooting](#troubleshooting)
8. [Reporting a problem](#reporting-a-problem)

## What you need

- **A Switch with custom firmware:** Atmosphère with the Homebrew Menu (hbmenu).
- **A microSD card formatted as exFAT** with about **25 GB free**. The game data alone is about 21 GB.
- **Your own Lost Odyssey discs** (Xbox 360), as disc images (ISO), Games on Demand data or extracted folders. Two editions are supported, and their discs must not be mixed:

  | Edition | Disc media IDs (1–4) |
  |---|---|
  | Asian multilingual | `39F7D748`, `0EF8CEA8`, `309E3386`, `7B21A91D` |
  | USA/Europe | `368DE6DD`, `1888BE4E`, `6DD59D08`, `0C0E80B5` |

  Title updates and modified XEX files are not supported.
- **A PC (Windows or Linux)** to extract the discs with the PC version of Lost Odyssey Recomp.
- **The files from this release**, already in the folder layout of the SD card:
  - `LostOdysseyRecomp.nro`: the game.
  - `env.txt`: the recommended settings for this release.
  - `shaders/portable_vk.lospv`: the game's shaders.
  - `shaders/deko3d_shaders.lodk`: the same shaders, compiled ahead of time for the Switch's graphics API.
  - `shaders/pipelines_corpus.bin` and `cache/shaders/pipelines_vk12_1.bin`: lists of the game's graphics states, so they are prepared before you need them.

> **Shaders and game edition:** this release's shaders were built from the **Asian multilingual** edition. The USA/Europe edition should also run. If some objects or effects are missing with it, a shader the pack lacks is the likely cause; please report it.

## Step 1: Extract your discs on a PC

The Switch can't import discs itself, so use the PC version's importer to prepare them:

1. Download the PC version from the [Lost Odyssey Recomp releases](https://github.com/freefrank/LostOdysseyRecomp/releases/latest) and start it. The importer opens when no game is found.
2. Choose **Files** or **Folder**, select your disc images or dumps, check what it found, and confirm. Import **all four discs** if you have them; only disc 1 is required to start.
3. When it finishes, the game is in a `game` folder containing `disc1`, `disc2`, `disc3` and `disc4`. With the Windows ZIP, that folder is next to `LostOdysseyRecomp.exe`. Each disc folder holds `default.xex`, `LO.fpi` and the `.fpd` archives.

You don't need to play the PC version; it's only used to extract the discs.

## Step 2: Copy everything to the SD card

1. Extract the release ZIP into `sdmc:/switch/LostOdysseyRecomp/`.
2. Copy the **whole `disc1`…`disc4` folders** from the PC's `game` folder into `game/` on the card.

The card should then look like this:

```
sdmc:/switch/LostOdysseyRecomp/
    LostOdysseyRecomp.nro
    env.txt
    game/
        disc1/      default.xex, LO.fpi, LO.fpd, xenon_*.fpd
        disc2/
        disc3/
        disc4/
    shaders/
        portable_vk.lospv
        deko3d_shaders.lodk
        pipelines_corpus.bin
    cache/
        shaders/
            pipelines_vk12_1.bin
```

- The PC importer stores files that are identical on several discs only once. When they're copied to an exFAT card they take up their full size again, so the card needs the full ~21 GB.
- Settings, saves, caches and logs are created in the same `LostOdysseyRecomp` folder the first time you play.

## Step 3: Start the game

1. Start the Homebrew Menu **through title takeover**: hold **R** while starting any installed game, then pick **Lost Odyssey Recomp**.
2. **Don't start it from the Album.** The game needs the console's full application memory, which the Album doesn't give it, so it stops with a message there.
3. After the game starts, it takes about 40 seconds to prepare its graphics states in the background. Loading a new area or a battle can take 10–20 seconds.
4. The first time you enter an area, the game may compile a few shaders in the background, so it can briefly run slower. They are saved in `cache/deko3d_runtime.dkcache`, and the next visit is faster.

## Settings

### `env.txt`

`env.txt` holds settings that aren't in the game's menus. Each line is `NAME=value`, and lines starting with `#` are comments. The file in this release:

```
# Render with deko3d, the Switch's own graphics API (faster than NVK).
# Delete this line to use the Mesa NVK driver instead.
LO_SWITCH_BACKEND=deko3d
# deko3d's GPU savings: depth compression, depth culling, fewer GPU waits, leaner vertex shaders
LO_DEKO3D_GPU_FAST=1
# Dynamic resolution stays off in this release: with deko3d it still shows stripes
LO_DYNAMIC_RESOLUTION=0
# Render the 3D scene at 960x540, also when docked (720 is not yet tested with deko3d)
LO_SWITCH_RENDER_HEIGHT=540
# Use the native cutscene video decoder (faster, smoother cutscenes)
LO_NATIVE_MOVIES=1
# Spread the game's threads across the Switch's three CPU cores
LO_GUEST_AFFINITY=1
# Faster handling of the game's visibility tests
LO_ZPD_MODE=xenia
# Prepare every known graphics state in the background at start
LO_PIPELINE_PREBUILD=all
```

- **Docked at 720p:** delete the `LO_SWITCH_RENDER_HEIGHT=540` line. The scene then renders at 1280×720 when docked. This hasn't been tested with deko3d yet; if it shows problems, put the line back.
- **NVK instead of deko3d:** delete the `LO_SWITCH_BACKEND=deko3d` line. NVK is slower, and on its first start it builds a large driver cache.

### In the game

Open **Settings → Graphics → Graphics preset** and choose **Low-end**. It turns off dynamic shadows, motion blur, depth of field and bloom, which gives about 10–20% more speed. Choose **Original** to get the full effects back.

### Overclocking

The graphics chip is the limit, so a GPU overclock (for example with sys-clk) helps most. Overclock tools apply their settings per title ID, and a homebrew app runs under the title ID of the game you launched it from with title takeover, so set the profile for that title ID. Only overclock within what your console and cooling can handle.

## Updating

1. Replace `LostOdysseyRecomp.nro`, `env.txt` and the files in `shaders/` with the ones from the new release. The `.nro` and the shaders must come from the same release.
2. Keep everything else: your saves, settings and the `game/` folder.
3. **Coming from v1.0.6C or older:** two old files are no longer needed and can be deleted to free about 200 MB:
   - `cache/shaders/pipeline_cache_vk.bin`, NVK's driver cache, unless you switch back to NVK;
   - `cache/lsfg-vk-pipeline-cache.bin`.

## Troubleshooting

| Problem | What to do |
|---|---|
| A message about memory, or the game closes right away | Start it through title takeover (hold **R** on a game), not from the Album. |
| "Game not found" or a crash at start | Check that `sdmc:/switch/LostOdysseyRecomp/game/disc1/default.xex` exists, with that exact folder layout. |
| Vertical stripes, dark blocks or bands over the picture | Check that `env.txt` has `LO_DYNAMIC_RESOLUTION=0`. |
| Objects or effects missing, invisible characters | Check that `shaders/` has both `portable_vk.lospv` and `deko3d_shaders.lodk` from the same release as the `.nro`. With the USA/Europe edition, report it (see the shaders note above). |
| A graphics problem that looks like a bug | Try NVK by deleting `LO_SWITCH_BACKEND=deko3d` from `env.txt`. Report whether it happens with both or only with deko3d. |
| The game asks for another disc | Copy all four disc folders. The game changes discs by itself when they're present. |
| Very low frame rate | Choose the **Low-end** preset. A GPU overclock helps most. |
| No cutscene video | Remove `LO_NATIVE_MOVIES=1` from `env.txt` to use the original decoder, and report it. |

## Reporting a problem

For a detailed log, add `LO_DEBUG_LOG=1` to `env.txt` and play until the problem happens. The detailed log makes the game slightly slower and writes large files, so remove the line afterwards.

In `sdmc:/switch/LostOdysseyRecomp/` you'll find:

- `state/logs/runtime-*.log`: the game's log (the newest one is the last run).
- `stderr.log`: graphics messages.
- `crash.log`: written when the game crashes.
- `hang.log` and `exit.log`: written when the game freezes, or doesn't close properly.

Attach these to your report, together with:

- the release version;
- your game edition;
- handheld or docked;
- whether you use an overclock;
- your `env.txt`;
- what was on screen when it happened. A screenshot helps.

Logs can only be copied after the game has closed.

## Source and licence

This build is made from the source in [Ad0ky/LostOdysseyRecomp](https://github.com/Ad0ky/LostOdysseyRecomp), branch `LostOdysseyNX`. It is based on [freefrank/LostOdysseyRecomp](https://github.com/freefrank/LostOdysseyRecomp) and follows its licence (GPL-3.0). Build instructions are in [docs/SWITCH.md](https://github.com/Ad0ky/LostOdysseyRecomp/blob/LostOdysseyNX/docs/SWITCH.md).
