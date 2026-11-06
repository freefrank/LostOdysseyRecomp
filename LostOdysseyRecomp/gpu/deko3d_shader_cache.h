// LostOdysseyRecomp (2026-10-27): the DKSH caches of the Switch's deko3d back end (plume_deko3d's
// SetDeko3DShaderCache), keyed by the FNV-1a 64 of a variant's GLSL text.
//   shaders/deko3d_shaders.lodk   built on the PC from the Vulkan shader pack (tools/deko3d/): read on demand
//   cache/deko3d_runtime.dkcache  what the console compiled itself (uam), appended and synced as it goes
#pragma once

#include <filesystem>

namespace lo::deko3d {
    // Before the device makes its first pipeline. Logs what it found.
    void InstallShaderCache(const std::filesystem::path& packPath, const std::filesystem::path& runtimePath);
}
