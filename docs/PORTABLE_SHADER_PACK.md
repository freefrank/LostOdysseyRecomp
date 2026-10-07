# Portable shader packs

This is the distribution and tool reference for the SPIR-V `.lospv` pack, read
by Vulkan on Windows, Linux and Android and by Metal on macOS, and the D3D12
`.lospd` pack. Release packages no longer carry a pack: the game downloads the
pack for its renderer at startup from the `shader-packs` prerelease
([startup download](#startup-download)). Both are built with one script on
Windows or Linux ([building packs](#building-packs)). A pack placed by hand
still works when its contract matches: `shaders/portable_vk.lospv` or
`shaders/portable_dx12.lospd` in the install folder or beside the executable. See [installation](INSTALLING.md) and
[current release status](STATUS.md) for the player-facing path.

Release measurements below are dated evidence. The initial implementation was
based on `menu@257f3866e9f9f5d3e65550c86dce453290cf7ee4`; its delivery archive and
unverified boundaries are retained in the final historical sections.

## Startup download

The runtime can fetch the pack for its renderer itself. The check runs after the update check and before shader preparation (`updater/shader_pack_download.cpp`, called from `main.cpp` after `XexLoader::Load`):

1. It computes the contract of the configured renderer from the loaded executable: Vulkan or DirectX 12 on Windows, Vulkan on Linux and Android, and on macOS the Vulkan contract, because Metal translates the same SPIR-V. The contract ignores the local DXC identity, so it is known before the renderer starts; `LoPortableShaderContractTest` checks that it equals the renderer's own.
2. It looks for a matching pack: `LO_SHADER_PACK_PATH` alone when set, otherwise the install folder, then `shaders/` beside the executable. The install folder is `shaders/` beside the executable in a portable layout (Windows) and `shaders/` in the user data folder otherwise (macOS app, AppImage, Flatpak). A pack for another contract counts as missing; the renderer also moves on to the next location instead of stopping at a rejected file.
3. Without a match it reads `index.json` from the `shader-packs` release. Only a listed contract is offered. A window shows the renderer and the size; Download (A) shows progress and can be cancelled with B. Skip (B) writes the contract to `shaders/declined-downloads.txt` in the install folder, and the offer comes back only for a new contract. Closing the window asks again at the next start.
4. The file is downloaded beside its target as `<name>.download-<pid>`; a transfer longer than the listed size is stopped. Its size and SHA-256 must match the index, and the pack must pass the contract check and `Reader::VerifyAll` before it replaces the old file. On any failure the old state stays and the game compiles locally; the window shows the reason.

Background (`LO_BACKGROUND`) and headless runs show no window. Developer shader settings (`LO_SHADER_PACK_PATH`, `LO_NO_PORTABLE_SHADER_PACK`, `LO_SHADER_EXPORT_PACK`, `LO_SHADER_FULL_SCAN`, `LO_SHADER_HLSL_DIR`, `LO_SHADER_RETRY_FAILURES`) skip the check, as they skip packs in the renderer. `LO_SHADER_PACK_DOWNLOAD=0` turns it off; `LO_SHADER_PACK_DOWNLOAD=1` downloads without asking, also in background runs. `LO_SHADER_PACK_INDEX_URL` replaces the index URL for tests; packs are always fetched beside the index, never from a URL in its text. The log line `shader pack: …` records the outcome.

### Index

Packs are assets of the `shader-packs` GitHub prerelease, named `<file stem>-<first 16 contract digits><extension>`. A prerelease is never the repository's latest release, which the updater reads. `index.json` on the same release lists them:

```json
{
  "schema": 1,
  "packs": [
    {
      "renderer": "vulkan",
      "contract": "4e123a08e148937e41b1b7dc636608377bd15a0043fb506a43a75cb6930ea6ae",
      "file": "portable_vk-4e123a08e148937e.lospv",
      "size": 180527457,
      "sha256": "5c412fd71c5cdae0e4e5fcfe67de536cc5c0a10f10ed71b1b0f6cdd5858600fb"
    }
  ]
}
```

`renderer` is `vulkan` or `d3d12`. The `metal` entry stays for v0.7.35 and older macOS builds, which read a separate `-O1` pack; newer runtimes skip it. A runtime ignores renderers it does not know. A malformed entry, a duplicate renderer and contract or another schema makes the runtime reject the whole index.

### Pipeline recipe corpus

The same release carries the pipeline recipe corpus (`pipelines_corpus.bin`, file version 2 of `gpu/pipeline_cache.h`): the pipelines the automated map, cutscene and battle tours recorded, tagged by scene, which the renderer prebuilds or prefetches while a scene loads ([P3 notes](notes/pipeline-first-use-stalls.md)). The index lists it like a pack:

```json
{
  "renderer": "pipeline-corpus",
  "contract": "1d7f0a17386480c1fcfa5e912f7fb8db7359c5ceb6e8957b9b1045628233fec2",
  "file": "pipelines_corpus-80afef9497d4bb14.bin",
  "size": 409184,
  "sha256": "80afef9497d4bb14b600c91693d48e9f876fcb6f00c546f44f634b377fac24dc"
}
```

The contract is the SHA-256 of `lo-pipeline-recipes-v2`; a new recipe format gets a new string and its own entry. The file name carries the first 16 digits of the file's SHA-256. Runtimes before the corpus skip the entry as an unknown renderer and still find their packs.

After the pack check the runtime starts a thread for it (`updater/pipeline_corpus_download.cpp`) and goes on at once:

- With no `shaders/pipelines_corpus.bin` in the install folder it reads the index and downloads the listed file. With one, it checks again only with automatic updates on and at most once a day (`shaders/pipelines_corpus.checked` records the last check), and downloads when the index lists another SHA-256.
- The file is downloaded beside its target as `<name>.download-<pid>`; size and SHA-256 must match the index before it replaces the old file. On any failure the old file stays. The renderer reads the corpus when it starts, so a new file takes effect at the next start.
- There is no window. `LO_SHADER_PACK_DOWNLOAD=0` and headless runs skip it, background runs skip it unless `LO_SHADER_PACK_DOWNLOAD=1`, which also skips the once-a-day limit. `LO_PIPELINE_CORPUS` (a corpus chosen by hand) skips it. `LO_SHADER_PACK_INDEX_URL` applies as for packs. The log line `pipeline corpus: …` records the outcome.

### Publishing

`LoShaderPackTool contract <xexdump image>` prints the contract of each renderer (`vulkan`, `d3d12`). `tools/release/publish_shader_packs.py --tool <LoShaderPackTool> --image <image> <packs>` maps each pack to its renderer by contract, runs `verify-runtime`, stages the renamed packs and the merged index in `out/shader-packs`, and uploads them with `--publish`: packs first, then the index. Entries for other contracts stay, so older runtimes keep finding their packs. After uploading it checks that the latest release is still a version. `--check` only reports whether the published index covers both contracts of the image (and whether it lists a corpus). `--corpus <pipelines_corpus.bin>` stages the corpus and its entry the same way, alone or together with packs; without packs it needs neither `--tool` nor `--image`.

### Building packs

`tools/shader_pack/build_packs.py` builds every pack the game downloads, on Windows or Linux:

```
python tools/shader_pack/build_packs.py --game <disc1> [--renderer vulkan d3d12] [--runtime <build folder>] [--sources <folder>...] [--stage]
```

For each renderer it runs the runtime from a copy in `out/shader-pack-build/work-<renderer>` with `--prepare-shaders-only`, `LO_GRAPHICS_API=<renderer>` and `LO_SHADER_EXPORT_PACK`, keeping that folder's shader cache so a rerun from the same source needs no DXC, then runs `LoShaderPackTool verify-runtime` against `LostOdysseyRecompLib/private/image_disc1.bin`. The default runtime is `out/build/windows-clang/LostOdysseyRecomp` (Windows) or `out/build/linux-clang/LostOdysseyRecomp` (Linux), with `LoShaderPackTool` beside it. `d3d12` needs Windows; `vulkan` serves Vulkan on every platform, Metal and Android. It refuses to run with developer shader settings in the environment (`LO_SHADER_PACK_PATH`, `LO_SHADER_FULL_SCAN` and the like). `--stage` passes the packs to `publish_shader_packs.py` without `--publish`; uploading stays a separate, deliberate step. `--sources` adds shader sources learned during play (`ps_*.bin`/`vs_*.bin` from the `source` folder of a shader cache); the game files alone give 28,484 shaders, and the collection of 950 learned sources used for the 2026-10-02 packs (`shaders/learned_sources` in the private build-inputs repository; 289 of them are new to the game files) brings both packs to 28,687 records, the coverage of the published packs. The script deletes the work folder's startup bundle before each run, because a bundle hit skips source discovery and would leave added sources out.

## One SPIR-V pack for Vulkan, Metal and Android (2026-10-02)

Android needed its own SPIR-V: many Android GPUs cap a storage-buffer descriptor at 128 MiB, below the 1 GiB vertex arena, so its shaders fetch vertices through the arena's device address and read the push-constant addresses as 32-bit word pairs. Desktop Vulkan and Metal now use the same shader prelude, and Metal no longer compiles at `-O1` (that only shortened local compiles, which the pack replaces). Vulkan on Windows, Linux and Android and Metal on macOS therefore share one contract and `portable_vk.lospv`. The HLSL text equals the former Android variant, so the Android pack made on Linux earlier has the same contract as one made on Windows now (`eecb4425…`). The common prelude is part of the D3D12 contract too, so the DX12 pack also changes (`239f8775…`) although its DXIL does not.

Validation:

- `build_packs.py` on Windows (RTX 5080): Vulkan 335 s and D3D12 103 s from an empty cache; with the learned sources 28,687 records each, `verify-runtime` passed. Vulkan pack 241,820,275 bytes (the shared SPIR-V is larger than the former 180 MB desktop pack), DX12 80,491,442 bytes.
- Performance, Vulkan, 4K, uncapped, frozen Uhra save, ABBA×2 with each build reading its own pack: main 218.0/221.0/204.1/216.7 FPS (mean 215.0), unified 201.5/210.4/220.5/213.8 (mean 211.6), within run-to-run variation. This scene is CPU-bound on that GPU (about 4.1 ms of CPU per frame), so it does not isolate the GPU cost of the vertex fetch.
- AMD Radeon 8060S (Strix Halo), Windows build through Proton on RADV: pack hit and a correct #118 world-map frame on Vulkan.
- M1 Max, Metal, `new-game-battle`: main with the published `-O1` Metal pack and the unified build with the shared pack both passed at the scenario's 30 FPS cap, with 0 on-demand shader compiles for the 28,687-record pack. A foreground run with HDR on an external EDR display (headroom 10.15) switched to HDR once the window was frontmost (`scene_enabled=true`, about 1,030 nits estimated); a window started over SSH stays behind other apps and gets no EDR headroom.
- Lenovo TB321FU (Adreno 750): the pack check runs at startup, finds the Linux-made Android pack with the shared contract (`vulkan pack present`), and the renderer hits it. With the pack moved away, the index was read over HTTPS through `HttpURLConnection` (`none published for contract eecb4425…`).
- Both packs were published to the `shader-packs` prerelease on 2026-10-02 (`portable_vk-eecb4425e7f53d44.lospv`, 241,820,275 bytes, SHA-256 `b9a2cd10…`; `portable_dx12-239f877563b6dc7a.lospd`, 80,491,442 bytes, SHA-256 `f5ab2e40…`; GitHub's digests match). A Windows build of the branch with `LO_SHADER_PACK_DOWNLOAD=1` and an empty folder downloaded and used both.
- On the tablet with the pack removed, the window offered the 230.6 MiB pack; the on-screen controller's A started the download (the overlay covers the window, so its A, B and D-pad left/right are read from the touch input), and the pack was installed after 57 s, hit by the renderer with 0 startup DXC calls.
- Not checked: Linux native Vulkan with the new build, GPU-bound frame time differences.

### Validation (2026-10-02)

- `LoShaderPackIndexTest` (35 checks: parsing, selection, asset names, URLs, decline record) passed with GCC 16 and clang 22 under ASan and UBSan; it runs in review-regressions.
- `LoShaderPackTool contract` printed the contracts of the three 2026-10-02 packs, and the publish script staged them with an index without uploading.
- A Windows build was run with `--prepare-shaders-only` against a local HTTP server serving that index. Installed and used: Vulkan and DX12 packs, and a Vulkan pack replacing one with another contract. Reported and left alone: an unpublished contract, a wrong SHA-256 (no file left behind), a transfer longer than the listed size (stopped, no file left), an unreachable index, a background run and a remembered decline. `LoPortableShaderPackIntegrationTest`, which compiles the renderer's pack code against fake services, passes with GCC and clang on Linux. In the window, Download, Skip, Cancel and the failure page were driven by keyboard messages, in English and Chinese.
- The three packs were published to the `shader-packs` prerelease on 2026-10-02 at 07:24 UTC; GitHub's SHA-256 digests match the exported files. The same Windows build, without an index override, then downloaded and used the Vulkan pack (180 MB, installed 9 s after start) and the DX12 pack (80 MB, 5 s) from it.
- On an M1 Max with macOS 26.6.2, a Metal build of main `2cd3fe6` computed the same three contracts, downloaded the Metal pack from the release (SHA-256 as published, installed 9 s after start) and loaded it; a second start found it in 0.24 s. The `new-game-battle` scenario then passed with an empty shader cache (2,047 draws, 30 FPS): no guest shader was compiled, so every one came from the pack, and the opening movie and the battle menu rendered correctly. The 17 compiles in that run were the renderer's own built-in shaders.
- v0.7.35 (published 2026-10-02T09:13:35Z) is the first version release built without a pack. The Linux release job's "Check published shader packs" step ran `publish_shader_packs.py --check` ([Gitea run 135](https://git.zkx.ca/freefrank/LostOdysseyRecomp/actions/runs/135)) and found the d3d12, metal and vulkan contracts in the index. The packages are 77,109,842 bytes (Windows ZIP), 76,270,072 (AppImage) and 54,113,384 (Flatpak), against 254,531,925, 253,770,232 and 231,397,328 for v0.7.25, which bundled the Vulkan pack. The release did not change the `shader-packs` prerelease, and no game run was made with the v0.7.35 packages ([release record](STATUS.md#v0735-published--2026-10-02)).
- Not yet checked: Linux runs (the new sources only compiled there) and mouse input in the window.
- Update, 2026-10-03: v0.8.0 (published 2026-10-03T06:41:51Z) and v0.8.5 (2026-10-03T18:45:17Z) also carry no pack. Their Windows ZIPs are 77,134,904 and 77,142,293 bytes, their AppImages 76,311,032 and 76,306,936 and their Flatpaks 54,113,296 and 54,129,440. The v0.8.5 runtime's contracts were already in the published index (DirectX 12 `239f8775...`, Vulkan `eecb4425...`; the `portable_dx12-239f877563b6dc7a.lospd` and `portable_vk-eecb4425e7f53d44.lospv` packs above): `publish_shader_packs.py --check` passed on a Mac before the tag and no pack was published for the release. The release run's mandatory Linux-job check succeeded; its output was not read here, and no game run was made with the packages ([v0.8.0 record](STATUS.md#v080-published--2026-10-03), [v0.8.5 record](STATUS.md#v085-published--2026-10-03)).

### Update, 2026-10-04: packs for translator version 27

PR #186 (the AMD shadow fix, shipped in v0.8.10) raises the shader translator version to 27, so both contracts changed and the packs of 2026-10-02 no longer match the v0.8.10 runtime. The new packs were built on Windows (RTX 5080) from `main` at `682fef55` (the v0.8.10 tag differs from it only in the source version in `CMakeLists.txt` and in documents) with `tools/shader_pack/build_packs.py --sources <the 950 learned sources> --stage`: Vulkan 373 s and D3D12 110 s from an empty cache, 28,687 records each, and `verify-runtime` passed for both. After the maintainer approved, `publish_shader_packs.py --publish` uploaded them to the `shader-packs` prerelease; GitHub dates the packs 05:14:56Z and `index.json` 05:17:13Z on 2026-10-04, before the tag (05:17:26Z). `publish_shader_packs.py --check` then passed locally ("lists packs for all renderers of this runtime"). The index keeps the entries for older contracts (the prerelease now holds seven pack assets), so older runtimes still find theirs.

| Pack | Bytes | SHA-256 | Contract |
|---|---:|---|---|
| `portable_vk-a7d1ab94ff3a0597.lospv` (Vulkan on Windows, Linux and Android; Metal) | 241,862,616 | `954c2acfc7d6a2d6eacc2dc7925703c86a7c94ab023d5477673dd149f2d214dc` | `a7d1ab94ff3a0597…` |
| `portable_dx12-48cf14e3720d8a64.lospd` | 80,492,620 | `b83563863ba2aca8f3698afd5bc6e29fe87d030d86bda3d374d2a96a202b5262` | `48cf14e3720d8a64…` |

GitHub's digests equal these SHA-256 values. [v0.8.10](STATUS.md#v0810-published--2026-10-04) was published at 2026-10-04T05:39:20Z; its Linux job runs the same `--check` and succeeded, and its output was not read. No game run with these packs, and no download of them by a running game, is recorded here.

### Update, 2026-10-04 (later): no new packs for v0.8.15 and v0.8.21

[v0.8.15](STATUS.md#v0815-published--2026-10-04) (published 11:52:16Z) and [v0.8.21](STATUS.md#v0821-published--2026-10-04) (22:02:56Z) keep the translator version 27 contracts of v0.8.10, so no pack was built or published for either. In both release runs the Linux job's "Check published shader packs" step passed (v0.8.15 at 11:48:13–11:48:16Z, v0.8.21 at 21:58:07–21:58:10Z), and its log, read after v0.8.21 was published, prints `d3d12: contract 48cf14e3720d8a641af24ce20f8ab6b4365a9be9f5fafff3c8ad22897971a3d6` and `vulkan: contract a7d1ab94ff3a05973e8c1192d888bf17a04ec671cddbd32a5219b301409c00c4`, then "The shader-packs index lists packs for all renderers of this runtime." `tools/release/publish_shader_packs.py --check` was also run on Windows with `LoShaderPackTool` built from the v0.8.21 tree and printed the same two contracts; the published index covers both. The `shader-packs` prerelease index (`index.json`) was last updated 2026-10-04T05:17:13Z, before v0.8.10 was published; the release holds that index and seven packs, the newest two being the translator 27 packs above (GitHub release metadata, read at about 22:10 UTC). No game run with these packs, and no download of them by a running game, is recorded here for the two releases.

### Update, 2026-10-05: no new packs for v0.8.30

[v0.8.30](STATUS.md#v0830-published--2026-10-05) (published 2026-10-05T20:54:12Z) keeps the translator version 27 contracts, so no pack was built or published. `tools/release/publish_shader_packs.py --check` passed before the tag was pushed against the existing published index (`d3d12` contract `48cf14e3720d8a64…`, `vulkan` `a7d1ab94ff3a0597…`). The release run's Linux job runs the same check and succeeded; its log was not read for this record. The `shader-packs` prerelease index (`index.json`) was last updated 2026-10-04T05:17:13Z and still holds seven packs (GitHub release metadata, read 2026-10-05 at about 20:56 UTC). No game run with these packs, and no download of them by a running game, is recorded here for v0.8.30.

### Update, 2026-10-06: no new packs for v0.8.37

[v0.8.37](STATUS.md#v0837-published--2026-10-06) (published 2026-10-06T07:46:23Z) keeps the translator version 27 contracts, so no pack was built or published. `validation/mac-pack-check.sh` was run on `main` at `22780369`, which has the same shader and translator sources as the tag (the diff from that commit to the tag touches the F1 debug menu code, its two tests, `CMakeLists.txt` for the version, the Changelog, the READMEs, the installation guides and `MACOS_RELEASE.md`), and it listed packs for both runtime contracts: `d3d12` `48cf14e3720d8a641af24ce20f8ab6b4365a9be9f5fafff3c8ad22897971a3d6` and `vulkan` `a7d1ab94ff3a0597...` (full value `a7d1ab94ff3a05973e8c1192d888bf17a04ec671cddbd32a5219b301409c00c4`), the same values as for v0.8.30. The release run's Linux job runs `publish_shader_packs.py --check` and succeeded; its log was not read for this record. No game run with these packs, and no download of them by a running game, is recorded here for v0.8.37.

### Update, 2026-10-06 (later): no new packs for v0.8.39

[v0.8.39](STATUS.md#v0839-published--2026-10-06) (published 2026-10-06T08:48:26Z) keeps the translator version 27 contracts, so no pack was built or published. `validation/mac-pack-check.sh` was run on the tag build (`fd7d82ce`) and listed packs for both runtime contracts: `d3d12` `48cf14e3720d8a641af24ce20f8ab6b4365a9be9f5fafff3c8ad22897971a3d6` and `vulkan` `a7d1ab94ff3a05973e8c1192d888bf17a04ec671cddbd32a5219b301409c00c4`, the same values as for v0.8.30 and v0.8.37. The release run's Linux job runs `publish_shader_packs.py --check` and succeeded; its log was not read for this record. The release does change what an installed pack causes at startup: since PR #248 a start that finds a matching pack deletes its renderer's startup bundle and the store records the pack's index lists ([note](notes/shader-store-2026-10-02.md#cache-cleanup-once-a-pack-is-installed-2026-10-06)). That was measured only on Proton Direct3D 12 with the Direct3D 12 pack (see the [v0.8.39 record](STATUS.md#v0839-published--2026-10-06)); the Vulkan and Metal paths were not run. No game run with the v0.8.39 packages, and no download of the packs by a running game, is recorded here.

### Update, 2026-10-06 (later still): no new packs for v0.8.44

[v0.8.44](STATUS.md#v0844-published--2026-10-06) (published 2026-10-06T17:34:21Z) keeps the translator version 27 contracts, so no pack was built or published. `validation/mac-pack-check.sh` was run on the Mac at `504e7cd0`, the tag commit, and listed packs for both runtime contracts: `d3d12` `48cf14e3720d8a641af24ce20f8ab6b4365a9be9f5fafff3c8ad22897971a3d6` and `vulkan` `a7d1ab94ff3a05973e8c1192d888bf17a04ec671cddbd32a5219b301409c00c4`, the same values as for v0.8.30, v0.8.37 and v0.8.39; the published shader-packs index lists packs for all renderers. The release run's Linux job runs `publish_shader_packs.py --check`; the run concluded success and the step's log was not read for this record. No game run with the v0.8.44 packages, and no download of the packs by a running game, is recorded here.

## Runtime contract and release check (after v0.7.25)

v0.7.25 shipped the Vulkan pack pinned for v0.7.10, although its runtime had moved from shader translator version 24 to 26 (the macOS merge changed predicate-push translation). The game rejected the pack (`portable shader pack rejected; local cache fallback: portable shader contract mismatch`) and compiled all 28,549 shaders on first launch: 180 s with 15 workers on a 16-thread CPU. Nothing in the release compared the pack with the runtime, and `verify-runtime` could not do it in general. The runtime hashed its guest image after `XexLoader` had written host-assigned import addresses into it, so the tool relied on one audited contract pair, which the next translator change made useless.

The contract now hashes the first `0x185C60` bytes of the image as parsed, before import binding (`XexLoader::UnboundIdentityPrefix()`). That prefix equals the start of `tools/xexdump` output, so `RuntimeContract`, shared by the renderer, its export and `LoShaderPackTool`, gives the same value offline. `verify-runtime` now checks SPIR-V and DXIL packs exactly, and the audited exception is gone from it and from `merge`. Packs made with the old definition are rejected. The local startup bundle still keys on the bound image, so local caches stay valid.

Until the startup download, release CI fetched a pinned pack from the private build-inputs repository, and the runtime build staged it only after `verify-runtime` accepted it. Packages now carry no pack. Instead the Linux release job builds `LoShaderPackTool` and runs `tools/release/publish_shader_packs.py --check`: a version release stops unless the `shader-packs` index lists all three contracts of its runtime. Branch builds only warn.

To refresh the packs after a translator, option or discovery change:

1. Build the runtime and `LoShaderPackTool` from the release source and run `tools/shader_pack/build_packs.py --game <disc1> --stage` ([building packs](#building-packs)). A complete local startup bundle or shader store made by that source is used without DXC; without one, the run compiles every shader.
2. Publish them with `python tools/release/publish_shader_packs.py --tool <LoShaderPackTool> --image LostOdysseyRecompLib/private/image_disc1.bin <packs> --publish` ([publishing](#publishing)). It runs `verify-runtime` on each pack first.

The pack refreshed on 2026-10-01 was exported in 18 s from the startup bundle that v0.7.25 had just built (no guest shader DXC calls). It has 28,549 records and 27,793 unique binaries, is 180,052,919 bytes, SHA-256 `f5eadb4fcc27a40bf4d76bae6bf83224bfb730fab8f49581ba3254c2d2f17c25`, contract `4e123a08e148937e41b1b7dc636608377bd15a0043fb506a43a75cb6930ea6ae`, and is pinned as build-inputs commit `5fae27a5a3c05262e7b64631f141ebcd19d5f656`. `verify-runtime` passed. A `--prepare-shaders-only` run with an empty cache reported a pack hit for all 28,549 records. The release build check failed with the v0.7.25 pack and passed with this one. The optional DX12 pack has not been regenerated for the new contract.

Update, 2026-10-02: the DX12 pack was regenerated for the new contract and published to the `shader-packs` prerelease together with a Vulkan and a Metal pack. A read-only GitHub API read at about 09:13 UTC listed `index.json`, `portable_dx12-5805497255fa73b4.lospd` (80,462,294 bytes), `portable_metal-6fe8486e1f59ca31.lospv` and `portable_vk-4e123a08e148937e.lospv`, all uploaded, and the index lists the d3d12, metal and vulkan contracts. The Vulkan pack published there is the 28,687-record, 180,527,457-byte file of 2026-10-02, not the 2026-10-01 file described above. See [Validation (2026-10-02)](#validation-2026-10-02) and the [shader store note](notes/shader-store-2026-10-02.md#packs-exported-on-2026-10-02).

## v0.7.10 shader-pack release

The v0.7.10 shader-pack work added backend-specific selection and a DX12
`.lospd` format alongside the existing Vulkan `.lospv` path. The new Vulkan
pack contains 28,546 records and is 180,198,461 bytes; it covers 8,186
installed v24 `.spv` cache entries, 62 source microcodes and 205 direct DLC
candidates. The new DX pack contains 28,546 records and is 80,222,382 bytes;
`LoShaderPackTool verify` passed for it. The CPU contract passed 74 checks and
the development game build passed.

These packs are published with v0.7.10. A Windows D3D12
`--prepare-shaders-only` run selected the DX12 backend and loaded the default-path
DX pack, reporting `28546 records`, `25057 unique binaries` and `80222382 file
bytes`; guest startup was intentionally skipped. The standalone DX asset is
published separately. The runtime-hit check used `--prepare-shaders-only` and did not
start the guest or validate GPU draws, image quality or full-game coverage.

## v0.6.15 release

The v0.6.15 GitHub Release was published on 2026-09-24T19:47:35Z from source `6eef30d257f2e14ce30a546217574a0dc74fad69` via Release CI [36044604844](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/36044604844). The updated portable Vulkan shader pack contains 28,527 shaders (178,332,830 bytes, SHA-256 `b486c87d121968bcec67fae6bc1aa7926378455281bc8b2a221409b8c06c6e2b`), consolidating 45 newly compiled microcodes from recent gameplay caches with the 28,482 baseline records.

Fixed hash and FidelityFX license gates passed in Release CI. The pack is bundled directly into the official Windows and Linux application packages; users do not need a standalone Vulkan bundle, and the standalone ZIP and sidecar were removed from public release assets. The DX12 bundle statement above supersedes the historical note that a DX12 bundle was not yet implemented.

## v0.6.1 release

The v0.6.1 GitHub Release was published on 2026-09-18. [Release CI
35374267882](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35374267882)
passed the Windows and Linux Release jobs and focused regressions. The standalone pack is
available from the [v0.6.1 release](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.6.1),
with SHA-256
`387a23b9328b8136847d48b37b574fddd600a526eb837807fbb08b758c6de4d9`.
It is byte-identical to the v0.6.0 pack and contains 28,482 records under the v0.6.1
asset name; this is release provenance rather than a claim of new shader coverage.

At v0.6.1 the source-side verifier permitted the audited exception for the raw image
contract `d5a2fab10441a46444b6b41ffcb4f1ba562bea75668a7b445fd43688aec67507`
mapping to runtime contract
`f6fd1179b50f6ff9b63d6be84c662d1337af6b7dfa78865a9a6c025509c9b77f`; other
images retain direct contract verification. This is required because `XexLoader`
patches import thunks before runtime shader preparation. The current local
verification covered a real 28,482-record pack with all payloads and runtime
compatibility, and `portable_shader_release_test.py` passed. Previous runtime
logs also recorded pack hits in two runs, but they came from a 0.5.14 dirty
source and are retained as supporting evidence only. The exception was removed
after v0.7.25, when the contract moved to the image before import binding (see
the runtime contract section above).

The uploaded pack matches the audited contract through the release verifier and
the published asset's sidecar and GitHub digest match. Native Linux GPU, Steam
Deck, AppImage update transactions and full-game shader coverage remain unverified.

## Implemented

- A separate, read-only `.lospv` distribution artifact for successful guest SPIR-V
  and all fields of `TranslatedShader` needed for rendering. HLSL, translation
  diagnostics and negative compiler records are not shipped.
- Exact SPIR-V binaries are deduplicated by SHA-256, while every `(stage, guest
  hash)` keeps its own metadata. There is no semantic/approximate shader merging.
- Zstandard compression in independent roughly 1 MiB blocks. A single unusually
  large shader can occupy a block up to 16 MiB. The writer streams blocks; the
  reader opens a small index and retains only its most recently decoded block.
  Neither needs to retain the whole decompressed binary corpus.
- Runtime module creation is lazy. Already-created shaders still take the normal
  single-map-lookup hot path. A valid portable pack bypasses startup guest shader
  discovery/translation/DXC. Missing shaders still use the existing local path.
- Before existing PSO preparation takes pointers into the shader maps, all shaders
  needed by those PSO recipes are loaded first. No map insertions during that
  pointer-collection phase. Existing PSO worker scheduling is otherwise unchanged.
- Local success caches, negative caches and schema-2 startup bundles keep their
  current compiler/path identity rules. No cache-version rollback or global
  weakening of `cache::IdentityKey` / `RuntimeIdentity`.
- Portable compatibility keeps schema, translator options, variant and layout
  markers needed to reject an incompatible pack. It does not bind startup to
  install paths, local DXC DLL/SO hashes or a full content digest.
- The reader checks the pack format, bounded Zstd frames, sizes, offsets and
  decompression boundaries. It loads records on demand and does not scan every
  SPIR-V payload at startup. A malformed record disables the pack and leaves
  the local fallback retryable, without inserting an invalid shader entry.
- Packages carry no pack (PR #144, first shipped in v0.7.35). The Windows ZIP,
  AppImage and macOS packaging helpers stage only the Zstandard license
  (`tools/portable_shader_pack_payload.py`), and the Flatpak payload check requires
  that license and rejects a stray `shaders/portable_vk.lospv`. The game downloads
  the pack for its renderer at startup. Before v0.7.35 the Windows ZIP and Linux
  AppImage staging copied `shaders/portable_vk.lospv`; the DX12 `.lospd` asset was
  published separately and never copied into the application payloads. Packaging
  does not scoop up `cache/shaders`, debug output, HLSL, both backends, or old
  cache versions.

## Building from the repository

The implementation is integrated into the repository. Follow the [build guide](BUILDING.md);
the original `apply_portable_shader_pack.py` delivery script is not a current
checkout prerequisite. Focused pack tools can be built independently as below.

## Build and CPU tests

The pack library needs static Zstandard. CMake uses an installed static zstd CMake
package, or fetches upstream v1.5.7 pinned to commit
`f8745da6ff1ad1e7bab384bd1f9d742439278e99`. For offline builds, install a static
package or set `FETCHCONTENT_SOURCE_DIR_LO_PACK_ZSTD` to the pinned source tree.
`LO_PACK_FETCH_ZSTD=OFF` makes a missing package a configuration error.

No game assets are required for these targets:

```sh
cmake -S tools/shader_pack -B out/portable-pack -DCMAKE_BUILD_TYPE=Release
cmake --build out/portable-pack --config Release --target LoShaderPackTool LoPortableShaderPackTest LoPortableShaderPackIntegrationTest
ctest --test-dir out/portable-pack -C Release --output-on-failure
```

The original `tools/tests/shader_prebuild_regression.py` remains a fake-DXC/GPU
orchestration test. Its fixture is updated to include the real new integration
methods/library, plus the settings/skip API that menu had already introduced.
It now also needs zstd development headers/library (`-lzstd`). Existing checks
are not deleted or rewritten to assert away a regression.

## Export without recompiling already-valid shaders

Use a runtime containing the pack implementation, keep the same game root / local cache / DXC
pair, select Vulkan, and enable normal shader preparation. In PowerShell:

```powershell
$env:LO_SHADER_EXPORT_PACK = "$PWD\shaders\portable_vk.lospv"
# Launch the rebuilt runtime with the normal game arguments.
```

An existing *compatible, complete* startup bundle is validated and streamed into
this exporter. That path does not translate or invoke DXC for its guest shader
records. HLSL is reconstructed one record at a time only to count discarded text,
then discarded again. If the local bundle is missing or incompatible, normal
preparation runs once and the exporter consumes its successes instead.

Export is explicit, and `.lospv` is required as its suffix to avoid overwriting a
local `.bundle`. Deterministic failures are counted but omitted. Transient
compiler/infrastructure failures, cancellation, incomplete discovery and device
module failures discard the temporary export. The destination is atomically
replaced only after completion. No partially completed export is published.

Remove `LO_SHADER_EXPORT_PACK` after export, otherwise every subsequent launch
continues to run the developer export path:

```powershell
Remove-Item Env:LO_SHADER_EXPORT_PACK
```

An export log ends with `portable shader pack published` and exact byte counts.
The file is deliberately not called `startup_vk12_v1.bundle`; old clients cannot
read it. Ship it only with this patched client.

### Metal pack

macOS reads SPIR-V too, but compiled at `-O1` (`cache::MetalOptions()`), which is a separate contract. A Vulkan run with `LO_SHADER_EXPORT_METAL=1` next to `LO_SHADER_EXPORT_PACK=<path>.lospv` exports that contract from Windows or Linux. Check it with `LoShaderPackTool verify-runtime <pack> <image> --metal`. A Mac reads it as `shaders/portable_metal.lospv`, where the startup download installs it ([startup download](#startup-download)). The packs exported on 2026-10-02 are listed in [the shader store note](notes/shader-store-2026-10-02.md#packs-exported-on-2026-10-02).

## Measure, verify and stage

```sh
LoShaderPackTool inspect shaders/portable_vk.lospv
LoShaderPackTool verify shaders/portable_vk.lospv
LoShaderPackTool verify-runtime shaders/portable_vk.lospv LostOdysseyRecompLib/private/image_disc1.bin
```

`inspect` reads structure/index. `verify` also visits all payload blocks and checks
SPIR-V framing. JSON reports raw bytes, deduplicated bytes, compressed bytes,
index bytes, omitted source/diagnostic bytes and final file size. Omitted HLSL
is the sum of reconstructed input strings; the old bundle shares its prelude,
so this is NOT a measurement of bytes saved from that old file. Use `file_bytes`
for the actual distribution size. `inspect` and `verify` report
`runtime_compatibility_verified: false`: they never trust an artifact's own header.
`verify-runtime` computes the runtime contract for the pack's format from a
`tools/xexdump` image and reports `true` only when the pack matches it.

To stage automatically during normal builds, add this CMake cache option to the
existing build configuration:

```text
-DLO_PORTABLE_SHADER_PACK=/absolute/path/to/portable_vk.lospv
```

This opt-in CMake path builds `LoShaderPackTool`, runs `verify-runtime` against
the configured private image, and copies the file to
`<executable-directory>/shaders/portable_vk.lospv`. Linux install uses `bin/shaders`.
This path is for development builds. Release CI no longer uses it (PR #144, first
shipped in v0.7.35) and the packaging helpers do not copy the file; instead a
version release stops unless the published `shader-packs` index lists all
contracts of its runtime (three in v0.7.35; two since v0.8.0, when Metal and
Android began to read the Vulkan pack) ([release check](#runtime-contract-and-release-check-after-v0725)).
Use the explicit tool commands above when changed pack inputs require inspection.

For a standalone downloaded pack, put it under `shaders` beside the final game
executable, or set `LO_SHADER_PACK_PATH` to an absolute file path. Lookup is based
on the executable directory, not the process working directory. No unpacking to
hundreds of megabytes in the user's writable cache is necessary.

`LO_NO_PORTABLE_SHADER_PACK=1` is the explicit pack bypass. Full scan, HLSL dump,
failure retry and export also bypass it. Ordinary `skip_shader_prebuild` and
`LO_NO_SHADER_PREPARE` suppress bulk prebuild, but still allow this index-only
pack path and on-demand cache use. The pack contains no negative records.

## Merge new shader caches into baseline packs

`LoShaderPackTool` provides a `merge` subcommand to import newly observed shader caches into an existing portable pack baseline without recompiling unchanged records:

```sh
LoShaderPackTool merge <baseline.lospv> <decrypted-image.bin> <manifest.tsv> <output-dir>
```

- **Manifest format**: A tab-separated file with header `action\tstage\thash\tsource\tprovenance`. Each row defines:
  - `action`: `include` or `exclude`.
  - `stage`: `vs` or `ps`.
  - `hash`: 16-character lowercase hexadecimal guest shader hash.
  - `source`: Relative path from the manifest file to the raw microcode binary; use `-` for excluded entries.
  - `provenance`: Origin description text.
- **Compilation & Preservation**: New microcodes are compiled using the current shader translator and DXC. `Contains` checks whether a candidate key already exists; `Writer::Import` preserves baseline metadata, omissions, and byte payloads without recompilation.
- **Safety & Atomicity**: The baseline pack and input sources are opened read-only. Merged packs and reports are assembled in an exclusive temporary directory before publishing to `<output-dir>`, and the tool rejects existing output destinations to prevent accidental overwrite. The tool CLI has no hardcoded count limit.
- **DXC requirement**: Compilation requires SPIR-V code generation support. Use the repository-bundled DXC binaries rather than generic system DXC installations that may lack SPIR-V emission.

### Development merge verification

A local test merge produced `out/merged-shaders/portable_vk.lospv` (178,332,830 bytes, SHA-256 `b486c87d121968bcec67fae6bc1aa7926378455281bc8b2a221409b8c06c6e2b`). Starting from the 28,482 baseline shaders, 45 raw microcodes gathered from recent gameplay testing were recompiled and merged, reaching 28,527 total shaders (0 skipped, 1 excluded: `vs_8f6ce5a4f714294a` due to missing supplementary source metadata; original cache entry retained). Verification via `LoShaderPackTool verify-runtime` confirmed `all_payloads_verified: true` and `runtime_compatibility_verified: true`. Detailed logs are recorded in `out/merged-shaders/merge-execution.log`, `verification.log`, and `merge-report.json`. On 2026-09-24, this merged 28,527-shader pack was packaged directly into the official v0.6.15 Windows and Linux application archives via Release CI [36044604844](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/36044604844); standalone bundles were omitted from publication.

## Initial implementation evidence and limits (historical)

The following paragraphs record the original source-only delivery before the
v0.6.1, v0.6.15 and v0.7.10 measurements above. Their absence-of-evidence claims
and proposed checks do not override those later results or require retesting an
unchanged release.

No complete real startup bundle or SPIR-V corpus was provided in this session.
Therefore **no real final pack size or game-data compression ratio is measured**.
800–900 MB may be a plausible raw corpus size; it is not an established download
size. The synthetic compression test is a functional test, not a predictor of
Lost Odyssey's compression ratio.

The implementation has native CPU checks for exact byte/metadata roundtrips,
negative-record omission, exact-binary deduplication, lazy reads, relocation,
contract invalidation, malformed/corrupt payloads, interrupted publication and
concurrent reads. The exact production `.inl` is separately compiled against
explicit fake GPU/DXC services. See the delivery validation report for commands
and outcomes.

Not established here: full runtime linking with the private generated game
inputs; actual DXC execution; actual driver module/pipeline creation; Windows,
Steam Deck or ARM execution; full-game shader coverage; FPS, load-time or peak
process-memory improvement. Core parsing checks are not full SPIR-V semantic
validation. Checksums detect corruption, not authorship: use trusted release
provenance/checksums and validate real shaders/drivers before publishing.

Not changed in this first implementation: shader translation/optimization,
specialization-constant conversion, PSO threading policy, builtin host shader
compilation, or removal of DXC from the application. Unlike reblue, this initial
codec uses block Zstd without smol-v. Exporting from a valid local bundle is the
lowest-risk release-generation path; an autonomous all-assets CI shader compiler
is not added here.

Before release, verify one exported pack on Windows Vulkan and Linux/Deck under
different paths, check that covered shaders do not invoke guest DXC, test an
uncovered shader's fallback, and compare real scene images and frame-time traces.

### Original patch-delivery procedure

The original delivery archive contained an application script and source payload,
without game shaders, an executable, game data or an exported pack. Its Python
3.10+ commands were:

```sh
python apply_portable_shader_pack.py --repo /path/to/LostOdysseyRecomp --check
python apply_portable_shader_pack.py --repo /path/to/LostOdysseyRecomp --apply
```

That historical script checked Git blob identities after CRLF normalization and
replacement anchors, and made `.lo-portable-backup` backups. It did not reset,
commit or push. `--allow-compatible-edits` permitted reviewed compatible edits;
`--diff candidate.patch` generated a patch against the local checkout. These are
archive instructions, not commands supplied by the current repository.
