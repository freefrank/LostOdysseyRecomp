## Root direct continuation: periodic effect prerequisites (2026-10-10 UTC)

Recovered periodic-effect prerequisites AB06A0 and AB0738 paired-status predicates, plus B2BBA0 rounded HP/MP application. The latter shares result arithmetic with B2B9E0 but uses the source non-action damage flag and accepts only modes 0 through 7, leaving mode 8 inactive. Focused checks cover each status alternative, absence, rounded damage/healing, MP bounds, HP floor and unsupported mode. The encompassing periodic sweep ACB120 remains unclaimed.

## Root direct continuation: battle restart orchestration (2026-10-10 UTC)

Recovered AD40D0 battle restart orchestration: retain or remove roster entries, select replacement actions, reset formation/resources, clear both gauges, rebuild stats/action timing, restart script events and force phase zero. Focused service-spy checks validate roster mutation, release flag, replacement action, service arguments/counts and final manager state. Nested services route through the composed runtime; the restart check is isolated orchestration validation, not full nested runtime or gameplay acceptance.

## Root direct continuation: battle phase machine (2026-10-10 UTC)

Recovered AAA7C8 battle phase machine: forced transitions, numbered phase progression, turn counter, actor refresh, encounter-specific randomized categories, side countdown, profile restore and terminal-state routing. Focused checks cover force/range gates, progression, phase reset callback, periodic-effect call ABI and terminal handling. Existing semantic helpers are composed directly; periodic effect sweep ACB120 and victory finalization AC6D88 remain explicit guest boundaries. No claim that all phase branches or the complete battle restart are runtime accepted.

## Root direct continuation: battle phase support (2026-10-10 UTC)

Recovered seven lower dependencies for battle phase transitions: object release flag, timer reset, next action selection, typed play accessor, guarded script restart, side-specific action countdown and profile record restoration. Focused synthetic checks cover all seven entries with real manager/type lookup, two roster sides, blocked countdown, first matching action and both profile record banks. The encompassing phase machine and battle restart remain unclaimed. Logical ABI coverage only, not gameplay acceptance.

## Root direct continuation: complete descriptor callback coverage (2026-10-10 UTC)

Recovered the final two descriptor callbacks B11878 and B0EF68, plus six action-state/timing helpers ACE208, ACE260, ACD998, ACDAA0, ACDC40 and ACDCF0. Linked properties preserve the source primary-bank peer-ID quirk and resolve live peer state before timing changes; existing ACD680 supplies timing arithmetic. Focused composition checks cover peer linking and half-time state transition, 130/150-percent action changes, duplicate suppression and item slow timing. All 83 distinct descriptor callback targets now have logical implementations. This table coverage is not whole-game completion or gameplay acceptance; unknown external boundaries remain explicit.

## Root direct continuation: descriptor revival and action reset (2026-10-10 UTC)

Recovered revival reset dependencies AC92B0, ACD3C0 and B0FF10, composing the existing AB31E0 action reset, plus B0FFF0 revival modes and B12870 paired HP/MP floor restoration. Reset preserves the designated bank-3 property payload, clears action timing, rebuilds stats/default actions and releases script flags. Focused checks cover real reset composition, activation-only and unconditional revival, paired floors, special full-HP report and same-actor action reset. Descriptor callback coverage now 81 of 83 distinct targets. Logical ABI checks only, not gameplay acceptance.

## Root direct continuation: descriptor property transformations and reports (2026-10-10 UTC)

Recovered B106B8 property-family conversion, duration masks and selected-target property assignment; AC8608 inserts, maximizes or accumulates property payloads. Recovered B11A20 temporary-immunity/category selection with real target-label binding and result reports. Focused checks cover family/value conversion, duration slots, random primary target, random immunity, empty-category no-mark behavior and category/class report selection. Descriptor callback coverage now 79 of 83 distinct targets; remaining 4 unclaimed. Logical ABI checks only, not gameplay acceptance.

## Root direct continuation: descriptor priced physical damage (2026-10-10 UTC)

Recovered B12D08 source-action flag shortcut or priced physical damage. The full branch uses shared physical/critical/shield/MP resolution, optional MP siphon and source-width profile cost arithmetic capped by available balance. The shortcut marks the effect and sets the source flag without running damage or spending. Focused checks cover shortcut preservation and full damage/siphon/spending composition. Descriptor callback coverage now 77 of 83 distinct targets; remaining 6 unclaimed. Logical ABI checks only, not gameplay or bitwise FP acceptance.

## Root direct continuation: descriptor exhaustion and distributed damage (2026-10-10 UTC)

Recovered B0DEB0 exhaustion damage and B0E300 distributed-source-HP damage. Exhaustion skips category amplification, consumes the source gauge, sets its action flag and applies chance-gated properties. Distributed damage divides source HP by target count, applies gauge reduction, and preserves the special mode8 healing-plus-MP-siphon behavior. Focused checks cover damage, source gauge/flag changes, property bookkeeping, HP distribution and mode8 siphoning. Descriptor callback coverage now 76 of 83 distinct targets; remaining 7 unclaimed. Logical ABI checks only, not gameplay or bitwise FP acceptance.

## Root direct continuation: descriptor bounded damage properties (2026-10-10 UTC)

Recovered B0BFD0 bounded random damage with dual property values. Preserves side/bank and chance gating, direct sentinel damage before normal mode suppression, property-only mode, common damage-mode resolution and both property payload applications. Focused integration covers bounded damage, primary/secondary payloads, property-only suppression and sentinel priority. Descriptor callback coverage now 74 of 83 distinct targets; remaining 9 unclaimed. Logical ABI checks only, not gameplay or bitwise FP acceptance.

## Root direct continuation: descriptor extra MP damage (2026-10-10 UTC)

Recovered B0BA98 physical damage with randomized extra MP loss and property followups. Draws the extra MP amount before eligibility, applies it separately after normal HP damage, and combines it with damage in MP-first mode before HP spillover. Retains source stack layout, property payload/mask bookkeeping and saved floating registers. Focused checks cover separate HP/MP damage and combined MP-hit spillover. Descriptor callback coverage now 73 of 83 distinct targets; remaining 10 unclaimed. Logical ABI checks only, not gameplay or bitwise FP acceptance.

## Root direct continuation: descriptor damage followups (2026-10-10 UTC)

Recovered B0B178 physical damage with optional capped MP siphon and B0B630 damage followed by chance-gated property effects. The siphon caps against current target MP and writes both source/target results; the property variant preserves primary bank0 flags, bank7 paired payloads, secondary masks and owner bookkeeping. Shared damage resolution retains each callback stack layout and saved floating registers. Focused integration covers siphon on/off, paired-property application and primary/secondary mask reporting. Descriptor callback coverage now 72 of 83 distinct targets; remaining 11 unclaimed. Logical ABI checks only, not gameplay or bitwise FP acceptance.

## Root direct continuation: descriptor level damage (2026-10-10 UTC)

Recovered B0D7F0 level-bounded random damage and B0D418 repeated level-scaled attack damage. Both use real effective-level lookup, side gating and shared damage-mode resolution. The repeated-attack variant preserves individual floating additions, creature coefficient, gauge/status reduction and its no-variance normalization. Focused checks cover bounded RNG damage, side rejection, repeated attack sum and creature coefficient. Descriptor callback coverage now 70 of 83 distinct targets; remaining 13 unclaimed. Logical ABI checks only, not gameplay or bitwise FP acceptance.

## Root direct continuation: descriptor missing HP damage (2026-10-10 UTC)

Recovered B0C9E0 missing-HP damage with conditional physical fallback. Source HP below the configured fraction uses max-minus-current HP; the healthy branch uses attack/category/status/critical/variance helpers and the original repeated normalization. Reuses source-faithful blocked/healing/MP-first/half/shield damage modes and reports owner172. Focused checks cover unconditional missing HP, healthy fallback and low-HP threshold selection. Descriptor callback coverage now 68 of 83 distinct targets; remaining 15 unclaimed. Logical ABI checks only, not gameplay or bitwise FP acceptance.

## Root direct continuation: descriptor physical damage (2026-10-10 UTC)

Recovered B0C4E8 physical damage through attack, defense, category, gauge, status, critical, variance and normalization helpers. Resolves blocked, healing, MP-first absorption, half/minimum and zero-damage modes, shield absorption, result slots and final target class marking. Focused integration covers normal damage, MP-to-HP spillover, half damage and damage-to-healing conversion; critical random fixtures preserve the original inclusive threshold behavior. Descriptor callback coverage now 67 of 83 distinct targets; remaining 16 unclaimed. Logical ABI checks only, not gameplay or bitwise FP acceptance.

## Root direct continuation: shared damage helpers (2026-10-10 UTC)

Recovered seven shared damage helpers: attack magnitude and cap, target defense attenuation, category matchup amplification and result marking, secondary critical chance, additive bounded randomness, minimum status reduction and effective-level cap. Uses guest-provided coefficients, exact source-width arithmetic and staged single-precision calculations. Focused checks cover normal and special action coefficient paths, category variants, critical bypass, random range, damage reduction and level cap. Descriptor callback coverage remains66 of83; these helpers support remaining damage effects. Logical ABI checks only, not gameplay or bitwise FP acceptance.

## Root direct continuation: descriptor HP MP drain (2026-10-10 UTC)

Recovered B10AA8 HP/MP drain: source HP restoration precedes target shield absorption and damage; optional MP restoration/depletion reuses the critical decision, while the special mode only depletes target MP. Preserves property142 blocking, result slots and both saved floating registers. Focused integration checks source healing versus shielded target damage, paired MP changes and MP-only mode. Descriptor callback coverage now 66 of 83 distinct targets; remaining 17 unclaimed. Logical ABI checks only, not gameplay or bitwise FP acceptance.

## Root direct continuation: descriptor action rebuild (2026-10-10 UTC)

Recovered B0F3E0 paired-property effect with side-mode eligibility, single-use target flags, special immunity cancellation, queued-action rewriting and property-triggered stat reconstruction. Focused integration exercises action-record replacement, flag/counter resets, paired payload insertion, repeat suppression and the real growth/skills/equipment/derived-stats chain. Descriptor callback coverage now 65 of 83 distinct targets; remaining 18 unclaimed. Logical ABI checks only, not gameplay acceptance.

## Root direct continuation: descriptor profile damage (2026-10-10 UTC)

Recovered B12A98 profile-aggregate damage: selects the actor statistics page, applies the encounter183-185 override, sums1024 counters with source-width arithmetic, and computes either direct or ratio-reduced damage before real gauge/status/shield/result processing. Focused integration covers encounter page selection, direct aggregate damage, ratio reduction and preserved FPR31. Descriptor callback coverage now 64 of 83 distinct targets; remaining 19 unclaimed. Logical ABI checks only, not gameplay or bitwise FP acceptance.

## Root direct continuation: descriptor property toggling (2026-10-10 UTC)

Recovered B0FBD0 dual property toggling and AC9548 masked flag toggles. Existing flags are removed, while additions respect passive and temporary immunity masks. The effect preserves its asymmetric kind3/chance gate, passive-property7 exception, primary bit reporting and optional secondary bank. Focused checks cover toggling both banks, immune additions and passive gate suppression. Descriptor callback coverage now 63 of 83 distinct targets; remaining 20 unclaimed. Logical ABI checks only, not gameplay acceptance.

## Root direct continuation: descriptor random insertion (2026-10-10 UTC)

Recovered B0E798 random single-bit primary property insertion plus optional secondary mask, eligibility/chance gating and the source single-use target flag. Preserves original-mask bookkeeping in owner196 and primary selected-bit index. Focused integration checks random primary choice, secondary application and repeated-use suppression. Descriptor callback coverage now 62 of 83 distinct targets; remaining 21 unclaimed. Logical ABI checks only, not gameplay acceptance.

## Root direct continuation: descriptor HP MP transfer (2026-10-10 UTC)

Recovered B0AD38 HP/MP transfer from source to target. Preserves same-ID and zero-available skips, truncated integer ratio with whole-available fallback, target restoration before source depletion, paired result slots and final effect mark. Focused integration covers both HP and MP transfers through the real result pipeline and same-ID suppression. Descriptor callback coverage now 61 of 83 distinct targets; remaining 22 unclaimed. Logical ABI checks only, not gameplay acceptance.

## Root direct continuation: descriptor shared properties (2026-10-10 UTC)

Recovered B10E98 shared-party property assignment or random target category selection, with ACA710/ACA830 shared-bank mutation, ACA838 shared payload clearing and AC8988 direct category insertion. Preserves source early return for already-set shared flags and first-full-mask payload indexing. Focused integration covers actual shared group lookup, source category and payload assignment, target category replacement and duration resets. Descriptor callback coverage now 60 of 83 distinct targets; remaining 23 unclaimed. Logical ABI checks only, not gameplay acceptance.

## Root direct continuation: descriptor random clearing (2026-10-10 UTC)

Recovered B0F920 random property removal and AC91E0 ordinal clearing. The effect counts four candidate banks using source mask-presence behavior, cancels its result marker if none qualify, selects a nonempty candidate and clears one set bit with both payload words. Preserves source exclusion of bit31 and source random retry semantics. Focused checks cover actual random selection, payload clearing and no-eligible-bank cancellation. Descriptor callback coverage now 59 of 83 distinct targets; remaining 24 unclaimed. Logical ABI checks only, not gameplay acceptance.

## Root direct continuation: descriptor cleansing effect (2026-10-10 UTC)

Recovered B0AA70 cleansing restoration: HP healing through real scaling and result application, optional MP restoration reusing the critical decision, followed by primary and optional secondary property-bank clearing. Focused integration checks HP/MP changes, both cleared masks, the zero-MP branch and preserved FPR31. Descriptor callback coverage now 58 of 83 distinct targets; remaining 25 unclaimed. Logical ABI checks only, not gameplay or bitwise FP acceptance.

## Root direct continuation: descriptor gauge damage (2026-10-10 UTC)

Recovered B0DB98 manager-scaled damage with B0A0D0 side-gauge attenuation, status normalization, shield absorption, blocked-damage mode and HP/result writeback. The attenuation helper preserves class bypass, disabled gauges and source comparison behavior; enabled attenuation marks the result record. Focused synthetic checks exercise both helpers together, actual shield depletion, HP changes and result slots. Descriptor callback coverage now 57 of 83 distinct targets; remaining 26 unclaimed. Logical ABI checks only, not gameplay or bitwise FP acceptance.

## Root direct continuation: descriptor cost effect (2026-10-10 UTC)

Recovered descriptor B13220 with eligibility and chance gates, the real scaling/critical/variance/normalization pipeline, HP result application and profile balance consumption. The spent total increases by the amount actually available when requested cost exceeds balance. Focused integration covers regular and balance-capped costs and the source-specific result-record offset. Descriptor callback coverage now 56 of 83 distinct targets; remaining 27 unclaimed. Logical ABI checks only, not gameplay acceptance.

## Root direct continuation: descriptor growth effect (2026-10-10 UTC)

Recovered descriptor growth-refresh callback B10368: script actor level override, source growth counter, creature stats refresh, equipment and derived stats rebuild, full HP restoration, target property attempt and primary mask index. Focused integration exercises the real lower-level growth/stat implementations with a synthetic template and an immune target. Descriptor callback coverage now 55 of 83 distinct targets; remaining 28 unclaimed. Logical ABI checks only, not gameplay acceptance.

## Root direct continuation: descriptor conversion effects (2026-10-10 UTC)

Recovered three more descriptor callbacks: fractional HP reduction with recorded delta, source HP-to-MP conversion, and randomized target class flags with fallback selection. Focused checks cover real result mutation, passive immunity, paired HP/MP outcomes and the source fallback quirk that treats the random index as a resource ID. Descriptor callback coverage now 54 of 83 distinct targets; remaining 29 unclaimed. Logical ABI checks only, not gameplay acceptance.

## Root direct continuation: restorative descriptor effects (2026-10-10 UTC)

