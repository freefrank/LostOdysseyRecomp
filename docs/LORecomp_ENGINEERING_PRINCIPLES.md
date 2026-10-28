# LORecomp Engineering Principles

[简体中文](LORecomp_ENGINEERING_PRINCIPLES.zh-CN.md)

Date: 2026-09-17  
Scope: the whole project, including the recompilation runtime, graphics, audio, input, file system, game import, saves, updates, caches, builds, tests, and human and AI-assisted development.

> Run *Lost Odyssey* as completely, correctly and efficiently as possible on supported, correctly exported game data, and keep game behavior consistent on the platforms the project supports.

This document sets the trade-offs for future development and refactoring. It does not mean that the current implementation already meets all of these goals.

## 1. Primary Goals and Trade-offs

Full operation and consistent cross-platform behavior are constraints on optimization. Within these constraints, reduce CPU, GPU, memory, disk I/O and power costs as much as possible, and improve startup, scene loading and sustained play. The implementation should stay as simple as possible so that a small hobby project can be maintained over the long term.

"Full operation" covers story, battle, scenes, cutscenes, menus, input, audio, reading and writing saves, disc switching and long play sessions for supported content. Reaching only the title screen, running one scene, or passing some unit tests does not count as full operation.

Do not trade performance numbers for skipping necessary drawing, simulating incorrectly, breaking timing, ignoring synchronization, or hiding missing features. Quality trade-offs that the user chooses deliberately should be kept separate from implementation optimizations that do not change game results.

Running at 720p60fps on a Steam Deck with a 15 W power setting is a reference optimization target. It must not be written up as an achieved result without measurement. Watch frame time, stutter, memory and energy use together, not only average frame rate.

## 2. Input Assumptions and Responsibility Boundaries

The project assumes a game export that the maintainers explicitly support. The current discussion is based on the user providing an identical, compatible `default.xex`. This does not extend support to all versions, modified builds or arbitrary combinations of assets.

The user is responsible for obtaining and correctly exporting their own game data. The project is not responsible for certifying the integrity of a dump, detecting every instance of data corruption, repairing game files, or building a general compatibility layer for unknown versions.

Import and startup perform only the version identification, file location and format reading needed to run. Where it is truly necessary to distinguish versions that affect recompiled code, use the minimum necessary identifier or a single targeted fingerprint. This must not expand into full-disk or full-resource integrity certification.

When a file is missing, a read or write fails, or a format cannot be processed, give a direct error message and stop the related operation. There is no requirement to detect all bad data in advance, and no complex automatic recovery system should be built for this purpose.

This boundary does not remove the project's own responsibilities. Program defects on a supported dump must not be blamed on the user. Import, save, cleanup or update operations must not damage source data, saves or unrelated files.

## 3. Project-wide Validation Strategy: Keep What the Function Needs

The test is whether a check directly serves correct operation or protects the program's own operations. The number of checks, the names of algorithms, or whether something merely looks safe are not the test.

| Scope | Keep by default | Remove by default or move to developer tools |
|---|---|---|
| Game import | Supported version identification, necessary file location, copy result and I/O error handling | Full-disk hashing, per-asset authentication, full read-back comparison after copying |
| Normal startup | Required configuration, resource index, interface and device capability checks | Install directory audits, repeated authentication of dependency files, full dump scans |
| Scene and resource loading | Actual reads, decompression, parsing and necessary bounds checks | Extra traversals, rereads and content hashing added only to prove a resource is unchanged |
| Rebuildable caches | Format and compatibility version, lookup keys, necessary length and offset checks | Multi-layer content authentication, install path binding, machine identity binding |
| Saves and config | Write success checks, avoidance of accidental overwrite, necessary temporary-file replacement | Authentication chains and complex recovery state machines unrelated to actual write reliability |
| Auto-update | HTTP/I/O errors, ZIP decompression CRC, write-scope control and rollback | SHA authentication, post-download size authentication, repeated read-back, re-verifying the whole installation on every launch, requiring local files to be completely unmodified before running |
| Build and release | Dependency versions, build errors, necessary license files, related functional tests | Mandatory audit processes with no practical runtime value, turning development records into a player launch gate |

Read lengths, offsets and allocation sizes must never cause out-of-bounds access. Actual failures of decompression, file operations and system APIs must be handled. Object lifetimes, thread synchronization, GPU resource state and save-write correctness are program functions. They are not "enterprise-grade defenses" that may be removed at will.

