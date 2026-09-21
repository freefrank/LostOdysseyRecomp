#!/usr/bin/env bash
set -euo pipefail
cmake -S source/tools/tests/motion_replay -B source/out/graphics -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_CXX_SCAN_FOR_MODULES=OFF -DLO_P2_VIDEO_COMPILE_CHECK=ON -DLO_P2_RENDERER_MAPPING_TEST=ON
cmake --build source/out/graphics --target LoDlssRendererMappingTest --parallel 2
ctest --test-dir source/out/graphics -R '^LoDlssRendererMappingTest$' --output-on-failure -V 2>&1 | tee mapping.log
! grep -E 'Validation Error|VUID-' mapping.log
