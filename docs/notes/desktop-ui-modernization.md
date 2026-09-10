# Desktop UI modernization

This record covers the additional installer, updater and Debug Menu work targeting v0.5.0. Intermediate 0.4.xx versions remain internal. Public v0.4.2 packages and historical test binaries retain their original identities.

## Design and scope

Retain lightweight Tk/native Win32 implementation, navy surfaces, silver text and a restrained warm accent. Keep persistent copy short: action names, useful state and errors. Window chrome must preserve taskbar, resizing, keyboard and DPI behavior. Existing import and update transactions remain the baseline; this work does not add a framework or change update channels.

## Installer — source 0.4.16

Completed locally and reviewed by the coordinating agent. The new Windows frame keeps native sizing, minimize/maximize, system menu and close behavior. Other platforms retain their OS frame. Content review supports both scroll directions and full details; long paths retain their original values. Unknown totals use indeterminate progress, cancellation preserves completed work, and busy close waits for worker completion. Unchanged status/progress values do not invalidate widgets during polling.

Nine focused UI checks and two directly affected existing controller checks passed. Tests cover resizing hit targets, minimum layout, long paths, cancellation/retry/close, native styles, idle invalidation, DPI metrics and minimizing without activation. Real normal/minimum layouts were reviewed after the user's copy reduction. Physical monitor moves and foreground taskbar/drag gestures were not exercised; no game or importer backend suite was repeated. Source evidence is in `out/v0.5.0/ui-modernization/installer/REPORT.md`.

Screenshots use actual widgets with synthetic preview rows on a private, inactive Windows desktop. No `SwitchDesktop` call occurs; the parent observes the same foreground HWND. Tk helper windows prevent immediate thread restoration, so the capture worker exits and the parent verifies the private desktop was released. These images are UI evidence, not content-import evidence.

Updater is committed in source 0.4.17 (`9c2dc76`); its focused fixture and reviewed normal/unknown/narrow Chinese renders are recorded in `out/v0.5.0/ui-modernization/updater/REPORT.md`. Debug Menu is committed in source 0.4.18 (`294df07`); the final focused evidence is recorded below. The 0.4.18 combined development build/package is recorded in `out/v0.5.0/ui-modernization/build/REPORT.md`; no public release is established by this checkpoint.

## Debug Menu — source 0.4.18

The native Debug Menu fixture passed pagination callbacks, child-focus F1/Escape/Gamepad-B close behavior, caption hit-close, viewport focus re-clamping, stable font/current-DPI metrics, bilingual busy/error states and cleanup. Evidence: `out/v0.5.0/ui-modernization/native/debug-run.log` (PASS, PID 32556, exit 0). The discovered 150% DPI layout failure was fixed by retaining scoped-DPI context while Toggle/Update controls are created and laid out; the shared chrome header remained unchanged. The final English screenshot refresh called Update before capture and did not rerun the passed functional checks. This fixture remains source-local evidence; the separate 0.4.18 combined build/package is recorded in the build report, without establishing manual user acceptance or a public release.

The game main window sizing request is tracked separately as QOL13: its client/presentation size should follow the selected physical pixel resolution without extra 100/125/150/200% desktop-DPI enlargement. Config, Installer, Updater and Debug Menu remain DPI-adaptive; this requirement does not alter the completed Debug Menu fix.

## Installer drag-dispatch re-entrancy checkpoint — 2026-09-10

The v0.5.3 release installer was reported to stall and crash during dragging; four local ucrtbase `0xc0000409` reports were consistent with the captured nested Tk/Win32 path. LLVM lldb analysis of dump 41648 ended at `PyEval_RestoreThread` with the GIL released and the current Python thread state `NULL`: a Tk binding synchronously entered `ctypes` `SendMessageW`, the Python window procedure called `CallWindowProcW`, and nested Tk processing reached the abort path.

The local fix in `tools/installer/window_chrome.py` posts `WM_NCLBUTTONDOWN` with the real signed screen coordinates, allowing the Tk binding to return before native message handling continues. `python -B tools/tests/test_installer_ui.py DragDispatch` passed 1/1 using a message-only HWND, without showing a window; it verifies native queue dispatch and negative coordinates. The earlier InstallerUI fixture setup failed its foreground-HWND assertion twice before drag behavior ran and is no longer used for this checkpoint.

The reporter confirmed the real installer drag fix. An installer-only local package is available at `out/installer-drag-fix/dist/InstallGame.exe` (11,888,743 bytes; SHA256 `707CD7D2E9F4AB3BF33363E172FAAD5CFCFA6B1A53161FEE0E7F53735B7C7FA7`), built with Python 3.12.10 and PyInstaller 6.22.2. Read-only inspection confirmed the embedded `WindowChrome.drag` uses `PostMessageW`, not `SendMessageW`, and the PYZ includes all four required modules. The agent did not launch the packaged executable; runtime acceptance comes from the reporter. This remains a local unpublished installer fix; no installer import regression, game test or publication is established.

## Real DLC validation checkpoint

Three original Lost Odyssey LIVE/STFS packages passed unified Folder import and explicit Files duplicate import. The second pass imported zero packages and recognized three unchanged packages; exported bytes/hashes/modification times, source packages and the isolated game-path marker stayed unchanged. The result covers the Dungeon Pack, Double Bonus Pack and Triple Bonus Pack. Evidence: `out/v0.5.0/dlc-validation/real-import.json`.

A single background, muted run of the historical DLC-capable EXE (`28415cb4b1d5a2b218dbf293e0871926fcf19c8e46262b463b26da3d340e5d46`, source 0.5.1) bound three content roots. It read 4,096 bytes of `LODLC002.fpi`, 2,048 bytes of `LODLC001.fpi`, and 64 bytes of `spa.bin`, then faulted reading guest address `0x1A`. `LODLC003.fpi` was not opened. No save, reward or dungeon was exercised; full real-DLC compatibility remains unresolved. This old EXE is not the current UI output, and its guest archive membership is not inferred from present build-directory libraries. Evidence and crash sequence: `out/v0.5.0/dlc-validation/runtime-01/evidence/manifest.json` and `read-and-crash.txt`.

This signature differs from the established Issue #12 GC/render race. The merged mitigation is validated only by diagnostic runs; current production validation remains pending. Claude paused after reaching its limit, and the Sol read-only recovery organized evidence; no player acceptance or broad performance coverage is established here.
