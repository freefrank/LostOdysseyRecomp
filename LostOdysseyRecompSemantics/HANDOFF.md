## Root direct continuation: polygon topology finalization (2026-10-09)

`mesh_polygon_topology61` closes BBB728: unique polygon edges, per-polygon u16 edge slices, edge-to-polygon incidence and normalized adjacent-normal sums. Two complete-original-upper/shared-build-and-sort cases pass Full72/RAM/CSR/events and independent cube topology, both lazy fresh build and prebuilt replacement. Exactly eight retained outputs match ownership. Full Clang library passes. Failure/recursive repair paths untested; original partial-allocation ordering retained. The main BBC110 CVHL writer is now the next direct target.

## Root direct continuation: owned polygon geometry rebuild (2026-10-09)

`mesh_polygon_build61` closes BB9AA8: replace 36-byte polygon records and packed byte indices, derive/orient planes against source faces and centroid, expand supporting planes, calculate projection intervals and regenerate triangle fans. Three original-upper/shared-concrete-chain cases pass Full72/RAM/CSR/events and independent six cube faces/planes/ranges/triangle membership, existing-storage replacement and open-mesh rejection. Exactly three owned outputs remain; full Clang library passes. Main CVHL serialization still awaits BBB728 topology finalization and BBC110.

## Root direct continuation: closed-mesh polygon collection (2026-10-09)

`mesh_polygon_collect61` closes BB9318 component-to-polygon collection and B7E504 guest stack probe. Three original-upper/shared-concrete-chain cases pass Full72/RAM/CSR/events: a twelve-triangle cube yields six four-vertex polygons, optional triangle membership covers all twelve IDs, and an open mesh is rejected. All temporary allocations are released. Full Clang library passes. Large stack probes and diagnostic/failure paths remain untested; two implementation entries, no historical mapping/runtime claim.

## Root direct continuation: components and ordered boundary chains (2026-10-09)

`mesh_boundary_walk61` closes BB8498 recursive/tail component collection, BC2A18 duplicate-undirected-edge cancellation and chain ordering, plus BD2988/BD2B90 owned word capacity/copy. Four composed-original cases pass Full72/RAM/CSR/events with independent visits, output order, closed loop and disconnected partial-output ownership. Concrete shared growth/copy/release retained; full Clang library passes. Four implementation entries, no mapping/runtime claim.

## Root direct continuation: packed triangle adjacency (2026-10-09)

`mesh_triangle_links61` closes BC38E8/BC3970/BD9218/BC3EC0/BC3B10/BC3CB0/BC3F20: emit normalized edge records, stable-group owners, write reciprocal packed triangle links, optionally tag geometric boundaries and release prefixed storage. Ten focused original-local-chain/shared-sort-and-topology cases pass Full72/RAM/CSR/callbacks plus independent packed links, boundary count and ownership. Both index widths and optional coplanar filter pass; diagnostics/nonmanifold/failure matrices omitted. Full Clang library passes; seven implementation entries, no historical mapping/runtime claim.

## Root direct continuation: polygon planes and byte winding (2026-10-09)

`mesh_polygon_plane61` closes BD9390 Newell plane accumulation and BC3880 byte-index reversal. Five complete-original-body cases cover scalar triangle, four-edge quad, quad plus scalar tail, invalid count and odd reversal. Full72/RAM/CSR and independent plane values pass; full Clang library passes. The plane offset uses vertex arithmetic mean, distinct from the surface-area centroid. Two implementation entries, no baseline/runtime claim.

## Root direct continuation: face and vertex normal construction (2026-10-09)

`mesh_vertex_normals61` closes 82656EB8/BC30A0 two-array ownership, BC3250 normalized face/vertex construction with optional angle weighting, and BB9160 mesh installation with final sign reversal. Five composed-original-chain cases pass Full72/RAM/CSR/callbacks and independent folded-mesh normal directions, borrowed u16/implicit indices, invalid positions, allocation and cleanup. Shared angle/memory/allocator lowers are concrete. Full Clang library passes; private constants external. Four implementation entries, no mapping/runtime claim.

## Root direct continuation: polygon fans and orientation (2026-10-09)

