# High-Resolution / High-Frame-Rate CG Replacement: Feasibility Study

[简体中文](hd-cutscene-replacement.zh-CN.md)

2026-10-09. Sources: static reverse engineering of the recompiled code (game not run), FMV asset export (`asset-inventory-20261002/fmv`), and external research. Related issues: #265 (players want 1080p/4K CGs) and #40 (mod support).

## Conclusions

(2026-10-12: the replacement video format changed to H.264 + MP4. See "Decoding" below.)

(2026-10-12: the Windows version is implemented. Media Foundation decodes the video, using hardware decoding when available. Both D3D12 and Vulkan draw onto the original quad. During replacement, original frame updates that cannot be reached are skipped, and the CG displays at the game frame rate. On psvita (Proton), 1080p60 and 4K60 were validated, along with natural end, skip, and switching back to the original when the replacement clip is shorter. Linux, macOS and Android do not have a decoder yet, so they still play the original.)

It is feasible, and it needs no change to the game's save, archive, or playback logic:

- The game keeps playing the original WMV as usual. Audio, subtitles, skip, and end timing do not change.
- Only the picture is replaced. At the position where FLO_MOVIE draws the quad, the replacement video decoded on the host is drawn instead.
- The resolution of the replacement video is independent of the game, so 4K works.
- The frame rate can be higher than the original. The game redraws this quad every frame (60/120 fps). As long as the replacement frame is chosen by the movie clock, a 59.94 fps replacement video can be shown frame by frame.

The project does not ship any replacement video. Players or mod authors make the replacement videos with their own tools (AI upscaling, frame interpolation), and install them as a mod.

## How the game plays CGs

| Item | Content | Basis |
|---|---|---|
| Material | 66 WMV files, `bin/xenon/mov/mv_XXv.wmv`, in `xenon_mov.fpd`. Video `wmv3` (WMV9 Main), 1280×720, 30000/1001 fps, about 7 Mbit/s. Audio WMA Pro, 1–3 tracks (languages) | Asset export `videos.csv` |
| Driver | Matinee track `UInterpTrackMovie`. UpdateTrack `sub_827E2DD0` calls `sub_82815248(M, name)` when a keyframe is passed, to start playback | Code |
| File open | The game opens the file by name itself (`sub_828140C8` → `sub_8284CD80`) and reads it into a cache. The player gets data only through two read callbacks and never sees the path. `FMVInfo.dat` determines the audio track (`entry[6 + voice ID]`) and the video track (`entry[4]`) | Code |
| Decoder | A statically linked Windows Media Format SDK player (strings `WMFDecodeX86`, WMV1/2/3/A/P/R FourCC checks). Not the `.xmv` API. 4 worker threads. The image contains 65 DXVA Xenos shaders, but no reference to them was found, so decoding should be pure CPU | Code (DXVA not used is an inference) |
| Audio | The player decodes WMA Pro and outputs through a statically linked XAudio2 | Code |
| Upload | `sub_82813D38` creates two sets (double-buffered) of three `D3DFMT_LIN_L8` textures: Y is W×H, U and V are W/2×H/2. On the render thread, `sub_823F0038` locks the back set. GetNextFrame (`sub_82CF8C38`) copies the decoded result line by line, then unlocks and switches the front set (M+12) | Code |
| Draw | FLO_MOVIE scene proxy Render `sub_823EFC08`: Y/U/V bound to texture slots 0/1/2. Pixel shader `ps_ce6fe349adbda428` does YUV→RGB. `sub_823E4B50` draws the quad (lr `0x823EFF88`, the same place `gpu/hor_plus.cpp` already hooks for black bars) | Code. The shader pairing is an inference |
| Timing | GetNextFrame compares the sample PTS (player+288) with the player clock (player+44). Late frames are dropped. Frames more than 38 ms early are not given. Frames more than 5 ms early cause `Sleep(difference − 5)` on the render thread. The texture updates at 29.97 fps on the movie clock. The front set is redrawn on every game frame | Code. Whether the clock follows the XAudio2 playback position is not confirmed |
| Pause / skip / end | Event 1 pauses (player vtbl[22]), 2 resumes (vtbl[23]), 4 releases. The end is decided by Matinee time. Subtitles are a separate track, `UInterpTrackLOSubtitles`, drawn by the game itself | Code. Subtitles are inferred from the class name |

