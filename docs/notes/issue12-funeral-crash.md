# Issue #12: funeral flower hand-in investigation

## Current checkpoint — 2026-09-08

[Issue #12](https://github.com/freefrank/LostOdysseyRecomp/issues/12) reports a crash when talking after collecting ten flowers for Lirum's funeral on an MSI Claw A1M 135H. At triage start, the user confirmed that only the Issue report was available, with no local reproduction. The reporter's “latest” does not identify an executable, backend, game edition or driver. After the maintainer [requested the log and save](https://github.com/freefrank/LostOdysseyRecomp/issues/12#issuecomment-5592448054), the reporter [uploaded `user00.zip`](https://github.com/freefrank/LostOdysseyRecomp/issues/12#issuecomment-5592895147) at 22:43:40 UTC. This supersedes the initial attachment-free checkpoint.

The reporter also said they had [bypassed the flower cutscene and encountered misplaced torch lighting](https://github.com/freefrank/LostOdysseyRecomp/issues/12#issuecomment-5592842816), followed by a separate [Cheat Engine 2x speedhack statement](https://github.com/freefrank/LostOdysseyRecomp/issues/12#issuecomment-5592845182). These comments do not establish when acceleration was active, how the scene was bypassed, or whether acceleration caused either symptom. The intact uploaded slot previews Kaim, level 18, 5:06, Cooke and Mack's House; its metadata and generic thumbnail cannot distinguish the house's two map variants or the flower/torch stage. Its 206,000-byte `save.bin` SHA256 is `c8501748a711e9806e8805419db572181546b988a82248d52c15d193b7dfd5b6`. The original archive and files are preserved under `out/issue12-triage/reporter/` for a separate normal-speed runtime check.

The latest public release checked in this task is v0.4.2. Local branch `0.5.0`, source `0.4.15`, commit `bd47a04` is a separate source checkpoint. Neither the local source nor an installation folder name establishes the reporter's binary. The local official-program run has now reproduced a crash at the reported flower hand-in boundary, and a second unchanged-official-program run captured the invalid object at the first execute-null exception. This remains a P1 investigation for milestone v0.5.0, with no confirmed repair, player acceptance or new release. See the [Claude investigation handoff](issue12-claude-handoff.md) for the bounded next experiment.

## Map and story evidence

Offline extraction of owned Asia Disc 1 resources identifies the reported objective in both English and simplified Chinese. Funeral Director Melvi requests ten white flowers for Lirum, and the messages distinguish an insufficient count, confirmation and completed flower hand-in. Later messages request torch branches and then declare all preparations complete.

| Native location | Script resource | Name from the current resource |
| --- | --- | --- |
| 108 | `nbr_1_scrw` | Ghost Town - City of Ruins, funeral variant |
| 109 | `nbv_1_scrw` | Ghost Town - Funeral Beach, funeral variant |

These IDs come directly from `xenon_loc.fpd`'s `loc/int/field/name_data.xml` and agree with the historical native map-definition capture. They do not use the previously misassociated font-display indices described in the [Issue #7 investigation](issue7-cutscene-crash.md#2026-09-07-correction-native-map-ids).

Both maps share the flower dialogue block, so the text alone does not establish where each pickup or NPC stands. In `xenon_scr.fpd`, the completed hand-in acknowledgement is zero-based JMD table row 51 at decoded offset `0x2B3C` in `mes/int/nbr_1_scrw.jmd`, and row 13 at `0xAB4` in `mes/int/nbv_1_scrw.jmd`. These are table indices, not yet proven runtime message operands. The latter file's rows 66–67 concern the later torch/preparation stages.

The actual debug-event text names `RT_072C` (gathering funeral branches), `RT_073C` (preparations complete), `RT_074B`, `RT_075C` and `RT_076A` as related funeral events. No execution link from the ten-flower hand-in to a particular one of these events has been established.

Selected original pickup scripts distinguish four flowers in location 109 (VM frames 54–57) and six in location 108 (frames 27–32) from the ten torch branches. The distinction follows each script's branch to Cooke's flower-count message. The C3 handler at `0x82A4BE28` provides a frame-associated object table at `manager + 0x5D20`; matching these pointers to runtime objects identified the four beach flowers. The official run has collected all four using the normal `Pick Flower` action, with actual 4/10 dialogue in `runtime/run-03/shot_34368.png`. Ordinary item points were pots and were excluded. No collection counter was written directly.

## Translation review and reproduction boundary

Focused original-PPC/current-generator review has located the custom story VM dispatcher at `0x829FDB40`, its loop at `0x829FD850`, scheduler at `0x829FD9F0`, and operand reader at `0x8229DDE8`. The selected review has not established a new translation defect. It does not rule one out elsewhere, and there is no evidence attributing this crash to Intel graphics.

The VM frame layout supports later bytecode correlation: at the dispatcher, `r4` is the frame, code pointer is at `+0x494`, PC at `+0x69C`, and event/coroutine index at `+0x6A0`. Its final indirect call is at `0x829FDBDC`. The actual failure below occurs in a different object virtual call, `0x823CB538`. Old ignored generated C++ files are not evidence of what the current executable contains.

The user authorized a fallback if static review cannot determine the cause: prepare an isolated native save before hand-in and let an agent actively trigger the story. The offline findings alone did not establish a reproduced crash. The runtime result below now provides a concrete failure for targeted analysis of the call preceding guest `LR=0x823CB53C`.

## Native save and official reload checkpoint

An isolated diagnostic program successfully entered the game's original event-debug map, `z0g_9_scrw` / native location 295. The real menu gesture is to hold LB and newly press D-pad Up; it is an input condition in the original script, rather than NPC dialogue. Selecting `Scenario Jump` and `RT_071_2C` entered location 109, played Melvi's funeral introduction and handed control to Cooke with the ten-white-flower objective. No flower counter or completion flag was manually edited.

This setup required a private, one-command map-jump bridge on the existing guest engine callback. It calls the original `0x82827580` implementation, which owns construction and copying of its FString/FURL request. The bridge preserves the full guest context; review of the original prologue found its stack writes below the incoming SP. Only one host translation unit was compiled and linked, with no guest regeneration or production-source modification. The diagnostic EXE combines retained development `0.5.1` host inputs with the previously audited semantics-r2 guest library; it is not the official v0.4.2 program. Its SHA256 is `f29252b5d602784554a2e5d921bc095240e5549c3406745c94db4bc98946a7c8`.

The game wrote a native starting checkpoint to slot 08 / `save/user07`: 206,000 of 206,000 bytes, status 0, Cooke level 15, Ghost Town - Funeral Beach. The operator had collected no flowers. The frozen `save.bin` SHA256 is `5010488c80b9814dc0aadaf1f5f46a48e2a019fc8895cc6ac856ae8f1b5b16dc`; metadata, thumbnail and write evidence are retained in `out/issue12-triage/runtime/checkpoints/flowers-zero/manifest.json`.

A separate process using the unmodified official v0.4.2 EXE (`13f1294bbb54efbf9a712a066441cb019ba5497cf1242bde6905e6c8bc1757b6`) loaded this checkpoint into location 109 with Cooke. Movement and normal interaction reached Melvi's still-active white-flower objective (`runtime/run-03/shot_7803.png`). All ten flowers were then collected using the original pickup action: four at location 109, followed by six at location 108 after the original map exit. Same-map coordinate assistance located the interaction points; normal positioning and button input performed each pickup without direct counter or completion-flag writes. The count reached 9/10 in `shot_58901.png`, then 10/10 in `shot_61684.png`.

After the original exit back to location 109, the official program wrote slot 09 / `save/user08`, 206,000 of 206,000 bytes with status 0, at runtime 2374.997 seconds. This frozen ten-flower checkpoint is under `runtime/checkpoints/flowers-ten/`; `shot_70717.png` shows the resulting Cooke level 15 Funeral Beach slot. Its `save.bin` SHA256 is `32c8b00e07b60ad7bbfa2b08aec0ac9ecc5eb66ee9cb5c19cbeb6f357dac9772`. The same-process hand-in did not reload it; the targeted capture below subsequently established independent reload.

Melvi acknowledged the gathered flowers in `shot_75402.png`. The next normal A input (serial 193, accepted at runtime 2578.404 seconds) was followed by process termination and the automatic crash report. `runtime/run-03/runtime.log` records `ACCESS_VIOLATION`, code `0xC0000005`, **execute address zero / host RIP zero**, guest `CTR=0`, `LR=0x823CB53C`, `r1=0x02343AA0` and `r3=0x06559AE0`. This is a real local reproduction at the reported hand-in boundary on the identified official program. It does not yet prove that the reporter encountered the same underlying fault.

The run used normal speed without Cheat Engine; 2x acceleration is therefore not necessary for this local failure. Reporter-save loading was suspended after this failure to preserve the focused investigation. The follow-up run had a specific missing-memory capture objective: it independently loaded slot 09, confirmed the completed-flower acknowledgement, captured the object headers and owner-array relationship at the first execute-null exception, detached cleanly and allowed the original crash handler to complete. The actual release, first corrupting writer and last valid object owner remain unobserved; the [Claude handoff](issue12-claude-handoff.md) records the next narrow watchpoint experiment.

## Exception-time object capture

The first failure supplied registers but no object bytes. A second unchanged-official-program run loaded the frozen slot 09 and reached Melvi's completed-flower acknowledgement (`runtime/run-04/shot_11163.png`), confirming that the 10/10 task state survived native save and independent load. A bounded external debugger then read the first execute-null exception after the next normal A input. It made no guest-memory, register, code or task-state writes. The process identity, ABI context, memory base, guest LR/CTR and readable stack checks all matched.

At the fault, guest `r3=0x03B6A480`. Its first word is `0x03B6A560`, a heap address one `0xE0` block later; that block points to `0x03B6A640`, then another block `0xE0` later. The second header word is 1, and the supposed function-table slot `+0x124` is zero. The owner relation is `BE32[r31]=0x82002900` for the static table, while the owner object's field is at `[r31+0x134]`; the captured owner entry then points to the six-item inner array. The original call at `0x823CB538` reads this entry through the owner container; the selected direct PPC load/rotate/call sequence showed no translation discrepancy.

The original allocator's `0x82298990` free path, beginning at `0x82298A1C`, writes `[block+4]=1`, links `[block]` to the old `[pool+0x10]` head and installs the block as the new head. The captured headers match that free-list node format. This narrows the failure to an invalid object representation retained or supplied to the virtual-call path. It does **not** capture the actual allocation, construction, release or overwrite, so the responsible upstream code and whether translation contributes remain unresolved. A null-call skip would conceal the broken object contract.

The probe returned the original exception as unhandled, detached and removed its own temporary thread suspension. The official crash handler then completed both report sections. Both the game and probe exited, with no remaining debugger attachment or suspension. Evidence: `out/issue12-triage/debug-probe/capture-run04/{capture.json,result.json}`, `runtime/run-04/crash-evidence/manifest.json` and `translation/crash-analysis/REPORT.md`. No production repair or additional broad gameplay validation was performed.

The generated slot 08 (0/10) and slot 09 (10/10, before hand-in) are retained in the small native-save ZIP under `out/issue12-triage/delivery/`. It contains the three original files for each slot and no player profile or reporter attachment. These are test checkpoints; preserve occupied player slots and use an isolated save directory.

Owned runs operate in the background without focus activation and mute only final audio output, preserving decoding and dialogue resource loads. Original saves and executables remain unchanged. The diagnostic process was ended by the harness after saving; this is not natural-shutdown coverage. The initial official control/load run and this independent task-state reload have different validation purposes; previously passed suites were not repeated.

During bridge preparation, the current shared build's guest-library input matched a retained pre-repair library. This identifies a stale input in the current reusable link recipe; without historical library identity or executable disassembly it does not prove what the older development EXE contains. It does not implicate the official release or establish Issue #12's cause. The diagnostic link explicitly selected the audited repaired library. The exact limitation is recorded in `out/issue12-triage/bridge/provenance-gap.json`.

## Evidence

Private extracts and detailed provenance remain ignored under `out/issue12-triage/text/`, including `REPORT.md`, `text-evidence.json`, `target-extractions.json`, `name-data.xml` and selected script resources. The extraction records archive offsets, stored/decompressed hashes and decoder checks; all 287 English script JMD files were searched without a decoding failure. The local game bytes are not the reporter's verified bytes. No raw game text corpus or binary resource is included in this documentation.

[Current status](../STATUS.md) · [Roadmap](../ROADMAP.md)