`mesh_polygon_triangulate61` closes BB8C08: replace owned triangle indices with polygon fans, compute the surface centroid and flip inward-facing triangles. Three original-upper/shared-concrete-geometry cases pass Full72/RAM/CSR/callbacks plus independent cube fan connectivity, outward winding and old-buffer replacement. Full Clang library passes. Original count/allocation preconditions retained; no broad failure matrix or runtime claim.

## Root direct continuation: mesh area, corners and surface centroid (2026-10-09)

`mesh_geometry_math61` adds BD8FD8 indexed triangle area, BC3128 corner angle and BC65F8 surface-area weighted centroid. Thirteen focused cases total pass Full72/RAM/host CSR and independent values, including unequal-area triangles and invalid mesh. Original corner/centroid bodies compose original atan2/area lowers. Full Clang library passes; private constants stay external. Three implementation entries, no historical mapping or gameplay claim. These close geometry dependencies for polygon triangulation and mesh serialization.

## Root direct continuation: normal angle encoding (2026-10-09)

`mesh_normal_encode61` closes 325048 guest-table arcsine and BB8FA0 sign/order masks plus two quantized angles. Four complete original encoder/arcsine cases pass Full72/RAM/host CSR and independent masks/angle values. The encoded components retain min(max(abs(x),abs(y)),abs(z)) and min(abs(x),abs(y)), rather than a sorted pair. Full Clang library passes. LO_NORMAL_ENCODING_CONSTANTS supplies a private 144-byte bundle (128 bytes at83214E08, then four float words820D60A8/820D6454/82000E40/822181C4); none is checked in. Broad nonfinite/precision-edge cases remain untested.

## Root direct continuation: adaptive mesh stream widths (2026-10-09)

Shared mesh stream code now includes BD7D00 halfword, BD7E70 alternate float scalar and BD7FF0 alternate float span; 14 cases total pass. Valence code adds BADFA0 word maximum and BD8668 adaptive byte/halfword/full-word output; eight cases total pass. The full-word path preserves the original float staging rather than assuming integer memcpy. Full72/RAM/host CSR/callbacks and independent bytes pass; complete Clang library passes. Five implementation entries, no extra runtime/mapping claim.

## Root direct continuation: topology and lazy valence construction (2026-10-09)

`mesh_cache_build61` closes BBDDF0 topology/filter orchestration, BBC9F0 per-vertex degree/neighbor cache and BB3130 lazy descriptor ownership/publication. Three composed-original upper-chain cases pass Full72/RAM/host CSR/callbacks and independent degrees, offsets, byte neighbors, borrowed published view and release-unretained ownership; all topology, sorting, geometry, allocator and lifetime lowers are concrete shared implementations. Full Clang library passes. Three addresses; partial-failure order remains unchanged, upper whole-mesh serialization still incomplete.

## Root direct continuation: boundary and angular edge flags (2026-10-09)

`mesh_edge_flags61` closes BBD4E8: classify boundary/shared edges by incidence and signed plane/dihedral angle, tag selected sides and propagate touched vertices. Three original-upper/shared-concrete-geometry cases cover coplanar/folded meshes, angular thresholds and both index widths; Full72/RAM/host CSR/callbacks, independent side/vertex/edge flags and zero remaining temporary allocations pass. Full Clang library passes. Private angle constants remain external; diagnostics/allocation failures are not broadly exercised.

## Root direct continuation: mesh plane and angle math (2026-10-09)

`mesh_geometry_math61` closes BD92C0 normalized triangle planes and 2DA388 guest-table rational atan2, retaining FP stages and signed-zero handling. Eight complete-original-body cases pass Full72/RAM/host CSR and independent plane/angle assertions. Full Clang library passes. The angle harness requires LO_MESH_MATH_CONSTANTS pointing to a private 184-byte block from guest address 83214E88; image constants are not checked in. Two implementation addresses; NaN/infinity behavior is not broadly tested.

## Root direct continuation: mesh cache lifetime and porting map (2026-10-09)

`mesh_cache_lifetime61` closes seven base/derived adapter, cache descriptor and prefix-offset entries. Five composed-original cases pass Full72/RAM/callbacks and independent borrowed fields, arrays-before-descriptor free order, empty cleanup and wrapping u16 prefix checks. Full Clang library passes. `docs/mesh_cooking_semantic_pipeline.md` records topology/stream layouts, ownership, Rust boundaries and explicitly unresolved upper dependencies. No complete cooking, runtime or historical mapping claim.

