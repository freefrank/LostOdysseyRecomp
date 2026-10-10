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

## Multi-kind action parameters

Added 82B11DF0 parameter setup for skill, item and special action branches, composing shared random selection and manager state writes. The focused fixture covers table fields, 25-unit normalization, value-99 cases, alternate override, resource-group override and mutable adjustment callbacks. Three adjustment helpers remain explicit guest boundaries; top-level effect-dispatch composition and native gameplay are unvalidated.

## Action adjustments

Recovered action parameter percentage adjustments and composed them into multi-kind setup. Focused fixtures now exercise actual normalized fields, live-resource transitions, property-based reductions/boosts and zero-rate scaling. Linked-resource removal and virtual behavior remain external services; top-level action-effect integration is pending.

## Final action parameters

Recovered final action parameter application, including merging, property scaling, global/script overrides and final resource flags. The focused fixture preserves the source explicit 600/15000 arithmetic without assigning gameplay meaning. Top-level effect-chain composition is next; native gameplay and bitwise equivalence remain unvalidated.

## Composed final action-effect chain

Top-level action effects now compose all recovered parameter handlers, percentage adjustments, manager setter and final resource application. The effect fixture executes all 32 kinds plus the default route and checks real resource fields. Five record/execution/opcode caller fixtures pass after removal of parameter/finalizer mocks, using actual actor fields and random cursors for observations. Imported character conversion, heap virtuals and linked-resource removal still remain platform/service boundaries; native gameplay is unvalidated.

## Battle property removal

Recovered the property-removal query and tail alias, including clearing both associated payload fields. Linked action adjustment now executes real property removal; its fixture checks both resource and peer masks. The peer virtual predicate remains a service boundary, with no native gameplay claim.

## Target property composition

Target filtering and preparation now compose resource property lookup, mask indexing and the narrower target-unavailability predicate. Fixtures use actual property masks and payload indexes instead of mocked predicates; readiness, adjustment, preparation, target and marshaling checks pass. Random selection and preferred-target selection remain pending composition steps; native gameplay is unvalidated.

## Preferred battle targets

Recovered preferred target selection and composed it into preparation. The flow builds live resource pools, selects a side using the shared probability wrapper and draws through the recovered random cursor logic. Focused fixtures check the source branch behavior, including its empty-pool contract without added guards. Native gameplay remains unvalidated.

## Battle probability wrapper

Added the probability wrapper 82AA0838, preserving threshold >= a 0..99 draw. Preferred target selection composes it and the focused fixture verifies equality behavior and side-specific cursor tags.

## Composed selection random state

Preparation, target filtering/refinement, skill picking and item selection now compose the recovered random-range helper. Five focused fixtures pass using actual per-group/per-tag cursors and controlled synthetic table values instead of random-service stubs, including self-target retry and equal-range no-advance behavior. Four existing record/execution/effect fixtures also pass with the shared synthetic random table. Native random-table data, statistical behavior and gameplay remain unvalidated.

## Numeric skill cost adjustment

Recovered and composed the property-based numeric cost adjustment: property 236 subtracts a truncating signed quarter; property 97 then forces zero. The cost fixture exercises actual table values, discount/free paths and category-specific NaN comparisons. Picker and marshaling fixtures pass without cost-service mocks; the picker retry case changes MP through its existing mutable virtual boundary. Native gameplay is unvalidated.

## Action eligibility

Recovered the action-kind eligibility dispatcher, raw property-zero query and result wrapper. The fixture checks the source gate distinctions and actor/manager restrictions while retaining setup and evaluation as explicit service boundaries. Picker/action integration is next; no native gameplay claim is made.

## Eligibility configuration and dispatch

Recovered and composed evaluator configuration and descriptor-based tail dispatch. Category setup resolves skill, item, special and inventory descriptors, permits property-zero targets only for descriptors 7/18, and rejects inactive targets. The leaf dispatcher preserves stack depth and invokes the descriptor callback through CTR. Targeted fixture passes actual setup and all dispatch categories; parameter initializer 82B121B0 and descriptor callbacks remain guest boundaries. Logical/ABI recovery only; native gameplay remains unvalidated.

## Eligibility parameter initialization

Recovered and composed category-specific evaluator parameter initialization: skill/item/special/inventory rows, paired field copies, adjusted costs, preserved untouched fields and random element selection. The focused fixture covers categories 0 through 32, actual cost/property helpers and random selection. Eligibility configuration now has no setup or initialization mock; only descriptor-specific evaluation callbacks and manager access remain boundaries. Logical/ABI recovery only, with no Full72, bitwise FP or native gameplay acceptance.

## Composed action eligibility

Composed real action eligibility, category configuration, parameter initialization and descriptor dispatch into skill pickers and action availability. Picker, action and marshaling focused fixtures pass without AD0C10/setup/initializer service mocks; descriptor-specific evaluation remains an explicit callback boundary. This is logical composition, not full game/runtime acceptance.

## Descriptor eligibility gates

Recovered eleven descriptor eligibility callbacks and composed them into the evaluator dispatch. Focused checks cover constant leaves, virtual status, identity/property gates and paired query ordering. Lower property predicates and target virtual callbacks remain explicit dependencies. The eligibility fixture still passes; logical recovery only, with no full equivalence or native gameplay claim.

## Property bank operations

Recovered bank-mask query/removal, type-table-gated availability/insertion and gated whole-bank population count. Source scans only bits 0 through 30; insertion payload uses the first bit of the original mask. Presence removal retains the source red-zone ABI. Actual paired-presence evaluator now composes the recovered query. Property and evaluator fixtures pass targeted mutation/query, high-bit and payload cases; native gameplay remains unvalidated.

## Property payload operations

Recovered typed property payload comparison, maximum update, additive update and insertion. Source semantics retain the first original-mask bit for payload addressing, signed comparisons, last processed eligibility result and low-31-bit scan. The read-only adapter clears both mutation flags. Paired payload evaluators now use actual bank state; focused property and evaluator fixtures pass. Logical recovery only, not native gameplay acceptance.

## Status admission

Recovered status admission query and mutation with type-table gates, immunity masks, bank-zero special status interactions, bank-seven restrictions and actor/manager notification side effects. The read-only adapter preserves its bypass argument. Paired admission callbacks now compose actual property state; focused fixtures check restrictions, typed presence, special-status replacement, actor flags and notification ABI. Only the notification manager services remain external for this path. Logical/ABI recovery only; native gameplay remains unvalidated.

