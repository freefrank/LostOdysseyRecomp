# Recovered tree pipeline and Rust migration boundaries

This describes the concrete tree family recovered on `trail/semantic-recovery`.
Names describe observed algorithms, not recovered original C++ type names.
It is implementation guidance, not a claim of runtime/gameplay acceptance.

## Logical pipeline

`82BD22A8` constructs a triangle tree with leaf threshold 8, exports leaf bounds
and packed index ranges, constructs a second box tree with threshold 1, remaps
its leaves, and binds one of four output strategies. The concrete geometry and
strategy dispatch surface is `tree_mesh_callbacks61::Apply`. Unknown addresses
return false without modifying state; the host retains its external fallback.

`82BD12F8` chooses a strategy from two flag bytes. The first selects internal-only
(compact) versus full-node topology; the second selects quantization. Storage
ownership remains with guest allocations. The adapter does not install hooks.

## Four stored representations

- Full float: `82BDBD90` binds, `82BDBC18` flattens, 36 bytes per record.
  Six binary32 fields are center xyz and half extents xyz. Three words at
  offsets 24/28/32 hold child indices and descendant count for internal nodes.
  A leaf stores `0x80000000 | leaf_id` at offset 24. Its remaining two words are
  not initialized by the flattener and must not silently be canonicalized.
- Internal-only float: `82BDD058` binds, `82BDCE38` flattens, 32 bytes per record.
  Only internal source nodes emit records, so stored count is leaf count minus
  one. A sole leaf child is ordered first. Offset 24 folds the first leaf ID with
  bit 31, and bit 30 indicates that the second child is also a leaf. An internal
  first child stores `0xDEAD`. Offset 28 holds emitted descendants when the
  second child is internal, otherwise zero. Do not substitute the full-tree
  child-index interpretation for these two words.
- Full quantized: `82BDC208`, 24 bytes per record. Six halfwords at offsets 0..10
  hold three signed centers and three unsigned half extents; offsets 12/16/20
  retain the full topology words. Construction uses 36-byte temporary records.
- Internal-only quantized: `82BDD1E8`, 20 bytes per record. The same halfword
  coordinates precede two compact topology words; temporary records are 32 bytes.

For each strategy, owner offset 4 is the record count and offset 8 is the payload
pointer, four bytes after an allocation's count prefix. Quantized owners also
store six binary32 reconstruction scales at offsets 12..32. Guest addresses and
stored words are 32-bit big-endian; source register arithmetic may remain 64-bit.

## Quantization

Construction finds six global absolute maxima over center and half-extent
coordinates, derives scales, truncates centers to signed 16-bit values and
half extents to unsigned 16-bit values, and copies topology unchanged. The
existing byte at `83216670` selects conservative extent handling: use 15-bit
extent precision, decode each candidate box, then enlarge extents until the
original bounds are covered or the original halfword-saturation path terminates.
Every binary32 arithmetic stage and the source comparison order remain explicit.

The current implementation shares this algorithm across both quantized formats.
It keeps format-specific allocation tags, count arithmetic, temporary stride,
register-save boundaries and continuation addresses rather than assuming their
ABIs are interchangeable.

## Import, export and lifetime

- Full/compact float input: `82BDBED8` / `82BDB350`
- Full/compact float output: `82BDD868` / `82BDDB20`
- Full/compact quantized input: `82BDC9F0` / `82BDB7F8`
- Full/compact quantized output: `82BDC838` / `82BDB660`
- Scalar word/float output adapters: `82BD7D58` / `82BD7E18`

A stream starts with count, then records. Quantized streams end with six float
scales. The endian flag reverses each 32-bit field, except quantized coordinates,
which reverse as six independent 16-bit fields. Output uses stack staging and
leaves source records unchanged. Float paths intentionally load/widen/narrow
through FP, including the scalar endian adapter; replacing them with raw copies
would change behavior for some nonfinite values.

Readers and allocators are borrowed, mutable service boundaries. Count changes
and old storage release can precede allocation failure. Payload-read status is
ignored where the original ignores it. In quantized construction, a failed later
allocation does not introduce a new cleanup of the temporary buffer. These are
preserved semantics, not recommended policies to copy into unrelated code.

`tree_strategy_release61` handles four base cleanup bodies and their deleting
wrappers. The payload allocation is freed at `payload - 4`, followed by the owner
only when the deleting flag requests it. Cleanup restores the common base table
and does not reset count/scales. `tree_mesh_lifetime61` handles source attachment
and whole mesh-owner cleanup, including owned map/raw storage.

## Suggested Rust separation

Keep typed record math and topology separate from the PPC ABI adapter. Model
borrowed source/reader memory, owned count-prefixed payloads, temporary staging,
and allocator callbacks explicitly. Preserve 32-bit guest-address wrapping and
big-endian field access; do not transmute guest bytes into host-layout structs.
Retain callback-visible reloads, guest stack writes and exact FP rounding in the
compatibility layer until there is evidence they can be removed. No Rust port or
new validation/rollback policy is introduced by this recovery batch.

## Evidence and limits

Focused pinned-original comparisons cover the individual algorithms and shared
format variants. Four nine-triangle integrations exercise full/compact and
float/quantized construction through concrete callbacks, then release every
tracked allocation with recovered teardown. The original upper in those
integration cases shares recovered lower bodies; this is not an independent
all-original whole-chain comparison. The per-unit draft manifests record the
precise cases. Nonfinite, fault/MMIO, concurrency, Windows and gameplay coverage
are not implied. Historical catalog mapping and runtime wrapper counts remain
unchanged.
