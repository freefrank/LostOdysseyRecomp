# Unicode startup and save paths (2026-09-06)

[Issue #4](https://github.com/freefrank/LostOdysseyRecomp/issues/4) reports a crash after the first save. The supplied log records a successful save write, followed by a changed encoding of the installation path when reopening the save. This points to a path-conversion defect, but does not establish the cause of the reported gameplay crash.

The local `save-fix` branch was created from `amd-fix`. Saved-content discovery and creation now use the same explicit UTF-8 conversion (`FileSystem::PathUtf8`) when registering roots. File-open, file-write and disc-selection messages also use UTF-8; volume device classification compares native paths without an ANSI conversion. This avoids the Windows code-page conversion previously performed by `std::filesystem::path::string()` on these paths.

The user subsequently reproduced a startup failure in a special-character directory: `default.xex` could not be read, and the log showed a prime (U+2032) in the game root but an acute accent (U+00B4) in the cache root. The command line included `--game`. The startup fix now obtains the original Windows Unicode arguments through `CommandLineToArgvW(GetCommandLineW())`, converts them to UTF-8, and uses `u8path` in `FindGameRoot`. Startup and XEX-loader path messages also use `PathUtf8`.

## Validation

The pre-fix storage test failed with exit code 1 under `Steamn´t games 存档`, reporting `No mapping for the Unicode character exists in the target multi-byte code page.` The failure occurred during the first file-open log conversion, before the later discovery/reopen stage. This reproduces a Unicode-path failure in the storage code, **not the complete in-game crash reported in the issue**. Local evidence: `out/save-fix-baseline-test.log`.

`LoStorageTest` built successfully. All eight post-fix runs exited 0: ASCII and Unicode directories, each tested with `write`, `read`, `overwrite` and `read-overwritten`. The tests exercise actual guest storage imports, same-process close/reopen with byte-for-byte payload validation, fresh-process discovery and readback, and overwrite-save behavior. Each read run also completed 8,000 concurrent positioned reads with zero mismatches. Local evidence: `out/save-fix-{ascii,unicode}-{write,read,overwrite,read-overwritten}.log`.

To repeat the checks in PowerShell from the repository root after building `LoStorageTest`, use new isolated directories:

```powershell
$storageTest = '.\out\build\windows-clang\LostOdysseyRecomp\LoStorageTest.exe'
$testRun = Join-Path $env:TEMP ('lo-save-path-' + [guid]::NewGuid())
foreach ($variant in @('ascii', 'unicode')) {
    $testDirectory = Join-Path $testRun $variant
    foreach ($mode in @('write', 'read', 'overwrite', 'read-overwritten')) {
        $testArgs = @($mode, $testDirectory)
        if ($variant -eq 'unicode') { $testArgs += 'unicode' }
        & $storageTest @testArgs
        if ($LASTEXITCODE -ne 0) { throw "$variant/$mode failed" }
    }
}
```

The Unicode test creates its non-ASCII child directory internally, so shell argument encoding does not determine the path under test. It uses synthetic payloads and requires no original game data or user saves.

The startup regression script `tools/tests/startup_path_test.py` passed all eight cases: ASCII, acute accent, prime and Chinese directories, each tested with explicit `--game` and a copied EXE launched without arguments. It uses a synthetic non-XEX file and verifies that the exact path is preserved and the file reaches the expected parse rejection. The script exited 0 (`out/save-fix-startup-test.log`); each runtime process intentionally exited 1 after rejecting the fixture. No guest or graphics execution takes place. The earlier binary containing only the save-path fix passed both ASCII cases but failed UTF-8 log decoding in the acute-accent `--game` case (`out/save-fix-startup-baseline.log`). Inspection of that fixture's runtime log confirmed the same startup failure reported by the user: a garbled game-root message, U+2032 in the game/d roots, U+00B4 in the cache root, and failure to read `default.xex`. The synthetic fixture therefore reproduces the startup path loss independently of gameplay.

```powershell
python tools/tests/startup_path_test.py out/save-fix-preview/LostOdysseyRecomp.exe
```

The updated runtime and `LoStorageTest` built successfully (`out/save-fix-startup-build.log`). The EXE/PDB copies in `out/save-fix-preview` are replacement binaries for a test installation, not a complete installation package. Runtime EXE SHA-256: `D584876DA6B73FA58B15D7C34BA6FA847591A99A3E545F46710217FC2791A9AD`. No game was run for these automated checks.

## Acceptance and publication

An early user attempt did not trigger the problem; its supplied path was ASCII, with U+0027 rather than the issue's U+00B4. The later special-character startup failure described above supersedes that attempt as reproduction evidence. The user described their system as using UTF-8; a separate PowerShell observation returned `GetACP() = 936`, while `Console.OutputEncoding` and `OutputEncoding` both reported 65001. These process observations do not establish the game's active code page.

The user has reproduced the startup defect, not accepted the updated fix in gameplay. The complete Issue #4 save-point crash remains unreproduced, and in-game acceptance is pending. The fix is included in published v0.2.2 from `f03efe370d444db1a8a9c1213c240da697f58504`. Hosted CI passed; the official EXE passed all eight startup-path cases, and a Chinese-working-directory Map 12 smoke run passed. See [release verification](release-0.2.2.md). Automated path and storage tests do not establish gameplay success.