Recovered four restorative descriptor callbacks using actual scaling, critical decision, jitter, normalization and result application: HP restore/full-HP sentinel, MP restore, combined HP/MP and HP-plus-property payload. Focused fixture checks restored values, result deltas, paired payload, full restore shortcut and side rejection. Descriptor callback coverage now51 of83 distinct targets; remaining32 unclaimed. No gameplay/full ABI or bitwise FP acceptance.

## Root direct continuation: effect magnitude scaling (2026-10-10 UTC)

Recovered shared effect scaling, critical-property/chance decision, bounded magnitude jitter and final status/partner normalization. Uses guest-provided coefficients and source single-precision stages, with no embedded private constants. Focused synthetic checks cover stat scaling, target status multiplier, bypass, critical categories, jitter/zero handling, source/target status overrides, partner short-circuit and rounding. Logical ABI coverage only, not bitwise FP or gameplay acceptance.

## Root direct continuation: descriptor property effects (2026-10-10 UTC)

Recovered five additional descriptor property effects: dual-side gates, paired payload insertion/removal, mutually exclusive categories, bounded random payload with source attribution, and selected property-family clearing. Focused checks cover actual paired values and masks, source ID retention, family clearing and opposite side-gate failures. Descriptor callback coverage now47 of83 distinct targets; remaining36 unclaimed. Logical ABI checks only, not gameplay acceptance.

## Root direct continuation: descriptor effect mutations (2026-10-10 UTC)

Recovered nine descriptor effect callbacks: indexed dispatch and nested eligibility, property insert/remove and numeric payload, chance-gated source flag, fixed/quarter HP caps with recorded deltas, and percentage gauge addition/reset. Composes actual eligibility/chance/property/result/gauge semantics. Focused fixture passes dispatch limits, side rejection, property changes/truncation, failed-chance flag order, both HP caps and gauge effects. Descriptor constructor now has 42 of83 distinct callback targets recovered; remaining41 are not claimed complete. No gameplay/full ABI acceptance.

## Root direct continuation: group gauge mutations (2026-10-10 UTC)

Recovered group gauge mutation AC7000/AC7178/AC71E8/AC80B8: property-adjusted depletion, capped addition, max-HP division and roster-share application. Preserves source behavior that applies each eligible share to the original resource, rather than silently changing it to each iterated peer. Focused checks cover caps, property multipliers, divide mode, repeated original-target reduction and actual removed-amount accounting. Logical ABI checks pass; no gameplay/full ABI or FP acceptance.

## Root direct continuation: script loading and initial dispatch (2026-10-10 UTC)

Recovered script loading and initial execution: asset lookup, per-capacity actor buffers/defaults, little-endian header/events/constants decode, bytecode allocation/copy, source failure release, profile-based startup and initial event dispatch. Reuses actual script-state allocation, integer decoding and opcode dispatch. Battle startup now composes actual script startup entries. Focused fixture covers loaded and spare actors, constants and code bytes, initial cursor writeback, missing assets, zero-code/capacity failure and no-script profile shortcut. Complex encounter layout remains external; no gameplay/full ABI acceptance.

## Root direct continuation: battle startup orchestration (2026-10-10 UTC)

Recovered battle startup AD20C0 and descriptor callback manager constructor AD1378. Startup allocates/registers ten managers and composes actual owner defaults, group/party rebuild, encounter population, both gauges, availability and stats reset. Constructor writes 167 callback slots with source hole 856 preserved; only 33 of its 83 distinct callback targets are currently recovered, so registration is not callback-completion credit. Focused empty-roster startup fixture passes manager allocations/registrations and composed state effects. Script startup and complex layout remain explicit service boundaries. No gameplay or full ABI acceptance.

## Root direct continuation: battle initialization baselines (2026-10-10 UTC)

Recovered battle manager baseline setup, position defaults, typed root lookup, small manager constructor and stats workspace reset. Manager setup composes real profile casting, releases two existing list buffers, reconnects embedded list headers and establishes source defaults. Focused fixture checks type success/failure, release ABI, workspace slots, descriptor defaults and untouched padding. Public synthetic inputs only; library builds without gameplay or full ABI acceptance.

## Root direct continuation: group construction and party rebuild (2026-10-10 UTC)

Recovered two-group construction AF60D8, shared profile restore ABFDD8 and party rebuild wrapper AF63E0. Rebuild now composes real group creation, party resource population and formation selection. Preserved typed object services, list capacity growth and paired shared-profile copy ranges. Focused fixture checks two objects, array resize, all shared bytes and empty-party rebuild composition. Object allocation remains external; logical ABI coverage only, not gameplay acceptance.

## Root direct continuation: party and encounter roster creation (2026-10-10 UTC)

Recovered party roster construction AF6290 and enabled encounter-row construction AF6448. Both compose the actual resource factory, profile/creature initialization and record setup. Party slots retain source slot IDs and compact selected group indexes; encounter rows preserve enable/class bytes, tags and per-resource marker, then call the existing layout service boundary AAC1E0. Focused roster fixture passes party and encounter paths and preserved nonvolatile state. Complex encounter layout remains a guest boundary; no gameplay acceptance.

## Root direct continuation: resource creation (2026-10-10 UTC)

Recovered resource reset and battle resource factory. The factory composes actual action-record initialization, profile restore or creature growth, skills/equipment recalculation, position/angle writes and active-state reset before roster append. Class lookup/load and object allocation remain explicit external services. Focused fixture covers failed class load, party and creature paths, actual record setup, roster append and nonvolatile preservation with public synthetic data. Library builds; no gameplay, full ABI or bitwise FP acceptance.

## Root direct continuation: profile restore and resource baseline (2026-10-10 UTC)

Recovered profile-to-resource restore ABFC50 with actual seven-span copy and passive-state reset, plus ABFE38 baseline counters/level limits and ABFE90 active-state/group flag reset. Focused roster fixture now checks save/restore data, the deliberate passive reset, 32 initial limits and all group-dependent mask cases. Library and focused logic checks pass; no gameplay/full ABI acceptance is claimed.

## Root direct continuation: roster persistence (2026-10-10 UTC)

Recovered roster persistence and group availability: refresh party resources through their virtual update, copy seven actual state spans back to profile rows, preserve inverse class flag, persist shared group ranges subject to script-mode suppression, and rebuild availability tiers with source short-circuit rules. Focused synthetic checks pass all copy spans, party filtering, count tiers and suppression. Uses accepted copy semantics without new raw data. Logical ABI coverage only, not gameplay or full ABI acceptance.

## Root direct continuation: formation placement (2026-10-10 UTC)

Recovered formation placement: side/class roster counting, matching formation row, ordinal slot lookup, player-profile rotation/translation using existing guest trig, and resource position/angle writes. Script global mode 12 composes the actual formation entry. Focused checks cover mixed sides and slot classes, missing sentinels, translated/rotated coordinates and preserved nonvolatile state. No host trig substitution, raw constants, gameplay acceptance or full ABI/FP acceptance.

## Root direct continuation: record group rebuilding (2026-10-10 UTC)

Recovered AFD2F0 linked-resource record rebuilding: owner IDs, cleared companion slots, ordered group membership and unordered peer groups across every record. Added AAB870 paired mode-bit update. Global script modes now compose both actual implementations. Focused group-record and global-mode checks pass, preserving the source single-pass order rather than sorting members. Library builds; logical ABI coverage only, not gameplay acceptance.

## Root direct continuation: shared runtime support (2026-10-10 UTC)

Extended the composed battle runtime with existing manager-release, string-storage and string-conversion semantics, plus accepted full-context copy and fill support. The graph now routes 511 battle entries and 29 shared/support entries. Focused composition checks pass UTF-16 length and actual copy/fill memory effects without guest escapes. No new recovery credit is claimed and native game-runtime hooking remains pending.

## Root direct continuation: battle graph composition (2026-10-10 UTC)

Added an opt-in battle semantic runtime entry that routes all 511 currently recovered battle entries across 64 units, including nested direct calls and known indirect targets. Unknown services still forward to the supplied guest implementation. The focused composition fixture crosses script completion into real manager/resource lookup, routes a known virtual target, and verifies unknown direct/indirect forwarding. This connects recovered logic without claiming game-runtime integration or gameplay acceptance; inventory entry counts are unchanged.

## Root direct continuation: packed scene positions (2026-10-10 UTC)

Recovered three-component guest D3D half packing 377168 and composed it into spatial scene requests. Preserves truncating mantissas, signed zero, subnormal shifts and the guest overflow/NaN sentinel 0x7fff, with guest flush-mode transitions. Replaces the last packed-position fixture boundary. Library and focused request-factory and scene-command checks pass; synthetic checks include XYZ, negative values, signed zero and overflow. Logical coverage only, not full VMX/volatile-register or runtime playback acceptance.

## Root direct continuation: positional scene requests (2026-10-10 UTC)

Added positional scene request factory B1AFE8 and packed XYZ assignment B356B0, composing the existing request constructor, parent selection and duplicate suppression. Scene script packed-coordinate requests now write actual task coordinates instead of using callback fixtures. Library and focused scene-request and scene-command checks pass. Packed half-vector conversion remains a platform boundary; logical ABI coverage only, not runtime playback or complete volatile-register acceptance.

## Root direct continuation: scene request factories (2026-10-10 UTC)

Recovered scene request key classification, default parameter blocks, tracked-parent lookup, 100-byte task construction and initialization, object attachment, collision-free IDs, duplicate suppression and failed-initialization cleanup. Script scene requests and periodic parameter events now compose the factories. Platform metadata lookup, immediate handle release and packed-vector conversion remain service boundaries. Library and focused request-factory, scene-command and periodic-parameter fixtures pass. Logical ABI coverage only; no runtime playback/gameplay or complete volatile-register acceptance.

## Root direct continuation: active scene handles (2026-10-10 UTC)

Recovered active scene-handle selection B63828, status probe B19CC0, cached parameter update B19C00 and conditional immediate/timed stop B1A048. Scene script commands now compose these paths. Backend handle enumeration, status and submission remain service boundaries. Library builds; focused scene-handle lifecycle, scene commands and existing task fixtures pass. Logical ABI coverage only, not runtime playback/gameplay or complete volatile-register acceptance.

## Root direct continuation: facing sectors (2026-10-10 UTC)

Recovered facing-sector classification A9B458 using actual scene lookup D40, vector-to-guest-angle conversion 323488 and guest floor semantics 2B94C8. Reuses the existing guest-table atan2 implementation; no host atan2 substitution. Hit evaluation and queued actor-flag application compose the recovered paths. Library builds and focused calculation, parameters and manager fixtures pass, including four facing sectors and absent-object handling. Private atan tables stay local. This remains logical coverage, not runtime gameplay or full floating-point/volatile ABI acceptance.

## Root direct continuation: resource growth (2026-10-10 UTC)

Recovered character template-stat loading, level-dependent stat curves and rounding/clamping, and layered creature initialization with archetype selection, equipment/traits and initial skills. The script level-change command now composes real growth, equipment stat refresh and HP/MP recomputation. Focused synthetic growth and command integration checks pass; library builds. Private coefficient tables remain external guest data. This is logical coverage, not gameplay or bitwise floating-point/full volatile ABI acceptance.

## Root direct continuation: group gauge (2026-10-10 UTC)

Recovered group gauge eligibility, roster HP aggregation and initialization, current-value clamping, ratio/rank thresholds, resource snapshot synchronization, two-group refresh and disabling. Binding and global script modes now compose these seven functions and existing inventory lookup. Focused gauge, binding and global-mode fixtures pass; library builds. Synthetic fixtures establish logical ABI behavior only, not gameplay, Full72, bitwise floating-point or full volatile-register equivalence.

## Root direct continuation: script action boundaries (2026-10-10 UTC)

Recovered script-specific property insertion ACA1B8, roster defaults AC3118, pending-action reset ACD530 and AB0B10, manager mode B48, and composition with the existing cyclic resource state B1F1D0. Script actions now compose these implementations and real property removal instead of guest fixtures. Four focused property, resource-stat, script-action and manager-access checks pass; library builds. Coverage remains logical ABI coverage, not gameplay, Full72 or complete floating-point/volatile-register acceptance.

## Root direct continuation: tracked scene factory (2026-10-10 UTC)

Recovered tracked scene-task lookup/create, constructor, membership extension, initialization and reference reset. Profile preset loading now creates actual named tracked objects and reuses matching names without duplicate membership. Focused checks cover real allocation/name storage, duplicate reuse, cancellation composition, slot-cache invalidation and scene-object reference clearing. Library passes. Remaining platform release and folded-name comparison calls stay explicit boundaries; this is logical coverage, not gameplay acceptance.

## Root direct continuation: task initialization (2026-10-10 UTC)

Recovered scene task reset and task-specific initialization, paired byte/word membership append and bounded UTF-16 comparison. Factory creation now composes actual member storage, full/relative resource names, prefix handling, state flags and owned-resource cleanup rather than an initialization mock. Library and focused scene-task/storage checks pass creation, reinitialization, relative-name trimming and owned/raw cleanup. Platform object release callbacks remain services; no native gameplay or full ABI acceptance is claimed.

## Root direct continuation: scene resource path (2026-10-10 UTC)

Recovered scene resource-path construction for six type-specific platform roots and direct-copy fallback. Generic scene task creation now uses actual UTF-16 concatenation, header assignment, temporary release and final copy instead of a path-building mock. Focused scene-task checks cover types11/13/14/15/16/17 and type12 fallback with synthetic guest strings; library passes. Platform root lookup and task-specific initialization remain service boundaries; no private strings or runtime acceptance are included.

## Root direct continuation: string concatenation (2026-10-10 UTC)

Recovered UTF-16 header copy/assignment, append, concatenation and shared array growth context entries. These compose existing memory copy and allocator-backed resize semantics while preserving terminators, self-assignment and empty-append behavior. Library and focused storage checks pass; allocator remains an explicit service boundary. These helpers support the next scene-path composition step; no runtime or full ABI acceptance is claimed.

## Root direct continuation: scene object factory (2026-10-10 UTC)

Recovered generic scene task reuse/create selection and object default initialization. Secondary row activation now composes duplicate reuse, parameter update, signed priority selection, wrapped ID search, pointer-list growth and actual manager allocation. Focused scene-task checks pass existing-object and new-object paths, constructor defaults and preserved padding. Path building and task-specific initialization remain services; no gameplay or full ABI acceptance is claimed.

## Root direct continuation: scene row creation (2026-10-10 UTC)

Recovered secondary scene-row creation, zero/default initialization and activation forwarding. Profile preset loading now uses actual row allocation and identifier collision scanning, including 16-bit identifier wrap, rather than a row-factory mock. Library and focused scene-task checks pass creation, later cleanup and collision handling. Generic task activation remains an external boundary; inventory growth is not a game-completion percentage.

## Root direct continuation: scene row cleanup (2026-10-10 UTC)

Recovered large scene-row destruction, nested row-array reset/removal and virtual scene-handle release. Cancellation now composes real row cleanup instead of a destructor mock, reusing recovered memory fill and storage helpers. Focused scene-task checks pass actual buffer clearing, nested release, identifier reset and virtual removal arguments. Allocator and object-specific virtual methods remain explicit services; no native gameplay or full ABI acceptance is claimed.

## Root direct continuation: event payload cleanup (2026-10-10 UTC)

Recovered event payload destruction and nested string-array cleanup, plus the capacity-reset tail adapter. Queue-reference removal now destroys payload names and nested strings through existing storage semantics instead of a destructor mock. Library and focused scene-task/storage fixtures pass with actual header clearing and allocation-service release counts. Reuses existing typed reset/array semantics; allocator callbacks and full volatile ABI remain outside acceptance.

## Root direct continuation: task unlink (2026-10-10 UTC)

Recovered final tracked-object unlinking and queue-reference cancellation for task types0/1/4/5/6. Cleanup now removes matching references from both 44-byte event queues, respects the mode13 second-queue exemption, invokes the object destructor and compacts the pointer list. Focused scene-task checks pass actual paired membership, repeated reference removal and array release. Event payload destructors remain explicit services; no gameplay or full ABI acceptance is claimed.

