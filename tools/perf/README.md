# City performance capture

`drive-city.ps1` is an **active game driver**, not a read-only analyzer. It
launches the supplied Windows build, sends automated menu and movement inputs,
requests screenshots, writes logs and a summary, then stops the process it
started. The game can also change its save files. Provide an isolated build,
output directory, and the player's save directory explicitly. The script
compares save metadata before and after but does not restore saves.

```powershell
pwsh -File tools/perf/drive-city.ps1 -Help
pwsh -File tools/perf/drive-city.ps1 `
  -RunDirectory 'C:\path\to\isolated-game-build' `
  -OutputDirectory 'C:\path\to\capture-results' `
  -PlayerSaveDirectory 'C:\path\to\player-save'
```

The driver stores screenshots under `<RunDirectory>/shots2`, `pid.txt` under
`<RunDirectory>`, logs and input controls under the system temporary directory,
and `drive-summary.json` / `drive-status.json` under `<OutputDirectory>`. If
available it runs the adjacent `classify-city-timing.ps1` on the resulting log.

`analyze-city-comparison.py` only reads saved summary and log files; its JSON
results go to stdout. Relative `log` and `retained_log` fields resolve next to
the summary file, independent of the shell's working directory:

```sh
python tools/perf/analyze-city-comparison.py /path/to/run-a/drive-summary.json /path/to/run-b/drive-summary.json
```
