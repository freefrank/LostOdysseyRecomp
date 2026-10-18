# Recompilation Configuration Notes

[简体中文](recomp.zh-CN.md)

> **Topic reference.** The implementation and measurements below apply only to the versions and scope stated for each one. For current delivery and acceptance status, see [project status](../STATUS.md) and [roadmap](../ROADMAP.md).

## Target binary

The decrypted and decompressed `default.xex` files from all four discs are byte-for-byte identical (md5 f07cb5afd8ab6fa8fbdc14f4fceb5858), so the recompilation target is unique. Using Disc 1 is enough. Image base 0x82000000, size 0x13C0000, entry 0x827CA440.

## Section layout (xexdump output)

| Section | Start | Size | Notes |
|---|---|---|---|
| .rdata | 0x82000600 | 0x21EF2C | Read-only data and strings (UE3 reflection names are found here) |
| .pdata | 0x8221F600 | 0x62798 | Function table. XenonAnalyse uses it for function boundaries |
| .text | 0x82290000 | 0xE4AA6C | Main code, about 14.3 MB |
| .embsec_* ×8 | from 0x830DAC00 | about 0x50000 in total | Embedded code sections. Their names are garbled. Most likely encrypted or compressed security sections, or related to XNET. Still to be checked |
| .data | 0x83130000 | 0x248E98 | Writable data |
| .tls | 0x83379000 | 9 | |
| .XBMOVIE | 0x83379200 | 0xC | |
| .idata | 0x83380000 | 0x452 | Import table |
| .XBLD | 0x83390000 | 0xD0 | |
| .reloc | 0x83390200 | 0x144A24 | |

## Located helper functions (tools/find_ppc_helpers.py, each pattern matches exactly once)

| Key | Address |
|---|---|
| restgprlr_14 | 0x82B7A700 |
| savegprlr_14 | 0x82B7A6B0 |
| restfpr_14 | 0x82B7B17C |
| savefpr_14 | 0x82B7B130 |
| restvmx_14 | 0x82DF2D98 |
| savevmx_14 | 0x82DF2B00 |
| restvmx_64 | 0x82DF2E2C |
| savevmx_64 | 0x82DF2B94 |

## Jump tables

XenonAnalyse found 841 tables (all are absolute address tables) and wrote them to `config/switch_tables.toml`.
Tables it missed are written by hand in `config/switch_tables_manual.toml`. After XenonAnalyse is run again, that file must be appended again.
So far, one table has been added by hand: the u16 offset table at 82FB21AC (16 entries).

## invalid_instructions

Between functions there are two kinds of 8-byte exception-handling descriptors (a handler pointer and a scope table pointer). The first word is `0x82B7A940` (C++ frame handler, 2165 occurrences) or `0x830D9F4C` (C specific handler, 35 occurrences). There is also 4 bytes of zero padding. If they are not skipped, XenonRecomp's pass2 boundary analysis treats the descriptors as code. It then swallows the whole function that follows and loops forever on functions that have jump tables. The symptom is a hang with no output. Also, four undecodable data words start at 830DA0BC near the end of `.text`.

## Explicit function boundaries (tools/gen_function_bounds.py)

XenonRecomp's boundary analysis treats `bctr` as a tail call. A leaf function that is not in .pdata gets cut short if it has a jump table. After it is cut, the `b` instructions inside the function become unresolved "jump out of function" targets (in the first round: 91 switch sites and 96 unresolved targets).

Script rules:
1. Each `// ERROR <addr>` target becomes a function (running to the next symbol, and not past the end of the containing pdata function). Targets accumulate across rounds in `config/branch_targets.txt`.
2. For each jump table whose labels are not covered by the containing pdata function, a function is generated from the nearest symbol to the next pdata/bl symbol after the farthest label.
3. `config/function_bounds_manual.txt` holds manual entries. There is currently one: 827C39F8, size 0x18. Without it, that entry would flow into pdata function 827C3A10 and take over its jump table.
4. **Pointer-only functions** (found at run time): functions referenced only by vtables or function pointer tables, not in .pdata, and not the target of any `bl`. Boundary analysis cannot see them, so they get merged into the tail of the previous function. For example, 0x823067C0 was merged into 0x82306660. When vtable slot 67 called it, the function table entry was a null pointer, and the host jumped to address 0 and crashed. The script scans the whole image outside .text for 4-byte aligned pointers into .text. It requires that the pointer is not inside a .pdata function, that the word before it is blr/bctr/b/0 padding, that it is not a switch table label, and that it is not already a start point in the **baseline mapping table** (`config/baseline_func_mapping.txt`, which is `ppc_func_mapping.cpp` from one run of XenonRecomp without pointer-only functions. It lists the 63032 start points that the recompiler finds by itself, for example by resuming after a return). 43 matches were found. They also act as hard symbols that cut the preceding function short. Without baseline filtering, the script produced 5000+ false positives (functions the recompiler finds on its own, such as by resuming after a return), and it generated `// ERROR` output. After each change, `grep -l '// ERROR' ppc/*.cpp` must return 0. At run time, `kernel/memory.cpp` fills every empty slot of the function table with a `MissingFunction` record stub. If the log shows "call to unrecompiled guest function", some functions were missed.
5. **Addresses materialized in code**: a .text address built by `lis rX,hi` followed by `addi/ori rX,rX,lo` (within a 64-byte window) must also not be in .pdata, not in the baseline mapping, and not a switch table label. The word before it must be `blr` or 0 padding. If `b` were allowed, the case labels of computed jumps (`lis/addi` base label + `bctr`) would be treated as functions. That produces 15 bad files. A run-time log line like "call to unrecompiled guest function ctr=0x82CF21C0" is this kind of case: there is no static reference, and the value is written into an object at run time. When rule 2 chooses "the function that owns the jump table", the candidates include the start points from rules 4 and 5. If a table comes after a pointer-only function, it belongs to that function.
   By default, the script **no longer** accumulates `// ERROR` targets from the recompiler output into `branch_targets.txt`. Errors caused by bad splits would permanently pollute it. Set `LO_ACCUMULATE_ERRORS=1` when needed.