## Root direct continuation: task cancellation (2026-10-10 UTC)

Recovered scene-task cancellation lookup, bulk row cleanup, paired membership lookup/removal and conditional task removal. Preset clear now executes these paths and real array compaction rather than mocking cancellation. Library and scene-task/runtime checks pass, including empty membership and clear-all release. Object destructors and final tracked-pointer removal remain lower service boundaries; no runtime acceptance is claimed.

## Root direct continuation: scene task lifecycle (2026-10-10 UTC)

Recovered tracked scene-task clear/reload orchestration and pointer-array append capacity growth. Script toggle handlers now compose actual task cleanup and profile-driven preset selection, including fallback names, two tracked task lists and descriptor state. Library and focused scene-task/runtime fixtures pass. Individual task factories/cancellation and folded name comparison remain service boundaries; this is script-flow recovery, not proof that an entire battle or game runs.

## Root direct continuation: scene state (2026-10-10 UTC)

Recovered active scene-object state lookup and removal, current-scene name matching, UTF-16 comparison and low-property-mask target selection. Scene removal composes destructor dispatch plus actual pointer-array compaction; script scene/target handlers compose the recovered checks. Library and focused completion/scene/target fixtures pass. Destructors remain explicit virtual services; no gameplay or full ABI acceptance is claimed.

## Root direct continuation: root initialization (2026-10-10 UTC)

Recovered lazy battle-root initialization orchestration: temporary name conversion/cleanup, type lookup, manager initialization, object construction and cached-root publication. Root getter now calls this implementation. Focused checks cover cached success, missing type, failed construction and retry. String storage uses existing semantics; registry and object constructors remain external services. Logical recovery only, not native initialization or gameplay acceptance.

## Root direct continuation: profile lookup (2026-10-10 UTC)

Recovered platform object type-chain validation and player profile table lookup, including lazy type registration and missing/type-mismatched objects. Battle completion and script predicates now use the actual profile lookup instead of a mocked profile getter. Library and manager/completion/runtime checks pass; platform object delivery and type registration remain external services. Logical recovery only, not gameplay or full ABI acceptance.

## Root direct continuation: array storage adapters (2026-10-10 UTC)

Added register-context adapters for previously recovered array resize and range removal, reusing allocation_array and memory_move semantics rather than duplicating those algorithms. Battle completion now performs actual stale-entry compaction and empty-array release. Library and focused storage/completion checks pass; allocator methods remain service boundaries. Logical coverage only, not full guest ABI or runtime acceptance.

## Root direct continuation: battle completion (2026-10-10 UTC)

Recovered battle completion predicates, tracked-ID lookup and subordinate state checks. Script conditional branches now compose actual completion logic. Focused fixtures cover rank bounds, optional task gates, pending/completed states, missing IDs and stale row removal routing; library and completion/runtime fixtures pass. Array erasure and platform profile lookup remain guest boundaries. Logical recovery only, not gameplay or full ABI acceptance.

## Root direct continuation: manager access (2026-10-10 UTC)

Recovered common battle manager accessors, resource-ID roster search and scene-object search, including lazy-root forwarding and fixed global manager selection. Main execution composes actual roster/list lookup; focused manager and execution fixtures pass. Lazy singleton construction remains a guest boundary; no gameplay or full ABI acceptance is claimed.

## Root direct continuation: special damage (2026-10-10 UTC)

Recovered nine-way special damage mode selection, skill-point/empty-slot scaling, side-count advantage, manager multiplier and composition of the existing property-based fixed attack. Added actual allied/enemy active roster counters with virtual status predicates and battle end gates. Main execution now composes special damage selection; focused calculation and execution checks cover all nine modes and actual HP change. Library builds; logical coverage only, not gameplay or bitwise floating-point acceptance.

## Root direct continuation: actor result publication (2026-10-10 UTC)

Recovered actor result publication and its guarded target adapter, including pending/blocked flags, result payload and result-kind bitfield. Main effect execution now applies actual actor flags rather than a notification stub. Library and focused runtime/execution checks pass; no runtime gameplay or full ABI acceptance is claimed.

## Root direct continuation: skill recomputation (2026-10-10 UTC)

Recovered AC0888 learned/equipment skill aggregation with equipment deduplication, cumulative bonuses, maxima/minimum and deferred property handling, immunity/category masks, rounded stat modifiers, and player skill capacity caps. Added AC8968 mutation adapter. Equipment refresh and theft now compose real skill recomputation instead of fixture callbacks. Focused resource-stat, theft and evaluator eligibility checks pass; library builds. This is logical ABI coverage, not runtime gameplay or bitwise floating-point acceptance.

## Root direct continuation: inline UTF-16 copy (2026-10-10 UTC)

Recovered the leaf forward UTF-16 copy used for equipment names, including terminator copy and source-register advance. Equipment refresh exercises it with synthetic empty names; existing string assignment fixtures remain passing. No runtime acceptance is claimed.

## Root direct continuation: equipment contributions (2026-10-10 UTC)

Recovered six-slot equipment contribution aggregation, item HP percentage adjustment and integer rounding, accuracy and accessory effects, plus weapon trait reconstruction and category masks. Refresh now executes actual equipment aggregation; focused checks cover percentage reduction, special rounding, accessory slot state and three weapon trait categories. Skill recomputation AC0888 remains the main guest dependency, and runtime gameplay is still unvalidated.

## Root direct continuation: resource stat refresh (2026-10-10 UTC)

Recovered gear-derived property clearing, aggregate HP/MP and battle-stat calculation, and equipment refresh orchestration. Theft now runs this refresh after removing equipment. Focused fixtures cover caps/refill, low-HP state, stat sums, two-bank removal and refresh order. Skill recomputation AC0888 and equipment aggregation AC2468 remain external dependencies; no gameplay, Full72 or bitwise FP acceptance is claimed.

## Root direct continuation: battle progression (2026-10-10 UTC)

Recovered damage/defeat progression counters and achievement-service forwarding, including threshold crossing and defeat-record insertion/deduplication. Result application and actor death transition now compose real progression rather than mocking counters. Library build and focused progression/property/result/evaluator/execution checks pass. Platform achievement delivery and gameplay remain unvalidated; no Full72 or full ABI proof is claimed.

## Root direct continuation: report string integration (2026-10-10 UTC)

Added guest-register adapters for the already recovered UTF-16 assignment and length semantics, reusing registered_metadata_string rather than duplicating its algorithms. Battle reward and source/target report labels now call real string assignment instead of a UI-copy mock. Focused checks cover copied text, alias no-op, empty assignment release, length and report-label integration. Allocation remains a guest service; no gameplay or full ABI acceptance is claimed.

## Root direct continuation: inventory table lookup (2026-10-10 UTC)

Recovered inventory table lookup behind the existing manager selector. The theft fixture now resolves inventory through the actual accessor and retains grant/removal/depletion checks. Global manager access remains an external service.

## Root direct continuation: death transition (2026-10-10 UTC)

Recovered actual death transition: actor event queue or pending completion flag, HP clearing, resource state reset and removal of properties164/242. Status insertion and HP normalization now compose this function instead of mocking manager notification. Targeted checks verify queued event, direct pending ID, actor flags and resource cleanup; the lower resource cleanup service remains external.

## Root direct continuation: theft and evaluator dispatch coverage (2026-10-10 UTC)

Recovered the theft evaluator and descriptor dispatch, source/target report labels, theft chance and inventory quantity removal. Focused checks cover relation rejection, missing loot, failed chance, actual inventory grant, already-looted state, equipment theft, inventory depletion and empty inventory, alongside stack/register restoration. The 97 statically recovered descriptor slots now have implementations for all 33 distinct callback targets; this is a dispatch-coverage milestone, not whole-game or runtime acceptance. Platform lookup, UI string assignment and equipment refresh remain external services.

## Root direct continuation: trait probability (2026-10-10 UTC)

Recovered trait probability scaling by battle mode and per-slot trait chance dispatch. Category3 details1/11,2/12,3/13 select thresholds30/60/100 and random tags58/59/60. Focused checks cover signed quarter scaling, full mode success and zero mode rejection; composed into post-hit follow-up orchestration.

## Root direct continuation: post-hit effect follow-ups (2026-10-10 UTC)

Recovered additional-effect application and orchestration after actual damage: status-mask insertion, HP/MP return, allied distribution, trait probability and pending actor effects. Main execution now composes real follow-up logic. Reward chance and report-label routing are recovered, with positive item grant relying on existing inventory semantics and remaining platform/UI services. Focused fixtures and library build pass; no native gameplay, Full72 or bitwise floating-point proof is claimed.

## Root direct continuation: result modes and damage cleanup (2026-10-10 UTC)

Recovered damage-triggered status cleanup and nine-way HP/MP result dispatch, including proportional HP adjustment, fixed/threshold HP caps, resource floors and critical-hit healing refunds. Actual effect execution no longer mocks post-damage cleanup, and the critical refund path is exercised through the real heal dispatcher. Focused checks cover resource writes/returns, excluded cleanup masks, wake-state reset and unchanged-HP behavior. Achievement counting, manager notifications and additional effects remain service boundaries. Logical recovery only, not native gameplay or full FP/ABI acceptance.

## Root direct continuation: status insertion (2026-10-10 UTC)

Recovered strict and permissive property insertion, including manager-gated low-HP state, duplicate suppression, immunity differences, property2/16 transitions and actor-aware death/property15 notification. HP normalization and the evaluator death callback now use real status mutation rather than mocking insertion. Focused checks verify actual flags, actor bits, payload initialization and notification arguments. The manager notification remains an external service; gameplay, Full72 and full ABI acceptance remain unvalidated.

## Root direct continuation: battle result application (2026-10-10 UTC)

Recovered result application from the battle effect pipeline into source and target HP/MP. Shield consumption, damage/heal normalization, survival-property removal and resource caps now execute through handwritten semantics. The execution fixture no longer mocks result application and verifies actual target HP/MP writes. Resource manager lookup, achievement counting, status insertion, additional effect handling and completion notification remain external services. Library build and focused application/execution/descriptor/eligibility checks pass; no gameplay, Full72 or bitwise floating-point acceptance is claimed.

## Root direct continuation: main battle effect execution (2026-10-10 UTC)

Recovered and composed the main effect execution sequence and linked-target preparation. Real stat, hit, response, critical, element, bonus, aggregation, property and result-record stages execute together. Targeted hit/miss/guard/absorption/MP-diversion checks and existing descriptor/eligibility fixtures pass. Final resource application, result normalization and notification are still external dependencies; no native gameplay or full floating-point acceptance is claimed.

## Root direct continuation: status consumption and return amount (2026-10-10 UTC)

Recovered status-counter consumption and return-amount recording, composed with property removal, source response classification and result flags. Focused fixture checks counter expiry, weakness rounding including odd amounts, lethal-damage cap, absorption/zero-response record fields and source result marking. Guest constants remain synthetic in tests; full floating-point and native gameplay equivalence are unvalidated.

## Root direct continuation: element response classifier (2026-10-10 UTC)

Recovered elemental response classification and its tail adapter. The classifier preserves source ordering across selected element bits and the shared property52 override, returning response codes6/7/8 for values0/1/2. Target response selection now uses actual classification instead of a service mock. Focused fixture checks codes, later-element overwrite and shared override. The duel manager remains a separate boundary; native gameplay remains unvalidated.

## Root direct continuation: elemental and target responses (2026-10-10 UTC)

Recovered elemental cancellation/weakness bonus, property-backed guard chance and ordered target response selection. Existing property, trait, interpolation and random helpers are composed. Focused fixture checks elemental permission/neutral gates, weakness multiplier, early-return state preservation, response modes0-5 and guard bypass. The external response-classifier and duel manager remain explicit service boundaries. Logical/ABI recovery only; native gameplay and bitwise floating-point equivalence remain unvalidated.

## Root direct continuation: trait bonus calculations (2026-10-10 UTC)

Recovered category-two and category-eight trait bonus calculations using actual trait matching and interpolation. The focused fixture checks overlap gating, selected multiplier, target-specific extra bonus, result-record flag, skip behavior and FPR28-31 restoration. No trait service mock remains in these paths. Guest constants are synthetic in the fixture; full floating-point and native gameplay equivalence remain unvalidated.

## Root direct continuation: trait matching and critical selection (2026-10-10 UTC)

Recovered three-slot trait matching with wildcard filters, selected-trait state writes, effect-mode float interpolation and critical selection. Critical selection composes actual trait lookup, interpolation, target property scaling and deterministic random gates; skip paths preserve the existing output byte. Focused fixture checks wildcard/no-match state, interpolation modes and critical/skip paths. Synthetic constants only; native gameplay and bitwise floating-point equivalence remain unvalidated.

## Root direct continuation: amount modifiers and aggregation (2026-10-10 UTC)

Recovered side-dependent amount attenuation and final amount aggregation, including exemption gates, result-record marking, status overrides, critical multiplier, count scaling, optional random modifier, rounding and cancellation. Focused fixture checks attenuation/bypass and the aggregation stages with synthetic guest constants and real property helpers. No bitwise floating-point or native gameplay equivalence is claimed.

## Root direct continuation: attack and defense bases (2026-10-10 UTC)

Recovered and composed attack and defense base calculations using real numeric properties and guest-loaded floating constants. The initial amount stage no longer mocks either stat calculation. Focused checks cover nonnegative difference, negative defense stat adjustment, status-dependent floor and preserved nonvolatile FPRs. Fixtures use synthetic constants; bitwise floating-point, exception flags and native gameplay equivalence remain unvalidated.

## Root direct continuation: battle effect calculations (2026-10-10 UTC)

Recovered six effect calculation stages and the status-result record setter. Targeted checks pass actual numeric property, random and result state paths, with attack/defense base services still explicit dependencies. The main effect application sequence is pending; no full floating-point or native gameplay acceptance is claimed.

## Root direct continuation: upper effect descriptors (2026-10-10 UTC)

Recovered four upper effect descriptors: chance-gated property insertion/removal, target status marking and two deactivation/report paths. Composed real source/target eligibility, chance gates, upper effect descriptors, property mutations and result-record flags. Focused effect and eligibility fixtures pass target flags, report codes250/15, scene-dependent chance and manager counter wrapping. Main effect application and final target notification remain explicit boundaries. Logical/ABI recovery only; full floating-point and native gameplay acceptance remain unvalidated.

## Root direct continuation: target relation eligibility (2026-10-10 UTC)

Recovered source/target relation eligibility: unrestricted, same-side and opposing-side modes, optional property-zero exclusion, source property242 and explicit low-byte override. The focused fixture checks all side combinations and modes, rejection, bypass and property override with real property helpers. This helper supports subsequent effect descriptor composition; native gameplay remains unvalidated.

## Root direct continuation: battle chance gates (2026-10-10 UTC)

Recovered numeric property lookup, target penalty and two chance gates using actual property/random helpers. Focused checks cover bonus subtraction, generic payload, override, rejection and threshold outcomes. These helpers are ready for upper descriptor composition; no full floating-point or gameplay equivalence is claimed.

## Root direct continuation: battle action result flags (2026-10-10 UTC)

Recovered seven action-record result flag operations and the descriptor tail adapter that selects the global result manager. Focused checks cover exact row/index stores and preserved neighbors; descriptor dispatch composition and the eligibility fixture pass. No full-equivalence or native gameplay acceptance is claimed.

## Root direct continuation: special descriptor gates (2026-10-10 UTC)

Recovered special target property admission with same-resource exclusion and opposing-side exception, plus category-specific manager restrictions. The special property check composes actual property state semantics; only the manager accessor and target virtual method remain service boundaries. Focused fixture covers category routes, manager flag, self-target exclusion and side exception. Logical/ABI recovery only; native gameplay remains unvalidated.

## Root direct continuation: property state transitions (2026-10-10 UTC)

Recovered payload/auxiliary property state admission and update, including exact option matching, immunity restrictions and bank-seven additive payload semantics. Added insertion/removal adapters and composed the source-property side effect in descriptor adapter B0D088. Focused property and effect fixtures pass real mutations without the property service mock. Low-31-bit scan and original-mask payload addressing are retained; no native gameplay or full-equivalence claim.

