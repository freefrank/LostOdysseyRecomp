#pragma once
// Which distribution pack a renderer reads and where the runtime looks for it.
// Shared by the renderer and the startup download, so both agree on the file,
// the contract and the settings that turn distribution packs off.
#include "portable_shader_contract.h"
#include <os/user_paths.h>
#include <cstdlib>
#include <filesystem>
#include <span>
#include <string_view>
#include <vector>

namespace xenos::portable_pack {
// Vulkan (Windows, Linux, Android) and Metal (macOS) read one SPIR-V pack.
enum class Flavor { Vulkan, D3D12 };

inline std::string_view FlavorName(Flavor flavor) {
    switch (flavor) {
    case Flavor::Vulkan: return "vulkan";
    case Flavor::D3D12: return "d3d12";
    }
    return {};
}
inline std::string_view FlavorLabel(Flavor flavor) {
    switch (flavor) {
    case Flavor::Vulkan: return "Vulkan";
    case Flavor::D3D12: return "DirectX 12";
    }
    return {};
}
inline PackFormat FormatOf(Flavor flavor) {
    return flavor == Flavor::D3D12 ? PackFormat::Dxil : PackFormat::Spirv;
}
inline std::string_view FileNameOf(Flavor flavor) {
    switch (flavor) {
    case Flavor::Vulkan: return FileName;
    case Flavor::D3D12: return Dx12FileName;
    }
    return {};
}
// The contract ignores the compiler identity, so none is needed here.
inline cache::Identity IdentityOf(Flavor flavor) {
    return cache::MakeIdentity(flavor == Flavor::D3D12 ? cache::Backend::D3D12 : cache::Backend::Vulkan, "");
}
inline Digest FlavorContract(std::span<const uint8_t> unboundXex, Flavor flavor) {
    return RuntimeContract(unboundXex, IdentityOf(flavor), FormatOf(flavor));
}

// Developer settings under which the renderer ignores distribution packs.
inline bool DistributionPacksDisabled() {
    const char* exportPath = std::getenv("LO_SHADER_EXPORT_PACK");
    return (exportPath && *exportPath) || std::getenv("LO_NO_PORTABLE_SHADER_PACK") ||
        std::getenv("LO_SHADER_FULL_SCAN") || std::getenv("LO_SHADER_HLSL_DIR") ||
        std::getenv("LO_SHADER_RETRY_FAILURES");
}
inline const char* ConfiguredPackPath() {
    const char* configured = std::getenv("LO_SHADER_PACK_PATH");
    return configured && *configured ? configured : nullptr;
}

// Downloaded packs go beside the executable in a portable layout, else to the
// user data folder: never into a signed app bundle or a read-only AppImage.
inline std::filesystem::path InstallDirectory() {
    return os::user_paths::UsePortableLayout() && !os::user_paths::g_executableDirectory.empty()
        ? os::user_paths::PortableRoot(os::user_paths::g_executableDirectory) / "shaders"
        : os::user_paths::DataDir() / "shaders";
}

// LO_SHADER_PACK_PATH alone when set; otherwise the install location, then a
// pack staged beside the executable by a development build.
inline std::vector<std::filesystem::path> CandidatePaths(Flavor flavor) {
    if (const char* configured = ConfiguredPackPath()) return {std::filesystem::path(configured)};
    std::vector<std::filesystem::path> paths{InstallDirectory() / FileNameOf(flavor)};
    try {
        const auto staged = DefaultPath(FormatOf(flavor)).parent_path() / FileNameOf(flavor);
        if (staged.lexically_normal() != paths.front().lexically_normal()) paths.push_back(staged);
    } catch (const std::exception&) {}
    return paths;
}
}
