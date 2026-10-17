# Repository publication

Develop in an isolated worktree and verify remote names with `git remote -v` before a push. `origin` is the public GitHub repository, `freefrank/LostOdysseyRecomp`. Publish reviewed changes on a feature branch and open a pull request; follow the maintainer's review gate before merging. Reuse relevant passing checks; a push alone does not require another build or game run.

```powershell
$publishBranch = git branch --show-current
$publishRevision = git rev-parse HEAD
git -c push.followTags=false push origin "${publishRevision}:refs/heads/${publishBranch}"
git ls-remote origin "refs/heads/${publishBranch}"
```

The readback hash must equal `$publishRevision`. Stop if a push fails; resolve connection, authentication or divergent-history errors without force-pushing. Review the unpublished diff for private or generated content before publication.

All CI/CD runs on GitHub Actions using GitHub-hosted runners. Pushes and pull requests start the native checks when their path filters match; manual dispatch can run a skipped workflow on a branch. There is no second CI remote to push to. See [GitHub Actions checks and releases](notes/ci.md) for workflows, results, credentials and caches.

The legacy `tools/push_all.ps1` helper hardcodes `origin` and `github`. It needs a separate remote-selection update before use with this publication path. Do not rename remotes merely to follow an old example.

The pre-cleanup history was retained only on the former Gitea mirror in `archive/pre-public-cleanup-2026-09-05`. This is historical storage, not a publication remote. Never merge that branch into `main` or publish it to GitHub. Do not use `--all`, `--mirror` or automatic tag pushing.

Release tags must be created from reviewed public commits and pushed explicitly to `origin`. A `v*` tag starts the release pipeline, creates a draft and builds the Windows ZIP, AppImage, Flatpak and Android APK. Upload the separately built macOS disk image to the draft before publication ([macOS releases](MACOS_RELEASE.md)).

```powershell
git push origin refs/tags/vX.Y.Z
gh run list --repo freefrank/LostOdysseyRecomp --workflow release.yml
```

For a build-only rehearsal, dispatch the workflow on the intended branch with an empty `release_tag`:

```powershell
gh workflow run release.yml --repo freefrank/LostOdysseyRecomp --ref <branch> -f release_tag=
```

Check the GitHub run and published assets before announcing a release. The publish job dispatches the Discord announcement workflow after publication; build-only rehearsals do not publish or announce anything.

Personal game data, saves, generated sources and analysis outputs stay ignored. Dependency submodules retain their upstream histories and licenses.
