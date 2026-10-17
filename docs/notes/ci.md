# GitHub Actions checks and releases

All active CI/CD workflows live in [`.github/workflows/`](../../.github/workflows/) and run on GitHub-hosted runners. The Gitea and T640 runners are retired. The [Gitea history](../archive/notes/ci-gitea.md) preserves earlier setup details, release records and timings; those commands are historical.

## Pull request checks

| Workflow | Jobs | Hosted runner |
|---|---|---|
| `fg-cpu-contracts.yml` | GPU-free FG contracts and reusable FG core | `ubuntu-24.04`, `windows-2022` |
| `fg-game-integration.yml` | Production renderer/video compile, FG bridge link and input contract | `windows-2022` |
| `review-regressions.yml` | Sanitizer regressions, updater AppImage restart and packaging script fixtures | `ubuntu-24.04` |
| `android-apk.yml` | Android arm64 debug APK, lint and runtime JVM tests through `tools/android/ci_build.sh` | `ubuntu-22.04` |

The FG and regression checks run on branch pushes, pull requests and manual dispatch, subject to each workflow's path filters. Pull requests targeting `LostOdysseyNX` remain eligible. Android keeps its push/manual triggers: it needs private inputs, so a fork pull request does not receive the deploy key. Results appear as native GitHub checks; there are no `gitea/<workflow>` commit statuses.

```powershell
git push origin <branch>
gh run list --repo freefrank/LostOdysseyRecomp --branch <branch>
gh workflow run fg-cpu-contracts.yml --repo freefrank/LostOdysseyRecomp --ref <branch>
gh run view <run-id> --repo freefrank/LostOdysseyRecomp --log-failed
```

When a path filter skips a check, dispatch its workflow on the branch explicitly. Repeat for the other three check workflows as needed. A passing build or fixture is not gameplay or device acceptance.

The existing macOS arm64 workflow continues to build libraries and tests without game data on its hosted macOS runner. Discord, issue triage, Claude and Mod Wiki workflows also remain on GitHub.

## Releases

[`.github/workflows/release.yml`](../../.github/workflows/release.yml) starts on a `v*` tag pushed to `origin`, or through `workflow_dispatch` with an optional `release_tag`. Push only reviewed public commits and the intended tag; do not use automatic tag pushing, `--all` or `--mirror`.

```powershell
git push origin refs/tags/vX.Y.Z
```

| Job | Hosted runner | Output |
|---|---|---|
| Create draft release | `ubuntu-24.04` | Draft with the matching `CHANGELOG.md` entry; checks that the tag exists |
| Prepare FSR Vulkan build inputs | `windows-2022` | FSR build-input artifact |
| Windows Release | `windows-2022` | Windows x64 ZIP |
| Linux Release | `ubuntu-22.04` | AppImage and Flatpak from one compiled AppDir |
| Android Release | `ubuntu-22.04` | Android arm64 APK |
| Publish complete release | `ubuntu-24.04` | Verified assets on the GitHub release, then publication |

Windows builds use four compile jobs and no compiler cache: ccache rejects the Visual Studio clang-cl C++23 flag used by this build. Linux uses four jobs for dependency builds, the available CPU count for runtime targets, and a 3 GB ccache. Android uses four jobs and a 2 GB ccache. Flatpak is built on the hosted Ubuntu machine with `sudo`; it does not need a privileged self-hosted runner.

Build jobs upload distinct `actions/upload-artifact@v4` artifacts. The publish job downloads the package artifacts with `actions/download-artifact@v4`, uploads missing assets to the draft, and checks the exact set:

```text
expected = Windows ZIP, Linux AppImage, Linux Flatpak, Android APK
expected <= names <= expected | {LostOdysseyRecomp-macos-arm64-<tag>.dmg}
```

Every release asset must be uploaded and nonempty. The macOS disk image is still built on the maintainer's Mac and uploaded to the draft before publication ([macOS releases](../MACOS_RELEASE.md)). An existing release retains its reviewed notes and assets; reruns validate an already public release without changing its publication state.

