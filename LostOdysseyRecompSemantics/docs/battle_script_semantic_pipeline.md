# Battle script semantic recovery

Battle script storage initialization/release, per-frame integer tick updates and the timed-wait opcode are recovered. The original integer conversion still yields zero ticks at 120 Hz; this semantic layer preserves that behavior rather than silently incorporating a new timing fix. The diagnostic/step boundary retains LR 8238AD64 and f31 so the existing fractional-tick hook can recognize its caller. Synthetic checks cover lifecycle ownership, first-frame gating, wait transitions and actor update selection. Parameter evaluation, diagnostics and actor sub-updates remain guest services; no full interpreter or gameplay acceptance is claimed.

## Parameter and action helpers

Parameter evaluation now resolves the original local, global, script, bit, constant and actor-field operand classes. Periodic action thresholds and queued actor flag changes are recovered and composed into the existing update/wait logic. Synthetic parameter, threshold/sentinel and queued-flag samples pass, as does the updated lifecycle/wait fixture. Actor lookup and action execution remain guest boundaries; diagnostics and the full interpreter remain unrecovered.

## Event queue and dispatch

Event insertion, completion-triggered events, sixteen-slot priority scheduling and the opcode dispatch loop are now recovered and composed into battle updates. The dispatch loop executes the recovered wait opcode locally, with other handlers delegated through the guest opcode table. A small synthetic program runs a callback opcode, initializes and resumes a wait over multiple frames, then exits; insertion, duplicate suppression and priority tie behavior also pass. This connects dispatch infrastructure, not every game opcode, and does not claim gameplay acceptance.

## Core opcode handlers

The basic opcode range 0..19 now resolves to recovered handlers: end, signed jump, conditional branch, assignment, wait, boolean set/clear, arithmetic, indexed bit changes, increment/decrement, bitwise operations, shifts and random assignment. The writable operand helper preserves original local/script bounds and read-only classes. A small assignment/add/conditional/wait/end program runs entirely through recovered handlers, apart from runtime services; random generation remains a guest boundary. Constructor registration addresses were checked with its spilled address bases, not guessed from proximity. This is core opcode coverage, not the full instruction set.

## Manager registration

The battle manager constructor now installs its guest vtable and 272 handler pointer slots while clearing only the original actor/script fields and byte flag. Its address setup was resolved with the original stack-spilled bases. The synthetic core program now uses this constructor-installed table successfully. Registration counts as one recovered constructor; external pointer targets remain runtime boundaries until individually recovered.

## Subroutine and arithmetic extension

The opcode loop now also composes multiply, signed divide, operand swap and subroutine call/return. Call pushes the next instruction offset onto the original 32-entry actor stack; return pops it or ends the event when empty. A constructor-registered call/multiply/return/end program passes, alongside division/swap and stack-full stopping. These are original logical semantics, not a hardened VM or complete instruction-set claim.

## Fixed-turn angle opcodes

The fixed-turn sine/cosine and direction-angle opcodes now compose the previously recovered guest-table polynomial and rational atan2 routines. The trig evaluator is shared with the existing hull math implementation without changing its formula. Private-constant fixtures check scaled outputs, zero/axes, cursor/stack preservation and shared-helper equivalence. No constant bytes are published, and this does not extend validation to exceptional FP or gameplay. Basic opcode slots 0..27 are now backed by recovered handlers.

## Cross-actor event transitions

Cross-actor event opcodes now resolve actor IDs (including the current-actor sentinel), enqueue events, wait for event start or completion, and stop on priority barriers or full queues. Their original per-slot phase transitions are preserved. A two-actor synthetic fixture exercises these transitions and the prior subroutine sample still passes. Basic opcode slots 0..31 are backed by recovered handlers, while later game-specific instructions remain pending.

## Actor control and counter extension

