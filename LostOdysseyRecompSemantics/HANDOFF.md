# Semantic recovery handoff

## Latest workspace checkpoint — 2026-10-09

Last verified published checkpoint: `f9c957e706afb584d6d2bbe90af66e017323a06c`.
Separate unresolved paired traversal WIP: `44ea9971`.
The current continuation adds seven selected-case validated units (23 cases):
`crt_random_thread61` (4), `transform_owner_build61` (3),
`geometry_quantized_unbounded61` (3), `geometry_tree_range61` (3),
`geometry_query_dispatch61` (4), `object_grid_probe61` (3), and
`grid_transform_pipeline61` (3). The full standalone library builds with the
existing Clang 19 configuration. These are genuine original-upper comparisons
with the shared lower boundaries described in each draft; they do not validate
all dependency routes. Direct TLS-record return restored the missing r12; the
pipeline callback required r28 initialization at the common entry. Both fixes
passed their existing targeted cases.

`geometry_paired_range61` (BD5910) remains partial: case 0 passes, case 1 agrees
on Full72, all vectors, RAM and callbacks, but recovered host CSR is 0x9fc1
versus original 0x9fc0. Case 2 is not reached. Instrumentation changes compiler
behavior, so its passing result is not acceptance. No status clearing was added.
Its source is buildable and linked by upper dispatchers; the validated upper
cases do not exercise this unresolved route. Next breakpoint is this precise
host invalid-flag difference, then the uncovered paired dispatcher route.
The body checker now accepts narrowly restricted named VMX save/restore helper
pins; genuine helpers are support-only and receive no mapping credit.

Baseline mapping stays **5,517/62,627 (8.809%)**; historical catalog membership
remains unavailable. Full semantic acceptance and new runtime replacements stay
zero. See per-unit drafts for untested branches, ownership/callback boundaries,
and host-status comparison limits. No new tests beyond focused cases, runtime
replacements or Rust port were introduced. Staged publication is authorized. The unresolved paired traversal is preserved
in a separate WIP commit; the seven passing units form the following checkpoint.

Additional validated export unit: `object_sort_export61` recovers B9DF18 and
B9DFA0 (4 actual-upper/shared-lower cases, library PASS). The output descriptor
borrows the destination and requires an exact encoded length; temporary writer
storage is released on either path. Cases use an empty-object payload. A bounded
search found no direct callers of these exports; further upward recovery needs
virtual-table/function-pointer call-site evidence. `grid_blob_routes61` additionally recovers B9DD90/BA60F8 with 2 connected
serialize/deserialize cases and library PASS. Blob descriptors are {count,pointer};
the output allocation transfers to the caller. Full scalar/vector/memory, callback,
machine and host CSR comparisons pass. Prior-grid deletion, failed allocation,
malformed blobs and alternate modes remain untested. Mapping credit and runtime
additions remain zero. Bounded direct-caller lookup found none for B9DD90 and one for BA60F8:
B9CBC0, now recovered in `grid_blob_forward61`. Its two dimension-2/4 tail-call
cases pass, including SP/LR and borrowed state forwarding, with library PASS.

## Active checkpoint — 2026-10-09, private inputs restored

The user authorized sustained dependency-ordered recovery, parallel workers and
staged commit/push to `trail/semantic-recovery`. The October 4 paused state below
is historical. Earlier October 9 stages: `7afb8031` and `7cc7a21e`, both pushed.

Private inputs were fetched into ignored `out/private-inputs`; the verified XEX
was copied to ignored `LostOdysseyRecompLib/private/disc1/default.xex`. Its digest
matches the supplied input. Generator gitlink `ddd128bc` plus the tracked project
patch was built with Clang 19.1.7; `ppc_codegen.py` produced the genuine sources.
Most cache fingerprint differences are CRLF. Config, build wrapper, codegen and
FP header differ materially from the private cache; the cache is not reused.
Selected complete bodies match the old pins exactly, with changed source lines:
BD0798 is now chunk181:2461 and BD2A28 is chunk181:7609. Current pins retain only
body SHA256 (UTF-8, LF, no final newline), source locations and instruction counts;
the checker supports these without publishing generated bodies or instruction dumps.

