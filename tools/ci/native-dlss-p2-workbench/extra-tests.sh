#!/usr/bin/env bash
set -euo pipefail
sudo apt-get install -y --no-install-recommends mesa-vulkan-drivers vulkan-validationlayers
mkdir -p "$GITHUB_WORKSPACE/dxc"
curl --fail --location --retry 3 'https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.8.2505.1/linux_dxc_2025_07_14.x86_64.tar.gz' -o "$GITHUB_WORKSPACE/dxc/archive.tar.gz"
printf '%s  %s\n' 'f2213da1fc99dc8778c8823078e16ba97c7f80f86a1d4520ab1adf4b462bc48c' "$GITHUB_WORKSPACE/dxc/archive.tar.gz" | sha256sum --check --strict
tar -xzf "$GITHUB_WORKSPACE/dxc/archive.tar.gz" -C "$GITHUB_WORKSPACE/dxc"
export LO_DXC_PATH="$GITHUB_WORKSPACE/dxc/lib/libdxcompiler.so"
export LD_LIBRARY_PATH="$GITHUB_WORKSPACE/dxc/lib:${LD_LIBRARY_PATH:-}"
export VK_ICD_FILENAMES
VK_ICD_FILENAMES=$(find /usr/share/vulkan/icd.d -name '*lvp*.json' -print -quit)
test -n "$VK_ICD_FILENAMES"
cmake --build source/out/renderer-check --target LoNativeDlssRendererTest LoSceneCopyCompositeTest --parallel 2
ctest --test-dir source/out/renderer-check -R '^(LoNativeDlssRendererTest|LoNativeDlssRendererNativeTest|LoSceneCopyCompositeTest)$' --output-on-failure -V | tee source/out/composite-console.log
! grep -E 'Validation Error|VUID-' source/out/composite-console.log
cmake -S source/tools/tests/motion_replay -B source/out/renderer-ngx -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_CXX_SCAN_FOR_MODULES=OFF -DLO_ENABLE_DLSS=ON -DLO_DLSS_SDK_ROOT="$GITHUB_WORKSPACE/sdk" -DLO_DLSS_STAGE_RUNTIME=OFF
grep -q 'LO_DLSS_SDK=1' source/out/renderer-ngx/build.ninja
cmake --build source/out/renderer-ngx --target LoNativeDlssRendererTest --parallel 2
ctest --test-dir source/out/renderer-ngx -R '^LoNativeDlssRendererTest$' -V --output-on-failure | tee source/out/native-renderer-build-check.log
! grep -E 'Validation Error|VUID-' source/out/native-renderer-build-check.log
