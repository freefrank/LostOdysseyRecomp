# uam in SwitchWakerHD

uam 1.1.0, https://github.com/devkitPro/uam at `5a5afc2` (LICENSE: zlib for uam's own files, MIT for
mesa-imported/). Built by `CMakeLists.txt` (no meson) as the static library `uamlib`; the renderer uses
it through `uam_api.h` (init / compile, one thread only). `generated/` holds what meson would produce
with bison, flex and mako: rerun `tools/generate.sh` (bison, flex, python3-mako; e.g. the `uam-host`
Docker image) after editing a `.yy`, `.ll`, `.y`, `.l` or `ir_expression_operation.py`.

Every change to upstream files is marked `SwitchWakerHD patch N`.

1. **Real mutexes** (`mesa-imported/c11/threads.h`). Upstream's header is a single-threaded stub; it
   now maps to pthread mutexes. uam is still NOT reentrant (static `gl_context` in
   `source/glsl_frontend.cpp`; three threads crash even with these mutexes): compile on one thread.
2. **DKSH in memory**: `DekoCompiler::WriteDksh(std::vector<uint8_t>&)` next to `OutputDksh`, same
   bytes without a file (`source/compiler_iface.*`).
3. **Log callback** (`source/uam_log.h`, `source/mini-os.c`): the frontend's and the compiler
   interface's `fprintf(stderr)`, Mesa's `_mesa_warning` / out-of-memory messages, `os_log_message`
   and nv50_ir's `ERROR()` (a no-op in release builds before) go to `uam_set_log_callback` (stderr
   when unset). `uam::compile` collects them in `Result::log`.
4. **No exit()/abort() from CompileGlsl**: the `exit(1)` in `st_glsl_to_tgsi.cpp`, the `abort()` in
   `lower_mat_op_to_vec.cpp` and `nv50_ir_from_tgsi.cpp`, nv50_ir's `FATAL()` and flex's
   `YY_FATAL_ERROR` (both lexers) call `uam_fatal()`, which logs and `longjmp`s back to
   `DekoCompiler::CompileGlsl`; the compile returns false and what it had allocated is leaked.
   Mesa's `assert`/`unreachable` stay compiled out (`NDEBUG` is always defined for `uamlib`); the
   `abort()`s in `ir_validate.cpp` exist only in `DEBUG` builds.
5. **Resident frontend** (`DekoCompiler::SetFrontendResident`, `uam::init(true)`): Mesa's built-in
   GLSL function library and type tables are built on the first compile and kept, instead of being
   rebuilt and freed around every `DekoCompiler` as the uam tool does. Same bytes; about 20 % less
   compile time on the host.
6. **Deterministic scheduling data** (`mesa-imported/codegen/nv50_ir.cpp`, `Instruction::init`):
   `sched` starts at 0x7e0. `SchedDataCalculatorGM107::setDelay` reads the wait mask of the next
   block's first instruction before that block is visited, i.e. uninitialised heap memory (valgrind:
   "Conditional jump or move depends on uninitialised value(s)" in `setDelay`). Upstream uam's stall
   counts therefore depend on what the heap held: compiled in one process after other shaders, 89 of
   the prototype's 1561 Cemu shaders came out different from the uam tool's files, and a fresh run of
   the prototype's own uam binary no longer reproduces 5 of the reference files it wrote. 0x7e0 (no
   barrier, nothing to wait on) is what the scheduler gives every instruction before it runs.

7. **No dual issue** (`mesa-imported/codegen/nv50_ir_emit_gm107.cpp`, `SchedDataCalculatorGM107::setDelay`):
   uam's Maxwell dual-issue scheduling (README: "enhanced with new experimental findings"; Mesa 20.1,
   which gfx/gl runs on, never dual-issues) made 4x8-pixel blocks (one 32-thread fragment warp) of
   sunlit surfaces flicker with wrong lighting on Outset and in Dragon Roost Cavern. Bisected on the
   console (2026-10-07) with the renderer's load-time patch of the scheduling words: as compiled =
   specks, no dual issue = clean, same fps. Every instruction now gets its normal stall count instead
   of a zero-delay pair.

8. **Stall 2 before a new block** (`mesa-imported/codegen/nv50_ir_emit_gm107.cpp`,
   `SchedDataCalculatorGM107::setDelay`; found in SwitchWaker, its patch 8, devkitPro/uam#7): Mesa
   20.1's rule restored. A dependency barrier becomes active one clock after the instruction that
   sets it, so Mesa gives an instruction that sets one stall 2 when the next instruction is in another
   block (whose waits are not known yet). uam asked the next block's entry instead, and that entry's
   "wait on every barrier" is only added when the block is visited: the last instruction of a block
   got stall 1 and the next block's first one could pass the barrier before it was active. On
   SwitchWaker's console (2026-10-08) the light volumes' fragment epilogue `MOV R0, 1.0` overwrote
   `IPA.SAT R3 = a[0x8c] * R0` (R0 = 1/w) before the IPA read it: flickering 4x8-pixel specks on every
   torch halo. Not seen here yet (latent: same uam, same pattern possible in any shader whose block
   ends on a barrier-setting instruction). The DKSH compiler name gains "patch 8", so every cached
   DKSH is rebuilt.

Check: `uam_dksh_test` (`-DWWHD_UAM_TEST=ON`) compiles a directory of GLSL files in one process and
compares the DKSH bytes with reference files written by the `uam` tool (target `uam_tool`), one
process per shader.

LostOdysseyRecomp (2026-10-27): `g_banjoGprCap` (`mesa-imported/codegen/nv50_ir_target_nvc0.cpp`) is a plain static,
not `thread_local`: after plume_uam_localize.cmake's `ld -r`, its TLS access lost its offset and the first compile
on the console wrote off the thread pointer. uam runs on one thread.