Validated this stage against genuine regenerated bodies: BD2A28 cleanup (3 cases),
BD10D8 append refactor (2), object lifecycle BB25D0/BAE1A0 (3), support
BD2A08/BD2C08/BD2C50 (3), buffer growth BD2870 (4). All pass selected Full72/RAM,
callback and actual host-CSR comparisons. Linux uses temporary copied fixture
headers under `/tmp/semantic-linux-oracle`, replacing only Windows guest-window
allocation with mmap/mprotect. It does not emulate PPC operations. Generated
fixtures stay in ignored `out/private-inputs`; executables/logs stay under `/tmp`.

The complete standalone CMake semantics library passed with Clang 19, single-job
build, `-Wall -Wextra -Werror -Wno-error=unused-function`. The warning exception is
for pre-existing unused formatter helpers. The first GCC build was blocked by
pre-existing Clang-only rotate builtins; no unrelated source repairs were made.
Library path: `/tmp/semantic-library-clang/libLostOdysseyRecompSemantics.a`.

Only the historically documented baseline member BD2A28 gains mapping credit:
**5,517/62,627 (8.809%)**. BB25D0 is external; the other newly validated lowers
stay in drafts with zero credit until the original cached `catalog.sqlite`
membership is available. Full semantic acceptance remains zero; runtime additions
remain zero. Finite ordinary-RAM checks do not establish Windows, nonfinite,
fault/MMIO, concurrency or gameplay behavior.

Validated lower stage was pushed as `92a49089` and remote-verified. BAF600 is
now recovered as `object_sort_reader61`: 559 original instructions become an
MSB-first bit reader, a 26-neighbor delta table, six explicit-coordinate escapes,
and output growth/append. Four actual-body cases pass Full72/RAM/callback/host-CSR
comparison (empty, all neighbor opcodes, explicit coordinate opcodes, mid-loop
growth), with incremental library PASS. Coordinate widths 6/7/8, malformed input
and trap paths remain untested. This new upper also stays zero-credit pending
fixed cached catalog membership; no runtime replacement.

BAF600 decoder stage was pushed as `74b26b83` and remote-verified. Two independent
grid units are now ready: `grid_storage_initialize61` (BB1E58 bounds/transforms,
allocation and cell fill; 3 genuine-body cases), and `grid_neighbor_update61`
(BB23C0 positive-corner occupancy tagging; 4 cases). Source review caught and
fixed a premature 32-bit product truncation in the neighbor helper; its signed
high-word arithmetic check, rerun oracle and incremental library passed. These
remain zero-credit without the historical catalog. Ordinary finite small-grid
checks do not cover extreme sizes, nonfinite inputs or alternate FP modes.

Grid initialization/tagging stage was pushed as `15535c9e` and remote-verified.
The upper chain is now also compared and built: BAE200 Morton-sort/delta encoder
(7 cases), BAFEC0 PMAP/color-group dispatcher (3), BB2098 grid reader plus
BD0A30 bounded seek (3 group/path + 2 seek cases). Encoder/dispatcher compare
original upper bodies against shared complete previously accepted lowers; they
are not fresh original-lower comparisons. Grid reader includes genuine selected
lower bodies; actual file I/O remains untested. Source review corrected encoder
post-store count reloads; actual comparisons corrected empty-input scratch/setup
state. All final comparisons and incremental library builds passed. These units
remain zero-credit pending the historical catalog; no runtime replacement.

Encoder/dispatcher/grid-reader stage was pushed as `84d90e1f` and remote-verified.
The next closed support cohort is built and compared: `grid_transform_routes61`
(BD7950 constructor over already mapped BD12D0; 1 case),
`grid_transform_support61` (six leaves: 822C5128/F2B308/BD1278/BD78C0/BD78D8/BD78E8;
9 cases), `geometry_support61` (BD43F8/BD3D80/BDDDF8/BD4438/BDDE18; 3 cases), and
`owned_tree_cleanup61` (BD9740 recursive reverse-array cleanup and BDAC88 composed
owner teardown; 6 cases). Original body pins, Full72/RAM/callback comparisons and
incremental full-library build passed. Review restored original floating-stage
order in the rounding helper; its nine cases were rerun. Borrowed objects,
tagged child ownership, release order and live callback state are explicit.
These remain zero-credit drafts pending historical catalog membership.