Do not build "strict / normal / fast" runtime paths to keep validation that has already been decided to be cancelled. Mechanisms with no clear purpose are deleted directly. Complete checks that are truly needed for troubleshooting may remain as manual developer tools, and they are not started automatically during normal runs.

## 4. Hash and Cache Usage Boundaries

Hashes may be used for lookup, deduplication, detecting content changes and cache compatibility. Normal builds, import, startup and updates do not add SHA or post-download size integrity authentication. After a download completes, install directly according to the ZIP format, path protection and rollback rules. Only the next formal Windows transition package generates an old SHA map once, for compatibility with the old updater. The new updater ignores its values, and that compatibility option is removed afterwards.

The next formal Windows transition package generates an old SHA manifest once, only to remain compatible with the already released old updater. It ships with the new updater and a file-size manifest so that players can upgrade automatically. The new updater uses file sizes. After the transition, the option that packages the old manifest is removed.

Immutable identities and keys should be computed once and reused. The same data should not be scanned repeatedly just because different modules need it for logging or identification. Hot paths should preferentially reuse existing IDs, states and versions. Where a content hash must be kept, choose the minimal scheme that meets the correctness requirement.

Static on-disk content of a supported dump may be handled under the input assumptions. Memory, textures, vertex data and render state that change while the game is running must still be correctly detected as changed. Necessary cache-invalidation logic must not be deleted on the grounds of "trusting the data".

Cache validity is determined by actual output and interface compatibility. Changing only menus, logs, install directories or unrelated code should not trigger a full shader recompile. Compatibility versions are updated only when shader semantics, resource layout or serialization format actually change.

Local compiler binary hashes are not a cross-machine loading condition for released shader packs. Local caches should also prefer an explicit compile contract or version. Keep a targeted producer identifier only when compiler outputs actually need to be distinguished. Do not re-authenticate the toolchain for every shader.

When changing a cache format or key, state the migration or discard policy explicitly, so that old-format data is not misread as the new format. Rebuildable caches do not need to bear the long-term compatibility and recovery responsibilities of save data.

## 5. Target Flow for Shaders and Release Resources

Shader translation and intermediate bytecode compilation that are highly repetitive and independent of the user's device should be scheduled during development or release. At runtime, players should preferentially use precompiled resources shipped with the version, so that every user does not repeat the same work.

The release package contains only the data required to run. HLSL text, detailed diagnostics, build logs and compilation failure records do not go into the normal runtime resource package. Compression, deduplication and on-demand loading should be decided by measured package size, loading time and memory data. They must not add noticeable gameplay stutter in pursuit of a smaller package.

Once a covered and compatible shader is found in the release package, it is no longer translated again or passed to the source compiler. Uncovered items may use a simple local fallback compile path. Missing shaders must never be ignored permanently to hide picture problems. The release package is kept separate from local generated caches and is never written back to release resources.

Device-specific pipeline creation must still be performed correctly. Warm-up should preferably use scene loading or a reasonable background work budget. It should not force waiting on game-wide tasks unrelated to the current startup, and it must not let background compilation occupy the game thread for long.

Do not treat driver-specific caches as universal cross-device resources. For shareable shader intermediate products, keep a unified format and compatibility rules. Do not artificially create different versions because of install path, host CPU vendor or OS name.

## 6. Multi-platform Consistency

Platforms should share game semantics, core logic, configuration meaning, save format, shader interfaces, resource formats and error handling rules. Platform differences are limited to necessary system interfaces, graphics backends, file paths, input devices and scheduling implementations.

The same fix should preferably land in the shared layer. Do not maintain, over the long term, a Windows build and a Linux build that behave differently. Do not require users to find platform-specific hidden switches to get the same basic functionality.

Effective specialization for platform, CPU and GPU is allowed, including SIMD, compiler optimizations, thread scheduling and cache budgets. Specialization must not change game logic, and it must not leak a specific machine's instruction-set requirements into the general build. Consistency does not require all platforms to use the same API, thread count, physical paths or byte-identical driver output.

Graphics backends should keep scene, material, depth, blending, coordinate and timing semantics consistent. Where a platform feature has a genuine gap, record it explicitly. Do not silently turn off an effect and then claim full consistency. A new platform or architecture must be validated separately and cannot be presumed usable from shared source code alone.

## 7. Simple Implementation and Low-cost Diagnostics