Packages contain no shader pack. The Linux job retains the mandatory `tools/release/publish_shader_packs.py --check`: tagged releases require the published `shader-packs` index to cover the runtime's Vulkan and DirectX 12 contracts. Metal and Android share the Vulkan pack. Branch rehearsals only warn if a contract is absent ([shader-pack procedure](../PORTABLE_SHADER_PACK.md#runtime-contract-and-release-check-after-v0725)).

### Build-only rehearsal

An empty `release_tag` builds the selected branch and keeps the Windows ZIP, AppImage, Flatpak and APK as GitHub Actions artifacts. It skips draft creation and publication, and sends no release announcement.

```powershell
gh workflow run release.yml --repo freefrank/LostOdysseyRecomp --ref <branch> -f release_tag=
```

To package an existing tag again, provide it explicitly:

```powershell
gh workflow run release.yml --repo freefrank/LostOdysseyRecomp --ref main -f release_tag=vX.Y.Z
```

## Credentials and repository settings

- `LO_BUILD_INPUT_KEY` is the existing read-only SSH deploy key for `freefrank/LostOdysseyRecomp-build-inputs`. It fetches pinned private inputs, including `android/debug.keystore` and the verified Android DXC binary. Private game files, the signing keystore and generated PPC sources must never enter public commits, caches or artifacts.
- Release writes use the job's `GITHUB_TOKEN` with explicit `contents: write`; the publish job also has `actions: write` to dispatch Discord. No `LO_GITHUB_TOKEN` PAT is required.
- Keep the existing `CLAUDE_CODE_OAUTH_TOKEN`, `DISCORD_BOT_TOKEN` and `ISSUE_TRIAGE_API_KEY` secrets for the workflows that already use them. The migration does not require new secrets.
- Branch protection or rulesets that require `gitea/*` statuses must be updated to require the corresponding native GitHub checks. Retain required reviews and other protection rules.

GitHub does not emit `release: published` for a release published using `GITHUB_TOKEN`. After successful publication, the publish job runs `gh workflow run discord-notify.yml -f release_tag=<tag> -f dry_run=false`. The Discord workflow serializes both entry points by release tag and checks for an existing announcement, preventing a release event and dispatch from posting the same release twice.

## Caches

Hosted jobs restore these inputs through `actions/cache@v4`; the workflow files hold the full key expressions and restore paths:

| Cache | Key pattern / invalidation |
|---|---|
| DLSS SDK | `<OS>-dlss-<pinned commit>-<fetch-helper hash>` |
| Streamline ZIP | `Windows-streamline-v2.14.1-<prepare-helper hash>` |
| XeSS ZIP | `Windows-xess-v3.0.2-<prepare-helper hash>` |
| FSR inputs | `Windows-fsr-<FidelityFX pin>-<XenosRecomp commit>-<FSR-tools/adapter-shaders hash>` |
| Linux ccache (3 GB) | `Linux-ccache-clang18-v1-<build-input hash>-<commit>-<run-id>`; restore by build-input hash, then compiler prefix |
| Android SDK / NDK | `android-sdk-Linux-cmdline11076708-api35-build35-ndk28.2.13676358-cmake3.22.1-v1` |
| Android DXC | `android-dxc-Linux-ndk28.2.13676358-<DXC/CI-script hash>` |
| Gradle | `android-gradle-Linux-<Gradle-config hash>`; restore by OS prefix |
| Android ccache (2 GB) | `android-ccache-Linux-ndk28.2.13676358-<build-script/CMake hash>-<commit>`; restore by build-input hash, then NDK prefix |

Android checks and release builds share these keys. SDK and FSR pins invalidate their inputs when dependencies change; compiler caches reuse unaffected objects across source revisions. Windows compilation, private game inputs and signing keys are not cached. Flatpak installs its runtime fresh on the hosted machine. There is no `LO_GIT_REFERENCE` mirror dependency.

GitHub caches are scoped to the current ref and the default branch; one tag cannot restore another tag's cache. After merging CI changes, an empty-tag rehearsal on `main` can seed caches for future releases ([cache scope](https://docs.github.com/en/actions/reference/workflows-and-actions/dependency-caching#restrictions-for-accessing-a-cache)).

Migration validation and per-job wall times are tracked in [PR #608](https://github.com/freefrank/LostOdysseyRecomp/pull/608). Configuration alone does not establish a successful hosted run or runtime acceptance.