Support/owned-tree stage was pushed as `eb2a5eda` and remote-verified.
`object_grid_transform61` now implements BB06D8: three-dimensional grid traversal,
eight corner layouts, staged coordinate conversion and visit-record lifetime.
Four original-upper-plus-leaf cases and incremental library build pass. The first
comparison exposed inverted complement-bit tests; all sixteen tests and eight
high-bit writes were corrected and reviewed before the final four-case PASS.
The ordinary-object contract excludes overlap between borrowed grid object,
word buffer/metadata and the 608-byte guest frame. No arbitrary-alias, extreme-size,
nonfinite, alternate-rounding-mode or gameplay claim is made; no mapping credit.

The BB06D8 stage was pushed as `beafccd9` and remote-verified. Recovery continues.
`grid_transform_buffer61` adds BD17F0 storage predicate, BD1830 repeated vertex-
address counting and BDAC60 descriptor clear. Four genuine-body cases plus
incremental library build pass; wrapped address comparison is explicit. A separate
address inventory in the checkpoint now records zero-credit implementations
without adding them to the fixed historical denominator.

`manager_release_context61` closes the Full72 boundary for 823F3340, its true
82388B58 tail alias and BDB260 owner teardown. Four original-body cases and
incremental library build pass, including genuine 827C5F38 lazy initialization.
The initializer composes the accepted narrower implementation through a live
full-state bridge; its allocator/concrete constructors remain explicit mutable
guest calls. Their internals are not claimed recovered.

Manager-release/descriptor stage was pushed as `f1c99579` and remote-verified.
Five more closed units are now built and compared: `geometry_math61` BD4448 (4
finite ray/triangle cases), `geometry_triangle_range61` BD4C40 (4),
`diagnostic_lock61` 822B29A0/822B3438 (2 acquire/release scenarios),
`transform_owner_routes61` BD1558/BD1770/BD7A20 (3 cleanup cases plus 4 already
mapped constructors), and `owned_tree_storage61` BD9858 (3 callback partition
cases). Geometry compares original uppers with shared validated growth/copy lowers.
Lock comparisons include Full72 plus local MSR/reserved state, genuine CAS and
recursive mutex operations, including a real competing store and failed retry.
This matches the selected little-endian generated reservation model, not a claim
about PPC hardware exclusive monitors or arbitrary concurrency. Source review
confirmed MSR merging, raw reservation bits, CAS branches and live cleanup state.
No runtime replacement is enabled; no historical mapping credit is inferred.

Triangle/synchronization stage was pushed as `b6c7269f` and remote-verified.
Next built and compared units: `geometry_box61` BD5550/BD5B40 (4 cases),
`geometry_quantized_box61` BD56D8/BD5CB0 (4), `geometry_query_prepare61` BD5F28
(4), `transform_owner_initialize61` BD12F8 (3), `owned_tree_build61` BD9928
(4) and `diagnostic_format_routes61` BC8B78/B9C298/B9D328/BD18C0 (6).
Geometry and diagnostic routes compare original uppers using shared previously
validated complete lowers. Query preparation's return means traversal is resolved,
not necessarily that a hit exists. Quantized layouts preserve signed centers and
unsigned extents. Tree building preserves five partition strategies, arena vs heap
ownership and live allocation state; recursive upper will cover direct half split.
Diagnostic fixture initially omitted locale classification data; after seeding it,
all six cases passed. Existing xexdump produced ignored `private/image_disc1.bin`
from the verified XEX; only digest metadata is public. Above-160-byte formatter
growth remains unexercised. No baseline mapping or runtime replacement added.

Query/tree/diagnostic stage was pushed as `5f0d857e` and remote-verified.
`owned_tree_expand61` BDAA48 now passes two complete original recursive-chain
cases, additionally exercising BD9928's direct half split; `owned_tree_construct61`
BDAD18 passes three original-upper/shared-lower cases. Second allocation failure
is preserved without a new guard but unexercised. `geometry_unbounded_range61`
BD6E28 passes three Full72 + all-128-vector cases with RAM, callback and host-CSR
comparison. Local borrowed VectorState and mutable vector-aware guest service
preserve extra state without extending the common ABI. Review confirmed lane
shuffle/mask/endian behavior, CR6 aggregation and recursive parent/count updates.
The diagnostic route receipt now explicitly notes that its fixture does not
independently compare actual host CSR; no added test infrastructure was needed.

