# Guest critical-section byte order

## Observed deadlock (2026-09-05)

An independent first-battle run stopped submitting frames at swap 6279 while
audio logging continued. The debugger found the main thread waiting for
`0x832d2d84`, owned by worker PCR `0x01e30000`, and that worker waiting for
`0x832188cc`, owned by main PCR `0x00403000`.

Guest function `sub_82CC3FD0` reads `CS + 20` using `lwz`, releases the section
that many times, then reacquires it the same number of times. Its live `r30`
was `0x01000000`: the HLE had stored recursion count 1 as little endian.
The excessive leave loop permits another thread to acquire the section and
then corrupts its recursion count. This explains the observed circular wait.
The SDK comment that these fields are never accessed by guest code is false
for this game.

Local evidence: `out/fire-constants-01/host-stacks-full.txt`,
`critical-section.bin`, and `run.log`. The debugger was detached without
modifying guest memory. This is a first-battle deadlock; equivalence to the
reported camp hang remains unproven.

## Change and verification

`kernel/critical_section.h` now encodes both recursion count and owning thread
as big-endian words. Enter, try-enter and leave share this implementation.
Atomic owner CAS/wait use encoded thread IDs; release publishes a zero owner
only after writing the zero recursion count. The existing PCR identity and
owner-based host synchronization strategy remain in use. LockCount/event
emulation is outside this fix.

[Xenia's RTL implementation](https://github.com/xenia-project/xenia/blob/master/src/xenia/kernel/xboxkrnl/xboxkrnl_rtl.cc)
also declares recursion count and owner as big endian at offsets 0x14 and 0x18.
The inspected source is preserved locally as `out/xenia-rtl-reference.cc`.

`LoCriticalSectionTest` uses an independent byte-by-byte guest word decoder,
checks the guest release/reacquire pattern and try-enter behavior, and runs
100,000 contended recursive acquisitions across four host threads. Runtime
regression through battle and camp is still required before closing the hang
report.

Runtime regression: independent process 42620 completed five ordinary attacks, reached the tank introduction (shot_5861.ppm), and returned to a controllable battle menu (shot_6968.ppm, HP 364). Frame submission continued past the previous run's stall count. This supports the opening-path regression only, not camp coverage. Evidence is in out/critical-section-runtime-01; the test executable SHA-256 is 038E952E230A4FD1CEBFDF1ECF90B9EA006C5E61706ABC72F8A9B573DCEF1FB1.

Camp regression: out/critical-section-camp-01 uses the final build with layout assertions (SHA-256 92665429AF1764B2C6399AFD54D310AB5BBB4D93F9BD655C71515261EF4BC1F7). It loaded the independent Gorge save, walked the normal route without teleportation, triggered the soldier scene, and regained control in camp. Position changed from (2660.9949,419.99747,147.81354) to (2568.3096,184.4953,167.73782) after input. Menu open/close and chest interaction also worked. Screenshots 7339 (camp), 8609 (menu), 12213 (Open prompt), and 13350 (chest opening) are local evidence. No natural deadlock occurred on this run. Hidden-window coverage cannot establish a visible-window message-pump issue is fixed, nor prove the original camp report has the same cause.