## Extended descriptor gates

Recovered four-property availability conjunction, three-bank population gate, virtual-target/property-15 exception and availability/presence fallback. These callbacks compose actual property helpers, including the source empty-secondary success behavior and skipped bank 255 rows. Focused gate and eligibility fixtures pass. All existing paired descriptor predicates now resolve to recovered property semantics; target virtual methods remain boundaries. No native gameplay or full-equivalence claim.

## Descriptor effect adapters

Recovered ten descriptor effect adapters and composed them into evaluator dispatch. Focused checks preserve mode-write timing, mutable global manager reload and secondary property/numeric callback arguments. The eligibility fixture still passes. Lower preparation/application services remain to be recovered; logical/ABI coverage does not imply runtime gameplay acceptance.

## Effect preparation

Recovered and composed effect preparation: source/target and argument bindings, byte/word resets, secondary manager snapshots, target HP capture and source actor flag extraction. All ten effect adapters now initialize actual state before the remaining application callback. Focused fixture checks setup output, descriptor16 classification, low-byte input truncation and preservation of adjacent bytes. No full floating-point equivalence or native gameplay claim.

## Property state transitions

Recovered payload/auxiliary property state admission and update, including exact option matching, immunity restrictions and bank-seven additive payload semantics. Added insertion/removal adapters and composed the source-property side effect in descriptor adapter B0D088. Focused property and effect fixtures pass real mutations without the property service mock. Low-31-bit scan and original-mask payload addressing are retained; no native gameplay or full-equivalence claim.

## Special descriptor gates

Recovered special target property admission with same-resource exclusion and opposing-side exception, plus category-specific manager restrictions. The special property check composes actual property state semantics; only the manager accessor and target virtual method remain service boundaries. Focused fixture covers category routes, manager flag, self-target exclusion and side exception. Logical/ABI recovery only; native gameplay remains unvalidated.

## Action record result flags

Recovered seven action-record result flag operations and the descriptor tail adapter that selects the global result manager. Focused checks cover exact row/index stores and preserved neighbors; descriptor dispatch composition and the eligibility fixture pass. No full-equivalence or native gameplay acceptance is claimed.

## Property-derived chance gates

Recovered numeric property lookup, target penalty and two chance gates using actual property/random helpers. Focused checks cover bonus subtraction, generic payload, override, rejection and threshold outcomes. These helpers are ready for upper descriptor composition; no full floating-point or gameplay equivalence is claimed.

## Target relation eligibility

Recovered source/target relation eligibility: unrestricted, same-side and opposing-side modes, optional property-zero exclusion, source property242 and explicit low-byte override. The focused fixture checks all side combinations and modes, rejection, bypass and property override with real property helpers. This helper supports subsequent effect descriptor composition; native gameplay remains unvalidated.

## Upper effect descriptors

Recovered four upper effect descriptors: chance-gated property insertion/removal, target status marking and two deactivation/report paths. Composed real source/target eligibility, chance gates, upper effect descriptors, property mutations and result-record flags. Focused effect and eligibility fixtures pass target flags, report codes250/15, scene-dependent chance and manager counter wrapping. Main effect application and final target notification remain explicit boundaries. Logical/ABI recovery only; full floating-point and native gameplay acceptance remain unvalidated.

## Initial effect calculation stages

Recovered six effect calculation stages and the status-result record setter. Targeted checks pass actual numeric property, random and result state paths, with attack/defense base services still explicit dependencies. The main effect application sequence is pending; no full floating-point or native gameplay acceptance is claimed.

## Attack and defense bases

Recovered and composed attack and defense base calculations using real numeric properties and guest-loaded floating constants. The initial amount stage no longer mocks either stat calculation. Focused checks cover nonnegative difference, negative defense stat adjustment, status-dependent floor and preserved nonvolatile FPRs. Fixtures use synthetic constants; bitwise floating-point, exception flags and native gameplay equivalence remain unvalidated.

## Amount modifiers and aggregation

Recovered side-dependent amount attenuation and final amount aggregation, including exemption gates, result-record marking, status overrides, critical multiplier, count scaling, optional random modifier, rounding and cancellation. Focused fixture checks attenuation/bypass and the aggregation stages with synthetic guest constants and real property helpers. No bitwise floating-point or native gameplay equivalence is claimed.

## Trait matching and critical selection

Recovered three-slot trait matching with wildcard filters, selected-trait state writes, effect-mode float interpolation and critical selection. Critical selection composes actual trait lookup, interpolation, target property scaling and deterministic random gates; skip paths preserve the existing output byte. Focused fixture checks wildcard/no-match state, interpolation modes and critical/skip paths. Synthetic constants only; native gameplay and bitwise floating-point equivalence remain unvalidated.

## Trait bonus calculations

Recovered category-two and category-eight trait bonus calculations using actual trait matching and interpolation. The focused fixture checks overlap gating, selected multiplier, target-specific extra bonus, result-record flag, skip behavior and FPR28-31 restoration. No trait service mock remains in these paths. Guest constants are synthetic in the fixture; full floating-point and native gameplay equivalence remain unvalidated.

## Elemental and target responses

Recovered elemental cancellation/weakness bonus, property-backed guard chance and ordered target response selection. Existing property, trait, interpolation and random helpers are composed. Focused fixture checks elemental permission/neutral gates, weakness multiplier, early-return state preservation, response modes0-5 and guard bypass. The external response-classifier and duel manager remain explicit service boundaries. Logical/ABI recovery only; native gameplay and bitwise floating-point equivalence remain unvalidated.

## Element response classifier

Recovered elemental response classification and its tail adapter. The classifier preserves source ordering across selected element bits and the shared property52 override, returning response codes6/7/8 for values0/1/2. Target response selection now uses actual classification instead of a service mock. Focused fixture checks codes, later-element overwrite and shared override. The duel manager remains a separate boundary; native gameplay remains unvalidated.

## Status consumption and return amount

Recovered status-counter consumption and return-amount recording, composed with property removal, source response classification and result flags. Focused fixture checks counter expiry, weakness rounding including odd amounts, lethal-damage cap, absorption/zero-response record fields and source result marking. Guest constants remain synthetic in tests; full floating-point and native gameplay equivalence are unvalidated.

## Main effect execution

