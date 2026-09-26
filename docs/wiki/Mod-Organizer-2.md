# Mod Organizer 2 compatibility

LostOdysseyRecomp's mod ABI is designed so Mod Organizer 2 can manage files without understanding FPD/UE3 archives.

## Principle

MO2 owns installation, enable/disable state, ordering and file conflicts. LostOdysseyRecomp consumes the final filesystem view presented to the process.

Do not duplicate MO2's priority system inside a mod package. If two MO2 mods provide the same virtual path, the file visible through MO2's VFS is authoritative.

Native/non-MO2 installations continue to use LostOdysseyRecomp's normal `mods/` discovery.

## Recommended MO2 package layout

Each MO2 mod installs files below one common runtime overlay root:

```text
mods/
  overlay/
    manifest.ini
    textures/
      <runtime-fingerprint>.lotex
    fonts/
    models/
    movies/
```

The important property is that competing mods use the same destination path for the same resource. MO2 can then report and resolve the conflict itself.

Example:

```text
mods/overlay/textures/fnv1a64-0123456789abcdef.lotex
```

A second enabled mod replacing the same texture supplies that same virtual path. LostOdysseyRecomp sees only MO2's winning file.

## Runtime lookup order

For manager-neutral behavior:

1. Check the merged `mods/overlay/` namespace.
2. Fall back to standalone per-mod manifests under `mods/<mod-id>/mod.ini`.
3. Fall back to the original game resource.

The merged overlay has precedence because an external manager has already resolved conflicts.

## MO2 executable setup

The game executable must be launched through MO2 so its VFS is active. Configure the LostOdysseyRecomp executable itself as the managed executable; mods must not patch the imported game archives.

No MO2 plugin is required for the basic ABI. A future optional MO2 plugin may add resource browsing, manifest import and automatic PNG/DDS to `.lotex` compilation.

## Portable and installed layouts

The runtime must resolve its mod root using the same user-path policy as normal startup. MO2 packages should target the runtime-visible `mods/` tree rather than absolute machine-specific paths.

This also keeps the mod format usable outside MO2. Linux/Steam Deck managers can materialize the same overlay tree without USVFS.

## Conflict semantics

For `mods/overlay/`, conflict identity is the destination path:

```
textures/<runtime-fingerprint>.lotex
```

For standalone manifests, conflict identity remains:

```
AssetKind + canonical resource key
```

Standalone `priority=` remains supported only for the built-in per-mod loader. It has no effect on files already merged by MO2.

## Future asset types

The same overlay convention is reserved for:

```text
mods/overlay/fonts/
mods/overlay/models/
mods/overlay/movies/
```

Texture support is the first runtime implementation. The directory reservation does not imply that font/model/movie replacement is implemented yet.

## Validation

MO2 compatibility is not considered verified merely because the layout is VFS-friendly. Windows acceptance requires launching through a real MO2/USVFS instance and confirming:

- overlay files are visible to LostOdysseyRecomp;
- enabling/disabling a mod changes the visible replacement;
- MO2 conflict ordering selects the expected file;
- the original game archives remain unchanged;
- normal launch without MO2 still falls back correctly.

Linux/Steam Deck compatibility is a separate manager/materialized-overlay path; this document does not claim USVFS support there.