## Root direct continuation: effect preparation (2026-10-10 UTC)

Recovered and composed effect preparation: source/target and argument bindings, byte/word resets, secondary manager snapshots, target HP capture and source actor flag extraction. All ten effect adapters now initialize actual state before the remaining application callback. Focused fixture checks setup output, descriptor16 classification, low-byte input truncation and preservation of adjacent bytes. No full floating-point equivalence or native gameplay claim.

## Root direct continuation: battle descriptor effect adapters (2026-10-10 UTC)

Recovered ten descriptor effect adapters and composed them into evaluator dispatch. Focused checks preserve mode-write timing, mutable global manager reload and secondary property/numeric callback arguments. The eligibility fixture still passes. Lower preparation/application services remain to be recovered; logical/ABI coverage does not imply runtime gameplay acceptance.

## Root direct continuation: extended descriptor gates (2026-10-10 UTC)

Recovered four-property availability conjunction, three-bank population gate, virtual-target/property-15 exception and availability/presence fallback. These callbacks compose actual property helpers, including the source empty-secondary success behavior and skipped bank 255 rows. Focused gate and eligibility fixtures pass. All existing paired descriptor predicates now resolve to recovered property semantics; target virtual methods remain boundaries. No native gameplay or full-equivalence claim.

## Root direct continuation: status admission (2026-10-10 UTC)

Recovered status admission query and mutation with type-table gates, immunity masks, bank-zero special status interactions, bank-seven restrictions and actor/manager notification side effects. The read-only adapter preserves its bypass argument. Paired admission callbacks now compose actual property state; focused fixtures check restrictions, typed presence, special-status replacement, actor flags and notification ABI. Only the notification manager services remain external for this path. Logical/ABI recovery only; native gameplay remains unvalidated.

## Root direct continuation: property payload operations (2026-10-10 UTC)

Recovered typed property payload comparison, maximum update, additive update and insertion. Source semantics retain the first original-mask bit for payload addressing, signed comparisons, last processed eligibility result and low-31-bit scan. The read-only adapter clears both mutation flags. Paired payload evaluators now use actual bank state; focused property and evaluator fixtures pass. Logical recovery only, not native gameplay acceptance.

## Root direct continuation: property bank operations (2026-10-10 UTC)

Recovered bank-mask query/removal, type-table-gated availability/insertion and gated whole-bank population count. Source scans only bits 0 through 30; insertion payload uses the first bit of the original mask. Presence removal retains the source red-zone ABI. Actual paired-presence evaluator now composes the recovered query. Property and evaluator fixtures pass targeted mutation/query, high-bit and payload cases; native gameplay remains unvalidated.

## Root direct continuation: battle descriptor gates (2026-10-10 UTC)

Recovered eleven descriptor eligibility callbacks and composed them into the evaluator dispatch. Focused checks cover constant leaves, virtual status, identity/property gates and paired query ordering. Lower property predicates and target virtual callbacks remain explicit dependencies. The eligibility fixture still passes; logical recovery only, with no full equivalence or native gameplay claim.

## Root direct continuation: composed action eligibility (2026-10-10 UTC)

Composed real action eligibility, category configuration, parameter initialization and descriptor dispatch into skill pickers and action availability. Picker, action and marshaling focused fixtures pass without AD0C10/setup/initializer service mocks; descriptor-specific evaluation remains an explicit callback boundary. This is logical composition, not full game/runtime acceptance.

## Root direct continuation: eligibility parameter initialization (2026-10-10 UTC)

Recovered and composed category-specific evaluator parameter initialization: skill/item/special/inventory rows, paired field copies, adjusted costs, preserved untouched fields and random element selection. The focused fixture covers categories 0 through 32, actual cost/property helpers and random selection. Eligibility configuration now has no setup or initialization mock; only descriptor-specific evaluation callbacks and manager access remain boundaries. Logical/ABI recovery only, with no Full72, bitwise FP or native gameplay acceptance.

## Root direct continuation: eligibility configuration and dispatch (2026-10-10 UTC)

Recovered and composed evaluator configuration and descriptor-based tail dispatch. Category setup resolves skill, item, special and inventory descriptors, permits property-zero targets only for descriptors 7/18, and rejects inactive targets. The leaf dispatcher preserves stack depth and invokes the descriptor callback through CTR. Targeted fixture passes actual setup and all dispatch categories; parameter initializer 82B121B0 and descriptor callbacks remain guest boundaries. Logical/ABI recovery only; native gameplay remains unvalidated.

## Root direct continuation: battle action eligibility (2026-10-10 UTC)

Recovered the action-kind eligibility dispatcher, raw property-zero query and result wrapper. The fixture checks the source gate distinctions and actor/manager restrictions while retaining setup and evaluation as explicit service boundaries. Picker/action integration is next; no native gameplay claim is made.

## Root direct continuation: numeric skill cost adjustment (2026-10-10 UTC)

Recovered and composed the property-based numeric cost adjustment: property 236 subtracts a truncating signed quarter; property 97 then forces zero. The cost fixture exercises actual table values, discount/free paths and category-specific NaN comparisons. Picker and marshaling fixtures pass without cost-service mocks; the picker retry case changes MP through its existing mutable virtual boundary. Native gameplay is unvalidated.

## Root direct continuation: compose selection random state (2026-10-10 UTC)

Preparation, target filtering/refinement, skill picking and item selection now compose the recovered random-range helper. Five focused fixtures pass using actual per-group/per-tag cursors and controlled synthetic table values instead of random-service stubs, including self-target retry and equal-range no-advance behavior. Four existing record/execution/effect fixtures also pass with the shared synthetic random table. Native random-table data, statistical behavior and gameplay remain unvalidated.

## Root direct continuation: battle probability wrapper (2026-10-10 UTC)

Added the probability wrapper 82AA0838, preserving threshold >= a 0..99 draw. Preferred target selection composes it and the focused fixture verifies equality behavior and side-specific cursor tags.

## Root direct continuation: preferred battle targets (2026-10-10 UTC)

Recovered preferred target selection and composed it into preparation. The flow builds live resource pools, selects a side using the shared probability wrapper and draws through the recovered random cursor logic. Focused fixtures check the source branch behavior, including its empty-pool contract without added guards. Native gameplay remains unvalidated.

## Root direct continuation: target property composition (2026-10-10 UTC)

Target filtering and preparation now compose resource property lookup, mask indexing and the narrower target-unavailability predicate. Fixtures use actual property masks and payload indexes instead of mocked predicates; readiness, adjustment, preparation, target and marshaling checks pass. Random selection and preferred-target selection remain pending composition steps; native gameplay is unvalidated.

## Root direct continuation: battle property removal (2026-10-10 UTC)

Recovered the property-removal query and tail alias, including clearing both associated payload fields. Linked action adjustment now executes real property removal; its fixture checks both resource and peer masks. The peer virtual predicate remains a service boundary, with no native gameplay claim.

## Root direct continuation: compose final action-effect chain (2026-10-10 UTC)

Top-level action effects now compose all recovered parameter handlers, percentage adjustments, manager setter and final resource application. The effect fixture executes all 32 kinds plus the default route and checks real resource fields. Five record/execution/opcode caller fixtures pass after removal of parameter/finalizer mocks, using actual actor fields and random cursors for observations. Imported character conversion, heap virtuals and linked-resource removal still remain platform/service boundaries; native gameplay is unvalidated.

## Root direct continuation: final action parameters (2026-10-10 UTC)

Recovered final action parameter application, including merging, property scaling, global/script overrides and final resource flags. The focused fixture preserves the source explicit 600/15000 arithmetic without assigning gameplay meaning. Top-level effect-chain composition is next; native gameplay and bitwise equivalence remain unvalidated.

## Root direct continuation: battle action adjustments (2026-10-10 UTC)

Recovered action parameter percentage adjustments and composed them into multi-kind setup. Focused fixtures now exercise actual normalized fields, live-resource transitions, property-based reductions/boosts and zero-rate scaling. Linked-resource removal and virtual behavior remain external services; top-level action-effect integration is pending.

## Root direct continuation: multi-kind action parameters (2026-10-10 UTC)

Added 82B11DF0 parameter setup for skill, item and special action branches, composing shared random selection and manager state writes. The focused fixture covers table fields, 25-unit normalization, value-99 cases, alternate override, resource-group override and mutable adjustment callbacks. Three adjustment helpers remain explicit guest boundaries; top-level effect-dispatch composition and native gameplay are unvalidated.

## Root direct continuation: battle action parameters (2026-10-10 UTC)

Two concrete action parameter handlers now compose recovered resource properties, random-range selection and manager field updates. A compact fixture covers their normal/alternate and property-dependent branches. The field meanings remain neutral rather than inferred gameplay labels; effect-dispatcher integration is next.

## Root direct continuation: compose action readiness (2026-10-10 UTC)

Execution now calls the recovered readiness predicates. Four execution/opcode fixtures pass after replacing predicate mocks with actual resource flags, and marshaling failure checks now inspect the real actor busy count. Property lookup in target pickers remains a separate pending composition step. Logical validation only; native gameplay is unvalidated.

## Root direct continuation: battle action readiness (2026-10-10 UTC)

Recovered resource property lookup and action-unavailability predicates, including the distinct standard and extended property sets. A compact synthetic fixture checks all predicate branches and the resource flag fallback. Execution and target-picker integration is next; no native status-name or gameplay inference is made.

## Root direct continuation: battle random ranges (2026-10-10 UTC)

Recovered the shared battle random-range helper, preserving resource-group and call-tag cursor selection and wrap behavior. One compact fixture checks synthetic table values and endpoint paths. Composition into action and picker callers is next; native random-table data and gameplay behavior are unvalidated.

## Root direct continuation: compose action snapshots (2026-10-10 UTC)

Execution now composes alternate-record snapshot preparation. Four execution/opcode fixtures pass with the actual backup state and independent normal/alternate storage, including an assertion that readiness early-return still creates the alternate snapshot. The shared synthetic allocator now represents separate live allocations. Record/effect caller fixtures also pass; native gameplay remains unvalidated.

## Root direct continuation: battle action snapshots (2026-10-10 UTC)

Action snapshot preparation now has logical implementations for manager field updates, selective record copy and alternate-array backup. The focused fixture executes real storage and string lifetime and checks both preparation modes and flag preservation. Integration into execution callers is next; native gameplay and bitwise FP remain unvalidated.

## Root direct continuation: table-driven UTF-8 decoding (2026-10-10 UTC)

Recovered the table-driven UTF-8 decoder 827CA660 and composed it into codepage dispatch. The focused fixture now checks actual ASCII decoding, supplementary-plane surrogate output, size-only queries, partial-capacity error behavior and incomplete input using synthetic guest tables. Imported non-UTF8 conversion remains a platform boundary; native locale and full-image table validation remain unvalidated.

## Root direct continuation: string allocation adapter (2026-10-10 UTC)

Added the full-register 82486C88 allocation adapter by reusing AllocateManagerBuffer. Long-string conversion now reaches the manager virtual allocator through recovered code; both string conversion and storage fixtures pass.

## Root direct continuation: string conversion wrappers (2026-10-10 UTC)

String construction now composes byte-length and temporary-conversion wrappers, including local versus heap buffer selection and error routing. The long-string allocation request preserves the source four-times-count arithmetic. Two focused fixtures pass. Imported character conversion and UTF-8 decoding remain boundaries; native locale and gameplay behavior are unvalidated.

## Root direct continuation: reverse cleanup continuation (2026-10-10 UTC)

Recovered 82B7AE18 completes the reverse cleanup continuation from a captured end pointer. The focused destruction fixture now executes that loop and verifies callback order instead of mocking it. Native exception unwinding remains unvalidated.

## Root direct continuation: compose string lifetime (2026-10-10 UTC)

Action creation and destruction now compose recovered string construction/reset/release. Empty record labels execute the constructor directly; record cleanup executes string release instead of synthetic destructor callbacks. The destruction fixture observes all 1024 actual manager releases in source order. Storage and six caller fixtures pass. Nonempty character conversion, allocator internals, exceptional cleanup continuation and native gameplay remain unvalidated boundaries.

## Root direct continuation: string storage adapters (2026-10-10 UTC)

String storage now has full-register logical adapters for construction, reset and release, reusing the accepted array and manager implementations. A compact fixture exercises empty/nonempty strings and local/heap conversion buffers. The conversion helper and allocator remain service boundaries, and action-chain composition is next. This is not native runtime or bitwise equivalence validation.

## Root direct continuation: battle action destruction (2026-10-10 UTC)

Action record destruction now follows both 32-slot sections and their 16-string reverse cleanup loops through recovered code. Storage reset composes this path. A focused fixture checks callback order, empty loops, cleanup routing and ABI; storage and execution caller fixtures pass. String destruction and the exceptional cleanup continuation remain guest boundaries; native unwinding and gameplay are not validated.

## Root direct continuation: compose action storage (2026-10-10 UTC)

Recovered action storage is now composed into record creation and the reset-effect wrapper. Six caller fixtures pass through real growth, zeroing and nested record initialization instead of the former synthetic initializer. Narrow allocator, empty-string and record-destructor services remain fixture boundaries. Native gameplay and bitwise floating-point acceptance remain unvalidated.

## Root direct continuation: battle action storage (2026-10-10 UTC)

Four action-storage helpers now have logical implementations, including the 124208-byte record initializer and its nested defaults. A focused fixture verifies growth, repeated append, normal and alternate arrays, reset and ABI preservation. Existing ResizeArray logic is reused. String and destructor services remain boundaries; composition into the action execution chain is next. This is not native gameplay validation.

## Root direct continuation: battle action effects and lifecycle (2026-10-10 UTC)

Four action-effect and lifecycle helpers now run inside resource record creation: command routing, reset sequencing, actor completion marking and the resource-state cycle. A focused fixture checks all command routes and state transitions; the five affected chain fixtures pass using actual completion fields. Record-array initialization and concrete effect services still require recovery, and native gameplay is unvalidated.

## Root direct continuation: action-record chain integration (2026-10-10 UTC)

The recovered execution layer now calls the resource action-record implementations directly. The chain writes real record metadata and target lists, including linked-member flags and extra-target deduplication, instead of stopping at mocked emission callbacks. The four affected opcode/execution fixtures pass; their remaining synthetic record-array initialization is explicitly isolated. Guest allocation/reset, command configuration and finalization services are still unvalidated boundaries, and there is no native gameplay claim.

## Root direct continuation: battle resource action records (2026-10-10 UTC)

Four lower-level resource action-record routines now have logical implementations. They write record metadata, append target IDs, preserve prior-record flags, insert linked members and deduplicate additional targets. A compact synthetic fixture passes. Array initialization, command configuration and finalization remain service boundaries; integration into the execution-layer emission calls is pending, with no native gameplay claim.

## Root direct continuation: battle script skill cost and kind classification (2026-10-10 UTC)

Skill availability and action-kind classification now run inside the recovered picker chain. The source property-mask gate, category-specific cost table, signed kind thresholds and distinct unordered comparison behavior are retained. A compact helper fixture and the affected picker and marshaling fixtures pass. Dynamic cost adjustment remains a service boundary; native gameplay is unvalidated.

## Root direct continuation: battle script prioritized action pickers (2026-10-10 UTC)

Three action pickers and the 512-record learned-action eligibility scan now feed the recovered target-preparation and execution chain. Candidate priorities, exact-one skill gating, random tags and private table reads follow the source. The category flag selects preparation mode2 or17. A focused picker fixture and the updated marshaling chain fixture pass; concrete skill checks, kind lookup and native gameplay remain unvalidated.

## Root direct continuation: battle script target preparation chain (2026-10-10 UTC)

The eighteen-mode target preparation helper now composes the recovered pool builder and is connected through action execution and the calling opcodes. It preserves property-based overrides, random tags, selected-list and group selection, the source mode16 fallthrough and empty-pool behavior. A focused fixture and three affected chain fixtures pass. Concrete resource services, pathological random paths and native gameplay remain unvalidated.