Recovered and composed the main effect execution sequence and linked-target preparation. Real stat, hit, response, critical, element, bonus, aggregation, property and result-record stages execute together. Targeted hit/miss/guard/absorption/MP-diversion checks and existing descriptor/eligibility fixtures pass. Final resource application, result normalization and notification are still external dependencies; no native gameplay or full floating-point acceptance is claimed.

## Battle result application

Recovered result application from the battle effect pipeline into source and target HP/MP. Shield consumption, damage/heal normalization, survival-property removal and resource caps now execute through handwritten semantics. The execution fixture no longer mocks result application and verifies actual target HP/MP writes. Resource manager lookup, achievement counting, status insertion, additional effect handling and completion notification remain external services. Library build and focused application/execution/descriptor/eligibility checks pass; no gameplay, Full72 or bitwise floating-point acceptance is claimed.

## Status insertion

Recovered strict and permissive property insertion, including manager-gated low-HP state, duplicate suppression, immunity differences, property2/16 transitions and actor-aware death/property15 notification. HP normalization and the evaluator death callback now use real status mutation rather than mocking insertion. Focused checks verify actual flags, actor bits, payload initialization and notification arguments. The manager notification remains an external service; gameplay, Full72 and full ABI acceptance remain unvalidated.

## Result modes and damage cleanup

Recovered damage-triggered status cleanup and nine-way HP/MP result dispatch, including proportional HP adjustment, fixed/threshold HP caps, resource floors and critical-hit healing refunds. Actual effect execution no longer mocks post-damage cleanup, and the critical refund path is exercised through the real heal dispatcher. Focused checks cover resource writes/returns, excluded cleanup masks, wake-state reset and unchanged-HP behavior. Achievement counting, manager notifications and additional effects remain service boundaries. Logical recovery only, not native gameplay or full FP/ABI acceptance.

## Post-hit effect follow-ups

Recovered additional-effect application and orchestration after actual damage: status-mask insertion, HP/MP return, allied distribution, trait probability and pending actor effects. Main execution now composes real follow-up logic. Reward chance and report-label routing are recovered, with positive item grant relying on existing inventory semantics and remaining platform/UI services. Focused fixtures and library build pass; no native gameplay, Full72 or bitwise floating-point proof is claimed.

## Trait probability

Recovered trait probability scaling by battle mode and per-slot trait chance dispatch. Category3 details1/11,2/12,3/13 select thresholds30/60/100 and random tags58/59/60. Focused checks cover signed quarter scaling, full mode success and zero mode rejection; composed into post-hit follow-up orchestration.

## Theft and evaluator dispatch coverage

Recovered the theft evaluator and descriptor dispatch, source/target report labels, theft chance and inventory quantity removal. Focused checks cover relation rejection, missing loot, failed chance, actual inventory grant, already-looted state, equipment theft, inventory depletion and empty inventory, alongside stack/register restoration. The 97 statically recovered descriptor slots now have implementations for all 33 distinct callback targets; this is a dispatch-coverage milestone, not whole-game or runtime acceptance. Platform lookup, UI string assignment and equipment refresh remain external services.

## Death transition

Recovered actual death transition: actor event queue or pending completion flag, HP clearing, resource state reset and removal of properties164/242. Status insertion and HP normalization now compose this function instead of mocking manager notification. Targeted checks verify queued event, direct pending ID, actor flags and resource cleanup; the lower resource cleanup service remains external.

## Inventory table lookup

Recovered inventory table lookup behind the existing manager selector. The theft fixture now resolves inventory through the actual accessor and retains grant/removal/depletion checks. Global manager access remains an external service.

## Report string integration

Added guest-register adapters for the already recovered UTF-16 assignment and length semantics, reusing registered_metadata_string rather than duplicating its algorithms. Battle reward and source/target report labels now call real string assignment instead of a UI-copy mock. Focused checks cover copied text, alias no-op, empty assignment release, length and report-label integration. Allocation remains a guest service; no gameplay or full ABI acceptance is claimed.

## Battle progression

Recovered damage/defeat progression counters and achievement-service forwarding, including threshold crossing and defeat-record insertion/deduplication. Result application and actor death transition now compose real progression rather than mocking counters. Library build and focused progression/property/result/evaluator/execution checks pass. Platform achievement delivery and gameplay remain unvalidated; no Full72 or full ABI proof is claimed.

## Resource stat refresh

Recovered gear-derived property clearing, aggregate HP/MP and battle-stat calculation, and equipment refresh orchestration. Theft now runs this refresh after removing equipment. Focused fixtures cover caps/refill, low-HP state, stat sums, two-bank removal and refresh order. Skill recomputation AC0888 and equipment aggregation AC2468 remain external dependencies; no gameplay, Full72 or bitwise FP acceptance is claimed.

## Equipment contributions

Recovered six-slot equipment contribution aggregation, item HP percentage adjustment and integer rounding, accuracy and accessory effects, plus weapon trait reconstruction and category masks. Refresh now executes actual equipment aggregation; focused checks cover percentage reduction, special rounding, accessory slot state and three weapon trait categories. Skill recomputation AC0888 remains the main guest dependency, and runtime gameplay is still unvalidated.

## Inline utf-16 copy

Recovered the leaf forward UTF-16 copy used for equipment names, including terminator copy and source-register advance. Equipment refresh exercises it with synthetic empty names; existing string assignment fixtures remain passing. No runtime acceptance is claimed.

## Skill recomputation

Recovered AC0888 learned/equipment skill aggregation with equipment deduplication, cumulative bonuses, maxima/minimum and deferred property handling, immunity/category masks, rounded stat modifiers, and player skill capacity caps. Added AC8968 mutation adapter. Equipment refresh and theft now compose real skill recomputation instead of fixture callbacks. Focused resource-stat, theft and evaluator eligibility checks pass; library builds. This is logical ABI coverage, not runtime gameplay or bitwise floating-point acceptance.

## Actor result publication

Recovered actor result publication and its guarded target adapter, including pending/blocked flags, result payload and result-kind bitfield. Main effect execution now applies actual actor flags rather than a notification stub. Library and focused runtime/execution checks pass; no runtime gameplay or full ABI acceptance is claimed.

## Special damage