Opcode slots 32..37 now cover actor flag synchronization, range and mask branches, tick accumulation, signed remainder and the original upper-capped game counter. The actor handler preserves its asymmetric flag rules: actor state uses the input low bit, while the resource flag uses nonzero. Small synthetic checks pass, including the original lack of a lower counter clamp. Actor lookup/effect services remain guest boundaries; no native battle acceptance is claimed.

## Party and inventory handlers

Party and inventory handlers now update the original flag/list fields, transfer five party slots, accumulate capped inventory quantities and notify the play-data service. Recovery preserves two source quirks: list insertion may fill multiple vacancies, and the scratch-write comparison admits index 16. Reserved aliases retain their true no-op behavior without invented cursor advancement. The inventory wrapper uses its own nested frame so its temporary does not overwrite the caller saved register. Focused synthetic ownership-free state and callback checks pass; native inventory behavior and invalid input safety are not claimed.

## Text token expansion

Battle text expansion now composes script scratch numbers, actor names and seven-column text-bank names into the message opcode. Literal units retain the original byte order, while inserted numbers and names follow their source representations. The formatter preserves consumed-but-unused tokens and uses capacity only for initial clearing, as the source does. A small synthetic mixed-token program and message callback pass with full library compilation; actual localized text rendering and native gameplay remain unvalidated.

## Actor state and service handlers

Actor numeric state handlers now read four float fields and set/add two fields, preserving truncation and missing-resource defaults. Additional opcodes write an indexed play-data table, marshal integer/scaled-float arguments to eight original virtual slots, capture numeric results, and query a chained global service. One compact synthetic fixture checks the handler contracts and stack/cursor preservation. These service calls remain explicit boundaries; their concrete implementations and native combat effects are not claimed as recovered.

## Extended dispatch and operand aliases

Extended opcode dispatch now sets the original mode bit and tail-routes the second byte through slots 256 onward, reusing recovered handlers and explicit guest fallback. Zero-offset operand aliases, signed little-endian immediates, and two original fixed-length skip handlers are recovered. A constructor-registered prefix/skip/end sample preserves the caller LR and clears the mode bit on the next ordinary instruction. Full library compilation and the focused synthetic sample pass.

## Scene command handlers

Scene-service handlers now perform predicate branches, command dispatch, packed XYZ argument construction, result writeback and numeric parameter calls. The inline 32-unit text payload retains original byte order and expands its flag byte before invoking the presentation service. Original invalid mode behavior is preserved, including no cursor movement in the seven-way predicate. A compact synthetic callback fixture passes; concrete scene/presentation services remain explicit guest boundaries.

## Resource binding and battle state

Actor binding now distinguishes resource IDs from group IDs, removes an existing script binding when required, binds an available resource, and preserves the original stop behavior when no resource is present. Visibility updates retain their low-bit semantics and scene-specific notification exception. Battle transition, flag and state-query handlers are composed with the existing operand engine. A small synthetic fixture passes; runtime actor lists, controller callbacks and actual battle transitions remain guest services.

## Presentation mode dispatch

Three presentation handlers now marshal scalar floats, packed vectors/colors, integer commands and service state across their original mode tables. The command ranges preserve default selectors even for values between named modes. Numeric integer conversion retains the source float roundtrip. A compact table-driven fixture covers the dispatch families and packed ABI payloads; concrete presentation services and native visual effects remain outside this recovery stage.

## Action routing and resource properties

Action handlers now connect readiness predicates, target selection, preparation flags and event-specific action dispatch. Resource queries, indexed state resets, conditional counters and target effect commands compose with the recovered operand and actor lookup engine. Property-mask helpers read the original guest mapping table instead of assuming a new host mapping. A compact synthetic fixture passes; concrete action execution and effects remain explicit guest boundaries, so actual combat behavior is not yet validated.

## Action history and resource groups

Action-history commands now clear eight records, extract the newest record and search with the original descending/repeated-match semantics, including omission of slot zero. Additional handlers transfer scaled resource vectors, update manager/resource flags, clear linked resource groups and finish deferred actions. A compact synthetic state/callback fixture passes; actual battle history, native vector constants and completion effects remain unvalidated.