The generated results are written into the TOML `functions = [...]` list (116 entries, including 47 pointer-only functions). It converged at round 8, with 0 unresolved targets and 0 jump table errors.

## Generated output

`LostOdysseyRecompLib/ppc/`: 250 files, 252 MB, 62808 functions. One pass of XenonRecomp built with MSVC (`__builtin_bswap*` is mapped to MSVC intrinsics in `tools/xexdump/CMakeLists.txt`) takes about 90 seconds.

## Instructions missing from upstream XenonRecomp (implemented in local patches)

Upstream reports Unrecognized for 5985 instructions (3129 of them in the .embsec sections). All are 16-bit vector operations and a few scalar ones:
vslh 2351, vsrah 979, vsubshs 901, vspltish 604, vandc 265, vmaxsh 238, vminsh 92, vpkswss 81,
vctuxs 78, vcmpgtsh(.) 82, vcmpgtsw. 48, vaddsws 45, eqv 30, vaddsbs 26, vnor 20, vrlh 18,
vcmpequh(.) 24, vavguh 16, vsrh 12, vpkuhus 12, vpkswus 12, mulhd 12, mulhdu 8, vsububm 7,
vpkshss 7, vsrab 6, vpkuwus 6, cror/crorc 4, frsqrte 1. In addition, vcmpgtuh. has 30 cases that are missing the CR6 write-back.
The implementation is in `tools/patches/XenonRecomp-lostodyssey.patch` (a patch to the submodule's recompiler.cpp). `tools/build_tools.bat` applies it automatically before building. Vector elements are stored in reverse order on the host. Pack instructions take (vB, vA), following upstream's VPKSHUS convention. These implementations have not been tested yet. If the audio or DSP path fails at run time, suspect them first.

Remaining warnings: four data words at 830DA0BC (such as `46000201`), placed between the import stub's `mtctr r11; bctr`, cannot be decoded in the function recompilation loop. They are output only as comments and are never executed, so they can be ignored.

## Conditional bclr (XenonAnalyse boundary analysis fix, 2026-09-04)

Upstream `Function::Analyze` checks whether `bclr/bcctr` is conditional with `!(BO & 0x10)`. That only checks the "ignore CR" bit and ignores the "ignore CTR" bit (0x04). As a result, `bdzlr` (BO=0x12) and `bdnzlr` (BO=0x10) were treated as unconditional returns. The function ended there, and the following instructions were split off as separate functions.
This image hits the CRT `memset` (0x82B7BC40), whose byte tail is `stb; bdzlr; stb; bdzlr; stb; blr`. When the length mod 4 is 2 or 3, it writes 1 to 2 bytes too few. The symptom: 61 of the 63 entries of `USkeletalMeshComponent::SkelControlIndex` were filled with 255, and the battle scene crashed on an out-of-bounds access (see [kernel.md](../archive/notes/kernel.md)). The fix uses `(BO & 0x14) != 0x14`. It also changes the relative offset of `SearchBlock(lBase)` in that branch to an absolute address. The old offset never found anything, so it scanned repeatedly. The fix is in `tools/patches/XenonRecomp-lostodyssey.patch` (XenonAnalyse/function.cpp).
After rerunning, the function count dropped by two (0x82B7BCD0 and 0x82B7BCD8 were merged back into memset). Nothing else changed. Those two lines were removed from `baseline_func_mapping.txt`.
XenonRecomp rewrites only files whose content changed. However, a change in the function count renumbers every split file after 175, and all of them are recompiled.

## vupkd3d128 unpacking (2026-09-04)

Upstream implements only D3DCOLOR and SHORT_2. Other types generate `__builtin_debugtrap()`. This image has 36 FLOAT16_4 cases (IMM=20, the half-precision conversion used by XAudio. New game hits it when entering battle and crashes with BREAKPOINT) and one FLOAT16_2 case (IMM=12). The patch implements types 2 (UINT_2101010), 3 (FLOAT16_2), 4 (SHORT_4), and 5 (FLOAT16_4), following Xenia's channel mapping. The packed data is in the last one or two PPC words (host u32[0]/u32[1]). The result x..w maps to host f32[3..0]. `PPCHalfToFloat` is in XenonUtils/ppc_context.h (it is copied to ppc/ with the header during recompilation).
Now only one `blrl` trap remains in the output.

## To do
- [x] setjmp = 0x82DF34A0 (entry with the global hook check, 7 `bl` call sites. The body at 0x82DF34B4 saves f14-f31, r13-r31, and v into r3). longjmp = 0x82DF3060 (21 call sites, restores FPRs by hand, then calls RtlUnwind through 0x82DF334C).
- [ ] Nature of the .embsec_* sections (8 small code sections with garbled names. .pdata covers up to 8312D330).
- [x] Full clang-cl 22 build passed (out/smoke/all, object files about 250 MB total).