Recovered nine-way special damage mode selection, skill-point/empty-slot scaling, side-count advantage, manager multiplier and composition of the existing property-based fixed attack. Added actual allied/enemy active roster counters with virtual status predicates and battle end gates. Main execution now composes special damage selection; focused calculation and execution checks cover all nine modes and actual HP change. Library builds; logical coverage only, not gameplay or bitwise floating-point acceptance.

## Manager access

Recovered common battle manager accessors, resource-ID roster search and scene-object search, including lazy-root forwarding and fixed global manager selection. Main execution composes actual roster/list lookup; focused manager and execution fixtures pass. Lazy singleton construction remains a guest boundary; no gameplay or full ABI acceptance is claimed.

## Battle completion

Recovered battle completion predicates, tracked-ID lookup and subordinate state checks. Script conditional branches now compose actual completion logic. Focused fixtures cover rank bounds, optional task gates, pending/completed states, missing IDs and stale row removal routing; library and completion/runtime fixtures pass. Array erasure and platform profile lookup remain guest boundaries. Logical recovery only, not gameplay or full ABI acceptance.

## Array storage adapters

Added register-context adapters for previously recovered array resize and range removal, reusing allocation_array and memory_move semantics rather than duplicating those algorithms. Battle completion now performs actual stale-entry compaction and empty-array release. Library and focused storage/completion checks pass; allocator methods remain service boundaries. Logical coverage only, not full guest ABI or runtime acceptance.

## Profile lookup

Recovered platform object type-chain validation and player profile table lookup, including lazy type registration and missing/type-mismatched objects. Battle completion and script predicates now use the actual profile lookup instead of a mocked profile getter. Library and manager/completion/runtime checks pass; platform object delivery and type registration remain external services. Logical recovery only, not gameplay or full ABI acceptance.

## Root initialization

Recovered lazy battle-root initialization orchestration: temporary name conversion/cleanup, type lookup, manager initialization, object construction and cached-root publication. Root getter now calls this implementation. Focused checks cover cached success, missing type, failed construction and retry. String storage uses existing semantics; registry and object constructors remain external services. Logical recovery only, not native initialization or gameplay acceptance.

## Scene state

Recovered active scene-object state lookup and removal, current-scene name matching, UTF-16 comparison and low-property-mask target selection. Scene removal composes destructor dispatch plus actual pointer-array compaction; script scene/target handlers compose the recovered checks. Library and focused completion/scene/target fixtures pass. Destructors remain explicit virtual services; no gameplay or full ABI acceptance is claimed.

## Scene task lifecycle

Recovered tracked scene-task clear/reload orchestration and pointer-array append capacity growth. Script toggle handlers now compose actual task cleanup and profile-driven preset selection, including fallback names, two tracked task lists and descriptor state. Library and focused scene-task/runtime fixtures pass. Individual task factories/cancellation and folded name comparison remain service boundaries; this is script-flow recovery, not proof that an entire battle or game runs.

## Task cancellation

Recovered scene-task cancellation lookup, bulk row cleanup, paired membership lookup/removal and conditional task removal. Preset clear now executes these paths and real array compaction rather than mocking cancellation. Library and scene-task/runtime checks pass, including empty membership and clear-all release. Object destructors and final tracked-pointer removal remain lower service boundaries; no runtime acceptance is claimed.

## Task unlink

Recovered final tracked-object unlinking and queue-reference cancellation for task types0/1/4/5/6. Cleanup now removes matching references from both 44-byte event queues, respects the mode13 second-queue exemption, invokes the object destructor and compacts the pointer list. Focused scene-task checks pass actual paired membership, repeated reference removal and array release. Event payload destructors remain explicit services; no gameplay or full ABI acceptance is claimed.

## Event payload cleanup

Recovered event payload destruction and nested string-array cleanup, plus the capacity-reset tail adapter. Queue-reference removal now destroys payload names and nested strings through existing storage semantics instead of a destructor mock. Library and focused scene-task/storage fixtures pass with actual header clearing and allocation-service release counts. Reuses existing typed reset/array semantics; allocator callbacks and full volatile ABI remain outside acceptance.

## Scene row cleanup

Recovered large scene-row destruction, nested row-array reset/removal and virtual scene-handle release. Cancellation now composes real row cleanup instead of a destructor mock, reusing recovered memory fill and storage helpers. Focused scene-task checks pass actual buffer clearing, nested release, identifier reset and virtual removal arguments. Allocator and object-specific virtual methods remain explicit services; no native gameplay or full ABI acceptance is claimed.

## Scene row creation

Recovered secondary scene-row creation, zero/default initialization and activation forwarding. Profile preset loading now uses actual row allocation and identifier collision scanning, including 16-bit identifier wrap, rather than a row-factory mock. Library and focused scene-task checks pass creation, later cleanup and collision handling. Generic task activation remains an external boundary; inventory growth is not a game-completion percentage.

## Scene object factory

Recovered generic scene task reuse/create selection and object default initialization. Secondary row activation now composes duplicate reuse, parameter update, signed priority selection, wrapped ID search, pointer-list growth and actual manager allocation. Focused scene-task checks pass existing-object and new-object paths, constructor defaults and preserved padding. Path building and task-specific initialization remain services; no gameplay or full ABI acceptance is claimed.

## String concatenation

Recovered UTF-16 header copy/assignment, append, concatenation and shared array growth context entries. These compose existing memory copy and allocator-backed resize semantics while preserving terminators, self-assignment and empty-append behavior. Library and focused storage checks pass; allocator remains an explicit service boundary. These helpers support the next scene-path composition step; no runtime or full ABI acceptance is claimed.

## Scene resource path

Recovered scene resource-path construction for six type-specific platform roots and direct-copy fallback. Generic scene task creation now uses actual UTF-16 concatenation, header assignment, temporary release and final copy instead of a path-building mock. Focused scene-task checks cover types11/13/14/15/16/17 and type12 fallback with synthetic guest strings; library passes. Platform root lookup and task-specific initialization remain service boundaries; no private strings or runtime acceptance are included.

## Task initialization

Recovered scene task reset and task-specific initialization, paired byte/word membership append and bounded UTF-16 comparison. Factory creation now composes actual member storage, full/relative resource names, prefix handling, state flags and owned-resource cleanup rather than an initialization mock. Library and focused scene-task/storage checks pass creation, reinitialization, relative-name trimming and owned/raw cleanup. Platform object release callbacks remain services; no native gameplay or full ABI acceptance is claimed.

## Tracked scene factory

