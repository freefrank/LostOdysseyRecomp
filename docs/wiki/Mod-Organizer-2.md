# Mod Organizer 2 and external managers

The runtime implements a manager-neutral overlay contract. It does not ship a LostOdysseyRecomp MO2 game plugin, and real Windows MO2/USVFS operation has not been established by the standalone tests. Treat this page as the integration contract and acceptance checklist, not a claim of turnkey MO2 support.

## Package contract

Compile an overlay package:

```sh
python tools/modding/lo_mod.py pack my-menu/mod.json --layout overlay --output my-menu-overlay.zip
```

The ZIP contains paths such as:

```text
mods/overlay/images/key-fnv1a64-<16 lowercase hex digits>.lotex
```

The hash is derived from the canonical resource key, not from texture contents or a runtime fingerprint. Two mods targeting the same identity use the same path. There is no shared `manifest.ini` that competing packages can accidentally replace. The runtime validates the embedded full key after selecting the visible file.

## Runtime setup

A MO2 game integration/root mapping must project the package's `mods/` directory into the root actually opened by LostOdysseyRecomp. Launch the actual game executable through that managed process so the VFS applies. Installing files into an unrelated conventional `Data/` directory will not work.

Set the managed process environment to:

```text
LO_MODS_MODE=overlay
LO_MODS_DIR=<absolute path to the runtime-visible mods directory>
```

The directory override is optional only when the mapping exactly matches the application's portable or installed default. It points to `mods`, not `mods/overlay` and not MO2's directory holding separate installed mod packages. The project does not currently include a plugin or installer to configure this mapping automatically.

In overlay mode, the runtime sees the external manager's winning file. It ignores standalone `mod.ini` priorities and trusted providers entirely. A missing visible overlay falls back to the original asset, preventing a second standalone installation from silently reviving a disabled mod. Invalid selected payloads also use the original.

Close/restart the game after enable/disable/order changes. The runtime has no filesystem watcher. A future integrated manager may call the host reload API at a safe boundary.

## Other managers and Steam Deck

A manager may materialize the same overlay on disk without a VFS. For Linux/Steam Deck, deploy files under the selected mods root and use overlay mode. Remove no-longer-enabled winning files when updating the materialized view, and update it while the game is closed. Preserve unrelated mods and imported game data.

This describes native filesystem deployment; it does not claim that Windows USVFS works with a native Linux executable. Mod installation tooling and runtime image consumers are separate compatibility requirements.

## Windows acceptance still required

Use two deliberately different replacements for the same supported native-menu key. Verify that launching through a real MO2 instance exposes the mapped file, changing MO2 order selects the other artwork, disabling both restores the original, and a standalone/provider copy cannot reappear in overlay mode. Repeat with a non-ASCII installation path. Confirm imported archive hashes do not change, and verify a normal un-managed launch separately.

The runtime rejects symlinks that escape the selected root. VFS filesystem-query behavior must be tested with these containment checks; do not disable path validation merely to claim compatibility. General guest GPU textures still require the separate renderer work described in [runtime texture replacement](Runtime-Texture-Replacement.md).
