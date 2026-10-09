# WIP execution handoff — 2026-10-09

Transfer-only snapshot. All workers are stopped; do not restart subagents.
Parent and this executor do not share `/workspace` or `/tmp`. Fetch this WIP
commit into the parent clone before editing these files. No recovery work or
new behavioral test was performed while packaging this handoff.

## Preserved unfinished work

- `owned_tree_mesh_build61`: header/source/oracle/draft/batch and CMake entry.
  Library builds. Existing 3-case run fails the independent final map assertion:
  actual map is `0,16`, preserved index pointer is zero, live allocation count 5.
  The fixture still expects `0,1`; it is deliberately NOT fixed here. Full state,
  RAM and callback comparison reaches that assertion without a mismatch.
  BD2168 packs a byte-offset range, so the expected second word needs a direct
  source check before changing it. Upper comparison uses shared accepted lowers
  and explicit borrowed bounds/split/strategy callbacks. It does not validate all
  concrete vtable targets; see its draft. Failure cleanup remains asymmetric.
- `tree_triangle_split61`: same five files and CMake entry. Library builds and
  3 complete-original-body cases pass. Draft validation remains pending; no
  promotion or mapping credit was added in this transfer commit.
- Both result JSON files here are unchanged local receipts, not full recovery
  acceptance. The BD5910 host invalid-flag mismatch remains unresolved.

## Actual private-input acquisition and provenance

The acquisition used `functions.exec` -> `tools.exec_command` -> shell `git`,
not a connector binary-download API. Git metadata confirms a shallow clone,
`blob:none` filter and non-cone sparse checkout of `default.xex`,
`ppc/manifest.json`, and `README.md` from:
`https://github.com/freefrank/LostOdysseyRecomp-build-inputs.git`.
The private source commit is `225eb69180cd8607ace3dfc2448ba9b038140408`.
The clone lives in ignored `out/private-inputs`. The successful clone used the
executor's existing Git access; no credential value or authentication workaround
is included or recoverable from this handoff. Do not assume that access transfers
to another executor. The exact original command line is not retained; an
equivalent sparse checkout requires authorized Git access to that repository.

The parent reports its GitHub connector can read the repository. Relevant tools
available in the delegated session are `mcp__codex_apps__github_fetch_file` and
`mcp__codex_apps__github_fetch_blob`. They were NOT the acquisition method used
here. Use the connector's documented binary/blob export result at the pinned
commit; do not confuse the connector's authorization with shell Git credentials,
and do not assume a text-rendered or truncated response is a complete XEX.
Only the XEX is needed for regeneration; the cached PPC is not the new baseline.

Verified XEX: 6,623,232 bytes, SHA256
`40c7dbb12cca03921d52cf4177a0f700cc4ae94ab730ab594940e2bdffd8ecf2`.
It was copied to ignored `LostOdysseyRecompLib/private/disc1/default.xex`.
The generated source used the repository config and `tools/ppc_codegen.py`,
XenonRecomp gitlink `ddd128bcca99fe8bfbb99bea583c972351fa6ace`, and tracked
`tools/patches/XenonRecomp-lostodyssey.patch`. Apply that patch only once to a
clean submodule, following `tools/patches/README.md`. Cached config/generator/FP
headers differed materially, so cached generated PPC was not reused.
Current generated files are ignored `LostOdysseyRecompLib/ppc`.
Per-entry LF-normalized full-body SHA pins in drafts are the comparison identity;
source line positions may change if a different generator/config is used.
Private XEX/image/generated bodies must stay outside commits and public logs.
Historical baseline `catalog.sqlite` remains unavailable: mapping stays 5517/62627.

## Compiler and generation prerequisites

Used Linux x64, Python 3.11+, Clang 19.1.7, CMake 4.4.4 and Ninja 1.13.2.
The Clang debs were extracted under `/tmp/semantic-clang`; libLLVM 19 is also
required. CMake/Ninja Python packages live under `/tmp/semantic-build-tools`.
The old paths are conveniences, not files shared with the parent. Install the
same tools locally or adjust paths in this snapshot. The extracted layout needs:

```sh
export LD_LIBRARY_PATH=/tmp/semantic-clang/usr/lib/llvm-19/lib
export PYTHONPATH=/tmp/semantic-build-tools
```

The generator and xexdump were built through `tools/xexdump/CMakeLists.txt`
(which includes XenonRecomp) using Release, Ninja and Clang 19. The executable
in this executor is:
`out/build/semantic-tools-clang/XenonRecomp/XenonRecomp/XenonRecomp`.
Regeneration command, from the repository root, after restoring the verified XEX:

```sh
python tools/ppc_codegen.py generate --executable out/build/semantic-tools-clang/XenonRecomp/XenonRecomp/XenonRecomp
```

The standalone library configuration was:

```sh
/tmp/semantic-build-tools/cmake/data/bin/cmake -S LostOdysseyRecompSemantics -B /tmp/semantic-library-clang -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=/tmp/semantic-clang/usr/bin/clang++-19 -DCMAKE_MAKE_PROGRAM=/tmp/semantic-build-tools/bin/ninja -DCMAKE_CXX_SCAN_FOR_MODULES=OFF -DCMAKE_CXX_FLAGS=-Wno-error=unused-function
/tmp/semantic-build-tools/cmake/data/bin/cmake --build /tmp/semantic-library-clang --parallel 1
```

The project enables `-Wall -Wextra -Werror`; the only exception above is for
pre-existing unused formatter helpers. GCC was not substituted because existing
code uses Clang-only rotate builtins. No runtime/game build is required here.

## Temporary Linux oracle snapshot

`run_semantic_family.py` is the exact previously used temporary runner, retained
for transfer rather than installed as a general test framework. It contains no
credentials or original bodies. It composes pinned originals locally into ignored
`out/private-inputs`, compiles one family against the static library, and writes
local result files under `/tmp`. Do not publish its generated source or compiler
logs (they may contain private original excerpts). It uses Clang `-O2 -msse4.1`.
The static library itself was built in Release/O3.

The only changed fixture was `semantic_oracle_support.h`: Windows allocation
became mmap/mprotect. The attached patch carries only that public fixture change.
Recreate the temporary include overlay from tracked public files:

```sh
mkdir -p /tmp/semantic-linux-oracle
for f in semantic_oracle_support.h crt_stream_oracle_fixture.h crt_full_context_oracle_fixture.h crt_close_recursive_buffer_context_oracle.cpp object_sort_engine61_oracle_fixture.h grid_blob_routes61_oracle_fixture.h; do
  cp "LostOdysseyRecompSemantics/tests/$f" /tmp/semantic-linux-oracle/
done
patch -d /tmp/semantic-linux-oracle -p0 < LostOdysseyRecompSemantics/handoff/linux_oracle_20261009/linux_guest_window.patch
cp LostOdysseyRecompSemantics/handoff/linux_oracle_20261009/run_semantic_family.py /tmp/run_semantic_family.py
```

After independently resolving the preserved assertion, the direct continuation
command is `python /tmp/run_semantic_family.py owned_tree_mesh_build61`.
Do not rerun old suites or mark unresolved concrete callbacks as recovered.