Recovered tracked scene-task lookup/create, constructor, membership extension, initialization and reference reset. Profile preset loading now creates actual named tracked objects and reuses matching names without duplicate membership. Focused checks cover real allocation/name storage, duplicate reuse, cancellation composition, slot-cache invalidation and scene-object reference clearing. Library passes. Remaining platform release and folded-name comparison calls stay explicit boundaries; this is logical coverage, not gameplay acceptance.

## Script action boundaries

Recovered script-specific property insertion ACA1B8, roster defaults AC3118, pending-action reset ACD530 and AB0B10, manager mode B48, and composition with the existing cyclic resource state B1F1D0. Script actions now compose these implementations and real property removal instead of guest fixtures. Four focused property, resource-stat, script-action and manager-access checks pass; library builds. Coverage remains logical ABI coverage, not gameplay, Full72 or complete floating-point/volatile-register acceptance.

## Group gauge

Recovered group gauge eligibility, roster HP aggregation and initialization, current-value clamping, ratio/rank thresholds, resource snapshot synchronization, two-group refresh and disabling. Binding and global script modes now compose these seven functions and existing inventory lookup. Focused gauge, binding and global-mode fixtures pass; library builds. Synthetic fixtures establish logical ABI behavior only, not gameplay, Full72, bitwise floating-point or full volatile-register equivalence.

## Resource growth

Recovered character template-stat loading, level-dependent stat curves and rounding/clamping, and layered creature initialization with archetype selection, equipment/traits and initial skills. The script level-change command now composes real growth, equipment stat refresh and HP/MP recomputation. Focused synthetic growth and command integration checks pass; library builds. Private coefficient tables remain external guest data. This is logical coverage, not gameplay or bitwise floating-point/full volatile ABI acceptance.

## Facing sectors

Recovered facing-sector classification A9B458 using actual scene lookup D40, vector-to-guest-angle conversion 323488 and guest floor semantics 2B94C8. Reuses the existing guest-table atan2 implementation; no host atan2 substitution. Hit evaluation and queued actor-flag application compose the recovered paths. Library builds and focused calculation, parameters and manager fixtures pass, including four facing sectors and absent-object handling. Private atan tables stay local. This remains logical coverage, not runtime gameplay or full floating-point/volatile ABI acceptance.

## Active scene handles

Recovered active scene-handle selection B63828, status probe B19CC0, cached parameter update B19C00 and conditional immediate/timed stop B1A048. Scene script commands now compose these paths. Backend handle enumeration, status and submission remain service boundaries. Library builds; focused scene-handle lifecycle, scene commands and existing task fixtures pass. Logical ABI coverage only, not runtime playback/gameplay or complete volatile-register acceptance.

## Scene request factories

Recovered scene request key classification, default parameter blocks, tracked-parent lookup, 100-byte task construction and initialization, object attachment, collision-free IDs, duplicate suppression and failed-initialization cleanup. Script scene requests and periodic parameter events now compose the factories. Platform metadata lookup, immediate handle release and packed-vector conversion remain service boundaries. Library and focused request-factory, scene-command and periodic-parameter fixtures pass. Logical ABI coverage only; no runtime playback/gameplay or complete volatile-register acceptance.

## Positional scene requests

Added positional scene request factory B1AFE8 and packed XYZ assignment B356B0, composing the existing request constructor, parent selection and duplicate suppression. Scene script packed-coordinate requests now write actual task coordinates instead of using callback fixtures. Library and focused scene-request and scene-command checks pass. Packed half-vector conversion remains a platform boundary; logical ABI coverage only, not runtime playback or complete volatile-register acceptance.

## Packed scene positions

Recovered three-component guest D3D half packing 377168 and composed it into spatial scene requests. Preserves truncating mantissas, signed zero, subnormal shifts and the guest overflow/NaN sentinel 0x7fff, with guest flush-mode transitions. Replaces the last packed-position fixture boundary. Library and focused request-factory and scene-command checks pass; synthetic checks include XYZ, negative values, signed zero and overflow. Logical coverage only, not full VMX/volatile-register or runtime playback acceptance.

## Battle graph composition entry

Added an opt-in battle semantic runtime entry that routes all 511 currently recovered battle entries across 64 units, including nested direct calls and known indirect targets. Unknown services still forward to the supplied guest implementation. The focused composition fixture crosses script completion into real manager/resource lookup, routes a known virtual target, and verifies unknown direct/indirect forwarding. This connects recovered logic without claiming game-runtime integration or gameplay acceptance; inventory entry counts are unchanged.

Call `battle_semantic_runtime61::Apply(entry, memory, dependencies, registers)` to enter this opt-in graph. It retains one bridge for nested calls. Recovered direct and indirect targets route to handwritten implementations; unknown targets retain the original guest callback and all mutable register state. An unknown top-level entry returns false without invoking a callback. Existing narrow unit APIs are unchanged.

After adding or changing battle recovery metadata, run `python tools/generate_battle_semantic_routes.py` from the semantics project (or pass its repository-relative path). The generator reads only recovery drafts, rejects duplicate entry ownership and rebuilds the checked-in route include. The route count is integration coverage, not new recovered-function credit or a game completion percentage. This API is not yet installed as a native game-runtime hook.

## Shared support routing

Extended the composed battle runtime with existing manager-release, string-storage and string-conversion semantics, plus accepted full-context copy and fill support. The graph now routes 511 battle entries and 29 shared/support entries. Focused composition checks pass UTF-16 length and actual copy/fill memory effects without guest escapes. No new recovery credit is claimed and native game-runtime hooking remains pending.

The route generator also reads the three selected compatible shared-context metadata files. Copy/fill retain their existing accepted implementations and do not add catalog entries.

## Record group rebuilding

Recovered AFD2F0 linked-resource record rebuilding: owner IDs, cleared companion slots, ordered group membership and unordered peer groups across every record. Added AAB870 paired mode-bit update. Global script modes now compose both actual implementations. Focused group-record and global-mode checks pass, preserving the source single-pass order rather than sorting members. Library builds; logical ABI coverage only, not gameplay acceptance.

## Formation placement

Recovered formation placement: side/class roster counting, matching formation row, ordinal slot lookup, player-profile rotation/translation using existing guest trig, and resource position/angle writes. Script global mode 12 composes the actual formation entry. Focused checks cover mixed sides and slot classes, missing sentinels, translated/rotated coordinates and preserved nonvolatile state. No host trig substitution, raw constants, gameplay acceptance or full ABI/FP acceptance.