Current upper frontier is BB2638. Its only remaining direct semantic dependencies
are BD7A60 owner build and BB03B0 spatial query. BD7A60 now has its recursive tree lowers and is being composed. BB03B0 awaits BD7258 dispatch, the remaining VMX
walkers (BD5910/BD6C58/BD6FD0), and the BD2C48 random-number/TLS chain.
BD2C48 is a rand tail, not merely an error path. The vector walkers need local
borrowed 128-vector raw state and CR0/CR6/FP control; do not drop them into Full72
or silently replace unbounded SIMD routes with scalar ones. BD6E28 is the first
validated vector unit. Diagnostic MachineState remains caller-owned; the eventual
upper must share machine/vector state across guest calls. 822D3068 is an already
mapped empty leaf and adds no credit. Manager allocation/construction internals
remain explicit guest boundaries.
Implement and verify closed units before parents. BD2870 is implemented by
`reader_buffer_growth61`; small initialization/tail helpers by
`object_sort_support61`; object initialization/cleanup by `object_sort_lifecycle61`.
Integrate one complete upper at a time, perform narrowly selected original-body
comparisons and an incremental library build, then commit/push. Do not publish
private XEX, generated CPP/headers, compiled cache or credentials. Existing Windows
batch commands still require native Windows tools; Linux validation currently
uses a temporary fixture overlay rather than changing the shared runner platform.


Current checkpoint: **2026-10-04**, accepted recovery commit `f066d61a`, branch `trail/semantic-recovery`. The user requested stopping at about 2% remaining weekly quota, then documenting and delivering progress. Live usage reached 98%; the recovery goal is **paused**, all three `gpt-6.1-sol` workers stopped and the persistent runner exited. Documentation/draft delivery follows the accepted commit. Resume recovery only when the user resumes it.

Checkout: `C:/Users/freefrank/.codex/worktrees/semantic-recovery/LostOdysseyRecomp`. Delivery target: `origin/trail/semantic-recovery` on `https://github.com/freefrank/LostOdysseyRecomp.git`.

## Workspace continuation — 2026-10-09

Resumed from `958bcbe3` on `trail/semantic-recovery`; no main merge.
The user subsequently authorized staged commits and pushes to this branch.
`82BD2A28` now has a readable direct-Registers implementation, explicit borrowed
memory/payload-disposal contract, CMake source registration and a three-path oracle
harness. The batch now pins only `82BD0798` and `82BD2A28`; unrelated float-append
bodies and repeated prelude aliases were removed. Callback state remains live,
including reset-store r30/r31, FP bits/control and guest memory.

Local evidence: GCC C++20 `-Wall -Wextra -Werror` compilation of the cleanup and
accepted recursive-buffer source passed. A temporary Linux logic-only extraction
of the harness passed three finite cases (below threshold/nonempty, equal/empty,
equal/nonempty with mutable callback). This is **not a PPC oracle PASS**. The
Windows harness itself and the whole library have not been built here. Temporary
check files: `/tmp/run_sort_float_logic.py`, `/tmp/crt_reader_sort_float61_logic.cpp`
and `/tmp/crt_reader_sort_float61_logic` (outside the checkout).

`semantic_recovery.py check --manifest
LostOdysseyRecompSemantics/recovery_drafts/crt_reader_sort_float61_validation.json`
was blocked by the absent original `ppc_recomp.181.cpp`. Needed source locations:
`82BD0798` at 2508 and `82BD2A28` at 7818. Also missing are the Windows native
compiler/SDK and generated PPC oracle headers. Do not reconstruct an “original”
source file from the pin just to make validation pass. Run the narrowed batch
against genuine inputs and require library PASS before promoting this draft.

The first draft-completion stage was pushed as `7afb8031` and verified on the
remote. An adjacent `82BD10D8` readability refactor now also removes the local
Context/union/macros, exposes reader/node fields, and shares explicit mutable
Registers with accepted flush/growth lowers. Two finite no-growth/growth cases
passed against the pre-refactor implementation, comparing Full72/RAM/callbacks
and actual x64 host CSR; both implementations compiled with GCC C++20
`-Wall -Wextra -Werror`. This is a baseline comparison, not a new original-PPC
receipt. Temporary comparator: `/tmp/crt_reader_float61_comparison.cpp` and
`/tmp/make_float_comparison.py`. Its canonical manifest preserves historical
acceptance and separately records this refactor's narrower evidence.