## Target pools and selection

The battle target-selection path now partitions runtime resources into the original six pool counts, constructs candidate groups, applies numeric/status/group/action/inventory filters and writes either the full target list or one random selection. Duplicate field matches and later equal-score replacements are preserved. A small four-resource fixture connects these stages through the real operand and property-mask helpers; runtime combat and every native filter combination remain unvalidated.

## Target-list refinement

The companion target-refinement opcode now filters an existing byte-ID list without rebuilding runtime pools. It shares the original common filter logic while preserving its distinct category bit28, secondary-stat percentage filter5, 752-byte frame and random-service tag85. The focused fixture now composes initial selection with refinement, checks two team categories, secondary-stat thresholds, random tag and empty-list handling. Concrete game services and native combat remain unvalidated.

## Resource and reverse action queries

Resource query handlers now count selected categories, trace pending action records back to actors targeting a selected resource, deduplicate their IDs and select through the original random service contract. Property/effect, target-byte, actor parameter and banked inventory queries are also recovered. The detail-only action-filter exclusion and special inventory field are preserved. A compact synthetic fixture passes; native action lists, readiness and inventory providers remain unvalidated guest services.

## Availability-filter target selection

The companion availability-filter selector now builds mode1 resource pools, retaining active resources even when HP is zero or a property mask would exclude them from the normal selector. It filters through the original resource virtual slot292 nonzero result and writes all selected IDs or one random choice with tag86. The existing compact fixture exercises both list and random paths with a zero-HP resource; concrete readiness semantics remain a guest service.

## Runtime command routing

Runtime commands now resolve group/resource identifiers, find script actors bound to runtime resources, count flagged bindings, and marshal scene commands through their original direct and virtual services. Target selection and scene predicates retain their original return conventions, including no cursor advance for unsupported predicate modes. The compact state/callback fixture passes; native scene and service behavior remains unvalidated.

## Scene state and inline payloads

Scene-state handlers now preserve inline text buffers and actor labels, dispatch object commands, queue flag changes and apply the original suppression/state predicates. Affordable action selection scans the twelve source candidates, retains cost-compatible entries and invokes the original random contract. A compact synthetic fixture checks these data and callback paths; native scene effects, concrete costs and text rendering remain unvalidated.

## Resource mode dispatch

Two multi-mode resource opcodes now update original flag/indexed fields, synchronize resource refresh data, query banked state, pass packed numeric scene arguments and select among up to four eligible same-side resources. The equipment-like slot update preserves its unconditional follow-on service call, including when the local slot list is full. A compact synthetic fixture passes; the original empty-candidate random call is retained without invented protection or runtime acceptance.

## Status and predicate dispatch

Fifteen further battle instructions now connect readiness checks, the 60-tick phase handshake, action-result consumption, indexed resource state, group clearing, actor activation and scene predicates. The source low-byte result and exact-one branch rules, signed jump targets and suppression bypass are preserved. One compact synthetic fixture passes; concrete scene services and native gameplay are not validated.

## Preparation and scene marshaling

Four battle instructions now route preparation modes and marshal scene label and boolean commands. The original manager-state fallback, failure result, packed constant numeric arguments and 32-unit label copy are preserved. A separate source loop repeatedly overwrites its first label unit; recovery intentionally preserves this behavior. One synthetic fixture passes. Concrete preparation and scene services and native gameplay remain unvalidated.

## Owned scene labels

Seven scene handlers now build the original owned UTF16 label descriptors, preserve allocation and copy call contracts, and dispatch mode-controlled and predicate commands. The source parameter/payload offset overlap is retained. A compact synthetic fixture passes empty and nonempty labels, allocation alignment, descriptor self pointers, mode flags and branching. Concrete string, allocator and scene services and native gameplay remain unvalidated.

## Queues and resource state

Fifteen additional battle handlers and the bulk resource-action reset helper now implement expected/actual queue construction and comparison, source duplicate insertions, resource field updates and flag transitions. Allocation sizes, signed jump targets and numeric argument conversion follow the source. One compact synthetic fixture passes; concrete external services and gameplay remain unvalidated.

