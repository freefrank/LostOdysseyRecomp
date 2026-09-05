# Fire-hit lighting investigation (2026-09-05)

Status: reproduced; not fixed. The explicit zero-LOD translation change does not eliminate the observed Flame Thrower lighting jumps.

## Reproduction and evidence

An independent background run in `out/shadow-lod-01` completed five opening soldier attacks and reached the Heavy Tank. Selecting Defend preserves the tank for repeated attacks. Magma Blast and Flame Thrower are distinct attacks; a Magma Blast capture alone does not cover the user's reference scene.

- Magma Blast: screenshots 25332–25931, HP 364 to 319.
- Flame Thrower: screenshots 28552–28851. Kaim alternates between bright orange and dark lighting while the flame continues. Frame 28702 includes draw/resolve capture.
- Repeated Flame Thrower: full captures 38858 (bright), 38870 (dark), 38882 (bright). Their final images and intermediate layers are retained locally. `flame-material-comparison.png` compares the scene resolves before and after particles.

The difference is already present in the material scene resolve: bright frame 38882's `seq123_9fa0000` versus dark frame 38870's `seq108_9fa0000`. Fire particles and the final bloom/composite are applied later. This narrows the investigation to scene lighting/material inputs and rendering state rather than only final exposure.

Frame 38858 includes an additional fixed-format scene resolve and attenuation pass. However, 38882 is also bright without that extra pair of resolves. Their presence alone does not explain the bright/dark alternation.

## Trace limitations and next comparison

The old `draw consts` log contains only the VS constant bank, primarily bone matrices for character draws. It cannot establish the PS light intensity. Also, detail/constants/textures are printed **before** their corresponding `draw f...` summary. A parser must attach pending detail to the following summary, not to the previous draw.

The trace now includes `draw psconsts`, containing nonzero raw float4 words from the actual uploaded PS bank, identified by frame, PS hash, and upload offset. Texture trace entries additionally include all six raw fetch words to inspect filtering, addressing, and LOD state. These additions run only on requested trace frames and do not change normal rendering.

Next: obtain the same bright/dark Flame Thrower pair using the extended trace, compare light constants and sampler state on matching character draws, then test the resulting rendering change. Do not reduce exposure or suppress the fire effect as a substitute for fixing its cause. Xenia's own fire glitch is not a correctness baseline.

Extended trace validation: runtime build passed (local fire-trace-build.log). Independent background capture f110 in fire-trace-check produced nonzero PS raw-word entries and six-word fetch records; its test process was stopped after verification. This validates capture output, not the fire fix.

## Extended capture run

The critical-section fix runtime completed repeated tank turns in the independent out/critical-section-runtime-01 copy. Frame 16833 captures Flame Thrower with two character draws (9108 indices, PS 75c956d8d57a2a9e) using c4 RGB approximately (7.0774, 1.8788, 0.4154) and (7.2909, 1.9354, 0.4279). Both have c4.w=2, c5=(1,1,1,1), and the same four texture fetch descriptors. This proves the strong dynamic-light parameters reach the uploaded PS bank; it does not yet explain the brightness jump. Frames 16866 and 16898 are also bright; 16931 is after the flame has ended and is not a valid same-effect dark comparison. Dense capture is needed during the effect.

Capturing every draw step and resolve stalls a frame for roughly 0.7-1.5 seconds. Request serials written faster than that may be skipped, so compare actual capture frame IDs and screenshots, not request timing. The first batch 10383-10389 was the tank warm-up, not the character-hit phase. Magma Blast frames 15310 onward are also a separate attack.

## Dense bright/dark pair

Actual renderer frames 22008 (bright), 22009 (dark), 22010 (dark, corresponding light draws absent), and 22011 (bright) were captured during one Flame Thrower. Compare each frame's final *_714000.ppm resolve; resolve-flame.png is the correctly aligned contact sheet. For the two 9108-index character light draws, c4 RGB changes from (7.134737,2.987688,0.692796)/(6.976783,2.921544,0.677459) at 22008 to (0.00354495,0.00126051,0.00040971)/(0.00364972,0.00129776,0.00042182) at 22009. All four fetch descriptors match. Only c4 and unused residual light copies c6/c13 differ in the PS bank. At 22010 there are no draws with this PS; the following frame restores strong light constants. This directs the next investigation upstream into guest dynamic-light generation/submission rather than a global exposure adjustment. It does not establish whether the guest light change is itself erroneous or another missing lighting contribution exposes an intended flicker.

