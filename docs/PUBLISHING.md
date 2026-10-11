# Repository synchronization

Develop in the main checkout. Remote names are local configuration: run `git remote -v`
before a push. In the current checkout, `origin` points to the GitHub repository and
`zkx` points to the Gitea mirror. Do not assume a remote named `github` exists. The
earlier `out/github-release` export is a historical preparation artifact, not a second
development checkout.

After an authorized push has been requested, review and commit the intended changes,
then publish the same explicit revision to the verified remotes. Reuse the relevant
passing checks; a push alone does not require another build or game run.

```powershell
$publishRevision = git rev-parse refs/heads/main
git -c push.followTags=false push origin "${publishRevision}:refs/heads/main"
git -c push.followTags=false push zkx "${publishRevision}:refs/heads/main"
git ls-remote origin refs/heads/main
git ls-remote zkx refs/heads/main
```

Both readback hashes must equal `$publishRevision`. Stop if a push fails; resolve
connection, authentication or divergent-history errors without force-pushing.
Review the unpublished diff for private/generated content before publication.

Since 2026-09-30 the FG contract (including the reusable FG core), FG game integration compile and
review-regression pull request checks run as Gitea Actions on the `zkx` remote. A branch push to `zkx`
starts them (subject to each workflow's path filter), and each result is reported back
to the GitHub commit as `gitea/<workflow>`. Since 2026-10-01 release packaging runs
on Gitea as well: a `v*` tag pushed to `zkx` builds the Windows ZIP, AppImage and
Flatpak and publishes them to the GitHub release. The matching GitHub workflows are
manual-only; the Mod API and Wiki workflow and issue triage stay on GitHub Actions.
See [Pull request checks and releases on Gitea](notes/ci-gitea.md) for pushing,
reading results and the required Gitea secret.

The legacy `tools/push_all.ps1` helper still hardcodes `origin` and `github`.
With this checkout's `origin`/`zkx` configuration, even `-CheckOnly` cannot complete.
Its public-baseline, attribution and tracked-path checks remain useful historical
implementation detail, but the helper needs a separate remote-selection update
before it is usable here. Do not rename remotes merely to follow an old example.

The pre-cleanup history is retained only on Gitea in
`archive/pre-public-cleanup-2026-09-05`. Never merge this branch into `main` or
publish it to GitHub. Do not use `--all`, `--mirror`, or automatic tag pushing.
Release tags must be created from reviewed public commits and pushed explicitly
to both remotes, GitHub first, because the Gitea release workflow creates the
draft with `gh release create --verify-tag`:

```powershell
git push origin refs/tags/vX.Y.Z
git push zkx refs/tags/vX.Y.Z
```

The push to `zkx` starts the release build. Check the Gitea run and the
published assets before announcing the release.

Personal game data, saves, generated sources and analysis outputs stay ignored.
Dependency submodules retain their upstream histories and licenses.