## Root direct continuation: battle script action execution chain (2026-10-10 UTC)

Five action helpers are now implemented and composed into the existing action, preparation and item-selection opcodes. The chain emits first and subsequent target records, handles delimiters and busy indices, expands linked groups and consumes the correct inventory bank. A focused helper fixture and the three affected opcode fixtures pass. Concrete resource action builders and target preparation remain boundaries; native gameplay is unvalidated.

## Root direct continuation: battle script constructor handler coverage (2026-10-10 UTC)

The final two distinct handlers in the battle-script constructor table now have logical implementations: banked item-action selection and the 39-mode global transition instruction. Source tie-breaking, random tag88, zero-ID execution, first-group early exits and controller routing are preserved. Two small synthetic fixtures pass. Constructor handler coverage does not mean the engine service boundaries or native gameplay are complete.

## Root direct continuation: battle script scene and message commands (2026-10-10 UTC)

Ten remaining service-facing battle handlers now preserve scene command arguments, resource level refresh ordering, health and MP refill fields, four-way message routing and short-label glyph substitutions. Overlapping source operand offsets and zero-coordinate fallback remain intact. A compact synthetic fixture passes; concrete scene, resource refresh and message services and native gameplay remain unvalidated.

## Root direct continuation: battle script queues and resource state (2026-10-10 UTC)

Fifteen additional battle handlers and the bulk resource-action reset helper now implement expected/actual queue construction and comparison, source duplicate insertions, resource field updates and flag transitions. Allocation sizes, signed jump targets and numeric argument conversion follow the source. One compact synthetic fixture passes; concrete external services and gameplay remain unvalidated.

## Root direct continuation: battle script owned scene labels (2026-10-10 UTC)

Seven scene handlers now build the original owned UTF16 label descriptors, preserve allocation and copy call contracts, and dispatch mode-controlled and predicate commands. The source parameter/payload offset overlap is retained. A compact synthetic fixture passes empty and nonempty labels, allocation alignment, descriptor self pointers, mode flags and branching. Concrete string, allocator and scene services and native gameplay remain unvalidated.

## Root direct continuation: battle script preparation and scene marshaling (2026-10-10 UTC)

Four battle instructions now route preparation modes and marshal scene label and boolean commands. The original manager-state fallback, failure result, packed constant numeric arguments and 32-unit label copy are preserved. A separate source loop repeatedly overwrites its first label unit; recovery intentionally preserves this behavior. One synthetic fixture passes. Concrete preparation and scene services and native gameplay remain unvalidated.

## Root direct continuation: battle script status and predicate dispatch (2026-10-10 UTC)

Fifteen further battle instructions now connect readiness checks, the 60-tick phase handshake, action-result consumption, indexed resource state, group clearing, actor activation and scene predicates. The source low-byte result and exact-one branch rules, signed jump targets and suppression bypass are preserved. One compact synthetic fixture passes; concrete scene services and native gameplay are not validated.

## Root direct continuation: battle script resource mode dispatch (2026-10-10 UTC)

Two multi-mode resource opcodes now update original flag/indexed fields, synchronize resource refresh data, query banked state, pass packed numeric scene arguments and select among up to four eligible same-side resources. The equipment-like slot update preserves its unconditional follow-on service call, including when the local slot list is full. A compact synthetic fixture passes; the original empty-candidate random call is retained without invented protection or runtime acceptance.

## Root direct continuation: battle script scene state (2026-10-10 UTC)

Scene-state handlers now preserve inline text buffers and actor labels, dispatch object commands, queue flag changes and apply the original suppression/state predicates. Affordable action selection scans the twelve source candidates, retains cost-compatible entries and invokes the original random contract. A compact synthetic fixture checks these data and callback paths; native scene effects, concrete costs and text rendering remain unvalidated.

## Root direct continuation: battle script runtime commands (2026-10-10 UTC)

Runtime commands now resolve group/resource identifiers, find script actors bound to runtime resources, count flagged bindings, and marshal scene commands through their original direct and virtual services. Target selection and scene predicates retain their original return conventions, including no cursor advance for unsupported predicate modes. The compact state/callback fixture passes; native scene and service behavior remains unvalidated.

## Root direct continuation: battle availability-filter target selection (2026-10-10 UTC)

The companion availability-filter selector now builds mode1 resource pools, retaining active resources even when HP is zero or a property mask would exclude them from the normal selector. It filters through the original resource virtual slot292 nonzero result and writes all selected IDs or one random choice with tag86. The existing compact fixture exercises both list and random paths with a zero-HP resource; concrete readiness semantics remain a guest service.

## Root direct continuation: battle script resource queries (2026-10-10 UTC)

Resource query handlers now count selected categories, trace pending action records back to actors targeting a selected resource, deduplicate their IDs and select through the original random service contract. Property/effect, target-byte, actor parameter and banked inventory queries are also recovered. The detail-only action-filter exclusion and special inventory field are preserved. A compact synthetic fixture passes; native action lists, readiness and inventory providers remain unvalidated guest services.

## Root direct continuation: battle target-list refinement (2026-10-10 UTC)

The companion target-refinement opcode now filters an existing byte-ID list without rebuilding runtime pools. It shares the original common filter logic while preserving its distinct category bit28, secondary-stat percentage filter5, 752-byte frame and random-service tag85. The focused fixture now composes initial selection with refinement, checks two team categories, secondary-stat thresholds, random tag and empty-list handling. Concrete game services and native combat remain unvalidated.

## Root direct continuation: battle script target selection (2026-10-10 UTC)

The battle target-selection path now partitions runtime resources into the original six pool counts, constructs candidate groups, applies numeric/status/group/action/inventory filters and writes either the full target list or one random selection. Duplicate field matches and later equal-score replacements are preserved. A small four-resource fixture connects these stages through the real operand and property-mask helpers; runtime combat and every native filter combination remain unvalidated.

## Root direct continuation: battle script action history (2026-10-10 UTC)

Action-history commands now clear eight records, extract the newest record and search with the original descending/repeated-match semantics, including omission of slot zero. Additional handlers transfer scaled resource vectors, update manager/resource flags, clear linked resource groups and finish deferred actions. A compact synthetic state/callback fixture passes; actual battle history, native vector constants and completion effects remain unvalidated.

## Root direct continuation: battle script action routing (2026-10-10 UTC)

Action handlers now connect readiness predicates, target selection, preparation flags and event-specific action dispatch. Resource queries, indexed state resets, conditional counters and target effect commands compose with the recovered operand and actor lookup engine. Property-mask helpers read the original guest mapping table instead of assuming a new host mapping. A compact synthetic fixture passes; concrete action execution and effects remain explicit guest boundaries, so actual combat behavior is not yet validated.

## Root direct continuation: battle script presentation modes (2026-10-10 UTC)

Three presentation handlers now marshal scalar floats, packed vectors/colors, integer commands and service state across their original mode tables. The command ranges preserve default selectors even for values between named modes. Numeric integer conversion retains the source float roundtrip. A compact table-driven fixture covers the dispatch families and packed ABI payloads; concrete presentation services and native visual effects remain outside this recovery stage.

## Root direct continuation: battle script resource binding (2026-10-10 UTC)

Actor binding now distinguishes resource IDs from group IDs, removes an existing script binding when required, binds an available resource, and preserves the original stop behavior when no resource is present. Visibility updates retain their low-bit semantics and scene-specific notification exception. Battle transition, flag and state-query handlers are composed with the existing operand engine. A small synthetic fixture passes; runtime actor lists, controller callbacks and actual battle transitions remain guest services.

## Root direct continuation: battle script scene commands (2026-10-10 UTC)

Scene-service handlers now perform predicate branches, command dispatch, packed XYZ argument construction, result writeback and numeric parameter calls. The inline 32-unit text payload retains original byte order and expands its flag byte before invoking the presentation service. Original invalid mode behavior is preserved, including no cursor movement in the seven-way predicate. A compact synthetic callback fixture passes; concrete scene/presentation services remain explicit guest boundaries.

## Root direct continuation: battle script extended dispatch (2026-10-10 UTC)

Extended opcode dispatch now sets the original mode bit and tail-routes the second byte through slots 256 onward, reusing recovered handlers and explicit guest fallback. Zero-offset operand aliases, signed little-endian immediates, and two original fixed-length skip handlers are recovered. A constructor-registered prefix/skip/end sample preserves the caller LR and clears the mode bit on the next ordinary instruction. Full library compilation and the focused synthetic sample pass.

## Root direct continuation: battle script service handlers (2026-10-10 UTC)

Actor numeric state handlers now read four float fields and set/add two fields, preserving truncation and missing-resource defaults. Additional opcodes write an indexed play-data table, marshal integer/scaled-float arguments to eight original virtual slots, capture numeric results, and query a chained global service. One compact synthetic fixture checks the handler contracts and stack/cursor preservation. These service calls remain explicit boundaries; their concrete implementations and native combat effects are not claimed as recovered.

## Root direct continuation: battle script text expansion (2026-10-10 UTC)

Battle text expansion now composes script scratch numbers, actor names and seven-column text-bank names into the message opcode. Literal units retain the original byte order, while inserted numbers and names follow their source representations. The formatter preserves consumed-but-unused tokens and uses capacity only for initial clearing, as the source does. A small synthetic mixed-token program and message callback pass with full library compilation; actual localized text rendering and native gameplay remain unvalidated.

## Root direct continuation: battle script party and inventory (2026-10-10 UTC)

Party and inventory handlers now update the original flag/list fields, transfer five party slots, accumulate capped inventory quantities and notify the play-data service. Recovery preserves two source quirks: list insertion may fill multiple vacancies, and the scratch-write comparison admits index 16. Reserved aliases retain their true no-op behavior without invented cursor advancement. The inventory wrapper uses its own nested frame so its temporary does not overwrite the caller saved register. Focused synthetic ownership-free state and callback checks pass; native inventory behavior and invalid input safety are not claimed.

## Root direct continuation: battle script control extension (2026-10-10 UTC)

Opcode slots 32..37 now cover actor flag synchronization, range and mask branches, tick accumulation, signed remainder and the original upper-capped game counter. The actor handler preserves its asymmetric flag rules: actor state uses the input low bit, while the resource flag uses nonzero. Small synthetic checks pass, including the original lack of a lower counter clamp. Actor lookup/effect services remain guest boundaries; no native battle acceptance is claimed.

## Root direct continuation: cross-actor script events (2026-10-10 UTC)

Cross-actor event opcodes now resolve actor IDs (including the current-actor sentinel), enqueue events, wait for event start or completion, and stop on priority barriers or full queues. Their original per-slot phase transitions are preserved. A two-actor synthetic fixture exercises these transitions and the prior subroutine sample still passes. Basic opcode slots 0..31 are backed by recovered handlers, while later game-specific instructions remain pending.

## Root direct continuation: battle script angle opcodes (2026-10-10 UTC)

The fixed-turn sine/cosine and direction-angle opcodes now compose the previously recovered guest-table polynomial and rational atan2 routines. The trig evaluator is shared with the existing hull math implementation without changing its formula. Private-constant fixtures check scaled outputs, zero/axes, cursor/stack preservation and shared-helper equivalence. No constant bytes are published, and this does not extend validation to exceptional FP or gameplay. Basic opcode slots 0..27 are now backed by recovered handlers.

## Root direct continuation: battle script subroutines (2026-10-10 UTC)

The opcode loop now also composes multiply, signed divide, operand swap and subroutine call/return. Call pushes the next instruction offset onto the original 32-entry actor stack; return pops it or ends the event when empty. A constructor-registered call/multiply/return/end program passes, alongside division/swap and stack-full stopping. These are original logical semantics, not a hardened VM or complete instruction-set claim.

## Root direct continuation: battle script registration (2026-10-10 UTC)

The battle manager constructor now installs its guest vtable and 272 handler pointer slots while clearing only the original actor/script fields and byte flag. Its address setup was resolved with the original stack-spilled bases. The synthetic core program now uses this constructor-installed table successfully. Registration counts as one recovered constructor; external pointer targets remain runtime boundaries until individually recovered.

## Root direct continuation: battle script core opcodes (2026-10-10 UTC)

The basic opcode range 0..19 now resolves to recovered handlers: end, signed jump, conditional branch, assignment, wait, boolean set/clear, arithmetic, indexed bit changes, increment/decrement, bitwise operations, shifts and random assignment. The writable operand helper preserves original local/script bounds and read-only classes. A small assignment/add/conditional/wait/end program runs entirely through recovered handlers, apart from runtime services; random generation remains a guest boundary. Constructor registration addresses were checked with its spilled address bases, not guessed from proximity. This is core opcode coverage, not the full instruction set.

## Root direct continuation: battle script event dispatch (2026-10-10 UTC)

Event insertion, completion-triggered events, sixteen-slot priority scheduling and the opcode dispatch loop are now recovered and composed into battle updates. The dispatch loop executes the recovered wait opcode locally, with other handlers delegated through the guest opcode table. A small synthetic program runs a callback opcode, initializes and resumes a wait over multiple frames, then exits; insertion, duplicate suppression and priority tie behavior also pass. This connects dispatch infrastructure, not every game opcode, and does not claim gameplay acceptance.

## Root direct continuation: battle script parameter and action helpers (2026-10-10 UTC)

Parameter evaluation now resolves the original local, global, script, bit, constant and actor-field operand classes. Periodic action thresholds and queued actor flag changes are recovered and composed into the existing update/wait logic. Synthetic parameter, threshold/sentinel and queued-flag samples pass, as does the updated lifecycle/wait fixture. Actor lookup and action execution remain guest boundaries; diagnostics and the full interpreter remain unrecovered.

## Root direct continuation: battle script lifetime and timing (2026-10-10 UTC)

Battle script storage initialization/release, per-frame integer tick updates and the timed-wait opcode are recovered. The original integer conversion still yields zero ticks at 120 Hz; this semantic layer preserves that behavior rather than silently incorporating a new timing fix. The diagnostic/step boundary retains LR 8238AD64 and f31 so the existing fractional-tick hook can recognize its caller. Synthetic checks cover lifecycle ownership, first-frame gating, wait transitions and actor update selection. Parameter evaluation, diagnostics and actor sub-updates remain guest services; no full interpreter or gameplay acceptance is claimed.

## Root direct continuation: DLC overlay registration (2026-10-10 UTC)

DLC index registration now composes the resident loader, reuses vacant pointer slots or appends segmented storage, and rebuilds the stable descending priority order from owner+340. A synthetic three-owner case, hole reuse and segment growth pass. Path formatting and runtime file/lock/allocation services remain guest boundaries. The original nonzero loader-result insertion gate is preserved, even though its final return requires result 1; failure-path insertion has not been fixture-tested.

## Root direct continuation: archive startup (2026-10-10 UTC)

Base archive startup now constructs the root and filename, normalizes directory separators while preserving multibyte trail bytes, loads the resident index through the recovered loader, and updates the source readiness flags. A synthetic startup chain passes with the expected path, index and service calls. The startup wrapper preserves its original unconditional success return, while the outer initialization returns the registry singleton result.

## Root direct continuation: FPI index loading (2026-10-10 UTC)

The FPI loader now reads and converts the 64-byte header, enforces the original 1..512-sector size range, replaces the resident index buffer, and relocates archive descriptors and nested entry trees. Locked reads retain their guest status exchanges. Two endian variants and an invalid-sector sample pass with synthetic file services. Open/read/close, allocation, error reporting and atomic operations remain runtime boundaries. Auxiliary table relocation follows the original swap-only branch and has not been exercised by this fixture.

## Root direct continuation: CPX stream orchestration (2026-10-10 UTC)

CPX stream orchestration now composes the recovered block decoder and context helpers with split reserve handling, chunked reads, time-budget yields and completion cleanup. Registry unlink and context shutdown are also recovered. Two small synthetic paths cover a split-reserve two-block stream and a plain read; a separate lifecycle fixture checks linked-list removal and ownership. Runtime I/O, clock, allocator, registry construction and atomic status exchange stay explicit service boundaries. Cross-boundary scratch assembly and allocation failures are source-reviewed only; this is not native archive or multithreaded acceptance.

