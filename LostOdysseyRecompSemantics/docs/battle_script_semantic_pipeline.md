# Battle script semantic recovery

Battle script storage initialization/release, per-frame integer tick updates and the timed-wait opcode are recovered. The original integer conversion still yields zero ticks at 120 Hz; this semantic layer preserves that behavior rather than silently incorporating a new timing fix. The diagnostic/step boundary retains LR 8238AD64 and f31 so the existing fractional-tick hook can recognize its caller. Synthetic checks cover lifecycle ownership, first-frame gating, wait transitions and actor update selection. Parameter evaluation, diagnostics and actor sub-updates remain guest services; no full interpreter or gameplay acceptance is claimed.

## Parameter and action helpers

Parameter evaluation now resolves the original local, global, script, bit, constant and actor-field operand classes. Periodic action thresholds and queued actor flag changes are recovered and composed into the existing update/wait logic. Synthetic parameter, threshold/sentinel and queued-flag samples pass, as does the updated lifecycle/wait fixture. Actor lookup and action execution remain guest boundaries; diagnostics and the full interpreter remain unrecovered.

## Event queue and dispatch

Event insertion, completion-triggered events, sixteen-slot priority scheduling and the opcode dispatch loop are now recovered and composed into battle updates. The dispatch loop executes the recovered wait opcode locally, with other handlers delegated through the guest opcode table. A small synthetic program runs a callback opcode, initializes and resumes a wait over multiple frames, then exits; insertion, duplicate suppression and priority tie behavior also pass. This connects dispatch infrastructure, not every game opcode, and does not claim gameplay acceptance.

## Core opcode handlers

The basic opcode range 0..19 now resolves to recovered handlers: end, signed jump, conditional branch, assignment, wait, boolean set/clear, arithmetic, indexed bit changes, increment/decrement, bitwise operations, shifts and random assignment. The writable operand helper preserves original local/script bounds and read-only classes. A small assignment/add/conditional/wait/end program runs entirely through recovered handlers, apart from runtime services; random generation remains a guest boundary. Constructor registration addresses were checked with its spilled address bases, not guessed from proximity. This is core opcode coverage, not the full instruction set.