## Roster persistence

Recovered roster persistence and group availability: refresh party resources through their virtual update, copy seven actual state spans back to profile rows, preserve inverse class flag, persist shared group ranges subject to script-mode suppression, and rebuild availability tiers with source short-circuit rules. Focused synthetic checks pass all copy spans, party filtering, count tiers and suppression. Uses accepted copy semantics without new raw data. Logical ABI coverage only, not gameplay or full ABI acceptance.

## Profile restore and resource baseline

Recovered profile-to-resource restore ABFC50 with actual seven-span copy and passive-state reset, plus ABFE38 baseline counters/level limits and ABFE90 active-state/group flag reset. Focused roster fixture now checks save/restore data, the deliberate passive reset, 32 initial limits and all group-dependent mask cases. Library and focused logic checks pass; no gameplay/full ABI acceptance is claimed.

## Resource creation

Recovered resource reset and battle resource factory. The factory composes actual action-record initialization, profile restore or creature growth, skills/equipment recalculation, position/angle writes and active-state reset before roster append. Class lookup/load and object allocation remain explicit external services. Focused fixture covers failed class load, party and creature paths, actual record setup, roster append and nonvolatile preservation with public synthetic data. Library builds; no gameplay, full ABI or bitwise FP acceptance.

## Party and encounter roster creation

Recovered party roster construction AF6290 and enabled encounter-row construction AF6448. Both compose the actual resource factory, profile/creature initialization and record setup. Party slots retain source slot IDs and compact selected group indexes; encounter rows preserve enable/class bytes, tags and per-resource marker, then call the existing layout service boundary AAC1E0. Focused roster fixture passes party and encounter paths and preserved nonvolatile state. Complex encounter layout remains a guest boundary; no gameplay acceptance.

## Group construction and party rebuild

Recovered two-group construction AF60D8, shared profile restore ABFDD8 and party rebuild wrapper AF63E0. Rebuild now composes real group creation, party resource population and formation selection. Preserved typed object services, list capacity growth and paired shared-profile copy ranges. Focused fixture checks two objects, array resize, all shared bytes and empty-party rebuild composition. Object allocation remains external; logical ABI coverage only, not gameplay acceptance.

## Battle initialization baselines

Recovered battle manager baseline setup, position defaults, typed root lookup, small manager constructor and stats workspace reset. Manager setup composes real profile casting, releases two existing list buffers, reconnects embedded list headers and establishes source defaults. Focused fixture checks type success/failure, release ABI, workspace slots, descriptor defaults and untouched padding. Public synthetic inputs only; library builds without gameplay or full ABI acceptance.

## Battle startup orchestration

Recovered battle startup AD20C0 and descriptor callback manager constructor AD1378. Startup allocates/registers ten managers and composes actual owner defaults, group/party rebuild, encounter population, both gauges, availability and stats reset. Constructor writes 167 callback slots with source hole 856 preserved; only 33 of its 83 distinct callback targets are currently recovered, so registration is not callback-completion credit. Focused empty-roster startup fixture passes manager allocations/registrations and composed state effects. Script startup and complex layout remain explicit service boundaries. No gameplay or full ABI acceptance.

## Script loading and initial dispatch

Recovered script loading and initial execution: asset lookup, per-capacity actor buffers/defaults, little-endian header/events/constants decode, bytecode allocation/copy, source failure release, profile-based startup and initial event dispatch. Reuses actual script-state allocation, integer decoding and opcode dispatch. Battle startup now composes actual script startup entries. Focused fixture covers loaded and spare actors, constants and code bytes, initial cursor writeback, missing assets, zero-code/capacity failure and no-script profile shortcut. Complex encounter layout remains external; no gameplay/full ABI acceptance.

## Group gauge mutations

Recovered group gauge mutation AC7000/AC7178/AC71E8/AC80B8: property-adjusted depletion, capped addition, max-HP division and roster-share application. Preserves source behavior that applies each eligible share to the original resource, rather than silently changing it to each iterated peer. Focused checks cover caps, property multipliers, divide mode, repeated original-target reduction and actual removed-amount accounting. Logical ABI checks pass; no gameplay/full ABI or FP acceptance.

## Descriptor effect mutations

Recovered nine descriptor effect callbacks: indexed dispatch and nested eligibility, property insert/remove and numeric payload, chance-gated source flag, fixed/quarter HP caps with recorded deltas, and percentage gauge addition/reset. Composes actual eligibility/chance/property/result/gauge semantics. Focused fixture passes dispatch limits, side rejection, property changes/truncation, failed-chance flag order, both HP caps and gauge effects. Descriptor constructor now has 42 of83 distinct callback targets recovered; remaining41 are not claimed complete. No gameplay/full ABI acceptance.

## Descriptor property effects

Recovered five additional descriptor property effects: dual-side gates, paired payload insertion/removal, mutually exclusive categories, bounded random payload with source attribution, and selected property-family clearing. Focused checks cover actual paired values and masks, source ID retention, family clearing and opposite side-gate failures. Descriptor callback coverage now47 of83 distinct targets; remaining36 unclaimed. Logical ABI checks only, not gameplay acceptance.

## Effect magnitude scaling

Recovered shared effect scaling, critical-property/chance decision, bounded magnitude jitter and final status/partner normalization. Uses guest-provided coefficients and source single-precision stages, with no embedded private constants. Focused synthetic checks cover stat scaling, target status multiplier, bypass, critical categories, jitter/zero handling, source/target status overrides, partner short-circuit and rounding. Logical ABI coverage only, not bitwise FP or gameplay acceptance.

## Restorative descriptor effects

Recovered four restorative descriptor callbacks using actual scaling, critical decision, jitter, normalization and result application: HP restore/full-HP sentinel, MP restore, combined HP/MP and HP-plus-property payload. Focused fixture checks restored values, result deltas, paired payload, full restore shortcut and side rejection. Descriptor callback coverage now51 of83 distinct targets; remaining32 unclaimed. No gameplay/full ABI or bitwise FP acceptance.

## Descriptor conversion effects