## Root direct continuation: edge-to-triangle adjacency (2026-10-09)

`mesh_edge_build61` now also closes BBD1E0: count triangle incidence per unique edge, build prefix offsets, scatter triangle IDs and restore prefixes. Two original-caller-plus-original-edge-builder cases raise the family to six focused cases, both u32/u16 input widths; Full72/RAM/callbacks and independent degree/offset/incident-list/owned-allocation assertions pass. Full Clang library passes. One additional implementation address; partial allocation behavior retained and broad failure tests intentionally omitted.

## Root direct continuation: unique triangle edges (2026-10-09)

`mesh_edge_build61` closes BBD4C0 descriptor initialization and BBCE58 edge normalization, two stable-sort passes, unique undirected pairs and triangle-side mapping. Four original-upper/shared-concrete-sort cases pass Full72/RAM/callbacks and independent pairs, mapping, ownership and reuse assertions for u32/u16 indices. Full Clang library passes. Diagnostic/partial-allocation paths retain original behavior but are not exercised. No new fallback or historical/runtime mapping claim.

## Root direct continuation: compact adjacency streams (2026-10-09)

`mesh_valence_stream61` closes BD8360 degree maximum, BD83A0 byte/halfword packing and BBCC28 ICE/VALE output. Five composed-original local-chain cases pass Full72/RAM/callbacks and independent native/swapped compact/wide/empty stream bytes and temporary-allocation release-before-payload ordering. Header/scalar/growable/allocator lowers are concrete shared implementations; full Clang library passes. Three implementation addresses only, original allocation/count preconditions retained.

## Root direct continuation: nested support-map streams (2026-10-09)

`mesh_stream_write61` now shares NXS/ICE header construction and adds BD7DB0 virtual word output (11 focused cases total). `mesh_support_stream61` closes the borrowed BB3408/BB3420 adapter and BB3818/BB36F0 nested GAUS/SUPM serialization, including two counts and two byte arrays. Four composed-original cases pass Full72/RAM/callbacks and independent complete stream bytes in both endian modes, with actual shared header/scalar/growable components. Full Clang library passes. Six new implementation addresses; source aliases remain borrowed, no runtime or historical mapping credit.

## Root direct continuation: mesh stream payloads (2026-10-09)

`mesh_stream_write61` recovers BADA70 scalar float, BADCD0 float spans and BADD60 NXS section headers with four-byte tags and endian-aware versions. Seven original-upper/shared-concrete-growable-writer cases pass Full72/RAM/host CSR/callbacks and independent bytes/counts, including empty spans. Full Clang library passes. Three implementation entries; no new boundary guards, historical mapping/runtime credit or complete upper serializer claim.

## Root direct continuation: mesh cooking owner storage (2026-10-09)

`mesh_cook_storage61` closes eight constructor, teardown and temporary-array disposal entries used by upper mesh cooking. Original cleanup order includes the four-array descriptor, embedded tree, optional virtual owner and auxiliary buffers. Five composed-original cases pass Full72/RAM/host CSR/callbacks and independent layout/free-order checks; existing recovered tree/auxiliary components are shared by the comparison. Full Clang library passes. No new defensive lifetime policy, runtime hook or historical mapping credit; upper mesh cooking itself remains incomplete.

## Root direct continuation: mesh auxiliary storage (2026-10-09)

`mesh_auxiliary_storage61` closes BC6428/D33160/BC8588 construction and BC7E90/BC67C8/BC85F0 teardown. Aggregate storage frees once while individual storage frees its original ordered slots; optional descriptor cleanup uses the existing actual lower. Four composed-original cases cover constructor, aggregate, individual and empty destruction with Full72/RAM/host CSR/callback and independent layout/free-order checks. Full Clang library passes. Six implementation addresses; interior aliases remain untouched where PPC leaves them, no extra defensive behavior or runtime/mapping credit.

## Root direct continuation: serialization control (2026-10-09)

`serialization_control61` recovers E948 endian-aware virtual word output, E9B8 global context registration/duplicate diagnostic and B9CB70 mode selection, including its original caller-scratch fallback. Nine original-upper/shared-concrete-writer cases pass Full72/RAM/callbacks and independent values, output bytes and registration state. Full Clang library passes. No duplicate-registration policy or host-endian fallback is invented; concurrency unvalidated, historical mapping/runtime unchanged.