## Root direct continuation: overlay archive lookup (2026-10-10 UTC)

The archive lookup entry now normalizes case, slash and hyphen spelling, searches overlay archives in configured order under guest lock boundaries, and falls back to the base archive. It composes the recovered prefix/member lookup and preserves the original 240-byte output initialization. Synthetic overlay success and miss-to-base cases pass. Overlay root formatting and kernel locking/time services remain guest boundaries, not recovered filesystem behavior.

## Root direct continuation: archive member lookup (2026-10-10 UTC)

Archive lookup now composes candidate prefix selection with recursive 24-byte entry matching, decoded names, packed timestamps and output metadata. Focused synthetic checks cover regular files, directories (type 16), exact archive matches (type 17), loose members (type 0 with flag 4), path append and misses. Guest time conversion remains an explicit boundary; no archive file I/O or gameplay acceptance is claimed.

## Root direct continuation: archive metadata (2026-10-10 UTC)

Archive metadata now decodes the packed year/month/day/time fields, bridges them to the guest FILETIME conversion and applies the original guest-pattern suffix rewrite. The weekday calculation preserves the source single month-table load; the bridge intentionally omits that weekday field. A focused fixture checks field order, success/failure output handling and suffix matching. Kernel calendar/error services remain explicit guest boundaries.

## Root direct continuation: packed archive names (2026-10-10 UTC)

Archive names now expand packed base-40 halfwords through the guest alphabet, fold ASCII case and append descriptor suffixes/extensions. Prefix candidate selection preserves path boundaries, exact-match early exit and the original low-byte insertion ordering. A synthetic guest-table fixture validates the connected path without publishing private tables. Full member lookup and native archives remain pending.

## Root direct continuation: text-bank parsing and consumers (2026-10-10 UTC)

The documented text-bank chain now decodes offset-based UTF-16/narrow string tables, constructs one-column and seven-column (84-byte) records, releases consumed input/temporary strings and resolves menu text IDs through 60-byte rows. Existing recovered string assignment is reused through allocator callbacks. Small fixtures verify ignored length fields, empty/disabled records, replacement ownership and menu fallback. This is logic recovery, not a completed localization importer or game-runtime integration.

## Root direct continuation: CPX context and block index (2026-10-10 UTC)

CPX context recovery now owns the copied header/index, exposes block offsets and completion, reuses lazy 65552-byte scratch and releases its owned buffers. A two-block synthetic file runs indexed lookup through the recovered decoder with exact output and full cleanup. Lazy manager initialization remains an explicit guest boundary and preserves the requested allocation arguments. The asynchronous reader and registry integration remain pending.

## Root direct continuation: CPX block decoding (2026-10-10 UTC)

CPX block decoding now composes bit input, parameter tables, match lengths and literal/overlapping-back-reference output in both halfword and byte modes. Eight tiny blocks validate stored/no-copy and all three distance modes, with byte counts and block counters. The complete archive streaming/in-place orchestration remains pending; native-asset and gameplay acceptance are not claimed.

## Root direct continuation: archive index fields (2026-10-10 UTC)

Using the 72a5abdb findings, archive_index_fields61 now recovers descriptor field swapping, recursive entry-tree swapping/relocation and CPX reserve sizing. Direct caller inspection distinguishes 48-byte archive descriptors from 24-byte entries; the ledger wording and search labels are corrected accordingly. A nested fixture checks both endian modes and untouched fields. Full loader/decompression remains pending.

## Root direct continuation: complete cloth cooking (2026-10-10 UTC)

BAC6D0 now composes cloth descriptor validation, concrete import/topology, bounded scheduling, vertex permutation, inverse mapping, channel/index remapping and reversed tier output. B9CE98/B9D0A0 convert the public triangle/tetrahedral descriptors and cook/write CLTH with cleanup. Four type/endian combinations validate strided channels, duplicate triangle mapping, byte-identical stream roundtrip and readable public-entry output with complete ownership cleanup. This closes the synthetic cloth cooking logic chain; moving realloc, independent native assets and gameplay remain unvalidated.

## Root direct continuation: cloth constraint scheduling (2026-10-10 UTC)

BB6290 now builds adjacency, ranks compatible constraints, splits bounded child batches, advances conflict tiers and packs triangle/tetrahedral records. Three samples cover disjoint batch splitting, a connected chain requiring a second tier, both packed layouts and complete ownership cleanup. The input descriptor/orchestration and final vertex/channel remapping remain pending.

## Root direct continuation: cloth scheduling support (2026-10-10 UTC)

BB56D0/BB59A8 now build vertex-to-constraint adjacency and select scheduling candidates by bucket compatibility, matched-vertex count and bounding-box shape. A small boundary/tetrahedral sample validates stable incidence lists, ranking, tie breaking, conflict rejection, repeat rebuild and complete cleanup. The higher-level scheduler and complete cloth cooker remain pending.

## Root direct continuation: cloth vertex permutation (2026-10-10 UTC)

BB5518/BB7B48 now sort signed bucket/local-index keys and produce the original-to-packed vertex permutation. The first assigned bucket among the original three candidates wins; unassigned vertices sort before assigned ones. A five-vertex sample with three child lists validates local order, output shrinking, repeated reuse and complete cleanup. This connects output packing support; constraint scheduling and the complete cloth cooker remain pending.

## Root direct continuation: cloth tetrahedral constraints (2026-10-10 UTC)

BABE50 now builds one 68-byte constraint per tetrahedron, retaining input vertex order, signed six-volume and six edge lengths. Sorted endpoint/cell records assign each shared edge to its first cell, with negative lengths in later cells. A two-cell shared-face sample checks output shrinking, repeated reuse and full cleanup. Reallocation is in-place in this fixture; this is logical/ABI recovery, not bitwise FP or full cloth cooking.

## Local findings incorporated (2026-10-10 UTC)

Continuation is based on maintainer commit 72a5abdb6310e9ba43e232a19b6ab73706e79e23. Consult [guest function findings](../docs/notes/guest-function-findings.md) and its linked Ghidra annotations before selecting or naming further recovery targets. The ledger has 538 address rows (349 functions, 86 instruction sites, 56 globals, 33 data, 14 vtables); these are evidence/search labels, not implementation or runtime credit. Its archive/CPX, language/text-bank and battle-script chains provide concrete next targets after the current cloth chain. Source paths refer to main at 7d3c66a6 or explicitly named maintainer-local research, so unavailable local sources must not be presented as independently verified. Existing post-resume implementation overlap is 822A2FE0; retain its documented call-site evidence without promoting its uncertain purpose. No cloth cooking entries are supplied by this ledger.

## Root direct continuation: cloth triangle edge constraints (2026-10-10 UTC)

BAB468 now groups canonical edges from nonduplicate triangle faces and emits 68-byte cloth constraints, with endpoint/opposite vertices, edge lengths, opposite-vertex separation and the original polynomial angle approximation. A quad plus a duplicate face yields five edges and one shared diagonal with full cleanup. Only a private local 20-byte coefficient bundle is used; no original constants are published. The cloth scheduling/packing stages and full cooker remain pending.

## Root direct continuation: cloth strided import (2026-10-10 UTC)

BA8AE8/BA9530 now append strided triangle/tetrahedral inputs into owned cooking vectors, including optional per-vertex float/word channels, halfword/word indices and triangle winding selection. Four small combinations validate the data flow and ownership cleanup. These importers preserve the original growth path and early empty-input rejection; no additional validation layer or complete-cooker claim was added.

## Root direct continuation: cloth canonical face mapping (2026-10-10 UTC)

BA8208 canonicalizes each triangle by sorted vertex IDs, sorts the resulting key/face records and maps duplicate faces to the lowest original face ID. The owned output vector is resized and shrunk through SDK callbacks. A five-face sample validates orientation-independent duplicate classes, repeated reuse and complete cleanup. This connects one concrete topology step; the complete cloth cooker is still pending.

## Root direct continuation: cloth topology support (2026-10-10 UTC)

Cloth topology support now sorts 16-byte records by two or three keys and 12-byte triples by three keys, performs the original unique-pair lookup, and exports borrowed triangle/tetrahedral mesh descriptors. A focused fixture exercises each sort, successful and ambiguous lookup, and both descriptor layouts. These are prerequisites for topology generation; no full cloth-cooking or simulation claim.

## Root direct continuation: cloth stream readback (2026-10-10 UTC)

BAA130 now reads CLTH into owned guest vectors, retaining existing capacity across repeated loads, growing through the SDK allocator, building/shrinking an inverse permutation and restoring nested records. Both source topology types round-trip in both endian modes with exact cursor and complete cleanup. A repeated read verifies reuse and nested replacement. Fixture realloc is in-place; moving realloc and native-asset/gameplay integration remain unvalidated. Full cloth topology generation and cooking orchestration are still pending.

## Root direct continuation: cloth serialization (2026-10-10 UTC)

BA7760 now writes NXS/CLTH version3 streams for the two source topology types, including positions, indices, per-face data, auxiliary arrays and nested 32-byte constraint records with their distinct packed wire layouts. Four focused type/endian combinations verify key fields and exact 162/166-byte lengths. The cloth loader, topology generation and complete cooker remain pending; no native-asset or game-runtime acceptance is claimed.

## Root direct continuation: cloth cooking storage (2026-10-10 UTC)

Started the adjacent NXS/CLTH cloth-cooking chain with concrete storage ownership: ten vector descriptors, nested mesh release, capacity-retaining clear, full destruction and a 32-bucket workspace. A focused lifecycle sample releases all tracked storage. Cloth serialization, topology and the cooking main entry are still pending; this does not claim simulation or gameplay support.

## Root direct continuation: triangle runtime views (2026-10-10 UTC)

Triangle runtime views now expose channel count/format/stride metadata, optional auxiliary records and borrowed mass cache export. The three full-cook samples validate channel metadata and exported mass words after reload. Optional auxiliary data and absent-cache behavior remain source-reviewed; no defensive fallback or new runtime promise was introduced.

## Root direct continuation: complete triangle cooking (2026-10-10 UTC)

The default triangle cook entry B9CC00 now composes BA65C0 descriptor handling and BA6238 strided import with the actual clean/weld, BC1F00 convex grouping, BB4CF0 owner export, tree, bounds, edge flags, mass and NXS/MESH serialization. A tetrahedron succeeds through word-indexed, half-indexed and nonindexed inputs, then reloads with exact cursor and zero tracked ownership. One convex group and four angular categories are verified. The focused sample uses synthetic version17, so native asset compatibility and gameplay remain unproven. Complex concave/degenerate grouping, optional user callback and axis-plane variants are source-reviewed only. This supersedes the earlier pending-default-triangle-entry note.

## Root direct continuation: triangle partition support (2026-10-10 UTC)

Triangle partition support now initializes and releases label arrays, merges sufficiently aligned face labels using guest atan2, compacts labels, traverses edge-incidence components with a guest FIFO, and applies the original two-sided plane acceptance before extending a convex group. A focused sample checks merging, relabeling, FIFO reset, connected traversal and face acceptance with complete cleanup. The higher-level BC1F00 partition orchestrator remains pending; these helpers alone do not constitute the full default triangle processing entry.

## Root direct continuation: triangle owner serialization (2026-10-10 UTC)

Triangle owner serialization now composes storage, tree envelopes, coupled material/remap arrays, optional group/category data, edge flags and mass cache. BA6868 computes tetrahedron mass/centroid and reuses the cache; BC5ED8 builds the owned edge topology. BA6B18/B9D4F8 round-trip a nine-triangle fixture in both endian modes with exact cursor and complete cleanup. BAE0C0 preserves scalar byte/half input callbacks. The open-surface I/O fixture uses a pre-existing zero mass cache and synthetic version 17; compatibility with an independent native asset is not claimed. Wider index and negative-mass paths remain source-reviewed. The full default triangle processing entry and graph partition stage are still pending.

## Root direct continuation: triangle edge flags and bounds (2026-10-10 UTC)

BB42E8/BB4F40/BB5230 now sort and group triangle edges, retain incident face IDs and derive per-face flags. A coplanar quad gives expected diagonal bits with repeated replacement and full cleanup. BA6458/B9D410 reuse guest power and sphere solvers for bounds/tolerance and distinguish the two axis-plane sides. Negative-plane extension and both axis encodings pass. The descriptor option previously described as a nondefault leaf limit is an axis-plane option; corrected that earlier wording. No additional guards or full floating-point/gameplay claim.

## Root direct continuation: triangle cleanup and edge separation (2026-10-10 UTC)

BB4540 now composes existing indexed workspace cleanup, replaces position/triangle arrays, preserves changed original-face/material mapping, builds edge incidence and separates extra triangle pairs on nonmanifold edges with the original small bit-pattern position perturbation. BD9188 replaces the first matching triangle index. A four-face shared-edge fixture welds seven vertices to six, separates to eight, verifies every undirected edge has at most two incident faces and releases all tracked storage. A missing fixture growth constant initially collapsed buffers; seeding the original ordinary value 2 resolved it without implementation changes. Logical/ABI scope only; broader triangle orchestration remains.

## Root direct continuation: triangle tree and remapping (2026-10-10 UTC)

BB4160 builds the triangle spatial tree with a live BB4138 callback into existing mesh_attribute_reorder61 (BB3CF8, no duplicate implementation or credit). BC5DE8 binds geometry and replaces/loads the tree. A nine-separated-triangle path exercises actual reordering, keeps material/face mappings aligned, writes OPC/HBM through the linked writer, reloads through concrete memory input and releases all tracked allocations. The descriptor callback at82BB4138 is executable code, not a vtable. Nondefault axis-plane options remain source-reviewed. Logical/ABI scope only; higher triangle cooking still pending.

## Root direct continuation: triangle mesh storage and normals (2026-10-10 UTC)

Triangle mesh recovery now includes construction/defaults, vertex/triangle/material/remap arrays, layered release and deleting teardown in mesh_triangle_storage61. BC5D70/B9E0D8 lazily provide normals through mesh_triangle_normals61: BCA2E8 uses recovered guest atan2 for corner weights, BCC470 computes oriented faces, accumulates weighted vertices and retains first-face/Y-axis fallback. Two focused lifecycle paths validate normal values, cache reuse, prefixed nested links, sentinel ownership and complete tracked cleanup. Existing F2B308/BC3EC0/empty leaves are reused without duplicate credit. Floating-point/volatile equivalence and full triangle cooking/loading are not yet claimed.

## Root direct continuation: optional support-map load connected (2026-10-10 UTC)

BC8438/BC62D8 close ICE/GAUS counts and ICE/SUPM combined dual-byte-table input. Both byte orders round-trip with original owned-buffer aliases and concrete cleanup. A 40-point Fibonacci-sphere input now exercises actual support sampling, emits 8568 bytes, loads completely, scales by two, exports and reloads with doubled positions and zero tracked allocations. Three smaller indexed/plain/inflated paths also pass. Fixture bump-allocation window and fixed output capacity were enlarged to accommodate the longer sample; no production guard or failure policy was added. Prior optional-support load boundary is now concrete. Legacy format branches, bitwise floating-point and gameplay remain unverified.

## Root direct continuation: complete load-scale-export chain (2026-10-10 UTC)

BC4D80/BC5270 now bind and load complete cooked geometry, tree, bounds and mass data. BD15C8/BD1D08 restore OPC strategy and HBM mapping payloads; existing BD2200 ownership is reused without duplicate credit. B9C670 composes load, uniform scaling, export and cleanup. Actual indexed, point-only and inflated point-only files pass complete load, scale by two, export, reload, doubled coordinates and exact end cursor with no tracked allocations left. The inflated case includes a real quantized compact tree. Optional large-mesh/legacy support-map input remains a live untested callback. Logical/ABI scope only; no full-RAM, exhaustive legacy/endian or gameplay acceptance.