Recovered three more descriptor callbacks: fractional HP reduction with recorded delta, source HP-to-MP conversion, and randomized target class flags with fallback selection. Focused checks cover real result mutation, passive immunity, paired HP/MP outcomes and the source fallback quirk that treats the random index as a resource ID. Descriptor callback coverage now 54 of 83 distinct targets; remaining 29 unclaimed. Logical ABI checks only, not gameplay acceptance.

## Descriptor growth effect

Recovered descriptor growth-refresh callback B10368: script actor level override, source growth counter, creature stats refresh, equipment and derived stats rebuild, full HP restoration, target property attempt and primary mask index. Focused integration exercises the real lower-level growth/stat implementations with a synthetic template and an immune target. Descriptor callback coverage now 55 of 83 distinct targets; remaining 28 unclaimed. Logical ABI checks only, not gameplay acceptance.

## Descriptor cost effect

Recovered descriptor B13220 with eligibility and chance gates, the real scaling/critical/variance/normalization pipeline, HP result application and profile balance consumption. The spent total increases by the amount actually available when requested cost exceeds balance. Focused integration covers regular and balance-capped costs and the source-specific result-record offset. Descriptor callback coverage now 56 of 83 distinct targets; remaining 27 unclaimed. Logical ABI checks only, not gameplay acceptance.

## Descriptor gauge damage

Recovered B0DB98 manager-scaled damage with B0A0D0 side-gauge attenuation, status normalization, shield absorption, blocked-damage mode and HP/result writeback. The attenuation helper preserves class bypass, disabled gauges and source comparison behavior; enabled attenuation marks the result record. Focused synthetic checks exercise both helpers together, actual shield depletion, HP changes and result slots. Descriptor callback coverage now 57 of 83 distinct targets; remaining 26 unclaimed. Logical ABI checks only, not gameplay or bitwise FP acceptance.

## Descriptor cleansing effect

Recovered B0AA70 cleansing restoration: HP healing through real scaling and result application, optional MP restoration reusing the critical decision, followed by primary and optional secondary property-bank clearing. Focused integration checks HP/MP changes, both cleared masks, the zero-MP branch and preserved FPR31. Descriptor callback coverage now 58 of 83 distinct targets; remaining 25 unclaimed. Logical ABI checks only, not gameplay or bitwise FP acceptance.

## Descriptor random clearing

Recovered B0F920 random property removal and AC91E0 ordinal clearing. The effect counts four candidate banks using source mask-presence behavior, cancels its result marker if none qualify, selects a nonempty candidate and clears one set bit with both payload words. Preserves source exclusion of bit31 and source random retry semantics. Focused checks cover actual random selection, payload clearing and no-eligible-bank cancellation. Descriptor callback coverage now 59 of 83 distinct targets; remaining 24 unclaimed. Logical ABI checks only, not gameplay acceptance.

## Descriptor shared properties

Recovered B10E98 shared-party property assignment or random target category selection, with ACA710/ACA830 shared-bank mutation, ACA838 shared payload clearing and AC8988 direct category insertion. Preserves source early return for already-set shared flags and first-full-mask payload indexing. Focused integration covers actual shared group lookup, source category and payload assignment, target category replacement and duration resets. Descriptor callback coverage now 60 of 83 distinct targets; remaining 23 unclaimed. Logical ABI checks only, not gameplay acceptance.

## Descriptor HP MP transfer

Recovered B0AD38 HP/MP transfer from source to target. Preserves same-ID and zero-available skips, truncated integer ratio with whole-available fallback, target restoration before source depletion, paired result slots and final effect mark. Focused integration covers both HP and MP transfers through the real result pipeline and same-ID suppression. Descriptor callback coverage now 61 of 83 distinct targets; remaining 22 unclaimed. Logical ABI checks only, not gameplay acceptance.

## Descriptor random insertion

Recovered B0E798 random single-bit primary property insertion plus optional secondary mask, eligibility/chance gating and the source single-use target flag. Preserves original-mask bookkeeping in owner196 and primary selected-bit index. Focused integration checks random primary choice, secondary application and repeated-use suppression. Descriptor callback coverage now 62 of 83 distinct targets; remaining 21 unclaimed. Logical ABI checks only, not gameplay acceptance.

## Descriptor property toggling

Recovered B0FBD0 dual property toggling and AC9548 masked flag toggles. Existing flags are removed, while additions respect passive and temporary immunity masks. The effect preserves its asymmetric kind3/chance gate, passive-property7 exception, primary bit reporting and optional secondary bank. Focused checks cover toggling both banks, immune additions and passive gate suppression. Descriptor callback coverage now 63 of 83 distinct targets; remaining 20 unclaimed. Logical ABI checks only, not gameplay acceptance.

## Descriptor profile damage

Recovered B12A98 profile-aggregate damage: selects the actor statistics page, applies the encounter183-185 override, sums1024 counters with source-width arithmetic, and computes either direct or ratio-reduced damage before real gauge/status/shield/result processing. Focused integration covers encounter page selection, direct aggregate damage, ratio reduction and preserved FPR31. Descriptor callback coverage now 64 of 83 distinct targets; remaining 19 unclaimed. Logical ABI checks only, not gameplay or bitwise FP acceptance.

## Descriptor action rebuild

Recovered B0F3E0 paired-property effect with side-mode eligibility, single-use target flags, special immunity cancellation, queued-action rewriting and property-triggered stat reconstruction. Focused integration exercises action-record replacement, flag/counter resets, paired payload insertion, repeat suppression and the real growth/skills/equipment/derived-stats chain. Descriptor callback coverage now 65 of 83 distinct targets; remaining 18 unclaimed. Logical ABI checks only, not gameplay acceptance.

## Descriptor HP MP drain

Recovered B10AA8 HP/MP drain: source HP restoration precedes target shield absorption and damage; optional MP restoration/depletion reuses the critical decision, while the special mode only depletes target MP. Preserves property142 blocking, result slots and both saved floating registers. Focused integration checks source healing versus shielded target damage, paired MP changes and MP-only mode. Descriptor callback coverage now 66 of 83 distinct targets; remaining 17 unclaimed. Logical ABI checks only, not gameplay or bitwise FP acceptance.

## Shared damage helpers

Recovered seven shared damage helpers: attack magnitude and cap, target defense attenuation, category matchup amplification and result marking, secondary critical chance, additive bounded randomness, minimum status reduction and effective-level cap. Uses guest-provided coefficients, exact source-width arithmetic and staged single-precision calculations. Focused checks cover normal and special action coefficient paths, category variants, critical bypass, random range, damage reduction and level cap. Descriptor callback coverage remains66 of83; these helpers support remaining damage effects. Logical ABI checks only, not gameplay or bitwise FP acceptance.

