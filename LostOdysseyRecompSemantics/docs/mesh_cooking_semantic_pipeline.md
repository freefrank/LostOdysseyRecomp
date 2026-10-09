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
binary32.

BCCE48 integrates ten projected monomials through degree three; BCD0F8 lifts
them through the dominant-axis plane to twelve face moments. BCD400 uses
those moments for signed volume, first/second/product integrals, centroid and
tensors about both the origin and centroid. The seven-word descriptor is
vertex count, triangle count, position stride, index-record stride, positions,
indices and flags (bit 1: u16 indices; bit 0: reverse winding). BCD8A8 wraps
that descriptor with binary64 density. Output stores xyz float centroid +0,
binary64 mass +16, origin tensor +24 and centroid tensor +96. Keep guest
integration-enable gate and signed-volume behavior for owner-level policy.

B9F418 lazily fills owner +292 mass, +296 nine float origin-inertia values,
and +332 float centroid. It classifies results through the existing CRT
helper, rejects unsupported nonfinite categories, and logs/corrects negative
mass and tensor signs. An already nonnegative cached mass bypasses rebuild.

## Aggregate cooked mesh output

B9F6F0 emits NXS/CVXM with version from guest state, then the CLHL/CVHL/VALE
geometry bundle, length-prefixed OPC/HBM tree bundle, eleven scalar fields,
cached mass/origin-inertia/centroid and optional SUPM/GAUS support map. Its
stack writer adapters borrow the caller's stream and forward six scalar/block
slots. The temporary geometry adapter releases its valence cache and arrays
before returning; newly built geometry remains owned by the caller's mesh.
The original borrowed cache-payload publication is not an ownership transfer.

## Remaining upper dependencies

82B9C7D8 (mesh cooking orchestration) is still incomplete. Its constructor,
destructor and temporary-array release now exist, but BA5CF8 preprocessing,
B9F198 validation/build remain open. B9F6F0 aggregate serialization is recovered.

B9F6F0 already has recovered scalar/header/support and owner cleanup components.
Its main mesh section BBC110, cached geometry calculation B9F418 and
BB3220 adapter serialization are recovered. Lazy-cache construction BB3130/BBC9F0 is now recovered. The BBD4E8 edge-filter path and its normalized-plane BD92C0 / guest-table
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

## Cooked tree and support owners

B9E6A8 refreshes the borrowed descriptor at cook-owner +84: triangle count +92, vertex count +96, indices +100, positions +104. The owned tree begins at +8. Compact settings use global 832DC188 to choose quantization. The count getters and global accessor remain existing baseline leaves. Both modes compose concrete nine-triangle builds and ownership teardown.

B9EB58 first bounds polygons and vertices to byte indices, destroys existing +288 support owner, and creates a replacement only above 32 vertices. BC61B0 stores a borrowed mesh view at support +32. Resolution +4 is 16; sample count +8 is 1536. Generated +24/+28 tables are independently owned; loaded +12 owns one combined block, with +24/+28 aliases. Destruction frees either the single block or the two arrays, never both ownership forms. Rust should represent this ownership distinction explicitly while keeping original guest offsets and callback-visible state; do not infer ownership of +32. Original allocation-failure ordering is intentionally unchanged.

## Mesh bounds math

BCA410 reduces packed xyz points into separate min/max outputs. BC9040 keeps the actual points attaining each axis minimum/maximum, chooses the greatest squared endpoint distance, then expands the initial midpoint sphere in input order. This is an enclosing sphere, not a promise of the globally minimum sphere. Rust must retain binary32 intermediate rounding, point order, and guest scratch/FP mode when reproducing the exact state. The recovered wrapper fallback and pow dependencies are still pending.

BC9928 mutates a temporary array of point pointers: an outside point moves into the support prefix, and earlier points are reconsidered recursively. Support sets of size two/three/four use midpoint/circumcircle/circumsphere formulas plus guest epsilon. BC9AF0 owns only this temporary pointer array; context vtable +8 allocates and +20 frees. BC9C68 computes both candidates and uses the recursive candidate only if every component is accepted by the original classifier, its radius is nonnegative and no larger. Result 1 identifies the axis-extreme fallback, 2 the recursive sphere, and 0 invalid input. This closes sphere selection, but B9EA90 still depends on the original pow path.

