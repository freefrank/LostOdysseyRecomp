# Menu audit regression tests

These host-only tests protect the menu fixes based on `menu@6e48a9c956205f5550259bf59fa3285b54755dd0`.
They do not require game assets, generated PPC code, SDL, fonts, or a graphics SDK.

## Run

From the repository root, with Python 3, CMake 3.20+, and a C++20 compiler:

```sh
cmake -S tools/tests/menu_audit -B build/menu-audit -DCMAKE_BUILD_TYPE=Release
cmake --build build/menu-audit --config Release
ctest --test-dir build/menu-audit -C Release --output-on-failure
```

Clang/GCC address and undefined-behavior sanitizers (a separate build directory):

```sh
cmake -S tools/tests/menu_audit -B build/menu-audit-asan \
  -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug \
  -DLO_MENU_AUDIT_SANITIZERS=ON
cmake --build build/menu-audit-asan
ctest --test-dir build/menu-audit-asan --output-on-failure
```

## Coverage and limits

`extract_sources.py` extracts the current production function bodies during the
build. Tests do not maintain handwritten copies of those algorithms. The script
also checks that both presentation entry points use the shared preparation and
CPU-frame publication functions, and that guest input does not retire quarantine
bits. This extraction intentionally fails when the source layout changes enough
to require reviewing the fixture; do not silently substitute stale copies.

- Input: actual `ProcessHostInput`, real `ButtonQuarantine`, and 256 close callback
  schedules; a guest reads while the callback has not returned. Also tests hold,
  release, stale samples, complete close/release cycles, and concurrent read-only
  guest filtering. SDL sampling and the complete `hid::GetState` are not executed.
- Coordinates: actual overlay state/navigation/request logic with mocked services
  and the real pause helper. Edit Z from 300 to 900, refill, and assert the complete
  XYZ payload passed to `RequestTeleport` is `(100, 200, 300)`.
- Presentation: actual `PreparePresentation`, real `DisplayChangeTracker`, and
  fake swapchains/DXGI calls. Both platform branches run on either host. Covers
  zero-size recovery, empty handles, fullscreen ordering and failure, captured
  tickets, and protection against old frames completing newer transactions.
- Screenshot state: actual `StoreCpuFrame`, positive 720p/1080p/4K frames, invalid
  buffers, and replacing GPU provenance with CPU pixels and dimensions.

The Windows-model test on Linux is **not** a Windows runtime or ABI test. These
fixtures do not compile the whole runtime, render via real Vulkan/D3D12, or prove
real driver synchronization, game execution, or save safety. The test suite is a
standalone CMake project; it is not automatically run by the runtime build or CI.

## Hardware handoff

Build the repair branch using the repository's normal runtime instructions and
use copies of saves/configuration. First test 720p windowed startup on Windows
D3D12, Windows Vulkan, and Linux Vulkan. Open the overlay, stay paused for at least
30 seconds, navigate, close with B and LB+RB while holding them, release, and
confirm the next fresh press works without dismissing the underlying settings.

While paused, test two successive Alt+Enter changes, window resize,
minimize/restore, and D3D12 exclusive entry/exit. Confirm actual mode/dimensions,
not just an Applied status. Then test edited-Z refill, teleport, and CPU/presented
screenshots after changing output resolution. Keep driver validation enabled in
a diagnostic build when available.
