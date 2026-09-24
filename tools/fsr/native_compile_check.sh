#!/usr/bin/env bash
# Compile-only: no SDK linking, Vulkan device, renderer/game execution or assets.
set -euo pipefail
if [ "$#" -ne 3 ]; then
    echo "usage: $0 SDK_ROOT OUTPUT_DIR GLSLANG_VALIDATOR" >&2
    exit 2
fi
root="$(cd "$(dirname "$0")/../.." && pwd)"
sdk="$(cd "$1" && pwd)"
mkdir -p "$2"
out="$(cd "$2" && pwd)"
glslang="$3"
cd "$root"
python3 tools/fsr/prepare_adapter_shaders.py --glslang "$glslang" --output "$out/shaders"
spirv-val --target-env vulkan1.2 "$out/shaders/fsr_prepare.spv"
spirv-val --target-env vulkan1.2 "$out/shaders/fsr_present.spv"
# Match the production Linux public context overlay; do not edit the SDK.
python3 - "$sdk" "$out" <<'PY'
from pathlib import Path
import sys
sdk, out = map(Path, sys.argv[1:])
p = Path('FidelityFX/host/ffx_fsr3upscaler.h')
text = (sdk / 'sdk/include' / p).read_text(encoding='utf-8')
old = '#define FFX_FSR3UPSCALER_CONTEXT_SIZE (FFX_SDK_DEFAULT_CONTEXT_SIZE)'
if text.count(old) != 1:
    raise RuntimeError('Pinned SDK Linux context anchor changed')
path = out / 'include' / p
path.parent.mkdir(parents=True, exist_ok=True)
path.write_text(text.replace(old, '#define FFX_FSR3UPSCALER_CONTEXT_SIZE (2 * FFX_SDK_DEFAULT_CONTEXT_SIZE)'), encoding='utf-8')
PY
includes=(-I "$out/include" -I "$sdk/sdk/include" -I "$out/shaders"
    -I LostOdysseyRecomp -I thirdparty/plume
    -I thirdparty/plume/contrib/volk
    -I thirdparty/plume/contrib/Vulkan-Headers/include
    -I thirdparty/plume/contrib/VulkanMemoryAllocator/include)
compiler="${CXX:-c++}"
for enabled in 0 1; do
    "$compiler" -std=c++20 -O2 -include bit -DLO_GPU_PLUME=1 -DLO_HAS_FSR="$enabled" \
        "${includes[@]}" -c LostOdysseyRecomp/gpu/fsr_upscaler.cpp -o "$out/fsr-$enabled.o"
done
"$compiler" -std=c++20 -O2 -include bit -DLO_GPU_PLUME=1 -DLO_HAS_FSR=1 \
    "${includes[@]}" -c LostOdysseyRecomp/gpu/temporal_upscaler.cpp -o "$out/temporal.o"
python3 tools/tests/fsr/sdk_memory_selection_regression.py --sdk-root "$sdk" \
    --vulkan-include thirdparty/plume/contrib/Vulkan-Headers/include --cxx "$compiler" \
    --report "$out/sdk-memory-regression.json"
echo "Native source compile and selector checks passed; no SDK link/GPU/game validation."
