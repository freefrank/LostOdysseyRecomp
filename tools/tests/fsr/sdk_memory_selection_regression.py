"""Compile the pinned SDK memory selector before/after the real CMake patch.

Uses a synthetic physical-memory table, not a Vulkan device/allocation. The
baseline includes the project's existing disabled-device-coherent exclusion.
No SDK checkout is modified and no GPU or game is started.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

BEGIN = "uint32_t findMemoryTypeIndex(VkPhysicalDevice physicalDevice,"
END = "VkBufferUsageFlags ffxGetVKBufferUsageFlagsFromResourceUsage"
PREAMBLE = r'''
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <vulkan/vulkan.h>
#include "vulkan_memory_policy.h"
#define FFX_ASSERT assert
static VkPhysicalDeviceMemoryProperties table{};
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceMemoryProperties(
    VkPhysicalDevice, VkPhysicalDeviceMemoryProperties* output) { *output = table; }
'''
TEST = r'''
int main() {
    const auto physical = reinterpret_cast<VkPhysicalDevice>(uintptr_t(1));
    VkMemoryRequirements requirements{};
    requirements.memoryTypeBits = 1;
    VkMemoryPropertyFlags properties = 0;
    table.memoryTypeCount = 1;
    table.memoryTypes[0].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    unsigned failures = 0;
    if (findMemoryTypeIndex(physical, requirements,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        properties) != UINT32_MAX) {
        std::puts("FAIL: compound request selected non-host-visible local memory"); ++failures;
    }
    table.memoryTypes[0].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    if (findMemoryTypeIndex(physical, requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        properties) != 0 || properties != table.memoryTypes[0].propertyFlags) {
        std::puts("FAIL: UMA's only eligible local memory type was rejected"); ++failures;
    }
    std::printf("SDK selector regression: %u/2 failures (synthetic memory table)\n", failures);
    return int(failures);
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk-root", type=Path, required=True)
    parser.add_argument("--vulkan-include", type=Path, required=True)
    parser.add_argument("--cxx", default="c++")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    sdk_file = args.sdk_root.resolve() / "sdk/src/backends/vk/ffx_vk.cpp"
    source = sdk_file.read_text(encoding="utf-8")
    with tempfile.TemporaryDirectory(prefix="fsr-memory-") as temporary:
        work = Path(temporary)
        patched_path = work / "patched.cpp"
        driver = work / "patch.cmake"
        driver.write_text(
            f'include("{(root / "cmake/LoFsrVulkanMemory.cmake").as_posix()}")\n'
            f'file(READ "{sdk_file.as_posix()}" source)\n'
            'lo_fsr_patch_memory_selection("${source}" patched)\n'
            f'file(WRITE "{patched_path.as_posix()}" "${{patched}}")\n', encoding="utf-8")
        subprocess.run(["cmake", "-P", str(driver)], check=True)
        patched = patched_path.read_text(encoding="utf-8")
        anchor = "if ((memRequirements.memoryTypeBits & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & requestedProperties)) {"
        if source.count(anchor) != 1:
            raise RuntimeError("Pinned SDK baseline anchor missing or ambiguous")
        baseline = source.replace(anchor,
            "if (memProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD) continue;\n        " + anchor)
        results = {}
        for name, text, expected in [("baseline", baseline, 2), ("patched", patched, 0)]:
            start = text.index(BEGIN)
            stop = text.index(END, start)
            unit = work / f"{name}.cpp"
            unit.write_text(PREAMBLE + text[start:stop] + TEST, encoding="utf-8")
            executable = work / name
            subprocess.run([args.cxx, "-std=c++20", "-O2", "-I", str(args.vulkan_include.resolve()),
                "-I", str(root / "tools/fsr"), str(unit), "-o", str(executable)], check=True)
            result = subprocess.run([str(executable)], capture_output=True, text=True)
            print(f"{name}:\n{result.stdout}", end="")
            if result.returncode != expected:
                raise RuntimeError(f"{name}: expected {expected} regressions, got {result.returncode}\n{result.stderr}")
            results[name] = {"failures": result.returncode, "stdout": result.stdout}
        report = {"scope": "pinned SDK function, real CMake transform, synthetic memory properties; no GPU",
                  "sdk_source_sha256": hashlib.sha256(sdk_file.read_bytes()).hexdigest(), "results": results}
        if args.report:
            args.report.parent.mkdir(parents=True, exist_ok=True)
            args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
