# Late-pass jitter follow-up — 2026-09-25

This note records the diagnostic blind-spot repair and the remaining review boundary for late-pass jitter candidates. The e810 trial is implemented in a narrow guarded path; its guarded build checks pass, the mapping was independently confirmed applied and bound, and the bounded user visual run is accepted.

## Diagnostic implementation

`jitter_candidates.py` now scans the full frame by default instead of stopping at the first resolve. Its existing `matching_depth_draws` field remains the strict depth-pair result; the new `matching_geometry_depth_draws` field carries cross-pass matches and their `pass_differences`. `export_jitter_fixture.py` keeps its existing explicit boundary and fixture format; the default triage change does not expand fixture authorization.

F1 render-state capture now records, for each successfully recorded draw, the actual jitter and uploaded-VP state together with texture slot 0 (`tex0`) binding kind, guest/host/parent extents, resolve age/gap and producer state. `guest_fetch_address` uses the mip fallback when the base fetch address is empty and is a guest fetch address distinct from any host allocation address. A recorded command is evidence that the capture writer observed the command, not proof that the GPU completed it. `producer_state` can remain `unknown` when the capture lacks enough evidence.

## Candidate boundary

The remaining 40 clip-XY candidates, after excluding `e810cfacc107fd3c` and the two mixed-camera cases, have no existing capture across the relevant 11/33 frames or schema-4 binding record. They cannot be added as a batch from the current evidence.

The e810 trial is limited to `DrawPositionVPSlot` exact `e810` plus `fe31`, and requires `constantScreenSample` before assigning slot 7; no VS-wide mapping is added. Renderer eligibility requires a valid 2D format 6 texture, nonzero base, guest extent 1×1, repeat UV, and `FindResolved` to find no same-address hit in any frame. The VS must not read tex0. The other 10 PS paths, any resolve, border mode and non-1×1 cases stay disabled. This is the implemented bounded trial path.

## Validation and runtime status

The diagnostic implementation passed 19 Python checks. The SDK-on runtime and `LoTemporalJitterTest` guarded build log exited 0. `--captured-f6131-e810` passed 66,252 checks across draw 746/depth 178, 32 phases and 1440p/4K, with old separation 0.487756 px. The prepared acceptance entry is `out/validation/issue64/manual-late-light1/Test.cmd` from `b2f1999+dirty`. In the bounded visual run, capture directory `out/validation/issue64/manual-late-light1/on/captures/render-17903683939323682-f2432` contains complete frames 2432–2434; the partial ZIP marker does not indicate a missing directory. The accepted EXE SHA-256 is `35A08E1568AAAB08070E54A2B55F141442732D58F43CB5FB629CDF9465ED8CCB`. Three DLSS Quality frames recorded `vendor_success`, `adopted`, `checked_submit`, `gpu_completed` and `motion_state=3`; preview ROI RGB range was at most 0.0293/255 across the three frames and seq05 was at most 0.03593/255. Independent capture review confirmed e810+fe31 applied on every frame at slot 7, phase 1/2/3, with texture 0 `GuestUpload`, 1×1 guest/host/parent extent, address `0x17000`, format 6 and no resolve; its geometry and uploaded VP matched depth draw 193. The existing 2078+4013 path was also applied on each frame across 15 slot-8 draws with same-frame resolved tex0. These checks establish the bounded scene mapping and visual acceptance, not whole-game coverage. The other 40 candidates remain held; broader runtime coverage and release remain open.
