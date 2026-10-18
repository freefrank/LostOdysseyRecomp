# Game Data Sources and XEX

[简体中文](xex.zh-CN.md)

> **Topic reference.** The implementation and measurements below apply only to the versions and scope stated for each one. For current delivery and acceptance status, see [project status](../STATUS.md) and [roadmap](../ROADMAP.md).

> Verification date: 2026-09-05. The Asian multi-language version named in the README is based on the project owner's description of the source version. The XEX fields below were read again from the local files. The region mask cannot prove the retail region or language list by itself.

## 2026-09-06 USA/Europe support update

v0.2 officially released support for the verified USA/Europe `0.0.0.3` four-disc XEX set. Code commit `dcc9462` and tag `v0.2` were both pushed. This support is not in v0.1. The full decrypted images of all four discs are 20,709,376 bytes each. They match the current Asian version's Disc 1 byte for byte, so the existing recompiled code and addresses are reused. The original XEX hashes and resource packs still differ, so they must not be mixed.

The importer strictly checks the SHA256 of each of the four discs. At runtime, before settings are loaded, it reads the version and disc number / Media ID of the installed XEX and selects the matching game language list. For the USA/Europe versions: 1 = English, 2 = Japanese, 3 = German, 4 = French, 5 = Spanish, 6 = Italian. The Asian versions are still 1 = INT, 2 = JPN, 7 = KOR, 8 = CHI, 9 = SCH. For the detailed test scope, see [Europe support](europe-support.md). The Asian data sources and investigation records from 2026-09-05 are kept below. They do not represent the verification scope of the updated cross-version support.

## 2026-09-06 automatic reading of imported discs

v0.2 includes the hookup for the original disc-change request. It first verifies the target disc's XEX identity, the FPI disc number and structure, the 13 FPDs, and the index range. Then it switches the game path, and the original manager continues to unload and reload the matching index. Each disc keeps its own FPI/FPD. They are not merged and rewritten into a single index. If the target disc is missing or invalid, the old mount stays in place and the operation returns a failure. Storage read-back for both versions and the controlled flow of the original manager for both versions have passed. Plot scenes at chapter boundaries are not yet verified. See [automatic disc selection](disc-selection.md). The "merge strategy" below is an early investigation record and is not the current implementation.

## Version and language scope (recorded 2026-09-05)

The development data source is the Asian multi-language version. Title ID `4D5307FA`, Disc 1 Media ID `39F7D748`. The title and base version are both `0x00000004` (0.0.0.4). The region mask is `0x00FFF900`. No port was verified against other regional XEX files or Title Updates.

The current HLE returns English 1 for the language item of `ExGetXConfigSetting`. Existing live runs use English and the `int` resource suffix. The image contains INT/JPN/KOR/FRA/DEU/ITA and other language codes. However, engine strings do not prove that this retail release contains the full matching text and voice. The complete original language list and the per-language switching of the port are still to be verified. No claim is made that all languages are supported.

## Original data (local paths, not in the repository)

The user supplies the four-disc data directory in GOD format (Games on Demand). Title ID `4D5307FA`, content type `00007000`. The source label is "CH". The project owner describes it as the Asian multi-language version. Do not decide Traditional Chinese support from the directory name alone. This round did not compare XEX addresses across regions. When referring to re:Blue or other projects' addresses, do not copy them directly.

| Container (4D5307FA/00007000/) | Disc | MediaID  | Size |
|---|---|---|---|
| 65975F33B848FC1188CC | Disc 1 | 39F7D748 | 5.1G |
| 62A4296DC2D351F07D77 | Disc 2 | 0EF8CEA8 | 4.3G |
| 6AC59496FC5538BBCDFA | Disc 3 | 309E3386 | 5.5G |
| DB4D07AEE9167C1AFB3A | Disc 4 | 7B21A91D | 5.7G |

DLC (STFS, content type `00000002`, Marketplace):
1. Other Labyrinths - Seeker of the Abyss (extra dungeons)
2. Reward item two-in-one set
3. Special item three-in-one set

## Extraction

The project already has `tools/god_extract.py` for extracting from GOD containers. All four discs have been unpacked separately into `LostOdysseyRecompLib/private/disc1..4/`.
`default.xex` is in the Disc 1 root.

## default.xex (Disc 1, read with tools/xex_info.py)

- Original PE name `XenonLaunch-RPGame.exe`, image_base `0x82000000`, entry `0x827CA440`, image_size `0x13C0000`
- Encrypted with AES, compressed with LZX (window 0x8000). The decryption and decompression built into XenonRecomp's XenonUtils work, and `tools/xexdump` can export a flat image.
- Title version 4, base version 4, no XEXP patch. The four discs' XEX files are the same size, each with its own media_id.
  The multi-disc media id list: 39F7D748 0EF8CEA8 309E3386 7B21A91D. On 2026-09-05, the four local decrypted images were checked again. Their MD5 is `f07cb5afd8ab6fa8fbdc14f4fceb5858` for all four. The original ciphertext differs.
- game_regions `0x00FFF900`: all PAL regions + Japan + other NTSC-J regions. It does not include North America or mainland China.
  When referring to the US version addresses, they must be located again.
- Imports: 136 from xam.xex, 319 from xboxkrnl.exe. Kernel version 2.0.6683.
- Static libraries (XDK 6274): XAPILIB XBOXKRNL LIBCMT MDISC XGRAPHC D3DX9 XNET XAUD XMP X3DAUD XMEDIA XONLINE D3D9LTCG
  → D3D9 is statically linked into the game, so the rendering layer must hook the D3D functions inside the game (the same approach as UnleashedRecomp).
  → MDISC is the multi-disc switching library. XNET and XONLINE need stubs.

## File layout on the disc

Every disc has the same root: `default.xex`, `LO.fpi` (the index, different on each disc), `LO.fpd` (the same on each disc),
`$SystemUpdate/`, and 12 `xenon_*.fpd` resource packs. `.fpd` files start with the magic `cpx`. This is feelplus's custom container, and UE3's UPK packages are inside it. The battle/chr/loc/obj/scr/sys/vfx/world files are exactly the same size on all four discs. event/field/mov/snd differ on each disc (plot progression content). Merge strategy: identify duplicates by file hash, and keep only one copy of each duplicate.

## Items to confirm
- [x] The local decrypted images of the four discs have the same MD5. The current recompilation target uses Disc 1. See the [recompilation notes](recomp.md).
- [ ] Whether any Title Update was released. If so, obtain its XEXP.
- [ ] `.fpd`/`.fpi` format parsing (stage 3)

## v0.2 release status

Officially released at 2026-09-06 17:16 UTC. The hosted Windows CI, release package hashes and manifest checks, the importer self-test included in the package, and a 30-second German menu rendering startup all passed. For details, see [release status and verification](../STATUS.md). Early investigation and local test records are kept. The release does not mean that the chapter-boundary plot or a full playthrough has been accepted.