## Descriptor physical damage

Recovered B0C4E8 physical damage through attack, defense, category, gauge, status, critical, variance and normalization helpers. Resolves blocked, healing, MP-first absorption, half/minimum and zero-damage modes, shield absorption, result slots and final target class marking. Focused integration covers normal damage, MP-to-HP spillover, half damage and damage-to-healing conversion; critical random fixtures preserve the original inclusive threshold behavior. Descriptor callback coverage now 67 of 83 distinct targets; remaining 16 unclaimed. Logical ABI checks only, not gameplay or bitwise FP acceptance.

## Descriptor missing HP damage

Recovered B0C9E0 missing-HP damage with conditional physical fallback. Source HP below the configured fraction uses max-minus-current HP; the healthy branch uses attack/category/status/critical/variance helpers and the original repeated normalization. Reuses source-faithful blocked/healing/MP-first/half/shield damage modes and reports owner172. Focused checks cover unconditional missing HP, healthy fallback and low-HP threshold selection. Descriptor callback coverage now 68 of 83 distinct targets; remaining 15 unclaimed. Logical ABI checks only, not gameplay or bitwise FP acceptance.

## Descriptor level damage

Recovered B0D7F0 level-bounded random damage and B0D418 repeated level-scaled attack damage. Both use real effective-level lookup, side gating and shared damage-mode resolution. The repeated-attack variant preserves individual floating additions, creature coefficient, gauge/status reduction and its no-variance normalization. Focused checks cover bounded RNG damage, side rejection, repeated attack sum and creature coefficient. Descriptor callback coverage now 70 of 83 distinct targets; remaining 13 unclaimed. Logical ABI checks only, not gameplay or bitwise FP acceptance.

## Descriptor damage followups

Recovered B0B178 physical damage with optional capped MP siphon and B0B630 damage followed by chance-gated property effects. The siphon caps against current target MP and writes both source/target results; the property variant preserves primary bank0 flags, bank7 paired payloads, secondary masks and owner bookkeeping. Shared damage resolution retains each callback stack layout and saved floating registers. Focused integration covers siphon on/off, paired-property application and primary/secondary mask reporting. Descriptor callback coverage now 72 of 83 distinct targets; remaining 11 unclaimed. Logical ABI checks only, not gameplay or bitwise FP acceptance.

## Descriptor extra MP damage

Recovered B0BA98 physical damage with randomized extra MP loss and property followups. Draws the extra MP amount before eligibility, applies it separately after normal HP damage, and combines it with damage in MP-first mode before HP spillover. Retains source stack layout, property payload/mask bookkeeping and saved floating registers. Focused checks cover separate HP/MP damage and combined MP-hit spillover. Descriptor callback coverage now 73 of 83 distinct targets; remaining 10 unclaimed. Logical ABI checks only, not gameplay or bitwise FP acceptance.

## Descriptor bounded damage properties

Recovered B0BFD0 bounded random damage with dual property values. Preserves side/bank and chance gating, direct sentinel damage before normal mode suppression, property-only mode, common damage-mode resolution and both property payload applications. Focused integration covers bounded damage, primary/secondary payloads, property-only suppression and sentinel priority. Descriptor callback coverage now 74 of 83 distinct targets; remaining 9 unclaimed. Logical ABI checks only, not gameplay or bitwise FP acceptance.

## Descriptor exhaustion and distributed damage

Recovered B0DEB0 exhaustion damage and B0E300 distributed-source-HP damage. Exhaustion skips category amplification, consumes the source gauge, sets its action flag and applies chance-gated properties. Distributed damage divides source HP by target count, applies gauge reduction, and preserves the special mode8 healing-plus-MP-siphon behavior. Focused checks cover damage, source gauge/flag changes, property bookkeeping, HP distribution and mode8 siphoning. Descriptor callback coverage now 76 of 83 distinct targets; remaining 7 unclaimed. Logical ABI checks only, not gameplay or bitwise FP acceptance.

## Descriptor priced physical damage

Recovered B12D08 source-action flag shortcut or priced physical damage. The full branch uses shared physical/critical/shield/MP resolution, optional MP siphon and source-width profile cost arithmetic capped by available balance. The shortcut marks the effect and sets the source flag without running damage or spending. Focused checks cover shortcut preservation and full damage/siphon/spending composition. Descriptor callback coverage now 77 of 83 distinct targets; remaining 6 unclaimed. Logical ABI checks only, not gameplay or bitwise FP acceptance.

## Descriptor property transformations and reports

Recovered B106B8 property-family conversion, duration masks and selected-target property assignment; AC8608 inserts, maximizes or accumulates property payloads. Recovered B11A20 temporary-immunity/category selection with real target-label binding and result reports. Focused checks cover family/value conversion, duration slots, random primary target, random immunity, empty-category no-mark behavior and category/class report selection. Descriptor callback coverage now 79 of 83 distinct targets; remaining 4 unclaimed. Logical ABI checks only, not gameplay acceptance.

## Descriptor revival and action reset

Recovered revival reset dependencies AC92B0, ACD3C0 and B0FF10, composing the existing AB31E0 action reset, plus B0FFF0 revival modes and B12870 paired HP/MP floor restoration. Reset preserves the designated bank-3 property payload, clears action timing, rebuilds stats/default actions and releases script flags. Focused checks cover real reset composition, activation-only and unconditional revival, paired floors, special full-HP report and same-actor action reset. Descriptor callback coverage now 81 of 83 distinct targets. Logical ABI checks only, not gameplay acceptance.

## Complete descriptor callback coverage

Recovered the final two descriptor callbacks B11878 and B0EF68, plus six action-state/timing helpers ACE208, ACE260, ACD998, ACDAA0, ACDC40 and ACDCF0. Linked properties preserve the source primary-bank peer-ID quirk and resolve live peer state before timing changes; existing ACD680 supplies timing arithmetic. Focused composition checks cover peer linking and half-time state transition, 130/150-percent action changes, duplicate suppression and item slow timing. All 83 distinct descriptor callback targets now have logical implementations. This table coverage is not whole-game completion or gameplay acceptance; unknown external boundaries remain explicit.