Prefer local, direct implementations that solve the actual problems of LO. Without a clear reuse need, do not introduce general plugin frameworks, identity-authentication systems, multi-layer fallback state machines or abstractions for unknown games.

Error handling should be simple and explicit. When no usable and correct result is available, do not fake success, do not retry indefinitely, and do not default to recursively searching the whole disk. A recoverable error uses one clear recovery path. For an unrecoverable error, keep enough information to locate the problem, then stop the related operation.

Normal logs retain the version, platform, backend, key stage timings and actual errors. Per-shader detailed tracing, source export, large-scale statistics and full integrity troubleshooting are enabled explicitly by developers. Do not run expensive hashing, synchronization or disk writes just to produce logs.

Shared algorithms, format constants and platform behavior should each have one authoritative implementation. When the validation system is trimmed, the corresponding tests, documentation and deprecated switches are cleaned up too, so that old requirements do not keep driving new code toward complexity.

## 8. Tests, Performance Evidence and Acceptance

Test resources should preferentially cover real game behavior on supported inputs: story and battle progression, complex scenes, audio-video sync, input, reading and writing saves, disc switching and cross-platform regression. Saving, importing and updating also need to cover the basic requirement that user data is not damaged under actual I/O failures.

Do not use the recognition rate of bad dumps, the number of abnormal-input cases covered, or the number of validation layers as project quality metrics. Unit tests, sanitizers and targeted abnormal-case tests may still be used to find the project's own out-of-bounds, race and lifetime errors. Each ordinary change does not need a complete hostile-input test system.

Performance comparisons fix the game input, scene, backend, settings and cache state. They report first startup, cache-hit startup, scene loading and steady play separately. When needed, split extraction, translation, source compilation, pipeline creation, decompression, I/O and hash timings. Do not forcibly attribute mixed timings to one item.

Changes that touch shared logic should check the affected platforms. Platforms not yet verified are marked as unverified. Small changes use related tests and representative scenes. Major runtime, graphics or timing changes expand the game regression scope, but a full replay of the whole game is not required for every commit.

Describe these separately: compilation passing, emulation-service tests passing, real-driver tests passing, representative scenes passing, and full-flow passing. Without complete shader data, do not guess the final package size. Without real-device testing, do not promise that frame rate or cross-platform compatibility has been achieved.

A simplification or optimization should satisfy all of the following: no functional regression on supported inputs, consistent behavior on the affected platforms, no damage to user data such as saves, and a verified reduction in extra I/O, runtime cost or maintenance complexity. Performance gains should come with measurements. Pure maintenance simplifications need not be described as speedups.

## 9. Later Refactoring and Code Review Rules

First distinguish existing mechanisms by their actual role: necessary game functions, runtime compatibility, user-data write protection, and extra input authentication. Delete or move the last category off the default path first. Do not delete the first three together with it.

Submit reversible changes by subsystem. Avoid replacing algorithms, cache formats, threading models and validation strategy all at once, which makes regressions impossible to locate. Do not add new manifests, authentication markers or persistent state databases to maintain integrity guarantees that have been cancelled.

Any new check, abstraction or background task should be able to answer three questions: what practical LO problem it solves; why the existing simple path is insufficient; and what its impact is on correctness, frame time, startup, memory and other platforms. "Safer", "more general" or "more robust" without a concrete benefit is not sufficient reason to introduce it.

This principle is used to revise old requirements within the project that conflict with it. It does not authorize batch-deleting checks without examining call relationships. Scope trade-offs are decided by the maintainer, and implementation and validation results must be recorded truthfully.

## Summary Suitable for AGENTS.md

> LORecomp's primary goal is to run *Lost Odyssey* fully, correctly and efficiently on supported, correctly exported game data, and to keep game behavior consistent across platforms. The user is responsible for the correctness of the dump. The project does not take on full-integrity authentication, repair, or enterprise-grade defense goals. The default runtime path forbids adding full-resource scans, repeated read-backs and multi-layer content hashes solely for authentication. Keep the necessary version identification, parsing boundaries, I/O error handling, cache invalidation, thread and GPU synchronization, and the basic measures that protect source files and saves. Move precompilation work forward into the build/release stages where possible, and have players use and reuse the results on demand. Adopt the simplest effective implementation and do not maintain multiple validation modes of different strictness. All optimization is constrained by game correctness and cross-platform consistency. Test and performance conclusions must match the actual validation scope exactly.
