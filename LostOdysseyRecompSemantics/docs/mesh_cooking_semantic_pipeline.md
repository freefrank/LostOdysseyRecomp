# Mesh cooking recovery boundary

This is a layout and dependency guide for the eventual Rust port, not a claim
that the upper mesh cooking path or gameplay is complete. Guest addresses,
endianness and original call-visible state remain explicit in the current C++.

## Recovered topology

`mesh_edge_build61` normalizes each triangle side to its smaller/larger vertex
IDs, performs stable sorting on both endpoint keys, emits unique undirected
edges and preserves a triangle-side-to-edge mapping. Input indices can be u32
or u16. The six-word descriptor has:

- +0: unique edge count
- +4: owned endpoint pairs, two u32 values per edge
- +8: triangle count; the descriptor initializer intentionally leaves this word
- +12: owned u32 edge IDs, three per triangle in original side order
- +16: optional owned eight-byte incidence records per edge
- +20: optional owned u32 triangle IDs grouped by edge

Incidence records store a u16 degree at +2 and a u32 prefix offset at +4.
BBD1E0 counts, prefix-sums, scatters triangle IDs and restores the offsets.
It retains original allocation order and partially initialized ownership on
failure. The cleanup at BBCD80 frees +20, +16, +4, +12 in that order.

A separate five-word valence cache stores vertex count at +4, adjacency-byte
count at +8, degree/offset records at +12 and adjacent vertex bytes at +16.
BC7F48 writes wrapping u16 prefix offsets into the second halfword of each
four-byte record. Its original nonempty-record precondition is retained.
Cache construction is not yet complete. `mesh_edge_flags61` now classifies
boundary and angle-selected edges: side bit31 selects the edge, side bit30
marks its first vertex as touched by selected edges, and incidence-record
bit0 mirrors edge selection. Two temporary byte masks are released in order.

## Recovered stream layers

- `growable_output61`: borrowed virtual writer, owned dynamic byte buffer
- `mesh_stream_write61`: optional-endian word/float output, float spans, NXS
  and ICE section headers (prefix, endian flag, four-byte tag, version)
- `mesh_support_stream61`: nested ICE/SUPM and ICE/GAUS sections, two counts,
  then two raw support byte arrays
- `mesh_valence_stream61`: ICE/VALE version 2, vertex/adjacency counts, maximum
  degree, compact degree array and raw adjacency bytes

VALE degrees occupy one byte when the maximum fits in 255, otherwise two bytes
with the selected endian order. Its temporary degree array is freed before
writing the raw adjacency payload. Float output preserves original single-
precision staging before and after endian conversion; this is observable for
some bit patterns and must not be replaced casually with generic host floats.

## Owners and lifetimes

The 348-byte cooking owner embeds tree state at +8 and auxiliary mesh storage
at +156. Base/derived construction and cleanup are recovered in
`mesh_cook_storage61` and `mesh_auxiliary_storage61`. The borrowed mesh adapter
keeps source aliases at +4/+12 and owns only its optional valence cache at +16.
`mesh_cache_lifetime61` disposes cache arrays before the cache descriptor,
clears +16 and restores the adapter's base table. Borrowed sources survive.

For Rust, separate serialized representations from runtime owners. Use explicit
u16/u32 endian reads and writes, keep triangle/edge/vertex IDs distinct, and
make source slices borrowed while array storage remains owned. Preserve layout
and ordering in the compatibility layer before introducing idiomatic APIs.
Do not add bounds, rollback or concurrency policies as part of a mechanical
semantic port; those would be separate behavior changes.

## Remaining upper dependencies

82B9C7D8 (mesh cooking orchestration) is still incomplete. Its constructor,
destructor and temporary-array release now exist, but BA5CF8 preprocessing,
B9F198 validation/build and B9F6F0 aggregate serialization remain open.

B9F6F0 already has recovered scalar/header/support and owner cleanup components.
Its main mesh section BBC110, lazy-cache build BB3130/BBC9F0 and cached geometry
calculation B9F418 still need completion. The BBD4E8 edge-filter path and its normalized-plane BD92C0 / guest-table
atan2 822DA388 math lowers are recovered. BBDDF0 orchestration still remains
to be connected. Do not infer upper completion from a
working lower stream or topology fixture.

## Evidence limits

Focused original-body comparisons compose actual original local callers where
available, while sharing explicitly identified recovered sorter, allocator,
stream and auxiliary components. Independent expected topology, bytes and
ownership checks accompany those comparisons. Full Clang library compilation
passes; this is not independent all-original whole-chain proof, a gameplay
check, a Rust implementation or additional historical mapping credit.