## Root direct continuation: borrowed memory input (2026-10-09)

`memory_input61` recovers E550/E568/E580/E598/E5B8/E5D8 scalar and block cursor reads with the actual copy lower. Six original-body cases pass Full72/RAM/host CSR and independent values/cursors. Callback dispatch exposes these reader targets; two additional 20/24-byte quantized import integrations consume count, payload and scales through concrete E580/E5D8 memory readers rather than synthetic read callbacks. The original upper shares these independently checked lowers. Eight import cases total and complete Clang library pass. No invented end-bound checks, mapping or runtime credit.

## Root direct continuation: global scratch arena (2026-10-09)

`scratch_arena61` recovers E628 aligned reusable reserve, E738 counting/high-water mode and E810 scoped region/alignment switching. Original allocation/reallocation, diagnostic and global field ordering remain explicit; no new rollback or free is added. Eight composed-original cases pass Full72/RAM/callbacks and independent alignment/counting/scope-state assertions. Full Clang library passes. Global concurrency and failure paths remain unvalidated; three implementation addresses, no historical mapping/runtime credit.

## Root direct continuation: growable output stream (2026-10-09)

`growable_output61` recovers E330/E378/E3C0/E408/E450 scalar virtual writers, E498 concrete buffer append and E2C8 payload cleanup. Shared append grows capacity to required plus 4096, copies used bytes and frees old allocation before appending; no new failure guard is added. Nine original scalar/append/cleanup cases pass Full72/RAM/host CSR/callback traces and independent byte/capacity/owner assertions with the actual shared copy lower. Full Clang library passes. Seven implementation addresses added, no historical baseline/runtime claim.

## Root direct continuation: box and triangle primitives (2026-10-09)

`geometry_primitives61` adds DF08 centered bounding cube, DFC8 box containment, E040 eight-corner export, E108 triangle winding reversal, E140 area and E1B8 centroid-relative/normalized expansion. Eight complete original-body comparisons pass Full72/RAM/host CSR and independent finite geometry assertions; full Clang library passes. Methods preserve FP stages and scratch state without allocation, new guards or runtime hooks. Six implementation addresses added, historical mapping unchanged.

## Root direct continuation: complete tree format codecs (2026-10-09)

`tree_flat_write61` adds D868/DB20 output for full36-byte and compact32-byte float nodes, retaining FP staging and optional word-endian conversion. Four composed-original output/scalar cases pass Full72/RAM/host CSR and independent stream/source assertions. Concrete callback dispatch now also exposes every recovered read/write/scalar route. `docs/tree_semantic_pipeline.md` documents all four layouts, topology distinctions, ownership/failure ordering and Rust migration boundaries. Full Clang library passes. No runtime hooks or baseline mapping changes.

## Root direct continuation: all four concrete mesh strategies (2026-10-09)

The mesh callback adapter now connects full/compact and float/quantized binders plus all strategy cleanup/deleting routes. Four nine-triangle integrations pass original-upper/shared-recovered-lower Full72/RAM/host CSR/callback/live ownership comparisons and independent representation topology/count assertions. Each then runs recovered mesh teardown: all tracked allocations release and owner storage slots clear. Three earlier borrowed-service cases remain passing. Full library passes; adapter/integration adds no addresses, runtime hooks or gameplay acceptance.

## Root direct continuation: compact quantization and strategy lifetime (2026-10-09)

Quantization now also handles D1E8 internal-only trees: shared six-axis quantization/conservative correction, CE38 32-byte staging, 20-byte output and two topology words. Six original binder/recursive-lower cases total pass for both representations. `tree_strategy_release61` adds CFE0/D170/D7F0/DA48 cleanup and DAC0/DCD8/DD38/DD98 deleting wrappers with shared lifecycle code; six composed-original cases pass, including free order, retain/empty paths and base-table restoration. Full Clang library passes. Nine implementation addresses added; no historical mapping/runtime or gameplay credit.

## Root direct continuation: compact internal-node construction (2026-10-09)