No new accepted mapping or runtime replacement: **5,516/62,627 (8.808%)**, full
semantic acceptance still zero. Adjacent `82BAE200` / FP helpers / `82BAFEC0`
remain the next chain; their original bodies/catalog are unavailable here and
no sufficient committed draft was found. The older saved-work paragraph below
records the pre-continuation state; this section supersedes its missing-harness
and CMake claims.

## Accepted progress and connected chains

Generated from HEAD-tracked JSON by `python -B tools/ghidra/semantic_recovery.py progress --runtime-wrappers 3168`: **5,516/62,627 unique mapped addresses (8.808%)**, 82 individual records, 238 families, 5,436 family addresses and two individual/family overlaps. Delta: **+275** over the previous recorded handoff (5,241 at `5c6ad0d2`), including **+57** since recorded continuation checkpoint `ca50df36` (5,459). These are entry mappings against the fixed cached baseline; full semantic completion remains zero.

**New runtime replacements: 0.** The supplied historical runtime count remains 3,168 behind eight default-off gates. This continuation adds library implementations and bounded comparisons; it supplies no new runtime, scene or player acceptance evidence. Machine-readable counts and chain references: [recovery_progress_checkpoint.json](recovery_progress_checkpoint.json).

| Connected chain | Accepted path | Commit evidence |
| --- | --- | --- |
| stream I/O | refill → byte/block read → close/reopen | 64829170 |
| scanner | 82DF4AF8 full scanner → numeric/float conversion; public scan wrappers | 8e2b2220,153d4f73 |
| reader object | constructor → block read → append/flatten → sink → owned cleanup/reallocate | bbf4ade5,5e2a9b61 |
| temporary stream | path creation → open/duplicate → initialization → public cleanup | a51df207,5e2a9b61 |
| native flush | stream follow → full flush → mutable status → live r13 thread errno → cleanup | b3126827,153d4f73 |
| integer sort | 82BD2DF0 full 500-instruction bucket/count/scatter → two-buffer allocation → memset | 379d41c6 |
| narrow recursive formatter | 82B827C0 → 82B7D260 → 82B7D168 → 82B827C0, plus 82B83338 | 315b39c4 |
| CRT formatted output uppers | 82DF4270 → error text (82DF4218) and 82DF28C8 → full narrow recursive formatter; buffer/stdout callers → cleanup/unlock; 827C8648 format → console → fgets | f066d61a |

Each accepted family records its focused original-PPC receipt and validation limits in its canonical manifest. Latest evidence: `crt_narrow_formatter61` four cases; `crt_stream_output_upper61` three; `crt_format_frame61`, `crt_error_format_upper61` and `crt_narrow_callers61` three each; reader bucket sort four, float append two and error text three. Their Windows native `/W4 /WX`, `MT` library builds passed. A mixed batch failure does not invalidate an independently passed family with library PASS; retain its receipt and rerun only the failed family after repair.

## Saved work and next dependency gaps

`crt_reader_sort_float61` for `82BD2A28` (31 instructions) is **an incomplete, uncompiled draft with no mapping credit**. Five files are saved: header, source, batch, draft map and body pin. Its oracle harness is absent, it is excluded from CMake, and the three finite threshold/payload cases are planned only. Complete the harness, compare the original body with accepted `82BD0798` and the actual table+12 mutable callback, then require focused comparison and library PASS before promotion. The batch cannot run in its present state. Non-finite FP and other platform behavior remain outside that planned scope.

Next prioritize the object-sort dependency chain: `82BAE200` (queued 1,279-instruction engine), FP helpers and then `82BAFEC0` (315). The latter also blocks `82B9DF18` (33) and `82B9DFA0` (42). The bucket sort alone does not close this parent. `82BB2638` is 627 instructions with several missing lowers including `BB2098` and `BAFEC0`, true FP arithmetic and FPR26..31 saves; `82BB25D0` is external 25-instruction FP support, with zero baseline credit. `B9DD90` still needs `BB25D0/BB2638/B9C298`; `BA60F8` needs `BB25D0/BB2638/BAE1A0`; `BB2098` still needs `BAF600` and FP support. None is accepted by the present checkpoint.

Bounded caller searches found no new baseline direct read/flush/scanner caller in the selected CRT chunks; open/close searches identified the gaps above in chunk179. This is scoped queue evidence, not a whole-game rescan. The old cursor/frame queue below is historical and must be checked against current canonical mappings before reuse.

## Recovery practices retained

