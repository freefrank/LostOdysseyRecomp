# Repository synchronization

Develop in the main checkout. `origin` is Gitea; `github` is GitHub. Both publish
the same sanitized `main` history. The earlier `out/github-release` export is a
historical preparation artifact, not a second development checkout.

After reviewing, testing and committing a change:

```powershell
.\tools\push_all.ps1 -CheckOnly
.\tools\push_all.ps1
```

The script checks the public baseline, new commit attribution after the reviewed
v0.5.0 public baseline, and tracked artifact
paths, pushes the exact same commit to both remotes, then verifies both remote
heads. It publishes committed changes only. It does not commit local edits or
replace code review and secret scanning. If the second push fails, fix the
connection or authentication and rerun; the first push is safe to repeat.

The pre-cleanup history is retained only on Gitea in
`archive/pre-public-cleanup-2026-09-05`. Never merge this branch into `main` or
publish it to GitHub. Do not use `--all`, `--mirror`, or automatic tag pushing.
Release tags must be created from reviewed public commits and pushed explicitly
to both remotes. Hosting a binary release is a separate packaging step.

Personal game data, saves, generated sources and analysis outputs stay ignored.
Dependency submodules retain their upstream histories and licenses.
