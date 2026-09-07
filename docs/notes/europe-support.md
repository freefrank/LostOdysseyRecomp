# USA/Europe edition support

Status: **2026-09-06, included in published v0.2**. Commit `dcc946299cdc2984783793ad5871a0ad0b90a2c9` and its `v0.2` tag were pushed to both remotes. The dated local test packages below preserve pre-release evidence; they are not the official release artifact. Chapter-boundary gameplay and a complete playthrough remain unverified.

## Combined resource metadata — 2026-09-07

Local built-in bare-FPD and CPX metadata now includes the Asian and USA/Europe resource sets. Existing resource-identity matching selects a layout automatically; there is no region selector or matching-code change. The CPX table contains 104 archive layouts, 12,857 package profiles and 275,186 locations, retaining the original Asian profiles. Both generators accept repeatable `--additional-root` inputs; this does not permit mixed editions inside one imported installation.

Both editions match all 52 archive layouts and use 52 bare-resource index hits with zero scanning. Asian CPX extraction indexes 10,060 packages and USA/Europe 10,198, both with zero fallback. Each edition's 20,686 source names and hashes exactly match its retained strict full-scan baseline, including the same collection SHA256 `57cb834795fd99419f0a1980f86e38c7123317eed3d627576269f61fd03055fc`. Entering through Disc 2 discovers the sibling discs and reuses the same manifest.

Resource reads are 142,093,208 bytes for Asia and 142,289,816 for USA/Europe, compared with USA/Europe's earlier 20,770,329,949 bytes. Observed discovery was 12.537/12.631 seconds, potentially overlapping the build; no controlled speedup percentage, whole-startup or FPS result is claimed. Metadata audit preserved all old CPX/raw profiles; scanner fixtures and the main build passed. Evidence is under `out/v0.4.0-followup/cpx-both-editions/{comparison.json,layout-audit.json,metadata-audit.json,fixtures.log}`.

The new ZIP `ed8e0627…` / EXE `c75946a3…` passed all 45 manifest checks and importer self-test. Both editions ran the packaged EXE with verified bundled DXC/DXIL and reached Map2 using Auto 1080p/AA3; effective size and inspected static screenshots were 1920×1080 without allocation fallback. Actual Asian/USA-Europe discovery recorded 13.482/13.367 seconds, with 52 indexed files, zero scanning and zero CPX fallback. Each runtime's 20,686 expected source hashes match the full-scan baseline. Seven protected user files, task seeds and EXE stayed unchanged; the harness ended both owned processes. The [combined-edition delivery](../../out/v0.4.0-followup/cpx-both-editions/DELIVERY.md) records full artifact identities and links `runtime-summary.json`. This does not expand original-battle, chapter-transition, full-game or player acceptance. Earlier fallback evidence below remains tied to `79308a15…`. Unknown-layout/content fallback, unread-content limitations and explicit full scanning remain available. See [metadata generation](../../tools/tests/README.md#regenerating-resource-metadata) and [shader evidence](shader-preparation.md).

## Local CPX follow-up — 2026-09-07

This section preserves the `79308a15…` package's evidence. Its USA/Europe 0/52 direct-layout coverage describes the earlier metadata; the combined table is tracked above.

The current archive-location metadata matches all 52 audited Asian-edition archives and none of the 52 USA/Europe archives. USA/Europe uses fallback discovery; the 30.015 → 13.280-second discovery comparison and 98.55% reduction in bytes read apply only to the tested Asian edition. This difference concerns discovery optimization, not a change to the published edition support below.

Normal discovery and strict full scanning each produced 20,686 sources in both editions, with every filename and per-file SHA256 equal within each comparison; both collections have SHA256 `57cb834795fd99419f0a1980f86e38c7123317eed3d627576269f61fd03055fc`.

| Normal discovery path | Asian edition | USA/Europe edition |
|---|---:|---:|
| Matching direct archive layouts | 52/52 | 0/52 |
| Bare-resource indexed/scanned files | 52/0 | 28/24 |
| CPX indexed/complete-fallback packages | 10,060/0 | 7,401/2,797 |
| Application resource-read bytes | 142,093,208 | 20,770,329,949 |

USA/Europe's CPX index hits use full-package SHA256 identity matching, not direct archive-layout reads. The edition checks ran alongside other validation; their timings are not a controlled performance comparison.

