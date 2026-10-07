# Pull request checks and releases on Gitea

Since 2026-09-30 the four short Windows/Linux pull request checks below run on
Gitea Actions at `git.zkx.ca`; GitHub queues were too slow for these short
jobs. Windows and Linux release packaging followed on 2026-10-01 (see
[Releases](#releases)). These four checks exist only on Gitea; their GitHub
copies were removed on 2026-10-02. The macOS arm64
workflow is an explicit exception: on `macos-26`, pushes to `main` and pull
requests targeting `main` build libraries and tests without game data; it does
not link the complete game runtime. Full ARM64 runtime and gameplay evidence
is recorded in [development status](../STATUS.md). The Mod Wiki publication
and issue triage stay on GitHub.

| Workflow (`.gitea/workflows/`) | Jobs | Runner |
|---|---|---|
| `fg-cpu-contracts.yml` | GPU-free FG contracts | Linux and Windows |
| `fg-game-integration.yml` | clang-cl compile of the production renderer/video units, FG bridge link and input contract | Windows |
| `reusable-fg.yml` | Shared FG core; DLSS, FSR and combined native adapters | Linux and Windows |
| `review-regressions.yml` (from the 2026-09-30 project review) | Sanitizer regression suite; updater AppImage restart test; AppImage and Flatpak packaging script tests | Linux |
| `android-apk.yml` (pushes to `packaging/android`, `tools/android`, `thirdparty/libadrenotools`, or dispatch) | arm64 debug APK through `tools/android/ci_build.sh` (PPC code generation, Android DXC, NDK build, Gradle assemble + lint, runtime JVM tests); artifact `LostOdysseyRecomp-android-arm64-debug`. SDK/NDK, DXC, Gradle home and ccache persist on `LO_CI_CACHE`; signed with the project's debug keystore from build-inputs | Linux (privileged) |

## Running the checks

Push the branch to the Gitea remote. Each workflow has a path filter and runs
on branch pushes, Gitea pull requests or a manual start. A newer push to the same branch cancels the older run.

```powershell
git push zkx <branch>
```

A push of a new branch whose commits all exist on other branches (for
example a branch that only merges others) can start no run. Start the
workflows manually in that case:

```powershell
tea api --login zkx -X POST -d '{"ref":"<branch>"}' /repos/freefrank/LostOdysseyRecomp/actions/workflows/<workflow>.yml/dispatches
```

Results are on the repository's Actions page at `git.zkx.ca`. `tea` 0.14
cannot list runs on Gitea 1.25 (`tea actions` needs 1.26). Use the API instead,
for example `tea api --login zkx "/repos/freefrank/LostOdysseyRecomp/actions/jobs?limit=20"`
and `.../actions/jobs/<id>/logs`.

Each workflow ends with a `github-status` job that writes the overall result
to the GitHub commit as the status `gitea/<workflow>`, linked to the Gitea run,
so GitHub pull requests show the outcome. It needs the Gitea repository secret
`LO_GITHUB_TOKEN`: a fine-grained GitHub token for the owner's repositories with
"Commit statuses: Read and write" and, for the release workflow, "Contents:
Read and write". Without the secret the job skips. A commit that has not been
pushed to GitHub yet gets no status (HTTP 422 in the job log).

In the agent's non-interactive Git Bash, `tea` started directly hangs, even
for `--version`. The zkx credential helper is `tea login helper`, so a push
from there hangs too. Started from PowerShell or through `cmd //c` it works.
A PowerShell wrapper script around `tea` must not bind `-d` as a script
parameter: PowerShell takes it as `-Debug`, and the request goes out without a
body (Gitea answers "Empty Content-Type").

## Releases

`.gitea/workflows/release.yml` is the port of the GitHub release workflow. A
`v*` tag pushed to `zkx` starts it, and so does a manual start with an
optional `release_tag`. Push the tag to GitHub first: the draft step runs
`gh release create --verify-tag`, which needs the tag there.

```powershell
git push origin refs/tags/vX.Y.Z
git push zkx refs/tags/vX.Y.Z
```

| Job | Runner |
|---|---|
| Create draft release (before the builds, so a missing CHANGELOG section or tag fails early) | `docker-runner` |
| FSR shader inputs, Windows build and ZIP | `win-t640` |
| Linux build, AppImage and Flatpak | `docker-lo-release-privileged` |
| Android arm64 APK (`tools/android/ci_build.sh`, same host code generation as Linux; release build type signed with the project's debug keystore from build-inputs, no release keystore by decision) | `docker-lo-release-privileged` |
| Publish: upload the Gitea artifacts to the draft, verify the set, make it public and latest | `docker-runner` |

The build jobs only produce Gitea artifacts. The publish job runs after both
platforms have built, downloads the three package artifacts and uploads the
ones the release does not have yet, so a failed platform leaves the draft
without a partial package set. Unlike the GitHub workflow, the build jobs do
not upload to the release themselves.

Packages carry no shader pack: the game downloads the one for its renderer
from the `shader-packs` prerelease (PR #144, first shipped in v0.7.35). Between
PR #127 and PR #144 both build jobs fetched a pinned Vulkan pack from the private
build-inputs repository, passed it as `LO_PORTABLE_SHADER_PACK` and ran
`LoShaderPackTool verify-runtime` against the private disc 1 image during the
runtime build (build-only run 485 printed `runtime_compatibility_verified: true`
on both). That fetch is gone. Instead the Linux job builds `LoShaderPackTool` and
its "Check published shader packs" step runs
`tools/release/publish_shader_packs.py --check` against the same image: a tagged
release stops unless the published index lists a pack for each of the runtime's
contracts, and a branch build only warns
([procedure](../PORTABLE_SHADER_PACK.md#runtime-contract-and-release-check-after-v0725)).
That was three contracts (Vulkan, DirectX 12 and Metal) in v0.7.35; since PR #152
(shipped in v0.8.0) Metal and Android read the Vulkan pack, so a runtime has two:
Vulkan and DirectX 12.

The publish job's set check requires the CI packages (the three desktop
packages, plus the Android APK from v0.8.5) and accepts at most
one more asset, `LostOdysseyRecomp-macos-arm64-<tag>.dmg`. CI cannot link the
Mac runtime without game data, so that disk image is built on a Mac and
uploaded to the release by hand (for v0.7.35, to the draft before the publish
job ran; see [macOS releases](../MACOS_RELEASE.md)).

The Gitea job token cannot reach GitHub, so every GitHub operation uses
`LO_GITHUB_TOKEN` with `GH_REPO` pinned to `freefrank/LostOdysseyRecomp`:
creating the draft, uploading the packages, publishing, downloading the pinned
Streamline SDK, and fetching the private `freefrank/LostOdysseyRecomp-build-inputs`
commits (no deploy key on Gitea). A manual start without `release_tag` builds
the selected branch and keeps the packages as Gitea artifacts; it writes
nothing to GitHub. To package an existing tag again, start it with that tag:

```powershell
tea api --login zkx -X POST -d '{"ref":"main","inputs":{"release_tag":"vX.Y.Z"}}' /repos/freefrank/LostOdysseyRecomp/actions/workflows/release.yml/dispatches
```

As on GitHub, an existing release keeps its reviewed notes and assets; only
missing assets are uploaded, and a public release stays as it is.

First release through this workflow, 2026-10-01: v0.7.25 (PR #115 merged as
`e3ad1b9`; tag commit `2c65f0b`) was the first release built and published by
it. Pushing the tag to `zkx` started [run 67](https://git.zkx.ca/freefrank/LostOdysseyRecomp/actions/runs/67),
and all five jobs succeeded: create draft 0.2 min, FSR inputs 1.9 min, Windows
8.8 min (`win-t640`), Linux AppImage and Flatpak 13.6 min (privileged docker
runner) and publish 2.9 min, which downloaded the Gitea artifacts, uploaded
them to the GitHub draft, verified the set and published it. About 19 minutes
passed from the tag push to publication at 2026-10-01T08:04:23Z. It was also
the first real GitHub write (draft creation, upload, publication) through
`LO_GITHUB_TOKEN`; earlier runs were build-only or the read-only v0.7.20
rehearsal (run 55). A separate check of the published Windows ZIP found its
SHA-256 equal to GitHub's digest and a manifest with version `v0.7.25`; see
[STATUS](../STATUS.md#v0725-published--2026-10-01). A green run shows that the
pipeline works, not that the release has been played or accepted.

Second release, 2026-10-02: v0.7.35 (tag commit `95f2c89`) was built and
published by [run 135](https://git.zkx.ca/freefrank/LostOdysseyRecomp/actions/runs/135)
(API id 525), and all five jobs succeeded: create draft 0.2 min, FSR inputs
1.8 min, Windows build and ZIP 7.9 min (`win-t640`), Linux AppImage and Flatpak
10.4 min (privileged docker runner) and publish 1.0 min, which uploaded the
three CI packages to the draft, verified the set and published it with
`--latest`. About 13.5 minutes passed from the tag push to publication at
2026-10-02T09:13:35Z (v0.7.25: about 19). The Linux job's "Check published
shader packs" step printed the d3d12, metal and vulkan contracts and "The
shader-packs index lists packs for all renderers of this runtime."; v0.7.35 is
the first version release held to that check. The macOS disk image was already
on the draft, uploaded by hand (GitHub dates the asset 09:01:25Z), and the set
check accepted it as the one optional asset. The `shader-packs` prerelease did
not change, and no game run was made with the packages; see
[STATUS](../STATUS.md#v0735-published--2026-10-02). A green run shows that the
pipeline works, not that the release has been played or accepted.

Third release, 2026-10-03: v0.8.0 (tag commit `eddbb7d`) was built and published
by [run 184](https://git.zkx.ca/freefrank/LostOdysseyRecomp/actions/runs/184)
(API id 574), and all five jobs succeeded (UTC, read through the Gitea API on
2026-10-03 at about 18:55 UTC): create draft 06:21:21-06:21:31, FSR inputs
06:19:51-06:31:06, Windows build and ZIP 06:31:09-06:38:06, Linux AppImage and Flatpak 06:31:11-06:40:50 (privileged
docker runner) and publish 06:40:51-06:41:52, which published at
2026-10-03T06:41:51Z. About 22 minutes passed from the run start to publication.
The workflow had no Android job then. The macOS disk image was uploaded to the
draft by hand (GitHub dates the asset 06:22:26Z). The Android APK was added to the
published release by hand afterwards (GitHub dates the asset 08:03:03Z). This
record does not include the output of the run's "Check published shader packs"
step. See [STATUS](../STATUS.md#v080-published--2026-10-03).

Fourth release, 2026-10-03: v0.8.5 (tag commit `30a76ac`) was built and published
by [run 212](https://git.zkx.ca/freefrank/LostOdysseyRecomp/actions/runs/212)
(API id 602) and is the first release with an Android job (PRs #170 and #177). All
six jobs succeeded (UTC, same read): create draft 18:26:07-18:26:17, FSR inputs
18:23:40-18:35:00, Android APK 18:26:19-18:32:42 (privileged docker runner),
Windows build and ZIP 18:35:01-18:42:02, Linux AppImage and Flatpak 18:35:03-18:44:18
and publish 18:44:21-18:45:18, which uploaded the four CI packages, verified the
set (the check now requires the APK) and published at 2026-10-03T18:45:17Z. About
22 minutes passed from the run start to publication. The macOS disk image was
uploaded to the draft by hand (GitHub dates the asset 18:26:44Z), before the
publish job ran. The published `shader-packs` index already covered this runtime's
contracts: `publish_shader_packs.py --check` passed on the Mac before the tag, and
the release's Linux job, which makes the same check mandatory for a tag, succeeded
(its step output was not read here). No game run was made with the packages; see
[STATUS](../STATUS.md#v085-published--2026-10-03). A green run shows that the
pipeline works, not that the release has been played or accepted.

Fifth release, 2026-10-03: v0.8.6 (tag commit `b639c39`) was built and published
by [run 214](https://git.zkx.ca/freefrank/LostOdysseyRecomp/actions/runs/214)
(API id 604), started at 20:24:55Z. All six jobs succeeded (UTC, read through the
Gitea API on 2026-10-03 after publication): create draft 20:26:20-20:26:31, FSR
inputs 20:24:55-20:36:12, Android APK 20:26:32-20:32:53, Windows build and ZIP
20:36:16-20:43:18, Linux AppImage and Flatpak 20:36:15-20:45:44 and publish
20:45:46-20:46:54, which published at 2026-10-03T20:46:53Z. About 22 minutes
passed from the run start to publication. The macOS disk image was uploaded to
the draft by hand (GitHub dates the asset 20:27:48Z), before the publish job ran.
The Linux job's "Check published shader packs" step (job log read through the
API) built `LoShaderPackTool` and ran `publish_shader_packs.py --check` at
20:42:30-20:42:34Z; the check passed and the step did not take the warning
branch. It printed the contracts `d3d12 239f8775...` and `vulkan eecb4425...`
(the values already recorded for v0.8.5) and "The shader-packs index lists packs
for all renderers of this runtime". As the tag is set, a failing check would have
stopped the job. No game run was made with the packages; see
[STATUS](../STATUS.md#v086-published--2026-10-03). A green run shows that the
pipeline works, not that the release has been played or accepted.

Sixth release, 2026-10-03: v0.8.7 (tag commit `a45bc74`) was built and published
by [run 227](https://git.zkx.ca/freefrank/LostOdysseyRecomp/actions/runs/227)
(API id 617), started at 23:27:14Z. All six jobs succeeded (UTC, read through the
Gitea API on 2026-10-04): create draft 23:28:45-23:28:55, FSR inputs
23:27:14-23:38:35, Android APK 23:33:29-23:39:50, Windows build and ZIP
23:38:40-23:45:38, Linux AppImage and Flatpak 23:39:51-23:49:17 and publish
23:49:18-23:50:30, which published at 2026-10-03T23:50:29Z. About 23 minutes
passed from the tag to publication. The macOS disk image was uploaded to the
draft by hand (GitHub dates the asset 23:29:05Z to 23:29:25Z), before the
publish job ran. No shader pack was published for the release; the Linux job's
mandatory shader-pack check passed and its output was not read. No game run was
made with the packages; see [STATUS](../STATUS.md#v087-published--2026-10-03). A
green run shows that the pipeline works, not that the release has been played or
accepted.

Seventh release, 2026-10-04: v0.8.10 (tag commit `1dc10ad`) was built and
published by [run 245](https://git.zkx.ca/freefrank/LostOdysseyRecomp/actions/runs/245)
(API id 635), started at 05:17:34Z. All six jobs succeeded (UTC, read through the
Gitea API on 2026-10-04 at about 05:45 UTC): create draft 05:17:34-05:17:45, FSR
inputs 05:17:37-05:28:51, Android APK 05:17:48-05:24:12, Windows build and ZIP
05:28:53-05:35:58, Linux AppImage and Flatpak 05:28:55-05:38:16 and publish
05:38:19-05:39:22, which published at 2026-10-04T05:39:20Z. About 22 minutes
passed from the run start to publication. The macOS disk image was uploaded to
the draft by hand (GitHub dates the asset 05:18:43Z), before the publish job ran.
The translator change in PR #186 changed both pack contracts, so new packs were
published to the `shader-packs` prerelease before the tag (05:14-05:17Z; see the
[pack reference](../PORTABLE_SHADER_PACK.md#update-2026-10-04-packs-for-translator-version-27));
the Linux job's mandatory check passed and its output was not read. No game run
was made with the packages; see [STATUS](../STATUS.md#v0810-published--2026-10-04).
A green run shows that the pipeline works, not that the release has been played
or accepted.

Eighth release, 2026-10-04: v0.8.15 (tag commit `a4162b0`) was built and
published by [run 252](https://git.zkx.ca/freefrank/LostOdysseyRecomp/actions/runs/252)
(API id 642), started at 11:29:36Z. All six jobs succeeded (UTC, read through the
Gitea API on 2026-10-04 at about 22:05 UTC, after v0.8.21 was published): create
draft 11:32:06-11:32:16, FSR inputs 11:29:36-11:40:59, Android APK
11:36:00-11:42:14, Windows build and ZIP 11:41:00-11:47:53, Linux AppImage and
Flatpak 11:42:16-11:51:21 and publish 11:51:23-11:52:17, which published at
2026-10-04T11:52:16Z. About 23 minutes passed from the run start to publication.
The macOS disk image was uploaded to the draft by hand (GitHub dates the asset
11:32:29Z), before the publish job ran. No shader pack was published: the Linux
job's mandatory check ran at 11:48:13-11:48:16Z and passed, and its log, read for
this record, printed the contracts of v0.8.10 (`d3d12 48cf14e3...`, `vulkan
a7d1ab94...`) and that the index lists packs for all renderers of this runtime
(see the [pack reference](../PORTABLE_SHADER_PACK.md#update-2026-10-04-later-no-new-packs-for-v0815-and-v0821)).
This run was recorded after the release, not when it was published. No game run
with the packages is recorded; see
[STATUS](../STATUS.md#v0815-published--2026-10-04). A green run shows that the
pipeline works, not that the release has been played or accepted.

Ninth release, 2026-10-04: v0.8.21 (tag commit `37ffd02`) was built and published
by [run 271](https://git.zkx.ca/freefrank/LostOdysseyRecomp/actions/runs/271)
(API id 661), started at 21:40:40Z. All six jobs succeeded (UTC, read through the
Gitea API on 2026-10-04 at about 22:05-22:10 UTC): create draft 21:43:11-21:43:21,
FSR inputs 21:40:40-21:52:01, Android APK 21:43:23-21:49:43, Windows build and ZIP
21:52:02-21:59:03, Linux AppImage and Flatpak 21:52:05-22:01:20 and publish
22:01:22-22:02:57, which published at 2026-10-04T22:02:56Z. About 22 minutes
passed from the run start to publication. The macOS disk image was built on the
Mac from the tag and uploaded to the draft by hand (GitHub dates the asset
21:43:41Z), before the publish job ran. No shader pack was published:
the Linux job's mandatory check ran at 21:58:07-21:58:10Z and passed, and its log,
read for this record, printed the same contracts as v0.8.10 and v0.8.15; the same
`--check` run on Windows with `LoShaderPackTool` from the tag tree printed them
too. No game run with the packages is recorded; see
[STATUS](../STATUS.md#v0821-published--2026-10-04). A green run shows that the
pipeline works, not that the release has been played or accepted.

Tenth release, 2026-10-05: v0.8.30 (tag commit `f1ebdc05`) was built and published
by [run 295](https://git.zkx.ca/freefrank/LostOdysseyRecomp/actions/runs/295)
(API id 687), started when the tag was pushed at about 20:32Z. All six jobs
succeeded (UTC): create draft 20:32:14-20:32:25, Android APK 20:32:25-20:38:45,
FSR inputs 20:32:13-20:43:29, Windows build and ZIP 20:43:32-20:51:10, Linux
AppImage and Flatpak 20:43:30-20:53:09 and publish 20:53:10-20:54:13, which
published at 2026-10-05T20:54:12Z. About 22 minutes passed from the tag to the
public release. The macOS disk image was built on the Mac from the tag and
uploaded to the draft by hand (GitHub dates the asset 20:33:41Z) while the
Windows and Linux jobs were building. This is the first release whose Windows ZIP
carries the Intel XeSS libraries (PR #236, which downloads the pinned XeSS SDK
3.0.2 in release builds); the ZIP is 150,316,921 bytes against 77,157,364 for
v0.8.21 and its contents were not inspected for this record. No shader pack was
published: `publish_shader_packs.py --check` passed against the existing index
before the tag was pushed (see the
[pack reference](../PORTABLE_SHADER_PACK.md#update-2026-10-05-no-new-packs-for-v0830)).
No game run with the packages is recorded; see
[STATUS](../STATUS.md#v0830-published--2026-10-05). A green run shows that the
pipeline works, not that the release has been played or accepted.

Eleventh release, 2026-10-06: v0.8.37 (tag commit `bfa6c824`) was built and
published by [run 318](https://git.zkx.ca/freefrank/LostOdysseyRecomp/actions/runs/710)
(API id 710; the link uses the id), started when the tag was pushed at about
07:23Z. All six jobs succeeded (UTC): FSR inputs 07:23:58-07:35:17 (11.3 min),
create draft 07:25:35-07:25:45, Android APK 07:29:18-07:35:42 (6.4 min), Windows
build and ZIP 07:35:21-07:42:45 (7.4 min), Linux AppImage and Flatpak
07:35:44-07:45:09 (9.4 min) and publish 07:45:11-07:46:24 (1.2 min), which
published at 2026-10-06T07:46:23Z. About 23 minutes passed from the tag push to
the public release. The macOS disk image was built on the Mac from the tag and
uploaded to the draft by hand (GitHub dates the asset 07:26:12Z) while the CI
builds were running. No shader pack was published: the shader sources equal
those of v0.8.30 and the Linux job's pack check succeeded (see the
[pack reference](../PORTABLE_SHADER_PACK.md#update-2026-10-06-no-new-packs-for-v0837)).
The job timings were read through the Gitea API by the maintainer's session and
are not re-read here. No game run with the packages is recorded; see
[STATUS](../STATUS.md#v0837-published--2026-10-06). A green run shows that the
pipeline works, not that the release has been played or accepted.

Twelfth release, 2026-10-06: v0.8.39 (tag commit `fd7d82ce`) was built and
published by [run 335](https://git.zkx.ca/freefrank/LostOdysseyRecomp/actions/runs/727)
(API id 727; the link uses the id), started when the tag was pushed at about
08:24Z. All six jobs succeeded (UTC): Prepare FSR Vulkan build inputs
08:24:42-08:36:05 (11.4 min), create draft 08:27:18-08:27:28 (0.2 min), Android
Release 08:31:25-08:37:44 (6.3 min), Windows Release 08:36:07-08:43:35 (7.5
min), Linux Release 08:37:45-08:47:24 (9.7 min) and publish 08:47:28-08:48:27
(1.0 min), which published at 2026-10-06T08:48:26Z. About 24 minutes passed from
the tag push to the public release. The macOS disk image was built on the Mac
from the tag and uploaded to the draft by hand at about 08:27:46Z (GitHub dates
the asset 08:27:47Z), right after the draft was created and before the CI
packages (dated 08:47:37Z). No shader pack was published: the translator
contracts equal those of v0.8.30 and the Linux job's pack check succeeded (see the
[pack reference](../PORTABLE_SHADER_PACK.md#update-2026-10-06-later-no-new-packs-for-v0839)).
The job timings were read through the Gitea API by the maintainer's session and
are not re-read here. No game run with the packages is recorded; see
[STATUS](../STATUS.md#v0839-published--2026-10-06). A green run shows that the
pipeline works, not that the release has been played or accepted.

Thirteenth release, 2026-10-06: v0.8.44 (tag commit `504e7cd0`) was built and
published by [run 743](https://git.zkx.ca/freefrank/LostOdysseyRecomp/actions/runs/743),
started when the tag was pushed at about 17:05Z. The run completed at about
17:34Z with status completed and conclusion success, which published the release
at 2026-10-06T17:34:21Z; about 29 minutes passed from the tag push to the public
release. Per-job timings were not read for this record. The macOS disk image
was built on the Mac from the tag and uploaded to the draft by hand (GitHub
dates the asset 17:09:34Z), before the CI packages (dated 17:33:15Z). No shader
pack was published: the translator contracts equal those of v0.8.30 (see the
[pack reference](../PORTABLE_SHADER_PACK.md#update-2026-10-06-later-still-no-new-packs-for-v0844)).
The run status and timestamps were read through the Gitea API by the
maintainer's session and are not re-read here. No game run with the packages is
recorded; see [STATUS](../STATUS.md#v0844-published--2026-10-06). A green run
shows that the pipeline works, not that the release has been played or accepted.

Differences from the GitHub release workflow:

- Flatpak needs bubblewrap, which needs a privileged container, hence the
  dedicated runner. `gh` 2.63.2 is downloaded in each Linux job.
- Windows jobs use T640's Python 3.12 (a venv for the packaging tools) instead
  of `setup-python`, 24 build jobs and 16 FSR shader threads. The Linux build
  uses 20 jobs. The Linux VM runs on T640 too, so the two builds share its
  64 threads; a Linux compiler process peaked below 0.5 GB.
- Compiler cache: both builds run compilers through ccache (4.14.1 in
  `C:\tools\ccache` on T640, the distribution package on Linux) with
  `CCACHE_DIR=$LO_CI_CACHE/ccache` and `CCACHE_BASEDIR` set to the workspace, so
  hits do not depend on the per-job directory. The generated PPC units only
  change with the XEX or the recompiler, and they took about 9 of the 11
  minutes of the first Linux build. The job log ends the build with
  `ccache --show-stats`. Translation units that use the runtime's precompiled
  header fall back to the compiler. On Windows only the C units hit (212 of
  837 compilations): the release build uses the Visual Studio clang-cl 19.1.5
  that `vcvars64` puts first on `PATH`, as the GitHub runners did, and CMake
  passes C++23 to it as `-clang:-std=c++23`, an option ccache rejects. clang-cl
  20+ accepts `/std:c++23preview`, which ccache caches and which produced
  byte-identical objects on clang-cl 22 with `/Brepro`; using it would mean
  building releases with LLVM 22 instead. The Windows build takes about
  4 minutes with 24 jobs and runs beside the longer Linux job.
- Runner caches: both release runners set `LO_CI_CACHE` (`D:\ci-cache` on
  T640; `/ci-cache` on the privileged runner, the `lo-release-cache` docker
  volume). `fetch_dlss_sdk.py` keeps a verified checkout of the pinned SDK in
  `nvidia-dlss-<commit>` there and copies it instead of fetching about
  0.5 GB again; a damaged entry is dropped and fetched anew. The Windows job
  keeps the Streamline archive in `streamline/` once it has passed the size
  and SHA-256 check. Windows submodules use the `LO_GIT_REFERENCE` mirror. The
  privileged runner also keeps `/var/lib/flatpak` in the `lo-release-flatpak`
  volume; the job runs `flatpak update` for the two runtimes so a kept copy
  matches a fresh install. The private game input is not cached. Without
  `LO_CI_CACHE` every step downloads as before.
- On the Windows host runner, a second `actions/checkout` in the same job fails:
  act re-fetches its cached copy of the action, and Windows denies access to
  the replaced pack file. The FidelityFX SDK and the private inputs are
  fetched with plain git; the token reaches git through `GIT_CONFIG_*`
  variables, so it is neither on a command line nor in `.git/config`.

## Runners

| Runner | Labels | Host |
|---|---|---|
| `docker-runner` | `ubuntu-latest`, `ubuntu-24.04`, `ubuntu-22.04` | Docker on the Gitea host |
| `docker-lo-release-privileged` | `ubuntu-22.04-privileged` (this repository only) | Docker on the Gitea host, privileged containers |
| `win-t640` | `windows-2022`, `windows-latest`, `windows` (host mode) | T640, Windows Server 2022, 64 threads |

The Gitea host (`192.168.1.7`) is a Linux VM on T640 with 32 threads and
31 GB of memory, about half of it free for jobs. The privileged runner is a
separate compose project in `/root/app/gitea-runner-lo` (container
`gitea-runner-lo-release`, one job at a time). It is registered to this
repository only, because a privileged job container can control the Docker
host. Only the release workflow uses its label. Its `data/config.yaml` sets
`LO_CI_CACHE: /ci-cache` under `runner.envs`, mounts the `lo-release-cache`
and `lo-release-flatpak` volumes through `container.options`, and lists both
in `container.valid_volumes`.

The Linux image has no CMake or compiler, so the workflows install `cmake`,
`g++` and, where needed, `python3` first. Since the SDL3 migration (#289,
2026-10-07) no workflow installs an SDL package: `review-regressions.yml`
initializes the `thirdparty/SDL` submodule and, when CMake finds no SDL3
package, builds the pinned SDL 3.4.18 statically with the X11 and Wayland
backends off (the fixtures use the dummy video driver). The Linux release job
adds `libxtst-dev` to its X11 development packages, because SDL3's CMake stops
configuring when only some X11 extension headers are installed. Before #289 the
review job installed `libsdl2-dev`.

On T640, `act_runner` runs from `C:\act_runner` as the `gitea-act-runner`
scheduled task (SYSTEM, at boot). The task runs `start-runner.cmd`, which sets
up the job environment and appends to `runner.log`. `config.yaml` sets three
concurrent jobs, `D:\act_runner\work` as the work directory, and the host
labels above. Without explicit labels, the generated default config lists
Docker labels and the runner exits looking for Docker.

T640 toolchain:

- Visual Studio 2022 Build Tools 17.14 (MSVC 14.44, Windows SDK 10.0.26100, ClangCL MSBuild toolset)
- LLVM 22.1.8 in `C:\Program Files\LLVM`, matching the development machine
- CMake 3.31.6 and PowerShell 7.4.6
- Python 3.12.10 in `C:\Program Files\Python312`, unpacked from the official NuGet package because the python.org installer fails on this machine, as `setup-python` did in 2026-09
- Git for Windows, Node.js, and aria2 in `C:\tools\aria2`
- GitHub CLI 2.63.2 in `C:\tools\gh` (on the runner `PATH`), used by the release workflow

`start-runner.cmd` also sets `MSBUILDDISABLENODEREUSE=1`, so idle MSBuild
worker processes do not linger and hold files between jobs.

Until 2026-10-01, T640 had TCP receive window auto-tuning and RSS disabled.
That capped every single TCP connection at 64 KB per round trip, about
1.9 MB/s to GitHub, while multi-connection tests such as fast.com still
showed 400 Mb/s. The first Gitea release run spent 14 minutes fetching the
DLSS SDK. With `netsh int tcp set global autotuninglevel=normal` and
`rss=enabled`, one connection reaches about 12 MB/s. If GitHub downloads on
T640 become slow again, check `netsh int tcp show global` first.

The VS-bundled clang package stalled for half an hour during setup (it
completed later), so `start-runner.cmd` sets `LLVMInstallDir` and
`LLVMToolsVersion=22`, and `-T ClangCL` uses the standalone LLVM 22, the same
version as the development machine. Ninja builds that run `vcvars64` first,
such as the release build, find the Visual Studio clang-cl 19.1.5 (installed
since) ahead of it on `PATH`, like the GitHub Windows runners.
`start-runner.cmd` also puts Git Bash first on `PATH`;
otherwise `shell: bash` resolves to the WSL `bash.exe` in System32.

### Submodule mirror

`D:\ci-cache\git\deps.git` is a bare repository holding the 15 upstreams that
the FG compile job clones: plume and its contrib libraries, XenosRecomp with
dxc-bin and its other dependencies, XenonRecomp with tomlplusplus,
unordered_dense, and SDL. Each upstream's tags live under `refs/rtags/<name>/`
so that equal tag names do not collide. The mirror was 518 MB on 2026-09-30.
The `gitea-git-cache-refresh` scheduled task fetches it every 6 hours
(`C:\act_runner\refresh-git-cache.cmd`, log `C:\act_runner\git-cache.log`).

`start-runner.cmd` exports `LO_GIT_REFERENCE=D:/ci-cache/git/deps.git`.
When it is set, the workflow passes `--reference` to `git submodule update`,
so objects come from the mirror and only newer ones from GitHub. A stale
mirror makes a job slower but does not break it. Runners without the
variable keep `--depth 1`. The mirror is owned by the setup account, so
`safe.directory` lists it in the system git configuration for the SYSTEM runner.
To add an upstream, add a remote with the same two fetch refspecs and
`tagOpt --no-tags`, then fetch.

It also exports `LO_CI_CACHE=D:\ci-cache` for the release caches described
under [Releases](#releases). Entries are named by the pinned version or
commit, so a pin change adds a new entry; delete old ones by hand.

## Differences from the GitHub workflows

- `actions/upload-artifact@v3`: Gitea rejects v4 as an unsupported GHES server.
- The pinned Streamline and FidelityFX headers come from a credential-free
  shallow sparse `git fetch`. `actions/checkout` would send the Gitea job token
  to github.com, which answers 401, and git then fails asking for a username.
  The sparse paths use cone mode, because Git Bash rewrites `/`-prefixed
  arguments into Windows paths. With an explicit `token:` and
  `github-server-url: https://github.com`, `actions/checkout` works for GitHub
  repositories; the Linux release job uses that.

## Timing

First green run, 2026-09-30, measured from job start to end:

| Job | Time |
|---|---|
| FG CPU contracts, Linux / Windows | 26 s / 80 s |
| Reusable FG core, Linux / Windows; native DLSS, FSR, both | 24 s / 26 s; 36–43 s |
| Review regressions (Linux) | 69 s |
| FG game integration compile (Windows) | 366 s; 167 s with the submodule mirror |

Without the mirror, the FG compile job spent about three minutes cloning
submodules from GitHub. With it, submodules and SDK headers took 25 s, CMake
configuration 25 s, and the build and tests 1 min 48 s.
