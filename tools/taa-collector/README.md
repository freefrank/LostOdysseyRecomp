# Optional TAA collection

Production endpoint: `https://lo.dotslash.pro/v1/taa`. Worker `lost-odyssey-taa-collector`; D1 `lost-odyssey-taa-collection`. No public database access or embedded client credential. Runtime source and deployment checks found no committed or client-embedded credentials; server bindings remain platform-managed. See the bilingual [privacy statement](../../PRIVACY.md) · [隐私说明](../../PRIVACY.zh-CN.md).

The historical position-evidence client build is `0.5.0-position-evidence-1`. It sends schema 2 on the same endpoint; schema 1 canonicalization and hashes remain unchanged, and the Worker accepts both formats without a D1 migration. Its earlier verified deployment was `b2c7cd15-dd98-4520-abc8-fa3e9e8b8ab0`; the current deployed Worker version is `4db7e156-8a21-4f57-8f0d-bacd0c4576ab`. Schema 2 adds nested conservative `position` evidence and independent temporal guards; it does not relax jitter classification or enable a mapping automatically.

The protocol implementation identifiers `0.5.0-shader-sources-1`, `0.5.0-position-evidence-1` and `0.5.0-temporal-1` remain unchanged. They identify wire and diagnostic contracts; they are retained independently of the current source and release version.

### Schema 3: TAA binding evidence

The development client adds a separate schema 3 record for a suspected TAA consumer binding. It keeps the existing VS/PS hashes and classification fields, and adds the consumer slot/phase, guest and uploaded VP bit patterns, viewport and jitter values, selected PS `c0`, and texture binding/format/extent/resolve fields. The texture section also carries producer state, draw count, producer frame age, resolve frame age and resolve gap, with the producer's bounded slot/phase/VP/viewport/jitter snapshot. These fields are diagnostic evidence for comparing producer and consumer state; they do not authorize a jitter mapping or prove a visual cause.

The client uses an independent fixed 64-entry POD queue, submits batches of up to eight records, and reuses the existing opt-in 60-second background/F1 upload trigger. The producer path performs no GPU readback, new wait, pointer or address capture. The CPU queue and producer fixtures passed 82 and 32 checks with zero producer allocations. The updated 9,761-byte fixture passed three real C++ → Worker → SQLite uploads with HTTP 200 responses; replay preserved eight deduplicated rows and the increased `max_draws`, with 1,048 record leaf values and 640 IEEE bit words preserved. The 0.5.2 development executable was built successfully with SHA-256 `c3711463fd4d21f17b68af27cecd2df35851af49e9567e956596b48a7ae1e259` and size 82,904,064 bytes. Evidence is retained under `out/v0.5.2/taa-binding-collection/`.

The deployed Worker version is `4db7e156-8a21-4f57-8f0d-bacd0c4576ab`; its health response advertises schemas 1, 2 and 3. It retains only the D1 and rate-limit bindings, has persistent logs disabled, and required no D1 schema migration. Current follow-up is limited to paired VS/PS and texture-0 producer/resolve evidence for the `e810cfacc107fd3c` path. There is no new game run, F1 menu acceptance or visual acceptance for that path. Do not treat a schema 3 row, `guards=31`, a matrix match or a resolve gap by itself as proof that jitter should be changed.

In schema 3, `draws` and D1 `max_draws` count samples of the same recorded state (at most one sample per target PS per frame, with at most two target samples), not all draws in a frame or a total draw count. `producerDraws` is a conservative CPU-side writer count. If it exceeds 65,535, the value is reported as `0` with `Unknown` producer state. `Uniform` means that the CPU-recorded writer matrix and jitter values were consistent; it does not prove GPU completion, per-pixel use or recursive texture dependency. The detailed serializer and artifact verification is retained in `out/v0.5.2/taa-binding-collection/REPORT.md`.

### Compact automatic diagnostics (unreleased)