`tree_compact_flatten61` recovers CE38, emitting only internal nodes into 32-byte records, ordering a sole leaf child first and folding leaf IDs/flags into the record. Three original-recursive cases cover leaf pairs, swapping and both-internal recursion. `tree_compact_strategy61` recovers D058, validating full-tree size and storing leaf-count minus one records, with replace/reuse ownership. Three original-binder plus original-recursive cases pass. Full72/RAM/host CSR, appropriate allocation traces and independent layouts agree; full Clang library passes. Valid internal-tree preconditions retained, no new leaf guard or historical/runtime credit.

## Root direct continuation: eight-word flat import (2026-10-09)

`tree_flat_load61` now also handles BDB350 32-byte records, sharing the 36-byte endian/read flow while retaining distinct size arithmetic, allocation registers and call continuations. Six total pinned-original native/swapped/allocation-failure cases pass Full72/RAM/callbacks and independent payload/count/owner assertions; full Clang library passes. No new test framework or defensive behavior. Historical baseline and runtime remain unchanged.

## Root direct continuation: compact quantized format (2026-10-09)

Existing quantized codecs now also recover BDB7F8 import and BDB660 export for 20-byte records (six halfwords, two topology words, six owner scales). Shared implementations select format-specific size arithmetic, register/frame layout and continuation addresses rather than duplicate the codecs. Both original 24-byte cases remain passing; six import and four export cases total pass Full72/RAM/host CSR and independent endian output/scale assertions. Full Clang library passes. Two additional implementation addresses only; historical baseline and runtime stay unchanged.

## Root direct continuation: quantized tree output (2026-10-09)

`tree_scalar_write61` adds BD7D58/BD7E18 integer/float endian append adapters, preserving the real float reinterpret-and-append route. Four original-upper/shared-writer cases pass. `tree_quantized_write61` adds BDC838, emitting count, mixed-width 24-byte nodes and six scales through recovered append lowers. Two original-upper plus original-scalar-wrapper cases pass Full72/RAM/host CSR and independent stream/source assertions. Full Clang library passes. Together with C208 construction and C9F0 import, quantized representation now has implementation coverage across build/read/write; no end-to-end gameplay, refreshed catalog or nonfinite claim.

## Root direct continuation: quantized tree import (2026-10-09)

`tree_quantized_load61` recovers BDC9F0: replace count-prefixed 24-byte node storage, optionally swap six 16-bit coordinates and three 32-bit topology fields per node, then read six float decoding scales. Three focused original-upper/shared-allocator cases pass Full72/RAM/host CSR/callbacks and independent payload/scales/ownership assertions, including allocation failure; full Clang library passes. Reader return behavior and existing failure ordering are preserved, no new defensive policy, baseline or runtime credit.

## Root direct continuation: quantized tree strategy (2026-10-09)

`tree_quantized_strategy61` recovers BDC208: flatten to temporary 36-byte nodes, compute six global coordinate maxima, emit 24-byte nodes with signed 16-bit centers/unsigned half extents and preserved topology, and save six reconstruction scales. The existing conservative option uses 15-bit extents and enlarges quantized extents until decoded bounds contain source bounds. Original allocation/failure ordering is retained. Three pinned original-binder plus original-recursive-lower cases (null and both finite modes, differing child boxes) pass Full72/RAM/host CSR/callback/live ownership and independent topology/coverage checks. Full Clang library passes; nonfinite/failure paths and gameplay untested, no baseline mapping credit.

## Root direct continuation: flat-node import (2026-10-09)

`tree_flat_load61` recovers BDBED8 reader-driven replacement of count-prefixed 36-byte nodes. Count and every node word optionally swap endianness; owner count changes and old storage frees before replacement allocation. Reader payload return remains ignored as in PPC. Three original-upper/shared-allocator cases pass Full72/RAM/callback traces with independent native/swapped payload and allocation-failure assertions; complete Clang library passes. No new rollback, validation guard, baseline credit or runtime hook.

## Root direct continuation: mesh owner lifetime (2026-10-09)

`tree_mesh_lifetime61` adds BD2200 borrowed-mesh attachment and BD2268/BD27F8 nondeleting/deleting cleanup routes. Validation happens before releasing old state. Cleanup frees owned raw storage and the count-prefixed map, restores the base table and conditionally frees the owner while retaining its original return pointer. Five pinned-original-upper/shared-recovered-lower cases pass Full72/RAM/callback traces and independent release order; complete Clang library passes. External build and allocator callbacks remain borrowed. No baseline mapping or gameplay credit.