## Recommended approach: host-side replacement picture

### Hook points

1. **Identify the movie**: hook `sub_82815248` (r3 = movie object M, r4 = name FString). Use the name to look up the mod resolver (`AssetKind::Movie`), and record M → replacement file.
2. **Sync**: hook `sub_823F0038`. When it returns 1 (a new frame was obtained), record the PTS of this frame (`LoadWord(M+20) + 288`) and the host time.
3. **Pause**: hook `sub_82813888` / `sub_82815010`. Freeze the host clock while paused.
4. **End**: close the decoder at `sub_82814570` (release command) or at `sub_82815640` on the render thread.
5. **Draw**: in the existing `sub_823E4B50` hook, send a private register packet (M, target rectangle, current time) as `gpu/movie_clear.h` does. When the renderer receives it, it does not draw the game's quad. It draws the replacement frame instead.

### Frame selection (high frame rate)

Replacement frame time = PTS of the game's front frame + host time elapsed since it became the front frame. Elapsed time is capped at one original frame (about 33 ms). This prevents the replacement picture from running ahead of the sound when the game stutters. A 60 fps replacement video changes frame every 16.7 ms at 60/120 fps. When the game frame rate is lower than the replacement frame rate, frames are skipped naturally.

### Decoding

Decision of 2026-10-12: use **H.264 + MP4**, not AV1. The original plan used dav1d for software AV1 decoding on the CPU. 4K60 would be too heavy for the Steam Deck. H.264 hardware decoding has the widest coverage (Windows, Steam Deck, macOS, and older phones), and authoring tools support it most widely.

