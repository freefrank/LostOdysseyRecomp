# Automatic selection of imported discs

Status: **2026-09-06, included in published v0.2**. Commit `dcc946299cdc2984783793ad5871a0ad0b90a2c9` and its `v0.2` tag were pushed to both remotes. The dated local test packages below preserve pre-release evidence; they are not the official release artifact. Chapter-boundary gameplay and a complete playthrough remain unverified.

## Behavior

When the original game requests another disc, the host selects its already imported `discN` directory and the original manager reloads that disc's own `LO.fpi` and archives. No disc-selection button is required. All discs must belong to the same supported edition and retain the importer layout under one parent directory. Existing handles continue referring to their original files; new game-path opens use the selected disc.

A missing or invalid target fails the request without changing the current mount. The player must import the required disc with InstallGame.exe. There is no rewritten combined FPI, and this implementation does not copy re:Blue code. The shared design goal is avoiding manual disc handling; Lost Odyssey retains its original index unload/reload flow.

## Implementation

- [disc_set.cpp](../../LostOdysseyRecomp/kernel/io/disc_set.cpp) reads bounded XEX execution metadata and verifies the target title, supported edition, Media ID and disc number. It then checks the FPI disc number, header/table ranges, all 13 archive files and each indexed data extent against the corresponding archive size. This checks installation structure and edition identity; the importer remains responsible for full XEX SHA256 authentication.
- [FileSystem::SelectDisc](../../LostOdysseyRecomp/kernel/io/file_system.cpp) validates the target before replacing the current disc root under a mutex. `game:`, `d:` and Cdrom0 device paths resolve through that root. Save/cache roots and previously opened file handles retain their existing targets.
- [XamSwapDisc](../../LostOdysseyRecomp/kernel/imports.cpp) rejects disc numbers outside 1–4 with error 87. A valid-number target that cannot be selected returns error 21 (`ERROR_NOT_READY`), leaves the old mount active and does not signal completion. Successful selection occurs before the completion event is signaled.
- The guest's original request function `82821E90` and manager `82320428` continue handling the index lifecycle. The former host stub that merely signaled success was replaced.

## Verification

| Check | Result | Boundary |
|---|---|---|
| `LoStorageTest`, USA/Europe and Asian discs | Both passed 1 → 2 → 3 → 4 → 2 → 1; five path aliases resolve correctly. For each selected disc, 4,096-byte midpoint reads from four differing archives matched the source disc; existing handles kept their original content. | Storage routing and sampled archive reads, not story progression. |
| `disc_set_test.py` synthetic installations | Seven cases passed: valid set, missing index, truncated archive, mixed editions, wrong disc number, out-of-range index and truncated XEX. | Structural rejection tests; does not replace source-data hashes. |
| `LoDiscRuntimeTest`, USA/Europe | Background test invoked original guest requests and completed 1 → 2 → 3 → 4 → 1 through the original manager. Each target `LO.FPI` was reloaded and rendering continued at about 30 fps without disc-selection button input. | Controlled manager requests, not a chapter-boundary cutscene or full playthrough. |
| `LoDiscRuntimeTest`, Asia | The same original-manager 1 → 2 → 3 → 4 → 1 sequence passed. The final `shot_999.png` was visually confirmed as the normal title menu without a disc-change dialog. | Original requests and index reloads, not chapter-boundary gameplay. |
| Updated local package | All manifest file hashes, frozen importer self-test and a 30-second rendered/captured launch with PATH limited to System32 passed. | Package smoke test; the original-manager runs above provide disc-selection coverage. |

All 19 importer tests passed again and the final Release build succeeded. Four additional storage regressions passed: write, read, overwrite and read-overwritten. Runtime evidence is in `out/eu-import-audit/disc-runtime-eu/runtime.log` and `out/eu-import-audit/disc-runtime-asia/runtime.log`. Private game data and captures are not distributed. Chapter-boundary progression, missing-disc recovery in real story flow and a complete playthrough remain unverified. Do not describe the controlled request sequence as user gameplay acceptance.

## Final local package

The ZIP is `out/disc-selection-preview/LostOdysseyRecomp-windows-x64-2d9ce9f6-dev.zip`, 38,247,110 bytes, SHA256 `285c9570bd208d06943291fbb46a788fad06138f9d06015e29e7c0cd15555ae8`. Its runtime SHA256 is `42d03e8d27591c8c1830a6faed7d17ec5ba0476b907c1a04f2dfac77daeece7e`. Verification is recorded in `out/eu-import-audit/disc-package-result.json`.

The extracted local copy uses `game-path.txt` to select the isolated complete USA/Europe four-disc import. The main development executable remains at its previous `135dca79` hash prefix; user save/profile were preserved. This is a local test package, with no commit, push or public release.

See [USA/Europe support](europe-support.md), [installation](../INSTALLING.md), [XEX evidence](xex.md) and [project status](../STATUS.md).

## v0.2 publication

The [official v0.2 release](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.2) was published at 2026-09-06 17:16 UTC after hosted Windows CI passed. The 38,185,445-byte ZIP has SHA256 `b125dae559a8d62d6ff79d0bcc80fd6af44fee6a41100db787b8bfca61facab6`. Download, API digest, checksum file and manifest matched; the importer self-test and 30-second isolated rendered German-menu launch passed. See [release verification](../STATUS.md). Earlier statements about local-only packages describe those experiments, not current publication status.