## Root direct continuation: concrete mesh integration (2026-10-09)

`tree_mesh_callbacks61` now resolves the ten recovered geometry/split/strategy callback addresses. A nine-triangle case passes through the actual first-tree threshold 8, second-tree threshold 1, packed remapping and owned flat strategy. Full72/RAM/host CSR/callback traces/live ownership agree with the original upper using the same independently recovered lowers; independent leaf counts, packed offsets and flat child indices also pass. The prior three borrowed-service cases remain unchanged. Only allocator services are synthetic in the new case. This adapter adds no PPC address credit or runtime hook; whole-chain original-lower independence and gameplay remain unverified.

## Root direct continuation: concrete split policies (2026-10-09)

`tree_split_policy61` recovers `82BB3B60` bounds-axis midpoint and `82BB3B88` unsigned count/threshold decision. Four pinned original-body cases pass Full72/RAM/host CSR plus independent results; full Linux Clang library builds. No historical catalog or runtime credit. Concrete bounds, centroids, split policies and flat strategy are now separately available for integration; the existing mesh upper oracle still uses borrowed geometry/strategy services.

## Direct continuation — 2026-10-09

Parallel recovery workers are stopped. The direct executor received the user's
private XEX upload and verified the expected SHA256 before regenerating PPC with
the pinned XenonRecomp plus repository patch. Private inputs remain ignored.

BD22A8's independent fixture assertion was corrected from ordinal leaf IDs to
packed ranges: BD2168 exports `((index_pointer - index_base) << 2) & 0xfffffff0`
plus `(count - 1) & 15`. Single-index leaves at byte offsets 0 and 4 therefore
encode as 0 and 16. All three original-upper/shared-lower cases now pass,
including Full72, RAM, host CSR, callback traces and ownership. Concrete
bounds/split/strategy callbacks remain borrowed services, not full-target proof.
BD8BF8's three complete original-body cases were independently repeated and
passed here. The complete Clang 19.1.7 Release static library builds. Both units
are recorded as selected-case validated with zero baseline mapping credit;
no runtime replacement or gameplay acceptance is implied.

Directly recovered `tree_box_centroid61` (BD8848/BD8888), the second-tree
vtable's axis and xyz box-centroid callbacks. Three complete original-body
comparisons pass Full72/RAM/host CSR, including output overlapping the input
box. All six input loads precede writes; separate binary32 arithmetic stages
and borrowed-buffer ownership are explicit. The complete library builds.

Directly recovered `tree_box_bounds61` (BDDE70/BD8EE0): merge two min/max
boxes and reduce an indexed box selection for the second-tree bounds callback.
Four genuine composed-original cases pass Full72/RAM/host CSR and independent
finite results. The lower merge is complete, with no algorithm stub. Empty
selection preserves output; merge reads before writes, while the first-box
copy retains original coordinate-by-coordinate order. Complete library builds.

Directly recovered `tree_flatten36_61` (BDBC18), the strategy-binding lower
that flattens 40-byte source tree nodes into 36-byte center/extent records with
explicit depth-first child indices, leaf tags and descendant counts. Three
recursive original-body cases pass Full72/RAM/host CSR and independent layout
assertions; the complete library builds. All storage is borrowed; no allocation,
algorithm callback stub or new failure guard is introduced.

Directly recovered `tree_flat_strategy61` (BDBD90), binding a borrowed tree to
owned count-prefixed flat-node storage. Count changes free/reallocate; matching
counts reuse. Three actual binder+recursive-lower cases pass Full72/RAM/host CSR,
mutable allocator callback traces, ownership and independent flat layout. No
failure rollback is invented. Complete library builds; failed-allocation and
overflow branches remain untested.

Next: recover BB3B60/BB3B88 concrete split policies, then integrate the concrete
callbacks without confusing the earlier borrowed-service upper check with a
fully closed mesh build. BD5910's host FP exception
flag difference remains an independent unresolved draft.

## Independent corpus continuation — 2026-10-09