The development compact stream is a separate opt-in diagnostic contract. It samples one fixed 32-frame CPU window at most every 180 seconds, with capacity for 24 VS/PS pairs and eight binding records. Each window reports final CPU history booleans and rejection values, coverage reason counters, pending counts and per-stream delivery results. It can join canonical schema 2/3 observations to stage and renderer-byte-FNV shader references, but it does not upload color previews or raw F1 ZIP contents and it creates no random, session, player or device identifier. Sparse GPU data remains D3D12-only, is not linked to this window, and does not claim GPU completion.

The current contract is implemented in `collection_diagnostics.h` and `collection-diagnostics.js`, with a strict maximum request size of 32 KiB and a compact receipt acknowledgement for the new stream. Existing summary, source, binding and temporal streams remain accepted through their existing HTTP 200 paths; the compact stream requires the strict receipt. F1 force flush remains optional and is not required for compact collection. Worker tests passed 9/9 in `out/v0.5.2/compact-diagnostics/worker-tests.json`. The C++ fixture passed 68 checks with zero allocations; its corrected 18,905-byte request passed two loopback HTTP 200 uploads into the actual SQLite schema, preserving one deduplicated row and all window fields. The 0.5.2 client build completed with SHA-256 `59f039ea84404c1ab85095a95a10b32d435bf1d39b1ca610b38d15edb44ce62e` and size 82,946,560 bytes. Eight new ledger checks and one archive check passed; the installed wrapper's current archive sync remained unchanged across 18 cases with identical source hashes and modification times. The private archive schema 4 allowlist is published in commit `728d6030980a7be39feca493319f54bfb1a62e74`, and Worker deployment `14e74c54-1213-4240-9877-047f35cdda75` advertises schemas 1 through 4 with temporal and shader-source capabilities enabled. The client release build and player acceptance remain separate release gates.

Completeness means 32 final CPU frame snapshots, not all draws or bindings and not GPU completion. Counters describe observation calls within the window; delivery counts are cumulative since the consent/device reset. Existing streams use HTTP 200 acceptance, while compact delivery requires the validated compact receipt. The wire carries source version and protocol build information; an unknown `runtimeCommit` does not identify the exact executable.

### Private D1 research archive

