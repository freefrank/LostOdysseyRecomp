# Modding validation

## Automated checks without private game data

Run these from a complete repository checkout (CMake 3.28+). They need no game data. The `macOS arm64` workflow builds and runs `LoModApiTest` together with the other tests that need no game data, but nothing in CI runs the Python checks, and the `Mod API and Wiki` workflow only publishes the Wiki. Run the checks yourself before you rely on a change.

```sh
cmake -S . -B build-mod-tests -DLO_BUILD_RUNTIME=OFF -DLO_BUILD_RECOMP_LIB=OFF -DLO_BUILD_TOOLS=OFF -DLO_BUILD_GPU=OFF -DLO_BUILD_MOD_TESTS=ON
cmake --build build-mod-tests --config Release --target LoModApiTest
ctest --test-dir build-mod-tests -C Release --output-on-failure
python -m pip install Pillow
python -B -m unittest tools.tests.mod_catalog_test tools.tests.mod_tools_test -v
```

The C++ target checks identity normalization, mod folder priorities and disabled mods, format v2 metadata and overlay discovery, `mod-list.ini` order and the manager's save, generation/reload, provider fallback and recursion, external-manager isolation, invalid modes, payload identity/size/format checks for images, runtime textures (LOTEX2, RGBA8 and DDS) and text, language packs and their font files, path traversal and bounded manifests. Symlink checks run when the host permits creating symlinks. Concurrent readers exercise immutable snapshots.

Python checks cover canonical keys and a fixed FNV path vector, UTF-8 identities, RGBA file layout, CSV filtering/deduplication, generated specifications, mod folder and overlay ZIPs, mod folder and overlay texture packs (RGBA8 and DDS), `init` from an export index, `overlay-to-mod` moves, dry run, refusals and rollback, `language-clean` (with the pack's fonts), the presentation forms `font-glyphs` shapes and a LOFONT1 file drawn from a system TrueType font (skipped without `freetype-py`, `uharfbuzz` or a font with Arabic letters; `LO_TEST_TTF` names one), deterministic output, duplicate identities, invalid dimensions, input containment, no-overwrite behavior and failed-output cleanup.

The asset inventory has its own checks, listed in `tools/asset_inventory/README.md`. Nothing tests `tools/modding/publish_wiki.py` or the Wiki pages; see [Wiki publication](#wiki-publication) for how to check them by hand.

A passing automated check proves the file formats and the loader, not that the game shows the result. The game acceptance steps below do that.

## Native-menu game acceptance

Use a game build containing the mod changes and your own imported game data. Record the build commit, platform, selected language, source key, original dimensions, chosen mod mode and root.

Start with the unmodified native settings menu, then install an unmistakable `UI_MAIN_00` recolor preserving its 512x1024 dimensions. Check the visual difference, alpha blending and unchanged hit/layout behavior. Disable the mod and restart to verify the original. Repeat with a supported font texture page, preserving glyph positions and dimensions; confirm unchanged metrics and fallback behavior.

Test two conflicting mod folders at different priorities, at equal priority and in `mod-list.ini` order (Settings → System → Mods writes the list). Test missing/corrupt files, an identity mismatch and a deliberately wrong extent. Failures must restore original content without crashes. Capture `[mods]` diagnostics and compare game archive hashes before/after.

A synthetic payload test proves the wire contract and loader behavior. It does not establish a real-game screenshot result.

## Texture and text acceptance

Textures: export some textures (see [Creating mods](Creating-Mods.md#export-the-original-assets)), then run `lo_mod.py texture-pack --test tint` on the export. In game, the surfaces that use those textures turn reddish, and with Settings → System → Debug log on the log shows `[mods] texture <fingerprint> replaced` lines. Repeat with `--test nearest4`: the picture must look unchanged. A real upscale is judged by looking at it, with the originals as the reference.

Text: install a language pack, pick it in Settings → System → Game language, restart, and check that the startup log lists it (`mods: language pack ...`) and that the translated lines appear. Pick another language and confirm the pack's lines no longer appear.

Fonts: `LoFontPatch --disc <disc1> --member bin/xenon/loc/int/menu/rpfontscommon_int.xxx --out <folder> --self-test` (a target of the runtime build) adds three test glyphs to every font of the package, checks that the two glyph tables agree and every page decodes, and reads each font back with the settings menu's reader; `--add <font>=<file.lofont>` applies a pack's files instead and writes the pages for a look. `LoModelImport ... --language-pack <id>` takes the runtime path (mod listing, index patch, archive tail) for one package. In the game, a pack with glyph files logs `[mods] font: N glyph files ...` at startup and its letters appear in menus, item text and dialogue.

## External-manager acceptance

Set up MO2 as described in [Mod Organizer 2](Mod-Organizer-2.md), then use two overlay packages with different artwork for the same native-menu key:

1. Start the game from MO2 and check the `mods:` log line shows `overlay folder: yes`, then that the menu shows the winning mod's artwork.
2. Swap the two mods in MO2's order and restart: the other artwork appears.
3. Disable both and restart: the original artwork returns.
4. Install a mod folder package through MO2 and check its id in the `mods:` line. It also loads with `--mods-mode overlay` or `standalone` in the executable's arguments.
5. Start the game once without MO2 and check that only files really in `mods/` load.

Repeat once from a game folder with a non-ASCII path. Linux and Steam Deck use copied or symlinked files instead of a virtual file system.

## Wiki publication

Wiki sources are maintained under `docs/wiki`. The `Mod API and Wiki` workflow publishes them: on a push to `main` (or the `feature/mod-asset-interface` branch) that changes `docs/wiki` or the modding sources, its `publish-wiki` job runs `tools/modding/publish_wiki.py`. The job publishes the managed mod pages and updates bounded navigation blocks in the Wiki Home and sidebar, preserving unrelated pages and history. It never force-pushes. No test job runs first.

The Wiki feature must be enabled and its Git repository initialized with a first page. If cloning or pushing the Wiki fails, the workflow fails rather than reporting a successful publication. Every published page starts with a line that names the exact source commit.

To check the pages by hand before you push, clone the Wiki somewhere outside the repository and stage into the clone without pushing:

```sh
git clone --depth 1 https://github.com/freefrank/LostOdysseyRecomp.wiki.git wiki-check
python tools/modding/publish_wiki.py --source docs/wiki --wiki wiki-check --repository freefrank/LostOdysseyRecomp --revision $(git rev-parse HEAD)
git -C wiki-check diff --stat
```

Read `Home.md` and `_Sidebar.md` in the clone, and open a page or two. Also check that every link between pages and every link to a repository file still resolves; links to repository files must point at files that exist on `main`.