Prior published checkpoint: `a5e492aa40f5947f07f5346c032e0ece44607ec8`.
`projection_extrema61` recovers BB34F8 (125 instructions) as explicit point
projection and strict min/max index selection. Three genuine complete-body
cases pass Full72/RAM/host CSR, including four-point unrolling plus tail and
first-index ties. Full library build passes. Original byte index truncation and
FP stage ordering are retained; nonfinite/large-index behavior is untested.
This is a zero-credit implementation pending historical catalog membership.
`owned_tree_refit61` recovers BDA248/BDA5F0 (330 instructions) as a shared
node-bounds update plus full reverse traversal or dirty-bit traversal. Three
complete original-body Full72/RAM/host CSR cases and library build pass, covering
empty/multiple-item leaves, paired children and cross-word dirty bits. Guest
arrays/nodes remain borrowed; no guest lower or allocation is introduced.
Finite bounds and valid indices are the selected scope; nonfinite/invalid input
and runtime remain unverified.
`cube_projection_table61` adds BB3430/BB38A0 (50+176 instructions): allocate
consumer index tables, sample and normalize directions over six cube faces,
and invoke the concrete projection slots. Three resolutions (0/2/3) pass original
sampler/preparer versus recovered logic with shared accepted extrema, comparing
Full72/RAM/callbacks/directions/host CSR. Library build passes. A zero-resolution
callback r25 placement discrepancy was corrected at the actual branch boundary.
Vtable 820D6280 slots +4/+8/+12 were confirmed as BB3430/BB34F8/822D3068;
private bytes are excluded. Resolution1, nonfinite and allocation failure remain
untested; original guest ownership and failure behavior are retained.

`curve_tangent_update61` recovers 8262B498 (94 instructions): a readable
control-point loop updates or retains tangents according to modes and tension.
Three complete-body Full72/RAM/host CSR cases pass, including mixed modes and
independent finite-result assertions. Pointer/count reloads and FP stages remain
explicit. No guest lower, allocation or ownership transfer is introduced.

`owned_tree_visit61` recovers BDA0A8/BDA1A0 (103 instructions): depth-first
visiting/pruning, and a distinct both-child-callback traversal with right-tail
iteration. Three genuine recursive-body Full72/RAM/callback/host CSR cases pass,
including callback-driven child rewiring. A leaf return r3 overwrite was fixed
at its actual branch boundary. Visitors are mutable guest service boundaries;
acyclic valid child pairs are the selected contract. Both libraries build.

`mesh_attribute_reorder61` recovers BB3CF8 (272 instructions): gather packed
coordinates and optional halfword/ID/category columns by index, free each old
buffer, reload the live owner and install the replacement allocation. Three
original-body cases pass Full72/RAM/allocation/free traces and ownership checks,
including repeated nonidentity indices and byte/halfword category layouts.
Library build passes. Count reload and zero-count scratch ordering after allocator
callbacks were corrected in review. Host CSR is not independently compared by
this integer-copy fixture; allocation failures/invalid indices remain untested.

`controlled_random_triplet61` recovers 8261E470/82620F40 (252 instructions)
with a shared explicit core parameterized by flag offset/bit layout. It updates
the global LCG seed and selects zero, one-sided or two-sided components, reloading
flags after each output store. Four complete-body Full72/RAM/host CSR cases pass,
including negative seed and output/flag aliasing; independent seed/components
are checked. Library build and review pass. No statistical or concurrency claim
is made, and these entries retain zero catalog credit.

`indexed_record_retire61` recovers 822CBA60 (187 instructions): reverse-scan
active halfword indices, retire over-threshold records by swapping the final
index into the removed slot, decrement count and clear the exact five-float
record region. Three original-body Full72/RAM/host CSR cases pass with independent
index/count/clearing assertions, spanning the four-item loop and tail. Library
build/review pass. Storage is borrowed; no allocator or broad guards are added.

`owned_tree_plane_query61` recovers BDA8B8 (99 instructions): reject against
active planes, clear fully-contained plane bits, aggregate whole-node indices
when the mask becomes zero, or recurse and visit intersecting leaves. Three
genuine recursive-body Full72/RAM/callback/host CSR cases and library build pass.
The existing guest frame+80 scratch is explicitly seeded and retained; no extra
argument is invented. Valid child pairs and finite small plane masks are the
selected scope. Caller F46DB0 is observed but not recovered in this unit.