- Use a small pool explicitly set to `gpt-6.1-sol`; avoid fixed roles that select an older model. Use `followup_task` to start completed/idle agents. Workers own separate files and never build, promote or commit; root freezes inputs and integrates several ready groups in one CMake/build batch, with native build `--parallel 1`.
- Reuse `semantic_integer_body.py`, `pinned_call_prelude`, `ppc_integer_context.h`, `crt_context_adapter.h` and `crt_full_context_oracle_fixture.h`. Share accepted complete lower bodies instead of copying them. A persistent `semantic_recovery.py serve` session reuses the compiler environment; use `login:false` for PowerShell to avoid profile startup cost. Old tool session IDs are not reusable.
- Parameterized families retain each entry's true tail-versus-call LR, save slots, backchain and 64-bit arithmetic. Actual `lwz r1,0(r1)` can truncate SP high bits. Stream ABI stores SP in `state.sp`, with `state.r[1]=0`; record the actual field in callbacks.
- Keep live callback GPR/FPR/CR/CTR and host FP-control changes. Qualify colliding `Apply`/`Event` names, keep preprocessor directives on separate lines, and preserve explicit selected boundaries when generating original-call aliases.
- Use actual fixture tables and stream identities: descriptor `B81F78` uses `83378D80/D68`; pointer sweep `B7B950` uses `83378E90/E94`. Built-in stdout/stdin use lock17/lock16; stdout follows Unicode console imports. Wrong table/encoding/overflow seeds can select an unintended path even when original and recovered outputs match.
- Catalog addresses are uppercase eight-digit TEXT. Count only `catalog.functions` baseline entries; external/support wrappers have zero credit. Exact membership corrected `DF2AC8` to a baseline entry. Keep ordinary representative cases, adding targeted checks only for demonstrated failures; skip old suites, repeated hashes, random matrices and full-tree scans.
- Generate numbers once, append batch deltas and keep validation limits centralized. After all fixes, one selected output cohort took 3.411s (library 1.094s, oracle compile2.126s, execute0.022s) with reused compiler setup. This is a single measured cohort, not a controlled before/after speedup result; receipt: `C:/Users/freefrank/worktrees/LostOdysseyRecomp/semantic-crt-output-upper61-lock-fixed-tests/semantic-recovery-result.json`.

## Validation boundary and resume

Evidence covers selected original PPC bodies, ordinary RAM, saved frames and selected register/callback state. Full72 comparisons do not expand typed lower/native service contracts. Native internals, faults/MMIO, concurrency, exception/unwind, ARM FP, runtime and gameplay remain unverified by these batches. Float append uses selected finite Windows x64 inputs and restores actual host CSR; it does not establish a NaN matrix or cross-platform result. Private PPC/image inputs and external receipts remain local.

When authorized to resume, reuse the complete primary PPC at `C:/Users/freefrank/ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc` and catalog `out/decomp-index/catalog.sqlite` there. Actual bodies override incomplete `static_calls`. Start with the saved draft and dependency gaps above, then batch validation through `semantic_recovery.py serve --library-build C:/Users/freefrank/worktrees/LostOdysseyRecomp/semantic-runtime-build --msvc-runtime MT`. Keep receipts outside the checkout/ownCloud and promotion dependent on both family PASS and library PASS.

## Historical checkpoint before this continuation

The following text is preserved as historical evidence; its current-state numbers and resume queue are superseded by the checkpoint above.

Checkout: `C:\Users\freefrank\.codex\worktrees\semantic-recovery\LostOdysseyRecomp`, branch `trail/semantic-recovery`. Last accepted recovery commit: `5c6ad0d2`; the handoff/draft commit follows it. The primary complete PPC bodies are under `C:\Users\freefrank\ownCloud\Git\LostOdysseyRecomp\LostOdysseyRecompLib\ppc`. The catalog is `C:\Users\freefrank\ownCloud\Git\LostOdysseyRecomp\out\decomp-index\catalog.sqlite`; cached `static_calls` data is incomplete, so use complete PPC bodies as the authority.

The committed recovery state is 82 individual records, 106 family manifests, and 5,241 unique mapped addresses after the two recorded overlaps (`829664E8` and `82B84D88`). This is 8.369% of the fixed cached 62,627-address baseline, not a whole-game completion rate. Runtime remains 3,168 wrappers behind eight default-off gates; complete individual recovery remains zero.

