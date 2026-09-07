# Kaim body-shadow flicker: v0.4.0 investigation

## Status — 2026-09-07

Suspended at the user's request after the final one-minute check produced no new root-cause lead. The reported flicker was not confirmed in the bounded local tests below. No production rendering change or verified fix was produced, and no reporter acceptance is recorded. This does not close the report or establish that v0.4.0 fixed it. The existing Map 13 body/environment-shadow report remains open; a relationship between the reports is unknown.

## Report and release identity

The reporter described shading on Kaim's body switching on and off, then used F1 and Capture render state. The supplied `render-17887894773758539-f3182.zip` passed CRC checks for all 211 entries, including 125 HLSL files. Its frame 3182 shows the opening Magic Khent Soldier battle command menu, with Kaim viewed from behind. The shadow atlas and mask contain data; recorded drop counters are zero. There is no runtime log or build manifest, so the reporter's build cannot be confirmed. A single-frame diagnostic export neither establishes both temporal states nor provides a complete replayable command stream.

Local gameplay checks used the official [published v0.4.0 package](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.4.0), whose manifest identifies commit `40362d78285d9ad829d2b0f6143d4fc54d3514d8`. ZIP SHA256: `92bf89f19d6eca3f6c2ec5c4a04d9ef372d0cadc9b3117546027e6e32e749cc1`; EXE SHA256: `6cbb1360c36a0492df78098a244c72bf4bba93e4f75052aab0e093c1ae30a09e`. A separate release-source regeneration/build/link also succeeded, but its executable was not used for these scene checks.

The reporter's capture identifies an RX 9060 XT, with the DXGI driver value decoded as `32.0.31041.1004`. Local tests used a Radeon 8060S with driver `32.0.23027.2005`, Asian-edition resources and English game text. The different GPU/driver limits conclusions about the original machine.

## Bounded checks

| Configuration | Captured scope |
|---|---|
| 720p internal/output, AA Off, 30 FPS setting | 600 consecutive opening-battle frames, including a similar rear view; 120 frames of screen depth, shadow atlas, shadow mask and final color. An additional 420 baseline frames covered target/command menus. |
| 1080p internal, 720p output, TAA option, 60 FPS setting | 600 idle frames and 600 further frames covering target selection, attack, enemy counter/Guard and return to the command menu; 30 frames of the same four raw surfaces. |

Contact sheets, suspicious adjacent-frame candidates and a short stable back/shoulder region were checked without confirming the reported shading toggles. Camera cuts and idle sword/pose changes account for prominent image changes in inspected candidates; they are not treated as shadow-flicker evidence. In one 29-frame 720p back-skin region, mean brightness ranged from 56.906 to 58.170/255 and maximum adjacent RGB mean absolute difference was 0.723/255. This only supports the absence of a large brightness jump in that short region and interval.

Across all 150 raw-resolve frames, sampled depth values were finite and the shadow atlas/mask retained content. Each raw final-color surface matched the next swap screenshot exactly in RGB, establishing the capture-frame correspondence. These checks do not exclude local defects elsewhere or flicker outside the sampled intervals.

The 1080p run produced actual 1920×1080 scene/depth and 1296×1296 shadow surfaces without an internal-allocation fallback. Its observed battle rate was mostly approximately 29–32 FPS despite the 60 FPS setting; this is not locked-60 or comparative performance validation. The TAA diagnostic quota was exhausted during startup, so battle history reuse was not verified frame by frame; the TAA-option path may include the existing SMAA fallback. The two known precompile failures (`ps78af7d75d932c582` and `vs291187f5ef8ba74a`) remain, with no new evidence connecting them to this report.

Tests used isolated profiles/saves and all owned game processes were stopped. Forced process termination is not natural-exit validation. Local captures, logs and analysis are retained outside public source control; raw private game data is not included in this note.

## Evidence needed to resume

Ask the reporter to confirm whether it still occurs with v0.4.0 on the RX 9060 XT and provide:

- A short video showing both the shaded and unshaded states, including whether it happens while idle or after an attack.
- GPU/driver details, settings and the complete runtime log from that same run.
- Separate render-state captures from the normal and abnormal states.

v0.4.0 implements independent English/Simplified Chinese Debug language switching and places Capture render state at the top. Those controls were implemented and validated in the [v0.4.0 development work](v0.4.0-development.md); this investigation does not add a new live Debug-language validation or a shadow fix.
