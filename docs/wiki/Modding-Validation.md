# Modding validation

## Automated checks without private game data

Configure the focused C++ target from a complete repository checkout (CMake 3.28+):

```sh
cmake -S . -B build-mod-tests -DLO_BUILD_RUNTIME=OFF -DLO_BUILD_RECOMP_LIB=OFF -DLO_BUILD_TOOLS=OFF -DLO_BUILD_GPU=OFF -DLO_BUILD_MOD_TESTS=ON
cmake --build build-mod-tests --config Release --target LoModApiTest
ctest --test-dir build-mod-tests -C Release --output-on-failure
python -m pip install Pillow
python tools/tests/mod_tools_test.py
```

The C++ target checks identity normalization, standalone priorities and disabled mods, generation/reload, provider fallback and recursion, external-manager isolation, invalid modes, payload identity/size/format checks, path traversal and bounded manifests. Symlink checks run when the host permits creating symlinks. Concurrent readers exercise immutable snapshots.

Python checks cover canonical keys and a fixed FNV path vector, UTF-8 identities, RGBA file layout, CSV filtering/deduplication, generated specifications, standalone and overlay ZIPs, deterministic output, duplicate identities, invalid dimensions, input containment, no-overwrite behavior and failed-output cleanup.

The `Mod API and Wiki` GitHub Actions workflow runs the focused checks on Linux and Windows. Consult the workflow's actual status; the existence of this page is not evidence of a green build or a complete runtime build.

## Native-menu game acceptance

Use a game build containing the mod changes and your own imported game data. Record the build commit, platform, selected language, source key, original dimensions, chosen mod mode and root.

Start with the unmodified native settings menu, then install an unmistakable `UI_MAIN_00` recolor preserving its 512x1024 dimensions. Check the visual difference, alpha blending and unchanged hit/layout behavior. Disable the mod and restart to verify the original. Repeat with a supported font texture page, preserving glyph positions and dimensions; confirm unchanged metrics and fallback behavior.

Test two conflicting standalone mods at different priorities and at equal priority. Test missing/corrupt files, an identity mismatch and a deliberately wrong extent. Failures must restore original content without crashes. Capture `[mods]` diagnostics and compare game archive hashes before/after.

A synthetic payload test proves the wire contract and loader behavior. It does not establish a real-game screenshot result or coverage of an arbitrary guest texture.

## External-manager acceptance

Follow [MO2 acceptance](Mod-Organizer-2.md). Real VFS mounting, order changes and disable restoration require a real managed game process. Materialized overlays on Linux/Steam Deck are a separate deployment path.

## Wiki publication

Wiki sources are maintained under `docs/wiki`. The workflow's publication job runs only on an allowed push, after both platform test jobs succeed. It publishes the managed mod pages and updates bounded navigation blocks in the Wiki Home and sidebar, preserving unrelated pages and history. It never force-pushes.

The Wiki feature must be enabled and its Git repository initialized with a first page. If cloning or pushing the Wiki fails, the workflow fails rather than reporting a successful publication. The source documents remain available in the PR. Every published page includes the exact source commit.