The latest seven commits added 29 tracked entries: stream/exception (+9), legacy-class (+2), object ranges (+4), frame unlock (+7), an extension-only object-range update (+2), frame-vtable (+3), and pending-record cleanup (+2). Their strict Windows native shared `MT` library builds and bounded comparisons passed. Each family manifest records its own focused receipt and scope; do not rerun old suites or expand case matrices.

The pending-record cleanup family covers `82373158` and `82290640` with idle handling, 32-bit store overflow/full-64-bit sum behavior, target self-aliasing, high-SP/full-LR frame restoration, ordinary RAM and selected GPR/CR/XER checks. Synchronization callback placement is represented. Host/PPC `lwsync` ordering, concurrency, faults, MMIO, native synchronization implementation, runtime and scene behavior remain unverified. Its receipt is `C:\Users\freefrank\worktrees\LostOdysseyRecomp\semantic-pending-record-cleanup-tests\pending-record-cleanup-result.json`.

`legacy_character_cursor` is a draft only and is not in CMake or recovery progress. Its draft pin is `recovery_drafts/legacy_character_cursor.json` for `822969A0` (`ppc0:15750`, 87 instructions, 10 labels). Planned cases are empty, ordinary, space/tab, quoted, escape and limit. Before promotion, preserve the unresolved scratch/CR6 behavior, high-64 GPR versus low-32 guest-address behavior, `r31` spill, and quoted-limit comparison. Its proposed files are `src/legacy_character_cursor.cpp`, `include/lo_semantics/legacy_character_cursor.h`, and `tests/legacy_character_cursor_oracle.cpp`. Audit the exact pin, use an empty batch prelude and one source, then run the smallest strict build and focused oracle. Promote only after the receipt passes by renaming the manifest to a `*_families.json` file; draft filenames must not use that suffix.

The next unimplemented candidate is `822C3CFC` (`ppc2:25468`, 10 instructions), sharing the caller-frame-unlock template with frame size `128`, member offset `80`, and return `822C3D14`. It has no recovery credit until implemented and compared.

Use Windows native tools only. Workers do not build, run tests, commit or push; the root build uses `--parallel 1` with the configured `MT` library at `C:\Users\freefrank\worktrees\LostOdysseyRecomp\semantic-runtime-build`. Receipts and build outputs stay outside the checkout and ownCloud. Use `semantic_recovery.py progress --runtime-wrappers 3168`, `check --manifest PATH`, and the bounded `run --batch ... --output ... --library-build ... --msvc-runtime MT` workflow. Preserve the existing source identity, runtime gates and open ABI/native/fault/MMIO/concurrency limits. Do not claim full semantic recovery or gameplay acceptance from library builds and bounded receipts.

The user requested stopping this session and opening a fresh one. Reuse a small pool of two or three workers through follow-up tasks; do not create a new agent per family. This session exhausted the total agent-thread limit despite free active slots; two new-agent attempts were rejected. Keep file ownership independent, integrate centrally, and report each commit's delta, cumulative `/62,627`, mapping percent and runtime-wrapper count.

Keep only necessary manifest implementation/validation notes. No routine README/CHANGELOG/checkpoint synchronization, hashes, whole-project rescans, old-suite repeats, random matrices or unknown-entry checks. Usually 2–10 focused cases per new cohort suffice; reuse accepted lower implementations and fix only demonstrated failures. Oracle cohort macros belong in the batch `prelude`; the runner does not consume a `defines` field.

Resume from this checkout with `python -B tools/ghidra/semantic_recovery.py progress --runtime-wrappers 3168`, then inspect the cursor draft and run `check --manifest LostOdysseyRecompSemantics/recovery_drafts/legacy_character_cursor.json`. Add its source to CMake and create its batch only when resuming implementation. The draft has never been compiled or run. Private PPC/image inputs, build outputs and external receipts remain local and are not published by this handoff.

Compiler environment startup is expensive. If needed, reuse the local persistent runner `C:\Users\freefrank\worktrees\LostOdysseyRecomp\semantic-crt-float-closed-tests\root_native_session.py`, which holds that environment only in RAM. Its old process was cleanly exited; tool session IDs cannot be carried into the new chat. Delivery target is the existing upstream `origin/trail/semantic-recovery` (`https://github.com/freefrank/LostOdysseyRecomp.git`).
