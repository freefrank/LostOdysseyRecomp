# Shader review rules

Start from the exact archived program and observed VS/PS pairing. Static slot guesses, inferred position flags or matrix matches are candidate evidence; none proves the reported visual symptom.

- Confirm which constants actually reach `oPos` and whether changing their X/Y components affects other outputs. Follow those varying components through the paired PS before deciding whether they are used, overwritten or restricted to W.
- A PS division such as `i4.xy / i4.w` followed by texture sampling establishes that use of the input. It does not by itself prove that the VS produced clip coordinates or that the input shares `oPos`'s VP matrix.
- For a position-dependent sampled texture, establish the final descriptor after overrides, producer/resolve ages, extents and crop, sampler/swizzle/sign, and producer/consumer camera and jitter. Preserve Unknown and Mixed provenance. CPU writer consistency is not GPU completion, pixel coverage or recursive texture-dependency proof.
- Check the collection allowlist. A missing binding record can mean the exact pair is never sampled by the client; waiting for more uploads will not fix that scope gap.
- For schema 4, join pairs and inline bindings to CPU frames only through the same window content ID and relative offsets. Use that window's capabilities when deciding what was collectible. Queue and delivery counters are aggregate evidence; they cannot assign a missing program's cause. See [compact diagnostics](compact-diagnostics.md).
- Evidence for one PS or backend does not cover every use of the same VS. New pairings reopen the relevant reasoning while preserving prior scoped results.
- Record source availability, offline program review, implemented mapping, automated validation, scene validation and player acceptance as distinct facts. A deployed collector, compiled client, passing translator or D1 row is not a verified flicker repair.
- `max_draws` is an observed maximum, not an additive frame or player count. Model/backend/driver associations do not identify distinct people or physical devices.

Retain report references, observation IDs, source identities and the exact scope reviewed. Use paired normal/abnormal or fixed-scene TAA comparisons for a visual conclusion; arrange any foreground gameplay under the repository's existing rules.
