# Guest controller atlas offline checks

This fixture reads the local (private) `rpmenurescommon_int.xxx` and
`rpfontscommon_int.xxx` entries from `disc1/xenon_loc.fpd` using
`out/issue40/ui-export/inventory.json`. It uses
the existing UI exporter package/LZO parser to reproduce the original tiled
BC3 data and then supplies endian-correct, pitched BC3 upload rows to the
production `gpu/controller_atlas.h` decoder. The complete RGBA SHA256 must
match the known Icon_Page_0 or Font Icon Texture2D_1 fingerprint. It also
checks real source silhouettes and the patch of every face, shoulder and
Select/Start cell. Negatives include altered and zeroed pixels, wrong fetch
format, dimension, mip and upload extent.

The second fixture compiles the *verbatim* production renderer
`SelectControllerAtlas` and `InvalidateRange` methods against a mock device,
command list, upload ring and fence. It checks Xbox→PS→Xbox→PS with stable
texture pointers and one upload. It calls the designer's actual
`PatchPlayStationAtlasRgba` API in the extracted production method and checks
that the uploaded bytes match the actual patch: face/shoulder alpha and pixels
outside the patched cells stay intact, while Select/Start alpha is rebuilt.
It also checks
rollover using the new command list/ring,
failed allocation/patch/upload fallback without retry, GPU-stop upload failure
returning null through `PlanSuppressed` without recording a draw, and source/child
retention through a failed fence wait. Generation also asserts the production
GetTexture cache/recognition sites and fence-release ordering. The extracted
post-draw trace checks that only still-bound candidates are reported, including
slots beyond zero, with a 64-record per-frame cap. The generator verifies that
production calls it after both draw command variants. The renderer TU must compile in the
normal project build. No game launch or vendor GPU is needed.

From repository root with the Windows toolchain and local game data:

```bat
call tools\setup_windows.bat
cmake -S tools\tests\controller_atlas -B out\issue40\controller-atlas-fixture -G Ninja -DCMAKE_CXX_COMPILER=clang-cl
cmake --build out\issue40\controller-atlas-fixture --target LoControllerAtlasFixture LoControllerAtlasRendererFixture
out\issue40\controller-atlas-fixture\LoControllerAtlasRendererFixture.exe
python -B tools\tests\controller_atlas\run_real_atlas.py
```

Set `LO_CONTROLLER_ATLAS_TRACE=1` on a future manual game launch to log only
fingerprint-verified controller atlas bindings after the draw command is recorded,
including frame, F1 `debugDraw`, shader hashes, stage, bank, slot, family,
guest address and host pointer (up to 64 records per frame). This
offline fixture does **not** establish which HUD scenes use that atlas.