Important alignment correction: renderer frame F corresponds to screenshot swap F+1. PM4_XE_SWAP increments the swap counter before presentation, whereas the renderer increments its frame after presentation. Direct final-resolve/screenshot comparison confirms this. The earlier screenshot contact sheets use swap IDs; do not attach same-numbered PS constants to them. Intermediate resolves and draw logs already share renderer numbering.

## Guest light update entry points

Read-only reflection inspection of the preserved background tank process identified LightComponent Brightness at +0x100 and LightColor at +0x104. SetLightProperties is native 824C4EC0; decompilation confirms these writes and calls 822D6810 when brightness/color/function changes. The latter marks component flags +0x50 with 0x20000000 and may notify its owner. This is a general component refresh path, not yet evidence that fire uses the script setter.

ParticleModuleTypeDataLight is an RPEngine class (live class 013D9300, vtable 820717E4). Four loaded module instances belong to packages xb_sys_046_00 and xb_sys_046_01, under Light_Blightness / Light. Package names alone do not establish which attack uses them. Live field dumps and object chains are retained in out/light-reflection.json and out/light-instances.json.

Its vtable +0x108 (82944608) evaluates the brightness distribution at module +0x54 through 822C7388, using emitter time +0x8C, and stores the result into particle payload +0x24. The +0x10C update (82944668) evaluates the same distribution using each particle's +0x0C age, unless flags +0x5C bit 0 is set. It uses emitter particle stride +0x78, active index list +0x3C, particle data +0x38, count +0x7C, and payload offset +0x104. The +0x14C factory (82944870) creates a 0x110-byte emitter through 8292B9D0. These addresses are decompiled from the original image, not inferred from generic UE3 documentation.

Next: inspect the distribution evaluator 822C7388 and emitter 8292B9D0/render submission, then correlate particle brightness with the captured PS c4 jump. Neither a bad distribution nor a broken instruction has been established. Local evidence: out/light-setter.txt, out/light-refresh.txt, out/particle-light-functions.txt. No game state or brightness was modified for this inspection.

## Distribution data and emitter submission follow-up

The four inspected module +0x54 distributions have header 00 01 01 01, four samples all exactly 3.0, zero lookup time scale/start, and a null distribution UObject at +0x18. 822C7388 therefore uses the cooked lookup evaluator 822C73E8; operation byte 1 takes the deterministic interpolation path, while operation bytes 2/3 select random paths. These particular brightness lookup tables do not encode flicker. This only covers these loaded modules; their association with Flame Thrower remains unproven.

Emitter constructor 8292B9D0 installs vtable 8206BF0C. Its +0x90 function 8292B3F0 maps active particles to light components via emitter +0xF4 indices and particle-system component +0x28C/+0x290 light array. It disables each light through +0x10C high bits, skips expired particles (age +0x0C >= 1), optionally multiplies the light brightness by an owner-derived +0x1B4 value, reenables flags, and calls 822D9088 with the transformed particle matrix. This provides a targeted future capture point for lifetime/owner scaling versus light intensity. Do not remove the multiplier before establishing its semantics and runtime values.

8292B768 separately builds particle render data, applying particle-system component +0x248..+0x254 and owner +0x170..+0x17C color/intensity multipliers; owner-derived +0x1B4 is also copied into the render data. Local decompilations: out/light-distribution-emitter.txt, out/light-raw-eval.txt, out/light-emitter-update.txt, out/light-emitter-submit.txt. A read-only scan for live 8206BF0C emitter instances in the preserved tank process at its current idle menu returned none in the inspected guest 0..128 MiB range. This is not evidence the attack lacks emitters; capture must occur during the effect. No gameplay values were changed.

## Live Flame Thrower particle sampling

