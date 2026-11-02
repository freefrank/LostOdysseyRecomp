# RecompLauncher integration

Portable Windows ZIPs built from this source include [`recomplauncher.json`](../recomplauncher.json) beside
`LostOdysseyRecomp.exe`. RecompLauncher reads it in preference to its catalog or boot profile.
It provides a folder-based setup step, graphics settings, and buttons for saves, mods and logs.
For delivery and validation status, see the [status ledger](STATUS.md#recomplauncher-integration).

## Set up and play

1. Install the Windows release through RecompLauncher.
2. Choose a source folder containing your supported Lost Odyssey discs: ISO images, extracted
   folders or GOD packages. Disc 1 is required for a new installation; later discs and optional
   DLC can be in the same folder or a subfolder. Use one edition and one source per disc.
   Keep unrelated games and title-update packages outside this source folder.
3. The launcher's setup screen runs our importer without opening another window. It validates
   disc identities and edition compatibility, then imports to `game/disc1` through `game/disc4`
   and `game/dlc`. `game-path.txt` remembers that installation.
4. Change graphics options on the game's page and press Play. The game still prepares shaders
   during its normal startup. The in-game importer remains available for additional content.

Use a separate source folder. Do not put source ISOs inside the destination `game/` folder.
Selecting the destination itself is supported only to adopt an already imported installation.
The importer must be able to read its sources for the duration of setup. On Windows it may need
substantial extra free space while replacing existing content; old selected slots remain until
publication and game-path persistence succeed. Repeating setup reimports the selected content.
Unselected discs, saves and graphics preferences remain in place.

## Graphics settings

The profile exposes the game's persisted graphics controls: backend, window mode, common output
sizes, aspect ratio, internal resolution, shadows, anti-aliasing/upscaling, provider quality,
DLSS model and optional neural-rendering tuning, FSR sharpness, frame generation, ambient
occlusion, anisotropic filtering, frame rate, loading transitions, display synchronization,
performance overlay, spatial scaling, depth of field, bloom, motion blur, scene fades, culling,
RGB range, black level, gamma and HDR preferences.

- Anti-aliasing and upscaling share a row, like the in-game menu. Selecting ordinary AA also
  disables upscaling; temporal providers retain the ordinary AA fallback.
- Off and On clear `variable_refresh_rate` as well as setting `vsync`. Adaptive synchronization
  enables `variable_refresh_rate` and retains the ordinary V-Sync preference, like the game.
- FSR and XeSS frame generation are fixed at 2×. DLSS offers requested multipliers 2×–6×,
  subject to hardware and driver support. Choosing an enabled provider selects fixed mode;
  the game normalizes FSR/XeSS to 2× while retaining the requested DLSS multiplier.
- XeSS requires DirectX 12. Select that backend first. DLSS needs compatible NVIDIA hardware;
  neural rendering additionally needs the player's `nvngx_dlssnr.dll`, which is not bundled.
  The profile cannot query hardware capabilities, so a visible choice does not promise support.
- DLSS, FSR and XeSS have separate quality rows, enabled only for the selected provider.
  FSR and XeSS share the persisted `fsr_quality` value. Tuning rows also use `when` conditions;
  the game remains responsible for capability checks, fallback and value normalization.
- GPU/display enumeration, display-specific modes, graphics presets and interactive calibration
  stay in the game. Numeric launcher choices cover common values; use the game for finer tuning.
  Values outside those choices cannot be represented exactly by the launcher's dropdowns.

The launcher edits `settings.ini` before starting the game. The game's settings readers accept
both `key=value` and `key = value`, including CRLF files. Numeric values are persisted IDs from
[`settings/config.h`](../LostOdysseyRecomp/settings/config.h), not translated labels. Keep the
profile synchronized with [`settings/config.cpp`](../LostOdysseyRecomp/settings/config.cpp).

## Headless setup command

```text
LostOdysseyRecomp.exe --prepare-content --source "D:\My discs" --destination "D:\Lost Odyssey\game"
```

`--source` can be repeated. Paths are decoded as UTF-8 after the Windows command line is read
as Unicode. The command runs before logs, updates, mods, windows or renderer initialization.
It uses the same scanner and transactional importer as the game, and commits `game-path.txt`
only after content publication. A failure to persist that path rolls the selected content back.
It does not download or compile shaders, change graphics preferences, or start the game.

Progress is written and flushed to stdout as `progress 0.4 Importing Disc 2`.
Failures go to stderr. Exit codes are 0 for success, 1 for an import failure, 2 for malformed
arguments and 3 for cooperative cancellation (SIGINT/SIGTERM). The profile allows up to two hours
for setup. The command is available on desktop builds; this launcher profile targets Windows.

A launcher that forcibly terminates the process cannot run its rollback handlers. Keep the
source files and any `.import-staging-*` folders after an interrupted publication; do not delete
staging folders blindly because they may contain old slots. The existing importer lock prevents
simultaneous import operations.

## Verification and save backups

The launcher developer checker calls this level 3 because it has a real `prepare` step.
**That does not mean the source folder has a verified-dump badge.** The current launcher only
compares `game.dumps` CRC32/size entries against one selected file. A folder containing several
ISOs cannot receive that badge. The importer checks supported XEX identities and edition
compatibility and the content needed to import the game. This validation is sufficient for the
supported setup flow. The clean-dump badge is optional and belongs to the launcher; this
integration adds no whole-disc integrity certification to the game.

**The current launcher excludes gameplay save data from its backups and cloud copies.** The
profile declares the correct folder, `{install}/save`, but the launcher filters out `.bin` files,
including `save/userNN/save.bin`. It can still copy and upload `__thumbnail.png` images from the
same slots and show a successful backup. "Same as This PC" compares the files it selected; it
does not establish that gameplay progress is protected. Back up the complete `save/` folder manually.

The profile cannot bypass that filter. Save backup/restoration and multidisc clean-dump badges
are launcher-side concerns, outside this game patch. Test both backup and restoration before
claiming gameplay save support after a launcher-side fix.

## Platforms and packaging

This profile targets the portable Windows ZIP only. `tools/package_release.py` copies it beside
the executable and includes its checksum in `manifest.json`. It does not ship `settings.ini`.
Existing settings are preserved when a release is installed over the game's folder.

AppImage, Flatpak, macOS and Android packages do not ship this profile. Their game paths and
per-user configuration directories differ; the Windows profile must not be copied to their
catalog entries unchanged. The shared headless command has Linux fixture coverage, but Linux
packaging/profile integration and an end-to-end Linux launcher test remain separate work.

## Validation

- Run `tools/tests/package_release_test.py` to check ZIP placement and the manifest checksum.
- On Linux, run `tools/tests/recomplauncher_config_test.py --out out/launcher-config` to compile
  production settings bodies and round-trip every declared choice, including spaced INI lines.
- Build `LoImportGameTest` and run `--prepare-content-test` for synthetic headless-import tests.
- In RecompLauncher, enable **Settings → Developers → Developer tools**, add the extracted build,
  and use **Check again** on its Developer tab. A missing `game.check` is expected: the folder may
  contain several source formats, and our preparation command performs the authoritative scan.
- Test a fresh installation and a previously configured game through the launcher on Windows.

Upstream contract: [developer guide](https://github.com/elripalda/RecompLauncher/blob/main/docs/DEVELOPERS.md)
and [schema](https://recomplauncher.com/schema/recomplauncher.json).
