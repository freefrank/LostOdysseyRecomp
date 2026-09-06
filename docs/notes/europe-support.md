# USA/Europe edition support

Status: **2026-09-06, local and unpublished**. The user requested implementation after a four-disc compatibility audit. These changes are not included in the published v0.1, and no commit, push or release was performed for them.

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
