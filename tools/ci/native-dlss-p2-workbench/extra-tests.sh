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
test -f "$LO_DXC_PATH"
cmake --build source/out/renderer-check --target LoSceneCopyCompositeTest --parallel 2
ctest --test-dir source/out/renderer-check -R '^LoSceneCopyCompositeTest$' --output-on-failure -V | tee source/out/composite-console.log
! grep -E 'Validation Error|VUID-' source/out/composite-console.log
