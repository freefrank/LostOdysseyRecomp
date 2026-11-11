# deko3d in LostOdysseyRecomp

deko3d 0.5.0, https://github.com/devkitPro/deko3d at `5cc144d` (tag v0.5.0; LICENSE: zlib). That is the
version of devkitPro's `deko3d 0.5.0-1` package in the switch-dev image: its `deko3d.h` is byte-identical to the
tag's, and its `libdeko3d.a` was built with devkitA64 r24 (GCC 13.2). `dk_cmdbuf.cpp` is the same on master
(`373bfd4`, 8 commits later). Only `include/`, `source/`, `LICENSE` and `README.md` are kept.

`CMakeLists.txt` builds it as `lo_deko3d` with upstream Makefile's release flags (`libdeko3d.a`: `-O2` with its
extra optimisations, `-DNDEBUG`, no RTTI or exceptions, `gnu++17`). `thirdparty/CMakeLists.txt` links it into
plume's deko3d back end; `-DLO_SWITCH_DEKO3D_SOURCE=OFF` links devkitPro's `libdeko3d.a` instead.

`generated/` holds what upstream's Makefile writes with devkitPro's dekotools
(https://github.com/devkitPro/dekotools, `c15523c`), which the switch-dev image does not ship:

    for d in source/maxwell/*.def; do n=$(basename $d .def)
        if [[ $n == *_3d ]]; then dekodef -h generated/$n.h -m generated/$n.mme $d
        else dekodef -h generated/$n.h $d; fi; done
    dekomme -o generated/mme_macros.h generated/engine_3d.mme $(ls source/maxwell/*.mme | sort)

Rerun it after editing a `.def` or `.mme` file (dekotools builds with meson, ninja, bison, flex and g++, e.g. in
a `debian:bookworm-slim` container). Generated on 2026-11-11 from the tag: the same bytes as SwitchWakerHD's
`runtime/third_party/deko3d/generated` (github.com/centollOS/SwitchWakerHD).

Every change to upstream files is marked `LostOdysseyRecomp patch N`.
