#!/usr/bin/env bash
set -euo pipefail
# Reviewed fixture-only correction: Vulkan uses separate t0 and t1 bindings,
# matching renderer.cpp; a two-element array at t0 is a different ABI.
python3 - <<'PY'
from pathlib import Path
import subprocess
p=Path('source/tools/tests/native_dlss_composite_gpu_test.cpp')
s=p.read_text()
old='builders_[1].begin(); builders_[1].addTexture(0, 2); builders_[1].end();'
new='builders_[1].begin(); builders_[1].addTexture(0); builders_[1].addTexture(1); builders_[1].end();'
assert s.count(old)==1
p.write_text(s.replace(old,new))
subprocess.run(['git','add','tools/tests/native_dlss_composite_gpu_test.cpp'],cwd='source',check=True)
subprocess.run(['git','diff','--cached','--check'],cwd='source',check=True)
PY
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
