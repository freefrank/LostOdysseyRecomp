# Compact automatic diagnostics

Schema 4 is a small CPU diagnostic window stored in the existing D1 `observations` table. Its canonical JSON SHA-256 is the content ID. The archive preserves that JSON and ID; the ledger retains it once in `compact_windows[id]`. Each case's `evidence.compact_refs[id]` points to entries in `pairs` or `bindings` by index. Resolve `first`/`last` or `offset` against that same window's `frames[].offset`. A content ID identifies evidence, not a player, device or session.

The window advertises its runtime version/commit, collection limits, exact binding pairs and backend capabilities. It contains up to 32 CPU frames, 24 pair summaries and 8 inline binding states. Shader programs remain in the existing stage/SHA-256 archive; pair references use renderer byte-FNV hashes and do not resolve a collision automatically. `draws` in a child is a sampled occurrence count; a schema 4 row's `max_draws` counts window occurrences. Neither is a player count or a value to sum across windows.

Use the evidence to separate these situations:

| Evidence | Supported conclusion |
|---|---|
| No archived program for an observed stage/hash | Missing in this archive; its collection/upload cause is unknown |
| Pair outside the window's `bindingPairs` | This client's binding collector does not cover the pair |
| `sparseSupported: false` | Sparse GPU collection is unavailable on this backend |
| Nonzero reason counter such as `full`, `busy` or `invalid` | The window observed that queue outcome; it does not identify which missing shader caused it |
| Nonzero `pending` | That queue had work pending when serialization froze the snapshot |
| Delivery `transportFailed` or `httpRejected` | That upload class reported failures since the current consent state was initialized; affected shader identities are unknown |
| Archived window and retained child reference | The server received this diagnostic content and it reached the archive |
| `needs_review` with source available | Current evidence needs analysis; requesting the same source again does not close that gap |

`delivery.scope` is `since-consent-reset`; these counters are aggregate snapshots, not individual receipts or population totals. `capabilities.strictAck` currently lists only `compact`: legacy uploader `accepted` counters reflect HTTP 200 and do not establish a matching content receipt. Delivery and pending values freeze with the window's first serialization, so retries keep the same content ID. Do not add counts from overlapping snapshots.

CPU frame fields describe TAA readiness, completion/reuse decisions, scene/history rejection masks, previous-frame delta, epoch agreement and reset state. `historyCaptured: false` means the detailed history mask was not recorded for that frame. These are CPU decisions; `gpuCompletion`, `colorImages` and `sparseWindowLinked` are false. An available sparse GPU sequence has no established join to this window. Do not infer final history blending, pixel rejection, visible flicker or a normal/abnormal image pair from CPU state.

The ledger derives shader review dependencies from nested shader diagnostics and binding state. It excludes window IDs, frame offsets, counters, metadata and child occurrence counts, and recognizes semantic repeats of legacy diagnostics. New VS/PS program content, exact PS pairings or binding state still reopen the relevant review while retaining manual states and history. CPU history can support a separate temporal investigation without invalidating an unchanged program analysis.

Prefer these automatic records for coverage and queue diagnosis. If the remaining question requires visual proof, state the exact missing evidence—such as a short same-scene TAA on/off comparison or a specific texture producer/binding—and request only that evidence. A full F1 export is not a prerequisite for ledger triage.
