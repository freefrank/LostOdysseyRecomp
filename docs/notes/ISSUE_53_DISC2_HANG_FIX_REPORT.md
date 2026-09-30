# Issue #53 Disc 2 loading investigation

Date: 2026-09-19

Status: **Defensive I/O locking fix implemented for the next release; original root cause unconfirmed.**

## Scope and evidence

This change started from clean `main` at `ff9093ec91a2fc6f05ce5f2f61b1821e429ea6c3`, source version 0.6.2. The reported player build was `v0.5.20 / 9a1617af1b05-dirty`. The [original attachment](https://github.com/user-attachments/files/32422006/issue-53-disc2-hang.zip) and matching released AppImage localize the blocked operation to `FileHandle::ioMutex` entered by `NtReadFile`: Thread 13 / LWP 153811, mutex `0x17c007468`, object + 8. The AppImage SHA-256 is `fac6a93384d7e8da09bfac680ef3defeb745df2c8be6aed73a3205262bb71dc8` and matches the reporter's environment record; newer symbols were not used to interpret the old binary.

The attachment contains no lock owner, registers, core, save or complete wait-for chain. There are 348 later swap heartbeats from the same log thread after the final GPU warning, beginning 0.724 seconds later. The original owner and cause remain unknown. Historical raw-pointer lifetime risk is a hypothesis, not proof of this incident's cause.

## Defensive fix

Read, write and scatter paths now hold `ioMutex` only through seek, transfer, position and size updates. Local results and a retained `shared_ptr<FileHandle>` support logging, IOSB/APC publication and event signaling after unlock, reducing nested lock dependencies. Each request still publishes its buffer and IOSB before notification; APC delivery remains on the issuing thread's alertable path. The baseline already had `shared_ptr` lifetime protection and destruction outside the handle-table lock. These are retained and covered, not claimed as newly implemented fixes.

Opt-in `LO_IO_DIAGNOSTICS=1` recording and the exported `LoDumpIoDiagnostics(path)` JSONL snapshot expose active requests before a mutex wait. Diagnostics default off and are bounded to 256 active requests and 2048 history records. Records include object instance/address, mutex address, host TID, guest PCR, path, requested/resolved range, stage and owner observations. Creation, close, destruction and disc selection are recorded. Close records use the actual object removed from the table, so reused handle tokens are not attributed to an earlier object. Snapshots never acquire the file I/O mutex or dereference recorded object addresses; owner observations and missing-record counters must be interpreted with the timestamps.

## Validation

Final Windows RelWithDebInfo and WSL Manjaro Linux Release `LoStorageTest` builds and their directly affected selectors passed, exit 0. Commands were run from the repository, using new output directories:

```text
cmake --build out/build/windows-clang --target LoStorageTest --parallel 3
python -B tools/tests/io_lifetime_test.py out/build/windows-clang/LostOdysseyRecomp/LoStorageTest.exe --mode io-lifetime --out out/issue53/windows-lock-scope-lifetime
python -B tools/tests/io_lifetime_test.py out/build/windows-clang/LostOdysseyRecomp/LoStorageTest.exe --mode io-diagnostics --out out/issue53/windows-lock-scope-diagnostics

cmake --build /home/freefrank/lo-menu-runtime --target LoStorageTest --parallel 3
python3 -B tools/tests/io_lifetime_test.py /home/freefrank/lo-menu-runtime/LostOdysseyRecomp/LoStorageTest --mode io-lifetime --out out/issue53/linux-lock-scope-lifetime
python3 -B tools/tests/io_lifetime_test.py /home/freefrank/lo-menu-runtime/LostOdysseyRecomp/LoStorageTest --mode io-diagnostics --out out/issue53/linux-lock-scope-diagnostics
```

| Final tested binary | SHA-256 |
| --- | --- |
| Windows `out/build/windows-clang/LostOdysseyRecomp/LoStorageTest.exe` | `c11a4efcd2ec224c909d3c0e7aeb467767076791b264edeeca9535c98547ef6a` |
| Linux `/home/freefrank/lo-menu-runtime/LostOdysseyRecomp/LoStorageTest` | `00646404f69127fcdd965b8932d02e02e6f310744ab008dd7c14f8bf7833976a` |

Real guest imports cover deterministic close/read lifetime windows, duplicate handles, independent-file progress, and 8000 positioned reads with zero mismatches on each platform. Read, write and scatter requests paused after unlock permit another read of the same file to finish; closing their handle still allows correct completion. The read case verifies buffer/IOSB publication before the event, one consumable event notification, and exactly one APC delivered on the issuing thread's alertable path. Diagnostic snapshots check an actual owner/waiter and `TransferDone → IoLockReleased → CompletionPublished` ordering.

Earlier Windows/Linux checks are reused for unchanged behavior: the standalone audit `--handles` selector (24 checks each), `io-invalid-handle` with diagnostics off/on (protected offset page through read/write/scatter imports), and all seven existing disc-fixture modes. The valid disc sequence is `1→2→3→4→2→1`, including retained old handles, first reads and rejected selections preserving the old root. Only lifetime/diagnostics were rerun after the later lock-scope change. No failing-before/passing-after reproduction of the original hang is claimed; these are deterministic regression and defensive-behavior checks.

The runner applies a 30-second process deadline and attempts a debugger stack capture before terminating a timed-out child. Build logs are `out/issue53/{windows,linux}-build-lock-scope.log`; command results and JSONL snapshots are in the four output directories above. Earlier checks are retained in `out/issue53/review-targeted-20260919/` and `{windows,linux}-discs.log`. `out/issue53/final-validation.json` records final artifact identities and reused evidence.

## Runtime and acceptance boundary

The separately supplied save described as Slot 15 is archive file `user16/save.bin`; the current load menu displays 17, Disc 2, 18:03, Kaim Lv21 and Mack Lv15. An isolated baseline Windows Release v0.6.2 loaded it and rendered Sorcerer's Shrine – Altar of the Abyss using USA/Europe data, Vulkan and the RTX 5080, then returned through the game menu to the title screen. This baseline predates the present lock-scope change: source `7f99786f302b4ef3e5f672eacdba2b7a62972fda`, EXE SHA-256 `438838a32bc6a7227e30cd6832310d029d730508a9b4952fbe02f5bb32ee80a7`. Its runtime source/dependency diff against the task base is empty. Original ZIP and all extracted save/thumbnail hashes remained unchanged; task copies were isolated.

Three consecutive warm 20-second baseline samples are retained in `out/issue53/runtime/runs/baseline-1/performance.json`. A candidate comparison was not completed, so no performance conclusion is made. The final defensive candidate's gameplay, same-process multi-disc gameplay, the actual end-of-Disc-1 story transition, a 27-hour session and complete Disc 2 playability remain unverified at wrap-up. WSL CPU/import tests do not establish native Linux NVIDIA Vulkan behavior. No player acceptance or release publication is claimed.

The reporter's restart recovery supports an intermittent fault. The user accepted a defensive correction without requiring reproduction. This change addresses the suspected file-lock interaction for the next release; it does not establish the original owner or a complete deadlock chain. [Issue #53](https://github.com/freefrank/LostOdysseyRecomp/issues/53) remained open in the verified 2026-09-19 read.