The preserved background process was healed using ordinary Healing Medicine actions, then advanced through Blade Up, the following attack, Magma Blast, and Defend against Flame Thrower. The final menu screenshot shows HP 137; input serial 52, screenshot request serial 196. No raw HP or save edits were used. out/critical-section-runtime-01/flame-live-contact.png visually confirms the Flame Thrower attack.

A read-only sampler captured the actual Flame Thrower emitter 03F95640, component 0E105800, module 0E055200. Its asset chain is xb_esk_001_20 / pts_PntLight / ParticleModuleTypeDataLight_2. Its cooked brightness header is 00 02 02 01 with samples [7,8,7,8,7,8], selecting random interpolation between 7 and 8. This supersedes the earlier constant-3 module lead for this attack: those four other modules were not the observed live source.

During the roughly 4.46–8.91 seconds sampled after Defend, the emitter generally had one active particle. Active light brightness remained near 7–8 after initial allocation, while particle color/alpha changed with age. Examples: age .320 => color a3fe8b46, brightness 7.6401; age .943 => 1d3a2714, brightness 7.8373; next particle age .075 => 26bf6030, brightness 7.8353. The cycle repeats; inactive preallocated lights retain brightness 1 and must not be counted as active samples. Some reads catch flags zero between update stages, so an external memory snapshot is not a frame-atomic rendering capture.

This establishes an authored random brightness range and lifecycle color fading in the actual flame asset. It does not establish whether the previous c4 ~2000x drop is a lifecycle boundary, a missing overlapping light, or a rendering/submission error. Next compare particle color/age and resulting light submission at synchronized draw frames, especially particle replacement boundaries. Do not force constant brightness or remove fades as a presumed fix.

Evidence: light-sampling-flame.json (module data, 156 active-particle observations), light-sampling-magma.json (separate attack, five emitter addresses), and screenshot request 138–196 to swap mapping in run.log. The local sampler out/sample-light-emitters.py scans emitter allocations in guest 0..128 MiB but follows their actual component/module pointers above that range; the earlier broad UObject scan missed those higher module allocations. 8292AFF8/8292AC08 decompilation in out/light-tick.txt confirms payload brightness +0x24 copied to light +0x100 and particle color +0x60 converted via 82912B00 to light +0x104 before attachment.

## Low-overhead constant trace and capture timing

The original dense-capture swap timestamps were 22009=767.550 s, 22010=768.251 s, 22011=768.899 s, 22012=769.611 s. Thus the corresponding renderer 22008–22011 captures were separated by roughly 0.65–0.71 seconds, rather than ordinary 33 ms frames. These are still valid images/constants for those draws, but they do not prove the game changes that much between normal-speed frames. Given the observed particle lifecycle fading, temporal distortion from capture must be excluded before changing guest light behavior.

renderer.cpp now has an independent opt-in LO_PS_TRACE_REQUEST file: `serial frames shader_hex first_constant constant_count`. For example, `1 180 75c956d8d57a2a9e 4 2` records c4/c5 of the character light PS for 180 frames. It reads the actual uploaded PS constants without resource readback or explicit GPU waits, logs up to 64 matching draws per frame, and emits an end-of-frame count including zero draws. Requests are bounded to 600 frames and 16 constants; zero frames cancels. It does not activate full capture. Frame IDs remain renderer IDs (swap screenshot is +1).

Build passed in out/ps-trace-build.log, executable SHA256 BC8FF36AAA415857465365C1288F9DEAB784213C43EAD5946AF7F05BDBE178CF. Independent startup process out/ps-trace-title-01 (PID 20008) verified exactly 60 frames for a matching clear PS and six zero-draw frames for a nonexistent PS; median trace end interval 33 ms. Screenshot 932 confirms it reaches the title screen. This is trace plumbing validation, not Flame Thrower validation. Current input absent, shots serial 1, ps-trace serial 2 completed. It remains available for a new-game trace run.

A separate copied-camp run out/ps-trace-runtime-01 (PID 11504, now terminated by its crash) failed while loading at guest null read in 827B6278, caller 823B62A0, worker path 8248AE80. No PS trace request had been accepted. Preserve its run.log; the crash has not been attributed to the trace change or diagnosed. Do not label camp reload universally fixed on the earlier successful runs alone.
