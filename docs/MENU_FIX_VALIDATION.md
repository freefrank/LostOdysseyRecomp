# Menu boundary fixes (base 6e48a9c9)

## Changes

- Serialize host input sampling/actions and guest input reads with the existing
  logical-input mutex. Publish B/chord quarantine before the close callback can
  resume guest execution; only the authoritative sampler advances release state.
  The HID device lock is released before calling menu services.
- Restore the omitted Z assignment in Fill Coordinates.
- Share display preparation between guest and host-only presentation. Apply DXGI
  mode changes, honor resize requests, recover empty swap chains, and obtain final
  output dimensions before rasterization. Keep a zero-extent transaction pending
  until restoration; do not acknowledge a newer ticket after rasterization.
- Publish CPU menu pixels, dimensions and screenshot provenance together.
  Validate pixel extent and row-pitch arithmetic before upload.

## Reproducible focused tests

From the repository root, with Python 3 and a C++20 compiler:

```sh
python tools/tests/menu_boundary_regression.py --cxx clang++ --sanitize
python tools/tests/menu_boundary_regression.py --cxx g++
```

Use `--out /path/to/results` to retain generated translation units and build/run
logs. The runner needs neither game data nor SDL/plume libraries. It compiles
current production source verbatim with explicitly simulated external services:
all of `hid.cpp` except includes, the overlay controller before rasterization,
and the shared display preparation/host presentation/CPU frame-cache routines.
It includes the real pause and display-transaction headers.

The four groups cover gated concurrent B/chord closes and release ownership,
HID device-lock freedom, XYZ submission after editing Z and refilling, empty and
zero-extent recovery, resize failure, equal-size transactions, stale/new ticket
separation, DXGI enter/exit/failure, and CPU screenshot provenance. The DXGI group
compiles the Windows conditional branch against fake DXGI types on Linux; it is
not a Windows or graphics-driver test.

Local validation on Linux: Clang 17 ASan/UBSan and GCC 14.2 optimized builds passed
all four groups. Five negative controls (original input and Z-fill code, premature
empty-chain return, stale GPU provenance, and late ticket lookup) failed the
corresponding assertions as expected.

## Still required before declaring runtime compatibility

A full runtime build, Windows execution, real SDL input, Vulkan/D3D12 validation,
and game scenes were not run in the repair environment. Use disposable copies of
saves/settings. Test Windows D3D12, Windows Vulkan, and native Linux Vulkan:

1. Open/close with keyboard and controller, hold B and LB+RB after close, release
   and press again. Keep the menu open for 30 seconds and confirm responsiveness.
2. Repeat Alt+Enter while paused; minimize/restore and resize. On D3D12 test
   exclusive entry/exit as well as borderless mode. Check actual window mode, not
   only a success status.
3. Edit Z, refill current coordinates, and verify all XYZ. Capture menus after
   switching from GPU scene presentation; test 720p and larger output dimensions.

These focused tests are regression checks, not a substitute for those runs.