B9EA90 is now closed: min xyz at owner +112, max xyz at +124, chosen center/radius at +136 and tolerance at +152. Tolerance multiplies the greatest max-coordinate by guest pow(2,-22), preserving the original formula even for translated or negative-coordinate meshes. The complete guest pow core is recovered, including its original rounding, stack spills and nonfinite behavior; it is not replaced by host pow. Two cooked-owner graph cases independently verify all output fields and temporary ownership.

## Vertex deduplication ownership

BC2DD0 borrows input count +0 and packed xyz pointer +4, retains unique count +8, owned unique xyz +12 and owned original-index remap +16. Three stable coordinate-word sorts group exact bitwise xyz matches; do not replace this with epsilon welding or numerical float ordering. Optional output aliases {xyz,count,map} do not transfer ownership. Cleanup releases +16 then +12 and leaves the count/borrowed input unchanged. Rust should retain the exact-byte equality and distinguish owned arrays from borrowed result aliases.

### Input uniqueness gate

BB8580 is connected to the stable deduplicator. It copies input xyz to variable guest stack storage, checks exact uniqueness, optionally compacts the original array and updates its count, then releases dedup ownership. A repaired duplicate input still returns zero; callers must retain this distinction. Seven focused family cases pass. The stack probe is reused from mesh_polygon_collect61.

### Near-degenerate face repair

BB88A8 now supplies the area-validation and shortest-edge-collapse gate used by convex construction. Repair rewrites indices and removes repeated-index triangles via tail swap; it does not move point coordinates. It refuses to continue with at most four surviving faces. Seven new original-chain cases pass; complete convex hull construction remains pending.

### Point-cloud convex construction

BBA028 now composes deduplication, deterministic guest perturbation, tetrahedral cavity insertion/circumspheres, hull triangle extraction, used-vertex compaction, orientation and area repair, polygon derivation, centroid and convexity. Its intermediate point/cell/face allocations are released in original order. Three focused original-upper/shared-concrete cases produce closed tetrahedron/cube hulls, including duplicate input, with zero allocations after auxiliary teardown. Private 40-byte hull constants remain external. BB3350 and upper owner orchestration still need connection; the separate indexed-input route remains.

### Cooked point input

BB3350 composes hull and valence, B9E3F8 owns the temporary adapter/cache, and B9E7B0 copies arbitrary-stride xyz input to guest stack before constructing owner+156 geometry. Its +108 bit0 records success. Four focused upper/local-wrapper cases pass, including a padded 20-byte input stride and complete teardown. Indexed input and B9F198 orchestration remain pending.

### Indexed input workspace

BBDF60/BBE070/BBE278/BBF590/BBF628 provide the owned workspace lifecycle and input channels beneath BB9800. Configuration copies or zero-fills xyz channels, optionally clears the third attribute component, and allocates 48-/36-byte-per-face arrays. Zero-face rejection keeps already-acquired channels until cleanup. Four focused original-local-chain cases and complete teardown pass; face insertion/processing remain.

### Indexed face insertion

BBE310 fills a 48-byte face record and three 12-byte corner channel tuples. Optional degeneracy filtering skips duplicate-index/zero-area faces without consuming capacity. It preserves caller winding, missing-channel -1 sentinels and original supplied-ID clamping. Seven new complete-original leaf cases pass, bringing the workspace family to eleven.

### Indexed channel splitting and tuple deduplication

BB3C00 appends xyz words through existing buffer growth. BBE948 optionally gives zero-smoothing faces private position IDs and marks their smoothing value, preserving suppression flags. BBEBE0 merges identical corner-channel tuples and rewrites the face corner references before replacing the owned tuple buffer. Six focused original/local-chain cases pass with complete shared-concrete teardown.

BBF208 now packs the selected output channels, including 2D/3D attributes according to the original option byte. Three export cases pass original-chain comparison and independent dimensions/values with teardown; channel family total is nine.

### Indexed face normals and incidence

BBED20 derives normalized face vectors and optionally emits them through the normal buffer, then builds position-to-face counts, offsets and adjacency. Five focused original-upper cases pass known-vector and incidence checks with complete workspace teardown. Original zero-area behavior is retained.

### Indexed batch remapping

BBF3F8/BBFEA8 now convert referenced corner tuples to final vertex IDs using a temporary sentinel map, preserving per-batch reset policy and shared corners. They emit four-word channel/smoothing records and face/new-vertex counts, then free the map. Three focused original-local-chain cases and complete teardown pass.