- **Replacement video specification** (for mod authors):
  - MP4 container, with `moov` at the start of the file (ffmpeg `-movflags +faststart`).
  - H.264 High profile video, 8 bit, `yuv420p`, progressive, BT.709 limited range, constant frame rate.
  - The frame rate does not need to be a multiple of the original. At runtime, frames are selected by time (MP4 timestamps are matched to the game's millisecond movie clock). Of the originals, 45 are 30 fps, 19 are 29.97 fps, and `the_end_v` is 29.92 fps. When using an interpolation tool, it is simplest to take twice the original (60 or 60000/1001). Using 60000/1001 for all is also fine, but the video must be resampled by timestamp, and the total duration must not change.
  - Keep a 16:9 aspect ratio. For desktop packs, use 3840×2160 (4K60 needs Level 5.2). For packs for the Steam Deck and phones, use 1920×1080 (Level 4.2) or 2560×1440 (Level 5.1).
  - No audio track. The game plays the original audio as usual (multi-language tracks and subtitles are unchanged). Any audio track included is ignored.
  - The duration matches the original, aligned from the first frame.
  - Bitrate reference: about 40–60 Mbit/s for 4K60, about 12–20 Mbit/s for 1080p60.
- **Decoder**: extend the FFmpeg already in the project (Xenia's 4.4 branch, with a hand-written source list in `thirdparty/ffmpeg.cmake`, which currently builds only audio). Add the `h264` decoder and the `mov` demuxer, so all platforms use the same code. Hardware decoding uses FFmpeg hwaccel per platform: `d3d11va` on Windows, `vaapi` on Linux and the Steam Deck (libva loaded at runtime), `videotoolbox` on macOS, and `mediacodec` on Android. When none is available, software decoding is used. Movies read from a `.loarc` archive use FFmpeg custom IO (the offset and length given by `OpenAsset`).
- **Threads and upload**: the decode thread queues 2–3 frames ahead. Hardware frames are fetched with `av_hwframe_transfer_data` as NV12. Y is uploaded as R8 and UV as R8G8, into a persistently mapped ring buffer. The shader converts to RGB using BT.709 limited range. A 4K frame is about 12 MB, about 750 MB/s at 60 fps. 1080p is about 190 MB/s. Later, hardware frames could be shared directly, which would avoid the fetch.
- **Performance estimate (not measured)**: with hardware decoding, the CPU cost is mainly the fetch and the upload. Software decoding of 4K60 H.264 is feasible on a desktop CPU but not on the Steam Deck. So the Deck pack uses 1080p60.
- **Patents**: the H.264 decoder has patent licensing issues. Hardware decoding is provided by the system and the driver. The software fallback is FFmpeg's LGPL decoder. Confirm again before release whether to keep the software fallback.
- **Original decoding still runs**: under Plan A, the game's WMV decoding still runs (720p, on the CPU). Later, the video track can be set to -1 (hook `sub_82815060`; the player already accepts -1), leaving only audio. This would save the original video decoding. That would also require skipping texture creation and upload, and the host would draw the picture itself. This step is a later optimization of medium difficulty.

### Mod pack format

Follows the Mod API. Movie keys are no longer hashed, and have no export sequence number:

- Key = the original movie file name without the extension, in lowercase. For example, the key for `bin/xenon/mov/mv_00v.wmv` is `mv_00v` (the same name as the `movies/<name>.wmv` written by `--export-assets`). A valid key is 1–64 characters of `[a-z0-9_]`. Input is case-insensitive for ASCII and converted to lowercase. Path separators, dots, `#`, empty strings and over-length keys are all rejected.
- mod.ini: `movie:mv_00v=movies/mv_00v.mp4` (the path is relative to the mod folder).
- overlay / `.loarc` entry name: `overlay/movies/<key>.mp4`. When listing folders, file names are compared case-insensitively. In `.loarc`, they must be lowercase.
- Image / Font / Model still use `key-fnv1a64-…`. Texture and Text are unchanged.

`lo_mod.py archive` collects movie files under this rule. When `ffprobe` is installed, it checks the encoding (it must be H.264, `yuv420p`, progressive). It warns when the frame rate is outside 24–120 fps, when the aspect ratio is not 16:9, or when `moov` is after `mdat`. MO2 needs no change.

## Effort

| Stage | Content | Estimate |
|---|---|---|
| 1 | Add `h264`, `mov` and the per-platform hwaccel to the FFmpeg source list (including each platform's `config.h`). Decode thread and frame queue | 3–4 days |
| 2 | Five hooks, the private register packet, Y/UV upload and YUV→RGB drawing on D3D12/Vulkan/Metal | 2–3 days |
| 3 | `lo_mod.py` packaging of `movie`, the wiki, and logging | half a day |
| 4 | Smoke test on psvita with a homemade 4K60 `mv_00v` replacement (not released) | half a day |

## Tracing to add on psvita before implementation

1. For each `sub_823F0038`: the return value, ttp, player+288, M+144/148, and the GetStatus time. Confirm the PTS unit, and whether the clock is driven by audio.
2. Whether the `Sleep` in GetNextFrame on the render thread actually happens, and how long each time. (This affects the frame pacing at 60/120 fps during CGs, independent of replacement. It is also worth knowing.)
3. The CPU time of each of the 4 player threads.
4. M+120/M+124 (width and height) and the texture pitch.
5. An audio-only experiment (video track -1): sound plays, the clock runs, no crash.
6. Which event fires when Back is pressed to skip, and whether it reaches `sub_82814570`.
7. The vertex shader paired with `ps_ce6fe349adbda428`, and the values of c254/c255 (the YUV matrix).

## Other options

- **Picture quality improvement without mods**: when drawing FLO_MOVIE, use a better upscaling filter for the original 720p frames (for example FSR 1 EASU + RCAS, or Lanczos) instead of bilinear. The cost is low and all players benefit, but it does not add detail. It can share the same draw hook as the replacement approach.
- **Real-time frame interpolation or AI upscaling at runtime**: too heavy, not considered. Offline tools do better, and their results go to mods.
- **Frame generation**: DLSS-G and FSR FG already cover the CG quad. The replacement approach does not change this.

## Navigation

- Asset export: `C:\Users\freefrank\worktrees\LostOdysseyRecomp\asset-inventory-20261002\fmv` (66 original WMV files, `videos.csv`).
- Reverse engineering scripts (strings, cross-references, function ranges, call graph, `bl` targets) are in `C:\Users\freefrank\worktrees\LostOdysseyRecomp\_keep\scratch-helpers\guest-re`. They read `LostOdysseyRecompLib/private/image_disc1.bin` and `out/decomp-index/catalog.sqlite`.
- [Mod API](../wiki/Modding-API.md), [Mod Organizer 2](../wiki/Mod-Organizer-2.md), [Remaining work on runtime texture replacement](../wiki/Runtime-Texture-Replacement.md).
