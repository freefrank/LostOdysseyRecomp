# Recompiler width and control-flow audit

## v0.4.2 published — 2026-09-08

Published v0.4.2 includes the nine corrections and the tracked dependency patch described here, with 3,258 passing generated-code checks and the original 109 word-switch checks. The full Council/save-reload result belongs to semantics-r2, built with frozen v0.4.1 runtime objects; it does not establish final v0.4.2 package behavior. CI, package identity/integrity and anonymous download verification pass; no application tests were repeated. See [STATUS.md](../STATUS.md) for that release evidence and the [Council record](issue7-cutscene-crash.md) for scenario limits.

The dated entries below preserve each candidate checkpoint and its validation and publication state.

Current Issue state checked on 2026-09-08: [Issue #7](https://github.com/freefrank/LostOdysseyRecomp/issues/7) is closed after the repair/release follow-up, with original-reporter retesting still unconfirmed. The generated-code and Council/save-reload results above retain their exact candidate scope; closure is not additional gameplay validation. See the [current Council record](issue7-cutscene-crash.md).

## 2026-09-10 generation guard follow-up

The repository now routes PPC generation through `python -B tools/ppc_codegen.py generate`. `tools/build_tools.bat` records a receipt for the built generator, and the wrapper checks the generator binary/source hashes, the TOML and recompiler inputs, generated output hashes and the absence of obsolete `ctx.r*.u64` jump-table switches. `python -B tools/ppc_codegen.py check` performs the manifest check without regenerating. The synthetic guard fixture has seven `unittest` cases covering matching and drifted manifests, stale context, obsolete switches, a stale tool receipt and failed-generation invalidation; it uses only a temporary synthetic tree.

The current generator source identity is `recompiler.cpp` SHA-256 `2a12d39169b09177b65d245c5ac54367ab4c4fc50880a183992c75ad0dc08842`, with context `ppc_context.h` SHA-256 `08d47f20df8d4b827cac35adee4665f39ee2803136ca487d362e7e03f6331385`. The resulting semantics remain the previously verified **3,258** instruction checks and **109** word-switch checks, with **843** recognized tables and **44,523** independent selector evaluations. These numbers reuse the earlier semantic evidence; they are not a new gameplay or package validation result. The old ignored generated tree initially had no `codegen-manifest.json`, and the new guard rejected it as intended. After the output-preservation fix, fresh generation and `check` completed successfully: the regenerated tree contains **247 C++ files**, with **843** low-word (`u32`) switch sites and **0** `u64` switch sites; hashes for **246** `ppc_recomp` instruction-comment streams are unchanged from the prior generated tree. The seven synthetic guard tests passed. No gameplay, package or reporter acceptance is claimed.

## 2026-09-07 implementation and regression

Nine confirmed translation defects from the scan below are now corrected in the production [recompiler and context template](../../tools/patches/XenonRecomp-lostodyssey.patch), at `tools/XenonRecomp/XenonRecomp/recompiler.cpp` and `tools/XenonRecomp/XenonUtils/ppc_context.h`. Their instruction-level regression and the semantics-r2 Council/save-reload regression pass. These local changes are uncommitted and unpublished; original-reporter acceptance and broader gameplay coverage remain unverified.

| Correction | Implemented behavior |
|---|---|
| Update-form loads/stores | Seven forms calculate an unsigned 64-bit effective address, use its low word for memory and write the full address to RA. Stores preserve the old source value when it aliases RA. |
| `RLWIMI` | Rotate the repeated source word at 64-bit width before applying the full mask, preserving selected high bits and source/destination aliases. |
| `SRAW` / `SRAD` | Calculate CA from the effective shift count and discarded source bits, including oversized shifts and register aliases. |
| Record bits | Eighteen formerly missing Rc forms update CR0 using the existing signed 32-bit comparison and XER.SO contract. |
| Atomic addresses | Four forms wrap the complete RA+RB sum to 32 bits before adding the host memory base. |
| Absolute addresses | All eight base load/store macros narrow the guest address before host pointer addition, including negative absolute displacements. |
| `BLRL` | Save and align the old LR target, write the return LR when enabled, call and continue, then invalidate the cached FP-mode assumption. |
| CTR indirect branches | Ordinary `bctr`, `bctrl` and `bnectr` clear the target's low two bits at the branch boundary. |
| `BDNZF` | Select the CR field and LT/GT/EQ/SO bit from BI while retaining full-width CTR decrement and the low-word zero test. |

Fresh builds of the real decoder and generator produce native code that passes **3,258 / 3,258** checks. The frozen pre-fix source fails **1,533** matching cases and passes 1,725, exposing every repaired category; six previously correct Rc forms pass all 48 controls in both variants. The fixtures cover defined register results, flags, aliases, all 1,024 `RLWIMI` mask pairs, all 32 `BDNZF` BI positions, guest memory values and safe indirect-call observations. The original word-switch regression still passes 109 checks; its old-selector control passes 20 ordinary cases before the expected instrumented trap. See [test instructions](../../tools/tests/README.md#recompiler-semantics-regression).

Generated-output comparison finds 99,219 changed instruction emissions, all in the expected opcode set, with no guest instruction/PC stream drift. All 843 low-word switches and 10,589 recognized table-data slots are preserved. The context-header change requires rebuilding every generated unit: the isolated `v0.4.1-issue7-semantics-r2` candidate rebuilds all 247 generated C++ units and links the new guest library with 45 frozen diagnostic-r2 runtime objects and nine unchanged project libraries. The independent input and ABI audit passes; this is not a full rebuild of the current shared runtime source. Its EXE SHA256 prefix is `87e6eb6a3ea0`; it excludes the separate TAA and background-capture work.

That exact executable completes the full Council sequence without a cutscene skip, restores Main Street movement, writes native slot 07 and independently reloads it with normal movement. The run uses Asia Disc 1, English, FXAA, 1280x720, 30 FPS and copied caches. Save-anywhere is used only to open the native save menu after the scene; the fresh reload uses neither it nor teleport. The harness stops both owned processes after verification, so natural shutdown and cold-cache performance remain untested. See the [runtime and package evidence](issue7-cutscene-crash.md#2026-09-07-follow-up-semantics-implementation); earlier switch-r1 results retain their separate scope.

These fixtures do not establish natural gameplay reachability for BLRL, unaligned targets or latent forms. Their indirect lookup uses a safe observer; it does not test the production host function-table allocation. `skip_lr=true` cases start with a valid LR and verify suppression of the return-LR write, not general LR tracking when an earlier `mtlr` was omitted. The actual game configuration uses `skip_lr=false`. Unconfirmed host signed-overflow candidates, general LR/return lowering, switch-default `__builtin_unreachable()` contracts and runtime table mutation, `vpkd3d128` type 3, remaining undecoded/empty output and broader SIMD/FP behavior are outside this repair.

Frozen source SHA256 prefixes are `2a12d39169b0` for `recompiler.cpp` and `08d47f20df8d` for `ppc_context.h`. The tracked dependency patch reconstructs their Git-normalized contents from the pinned submodule HEAD while preserving earlier changes. Local evidence is retained under `out/issue7-semantics-fix/`: `implementation/{IMPLEMENTATION.md,semantics-only.patch,generated-delta-summary.json,patch-sync.json}`, `tests/{REPORT.md,run-02/summary.json}`, `switch-regression/results.json`, `build-review/REPORT.md`, `runtime-validation.json` and `delivery/delivery-audit.json`. Private game images, generated game code and captures are not published with this note.

## Earlier 2026-09-07 diagnostic follow-up

The following preserves the pre-fix investigation and its counterexamples. Repair-pending statements describe that scan; the implementation and current validation boundaries are recorded above.

The [Issue #7 word-switch repair](issue7-cutscene-crash.md) prompted an offline scan for similar translation errors. The scan confirmed additional emitter defects using production-generated instruction fixtures and retained code fragments. Their gameplay impact is unverified. This investigation did not change production code, build or run the game, commit, push or publish; it adds no player acceptance. The earlier Council-scene repair and independent save-reload results retain their recorded scope.

The inventory uses the frozen switch-r1 generated tree, `out/issue7-switch-fix/ppc-fixed`, covering 246 translation units and 62,857 function implementations. Its 3,807,154 instruction slots include embedded data decoded as instructions. Removing 10,589 slots inside the 843 recognized switch tables leaves 3,796,565 slots, which still do not establish reachability or exclude other embedded data. Syntactic occurrence counts below are not counts of observed gameplay failures.

The instruction contracts come from IBM's [PowerPC User ISA 2.02, Book I](https://powerpc.dev/general/PPC_Vers202_Book1_public.pdf#page=25) (mirrored copy), including effective addresses, scalar results/flags and branch behavior, together with IBM's [BCCTR/BCC reference](https://www.ibm.com/docs/en/aix/7.3.0?topic=set-bcctr-bcc-branch-conditional-count-register-instruction) for indirect target alignment. The local reports tie those contracts to specific generated outputs and counterexamples.

## Confirmed defects with current code occurrences

| Area | Reproduced semantic difference | Current coverage and limit |
|---|---|---|
| Update-form loads/stores | Seven forms write only `RA.u32` after computing a 32-bit effective address, losing changes to the high word. For example, `0xDEADBEEFFFFFFFF0 + 0x1020` should update RA to `0xDEADBEF000001010`, but leaves `0xDEADBEEF00001010`; the low-word memory access is correct. | 51,087 occurrences across `lbzu`, `ldu`, `lwzu`, `stbu`, `stdu`, `stwu` and `stwux`. No actual game input crossing that boundary and consuming the wrong high word has been established. |
| `RLWIMI` wrapping mask | A 32-bit rotate loses the upper bits selected by a wrapping 64-bit mask. `rlwimi r3,r4,5,24,7` with old `r3=0x1122334455667788`, `r4=0x12345678` produces `0x0000000046667702` instead of `0x468ACF0246667702`. | 564 wrapping-mask occurrences outside recognized table data. A later low-word store alone does not establish visible impact. |
| `SRAW` / `SRAD` carry | For a source containing only its sign bit, an effective shift of 32–63 / 64–127 produces the correct all-ones result but clears CA instead of setting it. | 415 variable arithmetic-shift occurrences outside recognized table data. Trigger values and subsequent CA consumption remain unverified in gameplay. |
| `BLRL` | The instruction at `0x82B84DCC` emits `__builtin_debugtrap()` instead of calling the old LR target and setting the return LR. The extracted generated fragment traps with zero target calls and unchanged LR. | Original bytes, preceding `mtlr r3`, function mapping and direct callers establish a real code sequence. This scan did not execute its game path or associate it with a player symptom. |
| Indirect target alignment | Ordinary CTR dispatch passes the target without clearing its low two bits. A safe lookup observer confirms incorrect table offsets for low bits 1, 2 and 3; aligned and dirty-high-word controls agree. | 40,724 ordinary CTR dispatch outputs. No game-produced unaligned target was found, and the fixture did not dereference an invalid host pointer. |

The relevant production paths are `XenonRecomp/recompiler.cpp` and `XenonUtils/ppc_context.h` in the patched dependency; their changes are retained in the [dependency patch](../../tools/patches/XenonRecomp-lostodyssey.patch). The first four findings require instruction-specific corrections and meaningful regression controls before any gameplay fix can be claimed; this scan applies none.

A bounded local-use screen found no direct CA consumer after the 415 shift sites, or specified explicit high-word consumer after the 564 wrapping-mask sites, before an overwrite or basic-block boundary. It stops at labels and branches/calls and does not track register copies or flow across blocks. This narrows follow-up work without proving either defect harmless.

## Latent defects and excluded candidates

- **Atomic RA≠0 addressing:** generated pointer addition does not first wrap the guest register sum to 32 bits. Four generated atomic forms access the wrong test page. All 1,030 current atomic occurrences use RA=0, so the defective double-register form is absent from this game inventory.
- **Negative absolute address:** an RA=0 `lwz` with displacement `-4` reads `base-4` instead of `base+0xFFFFFFFC` in a generated fixture. All 89 apparent game occurrences were confirmed to be switch-table data decoded linearly; there are zero such candidates outside the recognized tables. This is a demonstrated template defect, not 89 game bugs.
- **Missing record-bit updates:** 18 tested Rc instruction variants leave CR0 unchanged despite correct defined result bits. All 18 variants have zero occurrences in the current inventory.
- **BDNZF condition selection:** the generic emitter hardcodes EQ, but all five current raw instruction encodings request EQ. This is a broader generator limitation, excluded as an independent current-game hit.
- Full-width CTR decrement followed by a low-word zero test is correct for the examined mode. Defined multiply/compare controls also agree; undefined result bits and division cases were not assigned invented expected values. Host signed-overflow concerns remain candidates without a demonstrated wrong optimized result.
- The other explicit debug trap is a `vpkd3d128` type-3 packing path. Its vector semantics and reachability, remaining undecoded/empty slots, and full SIMD/FP behavior are outside this audit's confirmed width findings.

## Switch checks and validation boundaries

An independent interpreter reads the original image and evaluates each known table's dispatch sequence. All 843 tables match their metadata: 14,841 legal cases, each with selector high words `0`, `1` and `0xFFFFFFFF`, give **44,523 matching evaluations**. No second concrete table error was found. This checks the recorded dispatch sequences, not all control-flow entry constraints, runtime table mutation or complete ISA behavior; `__builtin_unreachable()` defaults remain a contract risk rather than a newly reproduced table defect.

| Offline check | Result |
|---|---|
| Address fixtures, `address/run-03` | 31 cases: 12 expected semantic mismatches and 19 agreeing controls. |
| Scalar fixtures | 160 vectors: 32 semantic mismatches and 128 agreeing controls. The mismatches cover CA, CR0 and wrapping-mask high bits. |
| Exact control-flow fragments | Six cases: four expected mismatches and two agreeing controls. Traps are intercepted and the final indirect lookup uses a safe observer. |

Successful diagnostic execution means the expected defect was reproduced; it does not mean these semantic comparisons all passed. Address/scalar fixtures use the production decoder and generator. Control fixtures execute exact retained generated fragments, while the 44,523 switch evaluations use an independent small interpreter. None is a game run or complete compatibility proof.

The consolidated local report is `out/issue7-similar-scan/REPORT.md`, with detailed reports under `{address,control,scalar,inventory}/`. Key records include `scalar/{summary.json,observations.json,steps.json,sha256.json}`, `control/{control-evidence.json,fragment-fixture-results.json}` and `inventory/{instruction-summary.json,sink-summary.json,table-exclusion-summary.json,local-consumer-screen.json}`. They retain source identities, commands, raw instruction addresses and coverage limits without committing private game images or captures.
