# Battle script semantic recovery

Battle script storage initialization/release, per-frame integer tick updates and the timed-wait opcode are recovered. The original integer conversion still yields zero ticks at 120 Hz; this semantic layer preserves that behavior rather than silently incorporating a new timing fix. The diagnostic/step boundary retains LR 8238AD64 and f31 so the existing fractional-tick hook can recognize its caller. Synthetic checks cover lifecycle ownership, first-frame gating, wait transitions and actor update selection. Parameter evaluation, diagnostics and actor sub-updates remain guest services; no full interpreter or gameplay acceptance is claimed.

## Parameter and action helpers

Parameter evaluation now resolves the original local, global, script, bit, constant and actor-field operand classes. Periodic action thresholds and queued actor flag changes are recovered and composed into the existing update/wait logic. Synthetic parameter, threshold/sentinel and queued-flag samples pass, as does the updated lifecycle/wait fixture. Actor lookup and action execution remain guest boundaries; diagnostics and the full interpreter remain unrecovered.