## Scene and message commands

Ten remaining service-facing battle handlers now preserve scene command arguments, resource level refresh ordering, health and MP refill fields, four-way message routing and short-label glyph substitutions. Overlapping source operand offsets and zero-coordinate fallback remain intact. A compact synthetic fixture passes; concrete scene, resource refresh and message services and native gameplay remain unvalidated.

## Constructor handler coverage

The final two distinct handlers in the battle-script constructor table now have logical implementations: banked item-action selection and the 39-mode global transition instruction. Source tie-breaking, random tag88, zero-ID execution, first-group early exits and controller routing are preserved. Two small synthetic fixtures pass. Constructor handler coverage does not mean the engine service boundaries or native gameplay are complete.

## Action execution chain

Five action helpers are now implemented and composed into the existing action, preparation and item-selection opcodes. The chain emits first and subsequent target records, handles delimiters and busy indices, expands linked groups and consumes the correct inventory bank. A focused helper fixture and the three affected opcode fixtures pass. Concrete resource action builders and target preparation remain boundaries; native gameplay is unvalidated.

## Target preparation chain

The eighteen-mode target preparation helper now composes the recovered pool builder and is connected through action execution and the calling opcodes. It preserves property-based overrides, random tags, selected-list and group selection, the source mode16 fallthrough and empty-pool behavior. A focused fixture and three affected chain fixtures pass. Concrete resource services, pathological random paths and native gameplay remain unvalidated.

## Prioritized action pickers

Three action pickers and the 512-record learned-action eligibility scan now feed the recovered target-preparation and execution chain. Candidate priorities, exact-one skill gating, random tags and private table reads follow the source. The category flag selects preparation mode2 or17. A focused picker fixture and the updated marshaling chain fixture pass; concrete skill checks, kind lookup and native gameplay remain unvalidated.

## Skill cost and kind classification

Skill availability and action-kind classification now run inside the recovered picker chain. The source property-mask gate, category-specific cost table, signed kind thresholds and distinct unordered comparison behavior are retained. A compact helper fixture and the affected picker and marshaling fixtures pass. Dynamic cost adjustment remains a service boundary; native gameplay is unvalidated.

## Resource action records

Four lower-level resource action-record routines now have logical implementations. They write record metadata, append target IDs, preserve prior-record flags, insert linked members and deduplicate additional targets. A compact synthetic fixture passes. Array initialization, command configuration and finalization remain service boundaries; integration into the execution-layer emission calls is pending, with no native gameplay claim.

## Action-record chain integration

The recovered execution layer now calls the resource action-record implementations directly. The chain writes real record metadata and target lists, including linked-member flags and extra-target deduplication, instead of stopping at mocked emission callbacks. The four affected opcode/execution fixtures pass; their remaining synthetic record-array initialization is explicitly isolated. Guest allocation/reset, command configuration and finalization services are still unvalidated boundaries, and there is no native gameplay claim.

## Action effects and lifecycle

Four action-effect and lifecycle helpers now run inside resource record creation: command routing, reset sequencing, actor completion marking and the resource-state cycle. A focused fixture checks all command routes and state transitions; the five affected chain fixtures pass using actual completion fields. Record-array initialization and concrete effect services still require recovery, and native gameplay is unvalidated.

## Action storage

Four action-storage helpers now have logical implementations, including the 124208-byte record initializer and its nested defaults. A focused fixture verifies growth, repeated append, normal and alternate arrays, reset and ABI preservation. Existing ResizeArray logic is reused. String and destructor services remain boundaries; composition into the action execution chain is next. This is not native gameplay validation.

## Composed action storage

Recovered action storage is now composed into record creation and the reset-effect wrapper. Six caller fixtures pass through real growth, zeroing and nested record initialization instead of the former synthetic initializer. Narrow allocator, empty-string and record-destructor services remain fixture boundaries. Native gameplay and bitwise floating-point acceptance remain unvalidated.

