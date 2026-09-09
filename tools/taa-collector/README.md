# Optional TAA collection

Production endpoint: `https://lo.dotslash.pro/v1/taa`. Worker `lost-odyssey-taa-collector`; D1 `lost-odyssey-taa-collection`. No public database access or embedded client credential.

The current position-evidence client build is `0.5.0-position-evidence-1`. It sends schema 2 on the same endpoint; schema 1 canonicalization and hashes remain unchanged, and the Worker accepts both formats without a D1 migration. The verified deployment is `b2c7cd15-dd98-4520-abc8-fa3e9e8b8ab0` (`health` reports schema 1, schemas 1 and 2, and temporal 1). Schema 2 adds nested conservative `position` evidence and independent temporal guards; it does not relax jitter classification or enable a mapping automatically.

Client consent is stored separately in `taa-collection.ini`: absent/invalid=-1 undecided,0 declined,1 enabled. Fresh setup asks once after saving; existing players are asked when opening game Settings. Settings > Language > TAA shader collection allows changes without restarting. No is selected by default. Enabling requires confirmation; disabling clears queued in-memory entries immediately. A request already sent may finish. Opt-out persists independently of graphics previews. All five UI languages are provided.

While TAA is active, collect actual draw paths (including cached shaders), GPU/driver, renderer-byte-FNV64 VS/PS hashes, raster dimensions, known VP slot, exact camera-match candidate mask, viewport/jitter/depth flags and rejection code. Candidate mask bits0–5 mean slots0,4,7,8,230,233. Flags:1 viewport compatible;2 jitter enabled;4 jitter applied;8 depth write;16 same depth allocation as camera anchor. Candidate matches are leads, not verified position dataflow. No matrix values, local paths, shader source/bytecode, saves or images are sent. Map IDs and motion vectors are not collected in schema1.

Render-thread aggregation uses a try-lock, bounded2048 feature entries and saturating counters. Batches of up to32 unsent features are uploaded off-thread via WinHTTP HTTPS with no redirects and bounded timeouts: urgent shader anomalies every10 seconds, routine summaries every60 seconds. One accepted observation per feature per process; failures retry on the next interval, with no disk spool. Exit or disabling discards pending samples; recording may drop when busy or full. Payloads are small JSON, not raw shader logs.

The Worker validates a strict schema and caps bodies at64KiB. A native rate-limit binding allows10 requests/minute per source IP at the serving location (not a global quota). D1's SHA256 primary key covers canonical diagnostic content, excluding counts; concurrent users and retries update the same row. `max_draws` is the largest submitted observation count, not total draws, user count or prevalence. Raw IP is not stored in D1; Cloudflare still processes request metadata. Inactive records expire after30 days, with daily cleanup. No R2 object storage is needed for these structured records.

Use pinned Wrangler4.130.0: `npm ci`, `npm test`, `npx wrangler deploy`. Schema deployment: `npx wrangler d1 execute lost-odyssey-taa-collection --remote --file schema.sql`. Query/export only with authenticated Wrangler; never publish a write token in the game. To rank unknown candidates:

```sql
SELECT json_extract(diagnostic,'$.vs') AS shader, COUNT(*) AS distinct_features,
       MAX(max_draws) AS largest_observation
FROM observations
WHERE json_extract(diagnostic,'$.slot')=-1 AND json_extract(diagnostic,'$.candidates')>0
GROUP BY shader ORDER BY largest_observation DESC;
```

Initial schema-1 validation: three focused Worker tests passed; HTTPS health returned200; three concurrent identical synthetic submissions each returned accepted1 and D1 contained exactly one row. Synthetic row removed. Windows runtime linked successfully. These checks did not establish in-game UI or visual acceptance.

Position-evidence follow-up validation is recorded in [the focused report](../../out/v0.5.0/performance-fix/position-evidence-0.5.0/REPORT.md): 27 native C++ rule/guard checks, retained capture-corpus classification, Worker protocol checks, accepted generated schema 2 JSON, and an incremental source-0.5.0 Windows build passed. The current binary was not run in-game, so this does not transfer the earlier Ghost Town visual acceptance to this candidate.

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

The client analyzes supported translated HLSL once per valid VS, including first use of a warm cached shader. It logs `position-evidence`, at most eight distinct `coverage-candidate` flag/guard states per unknown VS, and the bounded per-frame `jitter_unknowns` counter. Guards are computed before jitter modifies constants and independently of the `UnknownShader` early return. Complex or ambiguous dataflow remains unproven. Neither a matrix match nor `guards=31` automatically grants jitter eligibility.

Current verification: seven Worker tests passed. The actual C++ serializer produced an accepted 343-byte request. Production schema-1 and schema-2 POSTs both returned200; a schema-2 retry updated `max_draws` from44 to54 in the same D1 row. Both exact fixture rows were deleted and an empty readback confirmed cleanup. No player-generated schema-2 data is claimed yet.

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
