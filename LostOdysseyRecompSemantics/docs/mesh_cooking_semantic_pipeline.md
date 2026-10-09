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
`mesh_cache_build61` constructs per-vertex degree/neighbor caches and lazily
publishes the borrowed descriptor+4 view into source+84. `mesh_edge_flags61` now classifies
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

## Polygon and normal support

BB8C08 replaces mesh +8 owned u32 triangles by fans from borrowed 36-byte
polygon records (count +36, records +40; u16 vertex count at record +0 and
byte-index pointer +4). It orients each triangle away from the surface-area
centroid. BD8FD8 area, BC3128 corner angle and BC65F8 weighted centroid are
recovered alongside plane/atan2 math. BB8FA0 packs sign/order flags and two
quantized angles using the guest arcsine approximation 82325048. These stages
retain single-precision rounding and original component selection.

BC3250 builds normalized face normals, accumulates uniform or angle-weighted
vertex normals, then normalizes each vertex. Its descriptor borrows positions
and u32/u16 indices; optional face/vertex destinations remain borrowed. The
two-word owner records newly allocated destinations only. BB9160 replaces mesh
+20 with sign-reversed angle-weighted normals and releases temporary face data.
Keep face and vertex ownership separate in a Rust port.

## Packed triangle links

BC3F20 builds three adjacency words per triangle, using side order (0,1),
(0,2), (1,2). Low 29 bits hold the neighboring triangle ID; all-ones indicates
a boundary. Top two bits hold its edge slot. Bit 29 carries the optional
geometric selection flag. Stable endpoint sorting groups manifold pairs;
original nonmanifold diagnostics remain in place. The owned payload follows
a four-byte count prefix and cleanup releases payload minus four.

## Rebuilt polygon geometry

BB9318 collects connected triangle components and ordered boundaries from a
closed mesh. BB9AA8 owns the resulting record and byte-index arrays. Each
36-byte record stores u16 degree at +0, borrowed byte slice at +4, normalized
plane at +12..24, and min/max vertex projection at +28/+32. Winding is aligned
with source triangles and checked against the surface centroid. Supporting
planes expand to cover all source vertices, then polygon fans replace the
owned triangle array. Component/edge/ordering scratch arrays are released.

BBB728 finalizes unique polygon edges: mesh +52 edge count, +56 owned byte
endpoint pairs, +48 owned u16 polygon-to-edge map, +64 owned eight-byte
incidence records (u16 count +2, u32 prefix +4), +68 owned polygon-ID bytes,
and +60 owned xyz edge bisectors from adjacent polygon normals. Polygon
record +8 borrows its corresponding edge-ID slice.

## CVHL version 5 stream

BBC110 emits ICE/CVHL v5 with endian flag, counts, vertices, adaptive-width
triangle indices, normal mode, vertex normals, centroid, and 36-byte polygon
records. In the output copy, record +4/+8 become byte offsets relative to the
mesh byte-index/u16-edge arrays; in-memory borrowed pointers stay intact.
The rest contains polygon vertex bytes, adaptive edge IDs, reserved words,
endpoint pairs, edge normals, both incidence halfword fields, incidence
prefixes and owner bytes. Normal mode zero packs two five-bit angle fields
and two three-bit masks; nonzero writes xyz binary32. Lazy construction
retains mesh-owned outputs; two incidence scratch allocations are released.
Rust should separate this owned mesh from a borrowed writer and serialized
relative offsets, preserving the original field widths and endian choice.

BB3220 wraps the geometry in ICE/CLHL v0 and appends ICE/VALE v2. It lazily
constructs the adapter +16 cache and publishes its borrowed payload at source
+84; the adapter retains ownership of that cache and its two arrays.

## Mass-property math

BCD300 reads positions at descriptor +80 with byte stride +72, preserving
binary32 geometry operations while exporting four binary64 plane coefficients.
BCCCA8 consumes volume/first/second/product moments at +288..360 and density
at +56, writes mass +48, and emits a symmetric binary64 inertia tensor about
the centroid. Centroid and parallel-axis products intentionally round through
binary32. Projection, face and volume integration are still open.

## Remaining upper dependencies

82B9C7D8 (mesh cooking orchestration) is still incomplete. Its constructor,
destructor and temporary-array release now exist, but BA5CF8 preprocessing,
B9F198 validation/build and B9F6F0 aggregate serialization remain open.

B9F6F0 already has recovered scalar/header/support and owner cleanup components.
Its main mesh section BBC110 is recovered; cached geometry calculation B9F418
still needs completion. BB3220 adapter serialization is recovered. Lazy-cache construction BB3130/BBC9F0 is now recovered. The BBD4E8 edge-filter path and its normalized-plane BD92C0 / guest-table
atan2 822DA388 math lowers are recovered. BBDDF0 now orchestrates the requested components and releases
unretained auxiliary arrays. Do not infer upper completion from a
working lower stream or topology fixture.

## Evidence limits

Focused original-body comparisons compose actual original local callers where
available, while sharing explicitly identified recovered sorter, allocator,
stream and auxiliary components. Independent expected topology, bytes and
ownership checks accompany those comparisons. Full Clang library compilation
passes; this is not independent all-original whole-chain proof, a gameplay
check, a Rust implementation or additional historical mapping credit.
