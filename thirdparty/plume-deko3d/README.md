# plume deko3d back end

plume's render interface on deko3d, the Switch's own GPU API, used by the Switch build when `env.txt` has
`LO_SWITCH_BACKEND=deko3d` (docs/SWITCH.md).

- `plume_deko3d.h`, `plume_deko3d.cpp`, `plume_deko3d_commands.cpp`, `plume_uam_localize.cmake`: from
  [ChanseyIsTheBest/sa2-nx](https://github.com/ChanseyIsTheBest/sa2-nx) `app/thirdparty/plume` at `adf3a4e`
  (2026-10-20; banjo-nx's back end), MIT like plume. Changes for LostOdysseyRecomp are marked
  `LostOdysseyRecomp (2026-10-27)`: SPIR-V shaders translated per pipeline layout, descriptor heaps shared by all
  descriptor sets, texture handles from the bound sets, push constant address windows, and the formats and plume
  interface of this project's plume.
- `plume_deko3d_spirv.{h,cpp}`, `plume_deko3d_api.h`: new here (SPIR-V to uam GLSL; the application's view).
- uam, deko3d's shader compiler, is `thirdparty/uam` (same origin; zlib, Mesa's files MIT, SwitchWakerHD's patches
  MPL-2.0; see its README and PATCHES.md).