Both editions ran the current package EXE (SHA256 `3aae46b8d171cec4f977733a5a5f288d62d51aff7a1f0eeefbb098399509ebae`) with live-module verification of its bundled DXC/DXIL. Logs identified Asia/default with five game languages and USA/Europe with six. Using the same Map2 seed and normal Auto 1080p/AA3/30 FPS settings, both reached the scene at effective 1920×1080 without allocation fallback; the two 1920×1080 static screenshots were inspected. Each runtime's 20,686 expected source hashes match its scanner results. Seven protected user files, the task seeds and EXE stayed unchanged; the harness ended both owned processes, without claiming natural-exit validation.

Both editions retain the same two known precompilation failures, `ps_78af7d75d932c582` and `vs_291187f5ef8ba74a`; these are not new extraction omissions. This covers source equivalence and a bounded Map2 scene, not the original reported battle, chapter transitions, complete language/audio coverage, whole-game compatibility or new player acceptance.

The [edition validation report](../../out/v0.4.0-followup/edition-validation/REPORT.md) summarizes the results. Evidence is under `out/v0.4.0-followup/edition-validation/`: `scanner/{layout-audit.json,comparison.json}`, `runtime-summary.json` and `{asia-runtime,usa-europe-runtime}/{result.json,loaded-libraries.json,scene.png}`. The package remains ZIP SHA256 `79308a1508edc459360d8b0791329e719cd5bb75a100ab57a61fab5f23059cef`; no source, package or publication state changed. See the [current shader-discovery contract](shader-preparation.md) and [follow-up handoff](handoff-v0.4.0-followup.md).

## Compatibility evidence

All four audited USA/Europe discs have Title ID `4D5307FA`, version `0.0.0.3` and four-disc execution metadata. Each complete decrypted image is 20,709,376 bytes and is byte-identical to the current Asian Disc 1 image, with SHA256 `cb756b46092e448923517bf660b44ad1a0651c2860882b43ae021f4ad4290f71`. Existing generated PowerPC code and hook addresses can therefore be reused for this audited set. Encrypted XEX files and resource archives differ; this does not establish compatibility with other builds or title updates.

| Disc | Media ID | Original XEX SHA256 |
|---|---|---|
| 1 | `368DE6DD` | `175ae53d109d480a83bebbd186e7b6871f7b03ce80af69ab388db2f747640de3` |
| 2 | `1888BE4E` | `1d8a78379349e4583957d34148d5dbf8a091955edb6c6877bc24bdc9c7b87541` |
| 3 | `6DD59D08` | `dd323967d7f4b99b48c00aa6a15a643c96df525e539669e9be49a876513b86f6` |
| 4 | `0C0E80B5` | `9204ba8b91836853ae1e9f0dc49090abd5e63709935599c5551c23b28ecf48d4` |

The local audit is recorded in `out/eu-import-audit/four-disc-audit.json`. Private images and captures are not distributed.

## Implementation

- The [importer](../../tools/installer/import_game.py) accepts the exact four new hashes alongside the existing Asian set and records the edition in import metadata. Input sets cannot mix editions. Incremental import checks the actual XEX of existing discs, including old installations without edition metadata, and rejects a mismatched edition or disc number. Existing discs remain protected from overwrite.
- Runtime reads bounded XEX execution metadata before settings/configuration and selects game languages for the audited edition. This also works with manually extracted folders; language detection is not a substitute for import-time SHA256 validation.
- USA/Europe game-language IDs are 1=English, 2=Japanese, 3=German, 4=French, 5=Spanish and 6=Italian. The Asian list remains 1/2/7/8/9. Both first-run setup and the in-game menu use the selected list; unavailable saved values fall back to English.
- The host voice selector reads the original resource-populated language list rather than hardcoding English/Japanese/Korean and three choices. The original USA/Europe settings page exposes English, Japanese, German, French and Italian voice choices; displayed names and cycling now follow that list. This does not establish complete voice playback coverage.
- Missing-data first launch opens the importer before setup so language choices match the imported edition. The settings interface still has English, Traditional Chinese, Japanese, Korean and Simplified Chinese translations; no new Western European interface translations were added.

## Validation and limits