## Action destruction

Action record destruction now follows both 32-slot sections and their 16-string reverse cleanup loops through recovered code. Storage reset composes this path. A focused fixture checks callback order, empty loops, cleanup routing and ABI; storage and execution caller fixtures pass. String destruction and the exceptional cleanup continuation remain guest boundaries; native unwinding and gameplay are not validated.

## String storage adapters

String storage now has full-register logical adapters for construction, reset and release, reusing the accepted array and manager implementations. A compact fixture exercises empty/nonempty strings and local/heap conversion buffers. The conversion helper and allocator remain service boundaries, and action-chain composition is next. This is not native runtime or bitwise equivalence validation.

## Composed string lifetime

Action creation and destruction now compose recovered string construction/reset/release. Empty record labels execute the constructor directly; record cleanup executes string release instead of synthetic destructor callbacks. The destruction fixture observes all 1024 actual manager releases in source order. Storage and six caller fixtures pass. Nonempty character conversion, allocator internals, exceptional cleanup continuation and native gameplay remain unvalidated boundaries.

## Reverse cleanup continuation

Recovered 82B7AE18 completes the reverse cleanup continuation from a captured end pointer. The focused destruction fixture now executes that loop and verifies callback order instead of mocking it. Native exception unwinding remains unvalidated.

## String conversion wrappers

String construction now composes byte-length and temporary-conversion wrappers, including local versus heap buffer selection and error routing. The long-string allocation request preserves the source four-times-count arithmetic. Two focused fixtures pass. Imported character conversion and UTF-8 decoding remain boundaries; native locale and gameplay behavior are unvalidated.

## String allocation adapter

Added the full-register 82486C88 allocation adapter by reusing AllocateManagerBuffer. Long-string conversion now reaches the manager virtual allocator through recovered code; both string conversion and storage fixtures pass.

## Table-driven UTF-8 decoding

Recovered the table-driven UTF-8 decoder 827CA660 and composed it into codepage dispatch. The focused fixture now checks actual ASCII decoding, supplementary-plane surrogate output, size-only queries, partial-capacity error behavior and incomplete input using synthetic guest tables. Imported non-UTF8 conversion remains a platform boundary; native locale and full-image table validation remain unvalidated.

## Action snapshots

Action snapshot preparation now has logical implementations for manager field updates, selective record copy and alternate-array backup. The focused fixture executes real storage and string lifetime and checks both preparation modes and flag preservation. Integration into execution callers is next; native gameplay and bitwise FP remain unvalidated.

## Composed action snapshots

Execution now composes alternate-record snapshot preparation. Four execution/opcode fixtures pass with the actual backup state and independent normal/alternate storage, including an assertion that readiness early-return still creates the alternate snapshot. The shared synthetic allocator now represents separate live allocations. Record/effect caller fixtures also pass; native gameplay remains unvalidated.

## Battle random ranges

Recovered the shared battle random-range helper, preserving resource-group and call-tag cursor selection and wrap behavior. One compact fixture checks synthetic table values and endpoint paths. Composition into action and picker callers is next; native random-table data and gameplay behavior are unvalidated.

## Action readiness

Recovered resource property lookup and action-unavailability predicates, including the distinct standard and extended property sets. A compact synthetic fixture checks all predicate branches and the resource flag fallback. Execution and target-picker integration is next; no native status-name or gameplay inference is made.

## Composed action readiness

Execution now calls the recovered readiness predicates. Four execution/opcode fixtures pass after replacing predicate mocks with actual resource flags, and marshaling failure checks now inspect the real actor busy count. Property lookup in target pickers remains a separate pending composition step. Logical validation only; native gameplay is unvalidated.

## Action parameters

Two concrete action parameter handlers now compose recovered resource properties, random-range selection and manager field updates. A compact fixture covers their normal/alternate and property-dependent branches. The field meanings remain neutral rather than inferred gameplay labels; effect-dispatcher integration is next.
