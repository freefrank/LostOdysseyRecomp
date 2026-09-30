# Menu / shader prebuild audit — 2026-09-16 (America/Edmonton)

## Scope and evidence

Reviewed runtime baseline: `2054bb8ebac7ae338953ca8ddbcf741aa643c106` on `menu`,
14 commits / 45 changed files ahead of `38550e3ffb7dd518b5ac78324b82932a8990c79a`.
The source snapshot `55538a5ad1e23379447788cca358ce5ef0f9d17d` adds only the initial
game-data-free audit workflow. Production changes below were developed against
that snapshot. This is a repository-wide, risk-focused static review plus CPU
regression testing, **not** a claim that every generated instruction or every
third-party dependency has been exhaustively verified.

Review covered startup/backend selection, shader discovery/translation/cache
publication, queue cancellation, GPU resource/geometry/cache boundaries, the new
menu and host UI, input quarantine, guest pause/time/audio interactions, settings
and display transactions, installer/import trust boundaries, and shutdown/error
paths. Generated PPC code, game assets and third-party implementations were not
re-audited. Full runtime linking, real DXC/driver execution and gameplay remain
separate validation requirements.

Issue [#22](https://github.com/freefrank/LostOdysseyRecomp/issues/22) reports both
an initial Windows backend-initialization failure and later long shader loading,
high memory use and a crash. The reporter uses a 16 TB external HDD, with the OS
on SSD. The external MEGA log archive was not obtained. HDD latency can amplify
small-file I/O and paging, but **the crash has not been reproduced or attributed
to I/O**. These fixes address independently identified defects and avoid treating
that hypothesis as a confirmed root cause. The issue should remain open pending
reporter/hardware validation.

## Fixed correctness and reliability findings

| Priority | Finding in the reviewed baseline | Repair / evidence |
|---|---|---|
| P1 | Bundle loading passed the bundle's own identity back as the expected identity. Current compiler/translator/backend/discovery compatibility could therefore be bypassed. | Derive `RuntimeIdentity` from the current typed cache identity, discovery rules, game-root spelling and already-loaded XEX. Reject mismatches before installing a record. Synthetic and actual-function tests change each contract component. |
| P1 | Consumer cancellation / exception cleanup changed a condition-variable predicate outside its mutex. Atomicity of the flag does not prevent a lost wakeup and a stuck worker join. | Set cancellation under the same mutex as the wait, including the RAII unwinding path. Preserve partial/all thread-start fallback; exercise cancellation and exceptions repeatedly with a full queue. |
| P1 | Exceptions creating a prebuilt device shader could escape the per-module failure path, be logged by the outer queue catch and allow initialization to continue. Worker allocation failures could be downgraded to ordinary shader failures. | Stop consumption, cancel/join workers, discard incomplete publication and set `initializationModuleFailure`, allowing existing backend selection to roll back. Actual production-function fixture injects device exceptions and `bad_alloc`. |
| P2 | The latest single-pass loader no longer invoked its event-pump callback. Its external stream buffer was configured after open and declared after the stream. | Keep single-pass loading, configure a 256 KiB buffer before opening and make it outlive the stream. Restore bounded-cadence progress/event pumping. Retain rollback for late footer/read/consumer failures. |
| P2 | Explicit full-scan/retry bypass also prevented replacing the startup bundle, so a later normal launch could resurrect stale state or a cached compiler failure. | Separate reuse from publication. Successful explicit scans/retries can replace the bundle atomically. Actual-function test retries a deterministic failure then checks the next normal launch. |
| P2 | Linux preparation-screen additions referenced Plume objects even with `LO_GPU_PLUME` disabled. | Guard the GPU helper and presentation branch. A fixture compiles the actual progress prefix without any GPU declarations; normal title updates still work. |
| P2 | Default prebuild retained HLSL for every installed shader and scaled concurrent DXC working sets with logical CPU count. | Retain metadata/binary device objects, not all reconstructed HLSL. Cap workers at four (serial override retained). Reconstruct HLSL only for shaders requested by an explicit later capture. |

The queue stress tests exercise the repaired race; they do not establish a
reproducible historical #22 stack trace. The module test validates propagation
into the existing initialization-failure flag, not a physical driver's recovery.

## I/O and memory policy

Normal launch trusts the installed game data to remain unchanged. The importer
currently checks executable identity/compatibility and required asset presence;
it does **not** cryptographically verify every byte of every FPD archive. Trust
is an explicit runtime policy, not an assertion of stronger import validation.

### Cold start / cache rebuild

* Extracted XEX/resource microcode and generated fixed/linked variants go through
  `SourceStore` in memory. No default prebuild `source/*.bin` exports, immediate
  read-backs or `resources.manifest` read/write/validation are needed.
* The store is bounded to 128 MiB of raw code and 100,000 sources; these are store
  limits, **not** a bound on total process or driver memory. Workers see an
  immutable job set. Offset/size/framing checks and identities of bytes already
  read are retained.
* Unique indexed resource name/size layouts skip four scattered integrity
  probes. Ambiguous layouts still need identification. CPX sparse extraction
  retains its small FPI/layout binding and reads only needed blocks. Indexed
  decoding no longer allocates the entire decoded package when only a few
  independently decoded blocks contain shaders. Unknown layouts still fall
  back to scanning; their required reads have not been removed.
* Legacy / gameplay-learned source files are enumerated once on a rebuild.
  Already-imported keys are skipped before opening. Unknown generated cache
  files are checked before use. This preserves learned coverage and existing
  installations; normal bundle hits skip that directory entirely.
* Fixed variants are generated once; linked generation consumes the same store
  and excludes the already-generated fixed set.
* Per-shader compiled binary and deterministic-failure checkpoints remain. They
  permit interrupted preparation to resume without recompiling successful
  shaders. Removing them would trade a one-time write reduction for worse crash
  recovery. A failed checkpoint write no longer prevents an independently
  successful aggregate bundle from being published.

### Warm start

Read one sequential startup bundle with bounded per-record staging. Derive the
expected identity without traversing game/source/cache directories. Do not
extract sources, translate, call DXC, reopen individual binary checkpoints or
reconstruct all HLSL strings. Record and completion checksums, length checks,
backend/compiler compatibility and transactional rollback still protect locally
generated caches against interruption/corruption. They are not an extra pass
over game data. Event pumping happens between records, not during a blocked
filesystem or driver call.

The bundle schema is now **2**; filenames are unchanged. Schema-1 bundles rebuild
once. The per-shader binary cache version/identity is unchanged, so matching
completed checkpoints are reused. Replacing game resources in place while
keeping the same root/XEX deliberately does not trigger an automatic inventory
scan: use `LO_SHADER_FULL_SCAN=1` for that launch. A successful forced scan writes
an updated bundle. `LO_SHADER_RETRY_FAILURES=1` retries deterministic failures;
`LO_SHADER_HLSL_DIR` and `LO_SHADER_DUMP_DIR` remain explicit diagnostic controls.
`LO_SHADER_PREPARE_SERIAL=1` remains available for a memory/concurrency control.
No debug dumps, collection consent or ordinary postmortem logging were silently
enabled by this change.

## Regression evidence and reproduction

No copyrighted game inputs are required for these commands:

```sh
# Historical audit commands; native_audit.py has since been removed.
python tools/tests/native_audit.py --cxx clang++ --sanitize --out out/audit/native
python tools/tests/menu_boundary_regression.py --cxx clang++ --sanitize --out out/audit/menu
python tools/tests/shader_prebuild_regression.py --cxx clang++ --sanitize --out out/audit/prebuild
# Independent optimized compiler pass:
python tools/tests/native_audit.py --cxx g++ --out out/audit/gcc
python tools/tests/shader_prebuild_regression.py --cxx g++ --out out/audit/prebuild-gcc
```

Use fresh output directories for a repeated run. Native fixtures report each
PASS/FAIL/SKIP and preserve build/run logs and `results.json`. Missing optional
Plume/unordered-dense headers are explicitly skipped; polygon-offset and bloom
GPU execution, plus real DXC compilation, are not relabeled as CPU passes.

Local Clang ASan/UBSan coverage includes 45 standalone CPU fixtures, five menu /
input / display / headless groups, and the production prebuild/GetShader
integration fixture. The latter compiles verbatim production method bodies but
uses explicit fake translation, DXC, SDL/window and GPU services. It covers cold
startup, warm startup with the game directory removed, compiler invalidation,
checkpoint recovery, forced scan/retry replacement, late capture and fatal
module/allocation errors. This is stronger than testing a duplicate policy
implementation, but weaker than compiling/linking and running the entire game.

The I/O fixture uses a synthetic 1 MiB resource containing a 24-byte shader:
trusted indexed extraction reads **24 bytes**, versus **16,408 bytes** with the
legacy probes, with **zero intermediate source writes**. It deliberately uses
an unusable cache subtree to detect accidental source exports. These are fixture
counts, **not** a measured HDD startup-speed improvement or a game-wide ratio.
A negative-control compile of the original unguarded Linux progress prefix fails
without GPU declarations; the repaired production prefix passes. Queue tests
cover full/partial launcher failure, bounded coverage, consumer and
worker exceptions, and saturated cancellation. Existing CPX malformed-input,
block-crossing, resource fallback, cache corruption/publication, geometry,
texture, pacing, temporal collection and no-allocation fixtures were retained.

Test infrastructure repairs are intentional: the old startup-cache assertion
assumed two validation passes although production had moved to transactional
single-pass loading; menu extraction boundaries matched a newly duplicated
forward declaration; the backend selection fixture assumed Windows on Linux;
several malloc-backed allocation counters lacked matching sized deletes under
sanitizers. A nonblocking `try_lock` fixture now permits documented transient
contention and retries within a bound instead of treating it as data loss.
`temporal_collection.h` also includes its own required `<cstdint>`.

## Remaining risks / hardware acceptance

1. No ROG Ally, Steam Deck, Windows driver, physical external-HDD or 15 W / 720p60
   run was performed. No frame-time/RSS/startup-duration claim is made. A full
   executable build additionally needs the pinned third-party and private
   generated game build inputs.
2. Startup still creates all known device shader modules and later prepares
   learned pipelines. Driver-side memory/compilation can dominate after host
   HLSL and I/O reductions. Four DXC workers is a conservative default, not an
   empirically optimal device-specific count.
3. The existing large-buffer vertex cache uses sampled content for larger
   buffers; a mutation outside sampled regions can require a stronger guest
   write-generation scheme. This was not changed without a scene reproducer.
4. Timestamp/audio threads have detached lifetimes in the reviewed code. Normal
   UI exit paths commonly use immediate process termination; a future graceful
   teardown needs explicit stop/join ordering before guest memory/audio globals
   are destroyed. No speculative shutdown rewrite was included here.
5. After this patch, obtain #22's exact build/driver/backend error and crash log,
   then compare a cold rebuild, warm launch and interrupted/restarted rebuild on
   the same scene/settings. Record peak private bytes/commit, GPU allocation,
   read/write counts and elapsed stage timings. Compare HDD and SSD without
   simultaneously changing renderer/backend settings. Keep #22 unresolved until
   the reporter's failure is actually reproduced or cleared.