The private repository [LostOdysseyRecomp-build-inputs](https://github.com/freefrank/LostOdysseyRecomp-build-inputs) receives a daily GitHub Action snapshot using content-addressed deduplication. The first successful run is [34493751731](https://github.com/freefrank/LostOdysseyRecomp-build-inputs/actions/runs/34493751731); it recorded 3,401 diagnostics, 431 unique VS/PS programs, 909 GPU associations, 31 temporal records and 462 validated payloads. The archive is a long-term research copy and does not inherit D1's 30-day inactive-record expiry.

The archive also maintains an offline feedback triage ledger at `feedback/triage/ledger.json`. The private archive is [LostOdysseyRecomp-build-inputs](https://github.com/freefrank/LostOdysseyRecomp-build-inputs); its local triage documentation is at `feedback/triage/README.md`. The ledger keeps review, implementation, validation and player acceptance independent and preserves historical reviews separately from current evidence. It is a local review record for an archive checkpoint; it does not replace live D1 reads or imply a shader fix.

Client consent is stored separately in `taa-collection.ini`: absent/invalid=-1 undecided,0 declined,1 enabled. Fresh setup asks once after saving; existing players are asked when opening game Settings. Settings > Language > TAA shader collection allows changes without restarting. No is selected by default. Enabling requires confirmation; disabling clears queued automatic-upload samples immediately. A request already sent may finish. Opt-out persists independently of graphics previews. All five UI languages are provided. The current published path sends bounded summaries and sparse temporal data. The existing F1 package is manually triggered; after capture, a background upload of pending VS/PS and structured data is attempted only when automatic collection is already enabled, and F1 does not change consent. It does not add a player or device identity.

### v0.5.2 F1 microcode extension

The v0.5.2 extension adds original VS/PS microcode captured by the program to the existing manually triggered F1 local package. If automatic collection is enabled, the completed capture wakes a background D1 attempt for pending VS/PS and structured records; it does not upload the F1 ZIP or runtime log and does not change consent. Each schema-1 request has exactly 6 top-level fields, 1–32 program records, at most 64 KiB per program and a maximum 256 KiB body. A successful HTTP 200 acknowledgement covers the submitted batch; acknowledged sources are not resent by the process, while remaining pending data waits for the next three-minute cycle or retries after failure. The intended server identity is one record per content stage and SHA-256 content identity, with GPU model association; it is not a user or physical-device database. The extension does not add usernames, email, hostnames, paths, serial numbers, disk serial numbers, MAC addresses, device UUIDs, installation IDs, cookies or tracking IDs to the automatic payload or D1 records. It does not turn the microcode into project source or upload user files. D1 records inactive for 30 days are deleted. Release CI built new v0.5.2 executables; functional validation is reused from the retained source-0.5.0 development binary.

Manual F1 packages are local artifacts and may contain more diagnostic data than the automatic upload, including runtime log text or local paths; inspect them before sharing. Automatic upload remains bounded by the consent switch and server schema. Automatic collection uses preallocated bounded memory, skips recording when a try-lock is busy or storage is full, performs no I/O, allocation or wait on the render hot path, and uses independent upload state; stopping or exiting does not join a network request. These bounds do not promise zero CPU cost. The Worker reads `CF-Connecting-IP` only for the platform's temporary rate limiter, not for D1 storage or user tracking. Persistent Workers Logs, invocation logs and traces are disabled for this collection Worker; Cloudflare still processes source IP and normal HTTPS request metadata for transport and security.

While TAA is active, collect actual draw paths (including cached shaders), GPU/driver, renderer-byte-FNV64 VS/PS hashes, raster dimensions, known VP slot, exact camera-match candidate mask, viewport/jitter/depth flags and rejection code. Candidate mask bits0–5 mean slots0,4,7,8,230,233. Flags:1 viewport compatible;2 jitter enabled;4 jitter applied;8 depth write;16 same depth allocation as camera anchor. Candidate matches are leads, not verified position dataflow. The schema 1/2 summary payload sends no matrix values, local paths, raw shader source/bytecode, saves or images; original microcode is handled separately by the source endpoint described above. Map IDs and motion vectors are not collected in schema1.

Render-thread aggregation uses a try-lock, bounded2048 feature entries and saturating counters. Batches of up to32 unsent features are uploaded off-thread via WinHTTP HTTPS with no redirects and bounded timeouts: urgent shader anomalies every10 seconds, routine summaries every60 seconds. One accepted observation per feature per process; failures retry on the next interval, with no disk spool. Exit or disabling discards pending samples; recording may drop when busy or full. Payloads are small JSON, not raw shader logs.

The Worker validates a strict schema and caps bodies at64KiB. A native rate-limit binding allows10 requests/minute per source IP at the serving location (not a global quota). For schemas 1–3, D1's SHA256 primary key covers canonical diagnostic content excluding the sample count; matching submissions and retries update the same row. Schema 4 hashes the complete frozen window, including its counters and nested sample counts; identical retries reuse that window row. `max_draws` is the largest submitted observation count, not total draws, user count or prevalence. Raw IP is not stored in D1; Cloudflare still processes request metadata. Inactive records expire after30 days, with daily cleanup. No R2 object storage is needed for these structured records.

Use pinned Wrangler4.130.0: `npm ci`, `npm test`, `npx wrangler deploy`. Schema deployment: `npx wrangler d1 execute lost-odyssey-taa-collection --remote --file schema.sql`. Query/export only with authenticated Wrangler; never publish a write token in the game. To rank unknown candidates:

```sql
SELECT json_extract(diagnostic,'$.vs') AS shader, COUNT(*) AS distinct_features,
       MAX(max_draws) AS largest_observation
FROM observations
WHERE json_extract(diagnostic,'$.slot')=-1 AND json_extract(diagnostic,'$.candidates')>0
GROUP BY shader ORDER BY largest_observation DESC;
```

Initial schema-1 validation: three focused Worker tests passed; HTTPS health returned200; three concurrent identical synthetic submissions each returned accepted1 and D1 contained exactly one row. Synthetic row removed. Windows runtime linked successfully. These checks did not establish in-game UI or visual acceptance.

Position-evidence follow-up validation is recorded in the focused report (`out/v0.5.0/performance-fix/position-evidence-0.5.0/REPORT.md`, retained locally): 27 native C++ rule/guard checks, retained capture-corpus classification, Worker protocol checks, accepted generated schema 2 JSON, and an incremental source-0.5.0 Windows build passed. At that original build-validation checkpoint, the candidate binary had not been run in-game, so this does not transfer the earlier Ghost Town visual acceptance to this candidate.

## Schema 2 position evidence

Schema 1 retains its original fields and canonical content IDs. Schema 2 requires all original fields plus the following fields in every record; unknown fields and unsupported versions are rejected. The schema number, position evidence and guards participate in the content ID, while `draws` remains excluded. Different schemas do not merge into one row.

| Field | Contract |
| --- | --- |
| `position.version` | Analyzer version, currently `1`. |
| `position.kind` | `0` unproven; `1` supported four-row linear position matrix; `2` direct vertex position. |
| `position.slot` | Matrix start `0..252` for kind 1; `-1` otherwise. This is separate from the existing static classification `slot`. |
| `position.issues` | Bit mask: relative constants `1`, control flow `2`, unsupported expression `4`, ambiguous output `8`, matrix-dependent input `16`, resource limit `32`. |
| `position.outputs` | 16-bit mask of interpolators that may depend on the position matrix; this does not establish actual pixel-shader use or safe jitter mutation. |
| `guards` | Independent check results: anchor present `1`, exact position-camera matrix match `2`, compatible full raster viewport `4`, existing depth predicate `8`, finite position matrix `16`. Failure bits stay clear. |

The client schedules supported translated-HLSL position analysis for the independent worker when a valid VS is first encountered, including warm cached shaders. Until a worker result is ready, the position is unproven; the producer only copies bounded raw inputs and does not translate, analyze, log, allocate dynamically or join a worker in `TryGet`. The worker captures independent CPU state and returns a guarded result. The former `position-evidence`/`coverage-candidate` producer logs and dedicated hot-path state were removed; unknown-shader counts and rendering behavior are unchanged. Complex or ambiguous dataflow remains unproven. Neither a matrix match nor `guards=31` automatically grants jitter eligibility.

The background-analysis fixture passed 17 checks covering blocked producer/destructor progress, raw-copy behavior, zero producer allocations, unproven/ready results, stop gating and detached-state lifetime; static review confirmed no translation, analysis, logging, dynamic allocation or join in `TryGet`. Existing source, summary and protocol suites were reused. See `out/position-evidence-collection-fixture/run-01.log` and `static-review.log`.

Pre-feedback verification: seven Worker tests passed. The actual C++ serializer produced an accepted 343-byte request. Production schema-1 and schema-2 POSTs both returned200; a schema-2 retry updated `max_draws` from44 to54 in the same D1 row. Both exact fixture rows were deleted and an empty readback confirmed cleanup. An authenticated production snapshot read at **2026-09-10 02:11 UTC** subsequently contained 768 schema-2 records, including 29 strong candidate state records across 11 currently unclassified VS hashes, plus 14 temporal groups covering 448 renderer frames. These are runtime receipt and bounded offline-consistency evidence; they do not establish a renderer fix, visual quality, player acceptance or identity with a particular executable. See the CF feedback review (`out/cf-feedback-review-20260910/REPORT.md`, retained locally) and its temporal analysis (`out/cf-feedback-review-20260910/temporal-review.md`, retained locally).

### Background runtime acceptance — 2026-09-10 UTC

The source-0.5.0 executable (`SHA-256 1beb8a50c5bef1ebf0fb147338b33d558a2193e87f82b0b8c579ecbcb856959d`) completed one hidden, muted D3D12 run on an NVIDIA GeForce RTX 5080 at 1280×720 experimental TAA and 60 FPS target. Background input loaded Map 16, Main Street. The first periodic upload added 23 programs and the second added 32, for 55 total (15 VS / 40 PS, 19,188 bytes). All received bytes matched the local source cache by byte content, SHA-256, renderer FNV and length; the snapshot had 55 source rows, 55 associations without duplicate keys, and 28 matching structured diagnostics. Detailed receipts are in the runtime report (`out/v0.5.0/shader-source-collection/runtime/REPORT.md`, retained locally) and D1 verification (`out/v0.5.0/shader-source-collection/runtime/D1-VERIFICATION.md`, retained locally).

Map 16 completed-present windows measured 59.67 FPS / p95 18.036 ms before the second upload and 59.84 FPS / p95 18.067 ms during it. Each window had one frame above 50 ms and none above 100 ms. This is continued rendering during the observed upload, without an opt-out comparison or proof of universally zero overhead. The process ended with `Stop-Process`; normal UI shutdown was not tested. The attempted `LO_CAPTURE_REQUEST` was a legacy trace request, not the product F1 menu export, so F1 ZIP/manifest and immediate post-capture upload remain pending under the selected background-only acceptance. Saves, test copy/profile and executable identity were unchanged.

To prioritize the stronger schema-2 candidates, retain the VS/PS and state fields for review:

```sql
SELECT diagnostic, max_draws FROM observations
WHERE json_extract(diagnostic,'$.schema')=2
  AND json_extract(diagnostic,'$.slot')=-1
  AND (json_extract(diagnostic,'$.flags') & 3)=3
  AND (json_extract(diagnostic,'$.flags') & 4)=0
  AND json_extract(diagnostic,'$.position.kind')=1
  AND json_extract(diagnostic,'$.position.issues')=0
  AND json_extract(diagnostic,'$.guards')=31
ORDER BY max_draws DESC;
```


## Sparse temporal collection (0.5.0)

`POST /v1/temporal` accepts `application/octet-stream` with `X-LO-Build`, `X-LO-Backend`, `X-LO-GPU`, and `X-LO-Driver` headers. No Content-Encoding. Maximum wire body is 256 KiB; decoded data must be exactly 154,384 bytes. The existing shader-summary JSON endpoint remains unchanged. Historical local Wrangler deployment version: `2191fd5e-42ef-426c-8ac3-9ad3c1ae28c7`.

The same optional consent enables both collections. Existing internal tester consent remains valid; the first-run/settings explanation now includes sparse depth, camera motion, jitter and camera matrices. No color images are included. This feature only runs when the supported TAA scene/depth path produces frames; it cannot collect from unsupported scenes or create object/skinned motion vectors. GPU point sampling produces a 32×18 R32_FLOAT depth target; the existing renderer fence completes its 256-byte-row-pitch readback. No extra GPU submission/wait or full-resolution readback is added. Initial shader/pipeline creation is lazy, once per renderer instance. GPU resources are reused only after the existing fence completes.

The CPU upload worker reconstructs camera-only MV from depth and unjittered camera matrices. MV is previous minus current in unjittered raster pixels (+X right, +Y down), stored as RG16F; invalid samples use half NaNs. Depth preserves raw R32F host reversed nonlinear values, including invalid values. Sampling uses `floor((grid + 0.5) * extent / gridExtent) + 0.5`, with current jitter removed once before reprojection. The previous jitter is metadata, not included in the unjittered MV. These sparse values cannot validate full-screen object motion or replace DLSS inputs.

Each group contains 32 consecutive renderer frames with constant dimensions/epoch, frame IDs, monotonic timestamps, flags, both VP matrices and raster conventions, four jitter values and 576 MV/depth samples per frame. One group may be queued, with a five-minute minimum interval after collection; failures retry on the existing 60-second upload tick. Opt-out clears pending CPU samples; consent generation rejects GPU samples recorded before revocation. No spool files; exiting discards incomplete/pending samples. A started HTTP request can finish after opt-out.

Binary contract is defined by `gpu/temporal_collection.h` and decoded by `temporal.js`. `LOZ1` uses bytewise XOR against the previous 4,824-byte frame followed by zero/literal runs (high bit = zero run; lower seven bits + 1 = length). The first 16 bytes remain undifferenced. `LOR1` is the raw fallback. Both wrap `LOMV`, schema 1, count 32 and stride 4824. Maximum client packet is 154,388 bytes. Compression is lossless relative to the RG16F/R32F packet, not relative to float64 camera reconstruction. Exact decoded sequence plus normalized hardware/build metadata is SHA-256 keyed; identical retries/encodings deduplicate. Different frames/timestamps or users' camera trajectories can yield separate records. Shader coverage still deduplicates by shader/state, independently.

`temporal_sequences` stores compressed BLOB, metadata and decoded summary with a 30-day inactive expiry. No public read endpoint. Apply `temporal-schema.sql` with the existing D1 binding before deploying. Inspect authenticated reception:

```powershell
npx wrangler d1 execute lost-odyssey-taa-collection --remote --command "SELECT id,metadata,summary,first_seen,last_seen,length(payload) AS bytes FROM temporal_sequences ORDER BY last_seen DESC LIMIT 5" --json
```

Checks completed: C++ camera-translation/half encoding/packet fixture, byte-identical Worker decode, malformed/oversized input rejection, encoding-independent content ID; shader compilation to DXIL and SPIR-V; Windows client build. A synthetic 154,384-byte sequence compressed to 5,617 bytes; this is not a gameplay compression estimate. Three production POSTs stored one row, then that synthetic row was deleted. A real opted-in run subsequently delivered one complete 32-frame group: 150.8 KiB before compression and 104.5 KiB on the wire, with 17,856/18,432 valid camera-MV samples and no invalid depth samples. This confirms the client-to-Worker path only; it does not validate visual quality or object/skinned motion.

`npm test` runs existing JSON endpoint checks. To run `npm run test:temporal`, first compile/run `tools/tests/temporal_collection_test.cpp` from the repository root with C++20 or later; its generated fixtures are written under `out/v0.5.0/performance-fix/`. No game or GPU runtime is launched by these checks.


## Shader anomaly priority (current 0.5.0 candidate)

Shader diagnostics take precedence over MV/jitter archival data. The upload worker checks every 10 seconds: compatible, enabled, unjittered unknown shaders with proven position evidence, no issues and all independent guards matching are first; unknown shaders with camera-match candidates follow; known shaders rejected by jitter guards follow those. Equal-priority entries retain discovery order, not hash order. Normal summaries remain on a 60-second cadence. Each request contains at most 32 summaries. Shader requests are attempted before temporal packets; a shader upload failure or remaining urgent summaries defers temporal upload.

Low-priority records admit at most four exact width/height variants per VS/PS/slot/candidates/flags/rejection/position/guards family. Their total admission is capped at 1024 of the 2048 entries, reserving capacity for anomalies; a more urgent entry can evict a less urgent one when full. Omitted variants are not merged into the first resolution and do not inflate its draw counter. Counts remain capped per exact recorded state. Queues remain memory-only and disappear on process exit; this change does not upload shader bytecode or HLSL.

The earlier scheduling build identified shader reports as `0.5.0-shader-priority-1`; the current build uses `0.5.0-position-evidence-1`. Temporal binary contract/build identifier is unchanged. Four capture-confirmed c7 paths (`e8ec18f1d3eac4df`, `1ea46291cb1c7298`, `7d403bdef896a97f`, `45ed0948b6b701a7`) are recognized; the user accepted this same-scene repair in the earlier candidate after reviewing six spaced samples from the Ghost Town slot-02 scene over approximately 11.37 seconds. This acceptance does not cover the new instrumentation, other scenes or the enemy-death c230 path.