- Release build passed in a separate output directory; the main development executable and user save/profile were preserved.
- All 19 importer tests passed, including edition identification, mixed inputs, incremental imports and checking existing XEX files rather than trusting metadata.
- Eleven isolated first-run setup tests passed: exact language lists for both editions, saving every supported ID, and fallback from incompatible previous settings. Evidence: `out/eu-import-audit/setup-tests/results.json`.
- Real four-disc import completed into `out/eu-import-audit/game`. All 60 imported files, totaling 23,063,969,792 bytes, matched the source ISO SHA256 hashes. Evidence: `out/eu-import-audit/import-hashes.json`.
- All six runtime language starts passed (`out/eu-import-audit/runtime-results.json`). English, German, French, Spanish and Italian title menus were visually confirmed in `out/eu-import-audit/menus.png`. The Japanese title menu intentionally uses English; the original Japanese brightness/settings page was confirmed separately in `out/eu-import-audit/japanese-brightness/shot_1100.png`. This covers menus/settings only. A 65-second French run remained in settings and is not gameplay evidence.
- The final Release build after the resource-based voice-selector fix passed (`out/eu-import-audit/build-final.log`); the USA/Europe host voice cycle was visually verified in `out/eu-import-audit/voice-eu/options.png`: French → Italian → English → Japanese → German → French. Runtime logs identify IDs 1/2/3/4/6 at indices 0–4. The Asian regression also passed visual inspection in `out/eu-import-audit/voice-asia/options.png`: English → Japanese → Korean → English → Japanese → Korean, using IDs 1/2/7.
- A cold USA/Europe shader run found 2,000 shaders using 28 indexed and 24 scanned resource files. Extraction took 17.0 seconds and compilation 7.7 seconds on the test machine, with the two known failures. The scan fallback works; a fully optimized USA/Europe location index is not established.

## Earlier local language-support package

The initial local package was superseded by the voice-selector fix. The language-support ZIP, built before the subsequent automatic-disc-selection change, is `out/europe-preview/LostOdysseyRecomp-windows-x64-2d9ce9f6-dev.zip`, 38,243,328 bytes, SHA256 `e8aedcce72e9e3ef5f55c1039e319e938b764ed384da55e335b5d1f1ba480dee`. Its runtime SHA256 is `1b2f68eda2b0554a8e2cf77f4704c1ec74ea405994bf16bc5aab18ece5911ac9`.

All packaged manifest file hashes were verified, and frozen `InstallGame.exe --self-test` passed. With PATH limited to System32, the packaged runtime detected USA/Europe and ran for 30 seconds with actual rendering and capture. Evidence: `out/eu-import-audit/package-result.json`. The extracted local copy beside the ZIP uses `game-path.txt` to select the isolated four-disc import. The package remains local; no commit, push or public release was performed, and the main development executable and user save/profile were not modified.

## Subsequent automatic disc selection

The user subsequently requested automatic access to imported discs. Local runtime now validates and selects the requested disc before completing `XamSwapDisc`; the original game reloads that disc's own index and archives without a disc-selection button. The previous immediate-completion stub is no longer the current implementation. Both editions passed storage tests, and both original managers completed controlled 1 → 2 → 3 → 4 → 1 sequences with each target FPI reloaded. The updated package in `out/disc-selection-preview/` passed manifest hashes, frozen importer self-test and a 30-second rendered/captured launch with PATH limited to System32; the package above predates this addition. Its hashes and final storage regression evidence are recorded in the disc-selection note. See [disc-selection implementation and evidence](disc-selection.md).

Chapter-boundary gameplay, full-language text/audio completeness and a complete playthrough remain unverified. A successful import or controlled manager sequence does not establish those results. See [project status](../STATUS.md), [installation](../INSTALLING.md), [XEX history](xex.md) and [settings evidence](settings-menu.md).

## v0.2 publication

The [official v0.2 release](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.2) was published at 2026-09-06 17:16 UTC after hosted Windows CI passed. The 38,185,445-byte ZIP has SHA256 `b125dae559a8d62d6ff79d0bcc80fd6af44fee6a41100db787b8bfca61facab6`. Download, API digest, checksum file and manifest matched; the importer self-test and 30-second isolated rendered German-menu launch passed. See [release verification](../STATUS.md). Earlier statements about local-only packages describe those experiments, not current publication status.
