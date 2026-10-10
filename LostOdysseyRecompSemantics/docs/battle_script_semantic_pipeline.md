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