`record_snapshot_gather61` recovers BD1900 (105 instructions): optional mutable
pre-callback, temporary owned snapshot of packed 12-byte records, permutation
gather back into borrowed storage, and temporary release. Three actual-upper
cases with shared complete allocator lookup pass Full72/RAM/callback and lifetime
checks (repeat gather/rejected callback/count mismatch), plus library build.
Host CSR is not independently compared; allocation failure and invalid indices
are untested. Next substantive parent BD22A8 additionally needs BD20F0 and its
BDB1C0/BDB208 visitor adapters; it is not yet claimed recovered.

`indexed_record_accumulate61` recovers 822CD290 (225 instructions): reverse
indexed record traversal with skip flags, scaled three-component accumulation
into two field groups, and a source reload between groups preserving aliases.
Three complete-body Full72/RAM/host CSR cases and library build pass, including
unrolled/tail paths and source/output overlap. Storage stays borrowed; no guest
lower or allocation is introduced. Finite valid records are the tested scope.

BD22A8 upper recovery is now in progress independently. Besides BD20F0 and the
two traversal adapters, its concrete callback targets BD1B50/BD2168/BD1B78 also
need complete implementations (last one calls accepted growth BD2870). These
are being recovered together; none is replaced with a fixture algorithm result.

`tree_triangle_bounds61` recovers actual first-table targets BD88E8/BD8AC0/
BD8B40 (196 instructions): indexed triangle bounds, axis centroid and xyz
centroid. Four complete-body Full72/RAM/host CSR cases with independent finite
outputs and library build pass. These match table 820D6C54 slots +4/+12/+16,
not scripted bounds results. BD8BF8 and the other concrete table/strategy targets
remain separate work; this does not yet close the full tree-building callback set.

`owned_tree_reorder_support61` recovers six required BD22A8 support entries
(BDB1C0/BDB208/BD20F0/BD1B50/BD2168/BD1B78, 147 instructions): traversal
adapters, depth/leaf count, packed leaf export, growable leaf-address append and
owned cleanup. Four actual-upper cases pass Full72/RAM/callback/host CSR using
genuine pinned fixed callbacks and shared accepted traversal/growth/release
lowers. Library build passes. Review corrected export truncation/scratch, compare
direction and conditional pointer clearing. BD22A8 upper remains pending its
own comparison; no concrete bounds/split strategy closure is claimed here.

Bounded private-image investigation establishes vtables at 820D58A0 (installed
by B9CC00) and 820D5C58 (installed by B9E220/B9E2D8/B9E388). Their slots +0C,
+14 and +18 point to B9DD90, B9DF18 and B9DFA0 respectively. B9CBC0 occurs at
pointer slot 820D5990, but its actual table base/installer remains unproven.
No receiver-typed call-site was established; these facts do not establish
reachability. No private image bytes or original bodies are published.

Manager candidate 823262B8 remains blocked on actual recursive registration:
8256F6B8 is a typed external getter boundary, not a complete implementation.
Its lower 823FFDD8 reaches open 8229C948/823FF338/82400620 subtrees. Existing
mapped typed interfaces cannot be promoted to complete mutable Full ABI by
assumption. `manager_cached_links61` now independently recovers 824002F0 (97 instructions):
two dirty phases invalidate cached links and unlink reciprocal owner/index
references while retaining bit58-marked targets. Three genuine-body Full72/RAM
cases and the library build pass. The integer-only fixture does not independently
compare host CSR. This leaf does not close the recursive manager parent.
`metadata_descriptor_construct61` also recovers complete 8240CC58/82410A28
(55+90 instructions): base/derived metadata fields and pending-list registration.
Three full original-body Full72/RAM cases and library build pass; stack arguments,
64-bit flags and preserved field gaps are explicit. Actual host CSR is not
independently compared by this integer-only fixture. Registration callbacks and
the remaining recursive manager subtrees are still outside these constructors.

## Latest workspace checkpoint — 2026-10-09

Last verified published checkpoint: `d1b014ffd8b11bedc902b4ed03c1e967f6d50f14`.
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
A targeted search found no static direct callers of B9CBC0 either. This chain
now needs concrete XEX vtable/registration/function-pointer xrefs before another
upper can be recovered; absence of direct calls does not establish unreachability.

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