## Root direct continuation: cooked geometry readback (2026-10-10 UTC)

BC6C20 now loads the ICE/CVHL aggregate geometry, relocates serialized polygon pointers, reconstructs adaptive triangle/edge indices and reads packed normals. BC8638 composes ICE/CLHL geometry and VALE adjacency. All three actual cook-main outputs (indexed, point-only, inflated point-only) reload their geometry and release every tracked allocation. No new input guards; original current-format ownership and mutation order retained. Older format branches are source-reviewed only. Overall BC5270 load and tree restoration remain next; no full-RAM, bitwise floating-point or gameplay acceptance.

## Root direct continuation: packed normal input (2026-10-10 UTC)

BC69E0 reads packed halfword normals, lazily generates the original 1024-entry sorted spherical lookup using concrete guest sine/cosine, and applies component/sign masks. Six signed axes pass both byte orders; lookup reuse and original830D9A60 registration target are checked. The angle step at820D6954 remains a private four-byte test input. CRT registration remains explicit. This prepares BC6C20 geometry loading; no full-grid/bitwiseFP/Full72/gameplay acceptance.

## Root direct continuation: VALE adjacency loading (2026-10-09)

BC7F98 now reads ICE/VALE, replaces its owned combined degree/edge buffer, expands packed degrees into four-byte degree/prefix records, reads adjacency bytes and computes halfword offsets with the existing BC7F48 wrapping-prefix semantics (no duplicate entry credit). Existing writer and new reader round-trip both byte orders with concrete stream adapters and full tracked cleanup. Original writer differential coverage stays separate from these logical reader checks. Allocation failure/count mutation order is retained; no new guards. Full geometry loader remains pending.

## Root direct continuation: ICE/adaptive index input (2026-10-09)

Eleven additional mesh_stream_codec61 entries cover six borrowed reader tailcalls, ICE header parsing, halfword/word spans and adaptive u8/u16/u32 index decoding. Existing two-endian smoke paths now parse ICE through actual wrapper tails and exercise all three input widths into word output. Halfword-output adaptive mode and floating adapter tails remain compile/source-reviewed. Stack probing is reused for temporary widening buffers. No extra input bounds/rollback policy. Geometry/valence loading remains next.

## Root direct continuation: mesh stream codec for load integration (2026-10-09)

`mesh_stream_codec61` adds nine logical NXS header/scalar/array read-write entries and forwards three existing float/header writers to mesh_stream_write61. Both endian modes round-trip header/version, u16/u32/f32 and a float array through concrete memory input/growable output; wrong format tag stops after8 bytes. No new stream bounds, rollback or error policy. u16-array writer is compiled/source-reviewed; no full-RAM/volatile/nonfinite/bitwiseFP matrix. Next load integration targets BC4D80 tree binding and BC5270 overall cooked-mesh load, with geometry/valence reader dependencies still to recover.

## Root direct continuation: cooked geometry scaling (2026-10-09)

`mesh_cook_scale61` recovers B9EC98 uniform scaling, B9F188/B9F190 count getters and B9CAF8 active settings update. Positions, face quantities, bounds, radius and centroid use the scale factor; inertia uses its square, while relative tolerance uses recovered guest power. Source settings choose explicit virtual refresh or concrete tree rebuild. One focused synthetic scale2 path checks derived data, getters/settings, ABI and a recorded accepted refresh boundary. B9C670 load/scale/export wrapper and BC5270 load are the next related integration targets. No Full72/bitwiseFP/gameplay claim.

## Root direct continuation: hull scratch registration and exit callbacks (2026-10-09)

First-use clip registration now checks the actual signed-addi callback targets830D9990/830D9930. Earlier untested constants incorrectly used830E9990/830E9930; corrected. Both original scratch destructors are concrete and release/zero their respective arrays. Focused smoke enters with unset registration flags, records exactly two accepted CRT registration calls, runs repeated clipping, then allocates/destructs both scratch arrays. CRT registry internals remain an explicit82B7BE48 boundary, not a no-op implementation. Full library passes.

## Root direct continuation: three complete cook-main paths (2026-10-09)

B9C7D8 now executes indexed, plain point-only and inflated point-only tetrahedron paths through concrete geometry, tree/bounds/support, NXS/CVXM serialization and complete owner/temporary cleanup. Flags0 and4 each emit564 bytes; flags12 emits1025 bytes. All return success with NXS headers, stack/low-LR preservation and zero tracked surviving allocations. Expanded smoke bump-allocation guest window accommodates the longer inflated pipeline; no implementation guard was added. Hull scratch first-use registration flags are explicitly seeded. These are logical main-path smoke checks, not original Full72, byte-identical cooked files or gameplay acceptance. Existing warnings/diagnostics and source failure ownership remain.

## Root direct continuation: inflated hull clipping connected (2026-10-09)

`mesh_hull_polyhedron61` adds eleven logical entries for matrix operations/three-plane intersection, compact half-edge box construction, validity checks, clipping-plane selection, clipping, inflation/output packing and release. BA5480 is connected in preprocessing. Focused box/half-box tests pass; BA5CF8 with cube inflation 0.1 returns 8 points and 12 triangles spanning [-0.1,1.1] in all axes, with complete tracked ownership cleanup. Clipping uses high-level host temporary vectors/maps and concrete guest outputs, not original scratch/volatile/bitwise-FP equivalence. First-use scratch callback registration remains explicit accepted guest 82B7BE48; smoke starts with registration flags already set. Full library and prior hull smokes pass. No gameplay acceptance.

## Root direct continuation: support planes for inflated hulls (2026-10-09)

BA4BF8 now concretely builds plain hull faces, adds edge support planes for sharp dihedrals, removes parallel redundant faces by area, appends unique support planes and releases the face objects. BA0DD0, BA1078 and BA1EE0 provide unfiltered support and four-word plane arrays. Cube checks produce six axis planes with a 120-degree edge threshold and eighteen axis/bevel planes at 45 degrees, followed by complete tracked cleanup. The global registry buffer intentionally remains owned by the caller until release. BA5480 clipping/output composition is still pending. Logical/ABI scope only.

## Root direct continuation: plain incremental hull connected (2026-10-09)

Five additional entries in `mesh_hull_incremental61` recover guest sine/cosine polynomial, stable perturbed support, simplex selection and BA40B8 incremental hull construction. BA4A88 now calls the concrete main. Tetrahedron and cube produce 4/12 faces; BA5CF8 normalizes, compacts and packs the cube into 8 points/12 faces/36 indices, with no tracked ownership leaks. High-level scratch frames were enlarged to prevent overlap with saved nonvolatile registers; a focused ABI preservation check passes. Full library and prior preprocessing smoke pass. These are logical/ABI checks, not volatile-register/bitwise-FP or gameplay equivalence. Inflated hull BA4BF8/BA5480 remains pending. Private trig/support constants are user-XEX inputs, never repository content.

## Root direct continuation: incremental hull primitives and extrusion (2026-10-09)

`mesh_hull_incremental61` adds thirteen logical entries for vector normalization/orthogonal axes, triangle normals/visibility/noncoplanarity, eligible support search, edge slots, face registry/max-distance selection, neighbor stitching and face extrusion. Four focused paths pass, including extruding one tetrahedron face into three, retaining six faces with reciprocal adjacency and no leaked allocations after cleanup. Full library passes. These are logical/ABI implementations, not Full72/bitwise floating-point proofs. Zero-vector diagnostic remains an explicit accepted guest call. Deep incremental BA40B8, support perturbation BA2010 and simplex selection BA3BE0 still need composition.

## Root direct continuation: hull preprocessing data flow (2026-10-09)

`mesh_hull_preprocess61` adds nine logic-first entries: BA0230 normalization/epsilon dedup/degenerate box, BA0998 first-use point remap, BA0D68/BA1138/BA11D8/BA2280 array ownership/growth/append, BA4A88 plain hull extraction, BA5A70 hull-mode selection and BA5CF8 output packing/cleanup. Four focused data/array smoke checks pass; indexed B9C7D8 still emits 564 bytes and frees all tracked allocations. Full library passes. BA40B8, BA4BF8 and BA5480 remain explicit mutable guest algorithm boundaries. Driver and packaging are compiled/source-reviewed, not a closed hull-path execution or original Full72 proof. Prepared hull and enclosing wrapper use separate stack scratch; retain that separation.

## Root direct continuation: executable indexed cooking main path (2026-10-09)

B9C7D8 now composes owner allocation/construction, B9F198 geometry/tree/bounds/support, NXS/CVXM output and owner/temporary destruction. One indexed tetrahedron smoke runs the actual recovered chain, produces 564 bytes with NXS header and leaves zero live allocations. B9F198 is therefore now executed on that path, beyond its previous compile-only checkpoint. Together with the two u16/u32 input cases this is three logic/ABI smoke cases. BA5CF8 hull preprocessing is an explicit mutable guest CallDirect boundary, not recovered code or credited coverage; it is not exercised by the indexed smoke. No no-op success fallback, runtime replacement, volatile-state differential or gameplay claim. Full library passes. Next recover the preprocessing subsystem and broaden real caller integration only as needed.

## Root direct continuation: indexed input and cooking orchestration (2026-10-09)

BB9800 packed-input adapter passes one additional original-upper/shared concrete pipeline case (12 family cases total), including duplicate-point removal and complete temporary cleanup. New `mesh_indexed_cook61` adds BBB0A8 owned indexed geometry/polygon build, BB3060 adapter metadata, B9E8A0 strided u16/u32 conversion and B9F198 validation/hull-or-indexed/tree/bounds/support composition. Two strided tetrahedron smoke paths check results, ABI preservation and retained ownership. B9F198 itself and the supplied-polygon branch are compiled/source-reviewed only. In line with the user’s renewed logic-first request, this unit does not claim volatile-register/Full72 equivalence or gameplay acceptance. Full library passes. BA5CF8 preprocessing remains before B9C7D8 overall cook.

At 8e653d9 the normalized address union is 5,799 (5,516 historical unique numeric addresses + 305 current entries - 22 overlaps), approximately 9.26% versus historical 62,627. The old 5,517 string count contains one formatting duplicate. All union addresses occur in regenerated PPC; the historical catalog itself remains unavailable. This separate implementation inventory does not change accepted mapping/runtime credit.

## Root direct continuation: indexed grouping and final pipeline (2026-10-09)

`mesh_indexed_vertex_output61` adds BC0930 stable face-label/smoothing grouping and BC0BA8 final pipeline composition. Compact/split/dedup/normal/channel/emission stages are concrete; borrowed result views, per-label summaries, incidence remapping and optional original-face permutations retain guest layout/ownership. Eleven family cases pass, with three original grouping-upper and four original pipeline-upper cases sharing concrete lowers, alongside the four emission cases. Full72/RAM/CSR/events/ownership, independent sorted IDs/keys/counts/result views and complete teardown pass. Full library passes; private atan2 constants stay external. BB9800 and the higher indexed cook adapters remain. This is focused semantic validation, not gameplay acceptance.

## Root direct continuation: indexed vertex output and smoothing (2026-10-09)

`mesh_indexed_vertex_output61` closes BC0108: face-batch remapping, indexed or expanded position/attribute output, smoothing-mask normal accumulation with optional guest-atan2 angle weighting, optional incidence records, final indices and batch metadata. Four original-upper/shared-concrete-geometry cases pass for expanded 2D attributes/unweighted normals, indexed attributes/angle weighting, disabled normal generation and an empty batch. Full72/RAM/CSR/events/ownership, independent emitted channel values/indices, normalized vectors, batch counts and complete teardown pass. LO_MESH_MATH_CONSTANTS stays external. Full library passes. Batch grouping and final indexed pipeline orchestration remain.

## Root direct continuation: indexed channel compaction (2026-10-09)

`mesh_indexed_compact61` closes BBF7F0 and BC0058: remove unused channel values, merge exact xyz duplicates, remap corner IDs and remove newly repeated-index position faces while preserving full retained face records. Six original-upper/local-wrapper cases pass with shared concrete dedup/growth/CRT: unique positions, merged positions, both attributes, all channels and position-preservation flag. Full72/RAM/CSR/events/ownership, independent compacted values/remaps/face counts/metadata and complete workspace teardown pass. Full library passes. Smoothing/output vertex construction and final indexed orchestration remain.

## Root direct continuation: indexed batch vertex remapping (2026-10-09)

`mesh_indexed_remap61` closes BBF3F8 and BBFEA8. A temporary sentinel map gives each referenced corner tuple a final vertex index, appends four-word channel/smoothing records, rewrites final face indices and emits batch face/new-vertex counts. Three complete-original local-chain cases pass with shared concrete growth/memset: carried vertex base, reset base and empty batch. Full72/RAM/CSR/events/ownership, independent packed records/final indices/counters and complete workspace teardown pass. Full library passes.

## Root direct continuation: indexed face normals and incidence (2026-10-09)

`mesh_indexed_normals61` closes BBED20: normalized per-face vectors, optional packed normal output, and owned per-position face counts/prefixes/adjacency. Five original-upper/shared-concrete-buffer cases pass for output on/off, all processing disabled, invalid count and a zero-area face. Full72/RAM/CSR/events/ownership, independent known vectors and incidence arrays, and complete workspace teardown pass. The fixture initially omitted the 82007784 constant page; expanding the guest mapping fixed the fixture without an implementation change. Full library passes. Vertex normal smoothing/reindexing and the final indexed pipeline remain.

## Root direct continuation: indexed output channel packing (2026-10-09)

`mesh_indexed_channels61` adds BBF208, appending selected position/attribute channels and preserving two- versus three-component attribute packing. Nine family cases now pass, including three complete-original export/append chains with shared concrete growth. Independent output counts/values, Full72/RAM/CSR/events/ownership and teardown pass for 2D, 3D and disabled output flags. Full library passes.

## Root direct continuation: indexed channel splitting and deduplication (2026-10-09)

`mesh_indexed_channels61` closes BB3C00 xyz append, BBE948 zero-smoothing face position splitting and BBEBE0 corner tuple deduplication/remapping. Six original local-chain/shared concrete buffer/dedup cases pass: append with growth or spare room, split enabled/suppressed/disabled, and duplicate corner tuples. Full72/RAM/CSR/events/ownership, independent copied xyz, remaps/counts/markers and complete workspace teardown pass. Full library passes. Face-normal/attribute processing and final indexed pipeline composition remain.

## Root direct continuation: indexed face insertion (2026-10-09)

`mesh_indexed_workspace61` adds BBE310. Face metadata, corner position/attribute IDs, winding reversal, missing-channel sentinels and supplied-ID clamping follow the original layout. Enabled repeated-index/exact-zero-area filtering succeeds without consuming a face slot; full capacity fails. Eleven family cases now pass, including seven complete-original leaf cases covering those branches and independent metadata/corner/count checks. Full library passes. Indexed channel processing and BB9800 orchestration remain.

## Root direct continuation: indexed-mesh workspace ownership (2026-10-09)

`mesh_indexed_workspace61` closes BBDF60, BBE070, BBE278, BBF590 and BBF628. Thirteen dynamic buffers, copied/zero-filled xyz channels, option bytes and per-face arrays retain the original reset/destruction order and partial-failure ownership. Four complete-original local lifecycle/configuration chains pass with shared concrete buffer/CRT lowers: 2D/3D attributes, zero-fill/zero-count channel, replacement of prior buffers/arrays, and zero-face rejection. Full72/RAM/CSR/events, independent bytes/counts/options and complete teardown pass. Full library passes. Indexed face insertion and workspace processing are next.

## Root direct continuation: cooked hull input adapters (2026-10-09)

`mesh_cook_hull61` closes BB3350 hull-plus-valence, B9E3F8 temporary adapter/cache ownership and B9E7B0 strided input copying into the cooked owner. Four original-upper/local-wrapper-chain cases pass with shared concrete hull/valence/copying: retained adapter, temporary adapter, packed input and 20-byte stride. Full72/RAM/CSR/events, independent closed hull geometry, owner flag preservation and complete teardown pass. Owner +108 bit0 is cleared before work and set on success. The temporary adapter cache is released in original order; do not retain its borrowed view beyond that lifetime. Full library passes. Indexed input and final B9F198 validation/orchestration remain.

