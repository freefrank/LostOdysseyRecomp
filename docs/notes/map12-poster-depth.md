# Map12 poster black-patch repair — 2026-09-05

Local artifact: `out/poster-fixed.exe`, SHA256 `AC3B3A73DCC04A87BB01B8A17120869D878B216B811E283719C6581B3976EFF8`. Installed into the main build directory after verifying no running game; user clarified they were not playing. No game launch or save changes. Window pump and ground-shadow fixes remain included.

## Evidence

- Map12 user02 save copied into independent test directories. Each test process terminates in finally; no concurrent research games.
- `out/poster-light-pass`: VS 87a76ceaf1eaec11, draw871 adds the foreground poster with a hard black patch. Base pass uses b7557072899a63a1, same 288 indices and vertex buffer.
- `out/poster-depth-bypass`: disabling only this VS's depth test removes the patch. This diagnostic is not enabled in the repair.
- `out/poster-geometry/geometry`: GPU vertex replay of base0047 and light0242 places the same foreground poster at x401–466/y426–520. z/w is 0.011666–0.012216. Maximum clip-z difference is 6.1035e-5, post-divide difference about 6.9e-8 (74 D32 ULPs). Base/light shaders accumulate translation in different orders.
- `out/poster-bias-trace`: the original lighting rasterizer bias is +24 D32 ULPs. At this shallow reversed depth it is smaller than the measured transform difference. Disabling all bias does not fix it.

## Change and boundaries

Retain the selected guest absolute polygon offset in PolygonOffset. For float24 color draws that only read depth and whose PS does not export depth, apply that constant through the existing vertex depth offset, keeping rasterizer slope bias and ordinary depth/stencil comparisons. Other depth writers retain the previous path. `LO_LEGACY_LAYER_BIAS=1` restores the old conversion for same-binary A/B. No shader hash or material-specific override, depth-test bypass, shader-cache reset, or shadow disabling is used.

This is a targeted correction to the D32 approximation, not full float24/MSAA emulation. Bias applied before clipping is not a complete emulation of post-rasterization polygon offset near clip planes; broader map/near-plane regression remains open.

Reference for the previous ULP conversion: Xenia `draw_util.h` (local reference `out/shadow-reference/draw_util.h`, lines171–225). Xenia documents its worst-case [0.5,1) approximation there. Its shader arithmetic notes also caution against fused MAD: https://github.com/xenia-project/xenia/blob/master/src/xenia/gpu/ucode.h . No speculative MAD change was made.

## Validation

Final AC3B3A73 binary, same save/config, separately started runs:

| Path | Frames | Frames with >20 dark pixels | Maximum dark pixels |
|---|---|---|---|
| Legacy offset | 824–1123 (300) | 169 | 374 |
| Corrected offset | 839–1138 (300) | 0 | 0 |

Metric: ROI (405,425)-(465,505); per-frame mean-RGB drop >30 from that run's temporal median. Scene/camera match, not identical animation phases. Contact sheets were visually inspected. Evidence: `out/poster-final-legacy/sequence.png`, `out/poster-final/sequence.png`, `out/poster-ab-results.json`. Earlier candidate300-frame run also showed no patches in the sampled contact sheet.

`LoPolygonOffsetTest` passes existing16 cases plus shallow-depth regression: old24-ULP path rejects the layer; absolute offset accepts it; a nearer occluder still rejects it. Log `out/test-poster-results.log`. Runtime/object build and final link pass; git diff --check passes with existing CRLF notices.

Map13 character shadow flicker remains deferred per user preference; no claim this repair closes it. User clarified they were not playing; the earlier interpretation of their message was incorrect. System reboot earlier recorded bugcheck0xD1; this investigation did not identify the responsible driver.