## Root direct continuation: point-cloud convex hull construction (2026-10-09)

`mesh_convex_hull61` closes BBA028: stable unique points, double-precision deterministic perturbation and enclosing tetrahedron, circumsphere cavity removal/face cancellation, hull extraction, compacted used vertices, orientation, degenerate-face repair, polygon construction, area centroid and convexity validation. Three original-upper/shared-concrete-geometry cases pass for tetrahedron, cube and duplicate cube input. Full72/RAM/CSR/callbacks/retained ownership, independent counts, closed-edge degree, polygon degrees and enclosure pass; shared concrete auxiliary teardown releases everything. Guest constants stay external through LO_HULL_CONSTANTS (40 bytes) and LO_MESH_MATH_CONSTANTS (184 bytes). No new guards, host hull replacement or exhaustive degeneracy matrix. Full library passes. BB3350/upper owner orchestration and the indexed input route remain.

## Root direct continuation: degenerate face collapse (2026-10-09)

`mesh_convex_check61` adds BB88A8. It measures cross-product magnitude, selects a shortest edge for near-zero-area faces, rewrites that vertex index across the mesh, removes repeated-index faces by tail swap, and repeats only while more than four faces survive. Check-only and insufficient-survivor paths return false. Twenty-two family cases pass, including seven complete-original collapse/BD91E0 cases covering three edge selections, check-only, good, empty and exhausted meshes. Full72/RAM/CSR, independent count and retained indices pass with a synthetic guest threshold; no allocation or exhaustive geometry suite. Full library passes.

## Root direct continuation: input uniqueness and compaction (2026-10-09)

`mesh_vertex_dedup61` adds BB8580, preserving the variable guest-stack point copy, check-only mode and optional in-place compaction/count update. Duplicate input still returns false even after successful repair. Seven family cases now pass original local dedup-chain comparisons with shared concrete sort/probe/initializer, Full72/RAM/CSR/callbacks and complete temporary cleanup. Added cases cover repair, check-only, already unique and empty. Existing B7E504 stack probe is exposed from its original family without duplicate recovery credit. Full library passes. Convex hull construction remains.

## Root direct continuation: stable vertex deduplication (2026-10-09)

`mesh_vertex_dedup61` closes BC2DD0, BC2D48 and cleanup tail alias BC38E0. Three coordinate-word sort passes preserve stable ranks, merge exact xyz bit matches, retain unique vertices and original-to-unique mapping, and optionally publish borrowed aliases. Three original-local-chain/shared-concrete-sort cases pass Full72/RAM/CSR/callbacks/ownership, independent remap/content and actual original tail cleanup: result present/absent and replacement of prior allocations. Mode 1 orders coordinate words, so negative float bits follow positive bits; the initial independent expected map incorrectly assumed numerical float ordering and was corrected, with no implementation change. Existing BC2D28 initializer uses the accepted field-assignment helper and receives no duplicate credit. Full library passes.

## Root direct continuation: convex geometry predicates and orientation (2026-10-09)

`mesh_convex_check61` closes indexed winding reversal BD8FA8, duplicate-index predicate BD91E0, point sidedness BD90A0, centroid-based triangle orientation/correction BB86C8 and polygon halfspace validation BB8E88. Fifteen cases compare complete original leaves/local chains with Full72/RAM/CSR and independent index/side/halfspace outcomes. Includes centroid four-wide and tail paths, correction enabled/disabled, already correct winding and invalid positions. No allocations or additional guards; original geometry/tolerance semantics retained. Full library passes. Convex construction/vertex deduplication still remain.

## Root direct continuation: cooked owner bounds closed (2026-10-09)

`mesh_bounds_select61` adds B9EA90, publishing owner min/max, the selected bounding sphere and max-coordinate-scaled 2^-22 tolerance through the actual guest pow path. Seven family cases now include two complete original cooked-owner graphs with all bounds, pow and sphere bodies composed. Full72/RAM/CSR/events/ownership plus independent min/max, sphere enclosure and exact tolerance pass. Full library passes. LO_POWER_CONSTANTS is 1448 bytes for this graph, with trailing cooking exponent; lower tests still use their required prefix. Overall B9F198 still needs convex/input-build dependencies, and BA5CF8 preprocessing remains.

## Root direct continuation: complete guest power graph (2026-10-09)

`power_math61` closes B7E860 with integer exponentiation, tabulated logarithm/exponential reduction, sign/parity, zero and nonfinite routing. Twelve cases compose the complete actual original graph, including baseline copysign. Full72/RAM/CSR and independent power/domain/sign checks pass; the cooking input 2^-22 is bit-exact. Constants remain external through LO_POWER_CONSTANTS (1440 bytes). No host pow replacement, duplicate copysign credit, exhaustive IEEE matrix or gameplay claim. Full Clang library passes. B9EA90 cooked bounds now has concrete lowers available.

## Root direct continuation: infinite power cases (2026-10-09)

`power_fp_support61` adds B7E6D8 infinite-base/exponent handling with actual parity composition. Twenty family cases pass, including signed zero from an odd negative exponent, negative infinite bases, magnitude comparisons for infinite exponents and the original indeterminate flag. Full72/RAM/CSR and independent value/sign checks pass; original guest NaN behavior is retained rather than normalized to host pow. LO_POWER_CONSTANTS now provides 1320 bytes, extending the special block at 83215500 to 40 bytes. Full library passes. All direct lower implementations for B7E860 are now available, including baseline copysign; main pow remains to assemble.

## Root direct continuation: guest natural logarithm (2026-10-09)

`power_log61` closes 82301A68, including normal/subnormal reduction, guest rational coefficients and split exponent contribution. Seven complete-original-leaf cases pass Full72/RAM/CSR and independent log/domain checks: identity, normal inputs on both sides of one, minimum subnormal, zero, negative and positive infinity. Constants stay external via LO_POWER_CONSTANTS (1296 bytes). Independent zero-domain checking caught an incorrectly seeded fixture special-value address: corrected 83215400 to 83215500; implementation unchanged. Full library passes. Pow orchestration still remains.

## Root direct continuation: power-function bit helpers (2026-10-09)

`power_fp_support61` closes B7E668 integer parity, B822F0 exponent extraction, B822C8 exponent replacement and B823C8 mantissa/exponent decomposition with subnormal shifts. Twelve complete-original-leaf cases pass Full72/RAM/CSR and independent parity, exponent and frexp/ldexp results for finite normal, zero and signed subnormal inputs. Full library passes. B9EA90 reads exponent -22 and base 2 from its guest constants, but the original pow state still must be recovered before claiming its upper chain exact; no host pow substitution.

## Root direct continuation: complete sphere candidate selection (2026-10-09)

`mesh_bounds_select61` closes BC9AF0 temporary pointer-array allocation/recursive candidate and BC9C68 finite, nonnegative, smaller-radius selection against the axis-extreme sphere. Five cases compose the complete original sphere graph (shared accepted classifier only), checking Full72/RAM/CSR/callbacks/ownership and independent enclosure: standalone candidate, exact planar fallback, tighter tetrahedral candidate, empty input, null input. Temporary array is freed through the actual context slots +8/+20. Full library passes. The initial symmetric seven-point fixture naturally selected the original sphere; changed only that fixture to asymmetric tetrahedral points to exercise the tighter-candidate branch. No implementation change was needed. B9EA90 still awaits original pow handling.

## Root direct continuation: recursive support sphere (2026-10-09)

`mesh_bounds_math61` adds BC9928 mutable support-prefix recursion. Outside points move to the front, join the boundary constraint, and recursively enclose earlier points; four support points terminate through the tetrahedral constructor. Eleven family cases now compose the actual original recursive and support leaf bodies, checking Full72/RAM/CSR, mutated pointer ordering and independent enclosure. External LO_BOUNDS_CONSTANTS is now eight bytes: radius epsilon and empty-radius sentinel. Full Clang library passes. BC9AF0 allocation/orchestration and BC9C68 candidate selection remain next.

## Root direct continuation: support-point circumspheres (2026-10-09)

`mesh_bounds_math61` adds BC9580/BC9600/BC9780 two-, three-, and four-point sphere constructors. Midpoint, cross-product circumcenter, and determinant/cofactor arithmetic preserve binary32 stages, radius epsilon, and red-zone FPR state. Eight family cases pass complete original leaves with independent known circumcenters/radii and enclosure. Four-byte radius epsilon is supplied externally through LO_BOUNDS_CONSTANTS; no image-derived bundle committed. Degenerate support sets are not part of focused validation. Full library passes. These close BC9928 recursive solver leaves; recursive orchestration and bounds-wrapper fallback still remain.

## Root direct continuation: expanding bounding sphere (2026-10-09)

`mesh_bounds_math61` closes BC9040: retain six axis-extreme points, choose the widest pair, initialize its midpoint sphere, and expand in input order. Five complete-original-leaf cases pass Full72/RAM/CSR, independent enclosure and known single/planar spheres. Includes null input, one/three/four/seven finite points, four-point batches and tail, and expansion. FPR red-zone saves and binary32 stages retained. No nonfinite or nonnull-zero-count claim. Full Clang library passes. BC9AF0/BC9928 fallback and B7E860 pow remain before the cooked bounds wrapper can close.

## Root direct continuation: packed point bounds (2026-10-09)

`geometry_primitives61` adds BCA410 packed xyz min/max reduction, an input to B9EA90 cooked bounds. Eleven family cases pass, including independent four-point extrema and original zero-count/null-input no-write behavior. Full72/RAM/CSR and full Clang library pass. Bounding sphere and the original pow dependency remain open; this does not close B9EA90.

## Root direct continuation: cooked support-map generation (2026-10-09)

`mesh_cook_support61` closes B9EB58 and BC61B0/BC6210/BC63C8 support construction/destruction. Preserve byte-index limits, replace old owner, sample 16x16 directions per cube face only above 32 vertices, and distinguish one loaded block from two independent generated arrays. Four complete-original-local-chain cases pass Full72/RAM/CSR/events/ownership and independent dimensions/index ranges/full teardown: small mesh, fresh large mesh, replacement of split arrays, replacement of loaded combined block. Cube sampling/extrema are shared concrete recovered lowers. Diagnostic and allocation failures are implemented but not in this focused set. Existing BC8310/BC8330 pointer-field leaves receive no duplicate credit. Full Clang library passes.

## Root direct continuation: cooked mesh tree orchestration (2026-10-09)

`mesh_cook_tree61` closes B9E6A8: release prior tree, refresh borrowed source descriptor using existing virtual count getters, construct compact settings with global quantization toggle, invoke the concrete tree builder, preserve diagnostic failure routing. Two original-upper/shared-concrete-tree cases pass Full72/RAM/CSR/callbacks/ownership, independent nine-triangle descriptor and compact storage checks, and complete teardown. Both quantization choices pass. Existing accessor and integer leaves receive no duplicate inventory credit. No diagnostic/fault matrix or gameplay claim. Overall preprocessing and validation/build remain open.

## Root direct continuation: aggregate cooked-mesh serializer (2026-10-09)

`mesh_cook_stream61` closes B9F6F0 NXS/CVXM aggregate output. Two original-upper cases use the concrete recovered graph: stack writer forwarding, lazy CLHL/CVHL/VALE, linked OPC/HBM staging and flat strategy payload, owner mass cache, optional SUPM/GAUS and temporary-cache cleanup. Full72/RAM/CSR/events/ownership pass; independently checked tags, tree size, scalar/mass fields, cube volume, support bytes and nine retained mesh allocations. The strategy is a prepared synthetic one-record input, not a new tree-build proof. Both endian and normal modes pass. Initial harness unmapped adapter-vtable page corrected; implementation unchanged. Full Clang library passes. B9C7D8 overall cooking still needs BA5CF8 preprocessing and B9F198 validation/build.

## Root direct continuation: tree envelope serialization (2026-10-09)

`tree_envelope_write61` closes BD14B8 OPC base header/strategy dispatch, BD1BF8 HBM leaf/triangle mappings, BD7CB8 linked-stream tag and BD8550 adaptive u8/u16/u32 indices. Four complete original local-chain cases pass Full72/RAM/CSR/events with concrete flat-strategy output and actual linked append lowers. Independent native/swapped envelope bytes, u16/u32 mappings, u8 payload and missing-strategy failure pass. Full library passes. This closes the previously missing indirect tree-writer boundary under B9F6F0; aggregate serialization is next.

## Root direct continuation: borrowed stream forwarding (2026-10-09)

`mesh_stream_write61` adds six B9E528..B9E668 methods used by the aggregate serializer's stack adapter. Each forwards the matching byte/u16/u32/float/double/block operation through its borrowed writer at +4 and returns the adapter. Twenty family cases now pass, including six complete-original forwarding bodies with concrete growable writers. Full library passes. Tracing B9F6F0's indirect object writer found BD1BF8/BD14B8 tree envelope and BD7CB8/BD8550 linked-stream helpers still to recover; do not claim every indirect lower was already closed.

## Root direct continuation: lazy owner mass properties (2026-10-09)

`mesh_mass_cache61` closes B9F418 using the complete mass integration chain and an explicit Full72 adapter to the already recovered CRT classifier. Three original-upper/shared-concrete-math cases pass Full72/RAM/CSR and independent translated cube unit-density mass, origin inertia and centroid; existing cache bypass and disabled-integration failure pass. The owner retains signed-mass diagnostic/correction behavior, but that diagnostic and nonfinite rejection are not exercised. Full Clang library passes. B9F6F0 aggregate serialization now has all known lower implementations available.

## Root direct continuation: complete mass integration chain (2026-10-09)

`mesh_mass_math61` adds BCCE48 projected polynomial integration, BCD0F8 plane lift, BCD400 signed-volume accumulation and BCD8A8 seven-word descriptor/density adapter. Eleven focused cases now compose the complete original local chain, comparing Full72/RAM/CSR and independent projected/face monomials plus translated cube volume, centroid and origin/centroid tensors. u32 direct and reversed-winding u16 wrapper paths pass, as does the disabled global gate. Constants are external/private via LO_MASS_CONSTANTS (128 bytes). Full Clang library passes. B9F418 cached-owner assembly remains next; no gameplay or historical mapping credit.

## Root direct continuation: mass-property plane and inertia (2026-10-09)

`mesh_mass_math61` closes BCD300 strided triangle plane and BCCCA8 density-scaled inertia with parallel-axis correction. Four complete-original-leaf cases pass Full72/RAM/CSR and independent strided/degenerate planes, translated uniform cube inertia and zero-volume handling. Full Clang library passes. Projection/face/volume integration and cached owner assembly remain next, so this is not a complete mass-properties pipeline or gameplay claim.

## Root direct continuation: CLHL geometry and valence wrapper (2026-10-09)

`mesh_geometry_stream61` adds BB3220: ICE/CLHL v0, complete CVHL geometry and lazily constructed ICE/VALE v2 cache. Six focused cases now pass; two compose the actual original CLHL and CVHL bodies with shared concrete cache/stream lowers. Independent nested field parsing, borrowed cache publication and twelve retained mesh/cache allocations pass. Full Clang library passes. Next dependency is B9F418 mass properties; no historical mapping or gameplay credit.

## Root direct continuation: complete CVHL geometry stream (2026-10-09)

`mesh_geometry_stream61` closes BBC110 ICE/CVHL v5: lazy polygon/topology/vertex-normal construction, adaptive triangle/edge indices, packed or raw normals, relocated polygon records and temporary incidence arrays. Four original-upper/shared-concrete-lower cases pass Full72/RAM/CSR/events and independent complete cube-stream parsing for both byte orders and both normal modes. Packed stream is 598 bytes, raw stream 798; nine mesh-owned arrays remain and both serializer temporaries are released. Full Clang library passes. No gameplay/all-original-chain claim or mapping credit. Next: BB3220 wrapper and B9F418 cached mass properties before aggregate B9F6F0.

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
