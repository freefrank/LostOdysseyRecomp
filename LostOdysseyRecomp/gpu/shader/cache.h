#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "../backend_selection.h"
#include "resource_cpx_index_sha256.h"

namespace xenos::cache {
// Shared by the runtime and offline compiler. Bump for translation changes.
inline constexpr unsigned Version = 21;
using Backend = gpu::backend::Backend;
enum class Format { Dxil, Spirv, Dxbc };
struct Identity {
    Backend backend = Backend::D3D12;
    Format format = Format::Dxil;
    unsigned translatorVersion = Version;
    std::string compiler;
    std::string options;
    std::string variant = "guest-microcode";
};
// This describes the current CompileHlsl argument contract, including the
// vertex-only SPIR-V Y inversion. Update it whenever those arguments change.
inline std::string DefaultOptions(Backend backend) {
    switch (backend) {
    case Backend::D3D12: return "main;vs/ps_6_0;HV2021;no-parentheses-equality;no-unused-value;all-resources-bound;O3;strip-debug;strip-reflect";
    case Backend::Vulkan: return "main;vs/ps_6_0;HV2021;no-parentheses-equality;no-unused-value;all-resources-bound;O3;strip-debug;spirv;vulkan1.2;dx-layout;vs-invert-y";
    case Backend::D3D11: return "reserved-dxbc-sm5-no-compiler";
    }
    return {};
}
inline Identity MakeIdentity(Backend backend, std::string_view compiler) {
    Identity result;
    result.backend = backend;
    result.format = backend == Backend::Vulkan ? Format::Spirv :
        backend == Backend::D3D11 ? Format::Dxbc : Format::Dxil;
    result.compiler = compiler;
    result.options = DefaultOptions(backend);
    return result;
}
inline bool ValidIdentity(const Identity& identity) {
    const bool pair = (identity.backend == Backend::D3D12 && identity.format == Format::Dxil) ||
        (identity.backend == Backend::Vulkan && identity.format == Format::Spirv) ||
        (identity.backend == Backend::D3D11 && identity.format == Format::Dxbc);
    return pair && identity.translatorVersion && !identity.compiler.empty() &&
        !identity.options.empty() && !identity.variant.empty();
}
inline bool RuntimeSupported(const Identity& identity) {
    return ValidIdentity(identity) && identity.backend != Backend::D3D11;
}
inline std::span<const uint8_t> IdentityBytes(std::string_view value) {
    return {reinterpret_cast<const uint8_t*>(value.data()), value.size()};
}
inline std::string IdentityKey(const Identity& identity) {
    std::string key = "lo-shader-identity-1:" + std::to_string(static_cast<unsigned>(identity.backend)) +
        ":" + std::to_string(static_cast<unsigned>(identity.format)) + ":" + std::to_string(identity.translatorVersion);
    // Length prefixes prevent values containing separators from aliasing.
    for (const auto& value : {identity.compiler, identity.options, identity.variant})
        key += ":" + std::to_string(value.size()) + ":" + value;
    return resources::Sha256Hex(resources::Sha256(IdentityBytes(key)));
}
inline const char* Extension(Format format) {
    switch (format) {
    case Format::Dxil: return ".dxil";
    case Format::Spirv: return ".spv";
    case Format::Dxbc: return ".dxbc";
    }
    return ".unsupported";
}
inline std::string ArtifactKey(bool pixel, uint64_t hash, const Identity& identity) {
    return resources::Sha256Hex(resources::Sha256(IdentityBytes(IdentityKey(identity) +
        ":" + std::to_string(pixel) + ":" + std::to_string(hash))));
}
inline std::string FileName(bool pixel, uint64_t hash, const Identity& identity) {
    char name[80];
    std::snprintf(name, sizeof(name), "%s_%016llx_v%u_b%u_f%u_", pixel ? "ps" : "vs",
        static_cast<unsigned long long>(hash), identity.translatorVersion,
        static_cast<unsigned>(identity.backend), static_cast<unsigned>(identity.format));
    return name + IdentityKey(identity) + Extension(identity.format);
}
// Legacy offline filenames remain available for source inventory and older
// tools. Runtime success caches use the identity-aware overload above.
inline std::string FileName(bool pixel, uint64_t hash, bool spirv = false) {
    char name[64];
    std::snprintf(name, sizeof(name), "%s_%016llx_v%u%s",
                  pixel ? "ps" : "vs", static_cast<unsigned long long>(hash), Version,
                  spirv ? "_vk12_1.spv" : ".dxil");
    return name;
}
// Reject incomplete/zero-filled cache writes before handing them to the driver.
// This is container framing validation, not cryptographic validation of DXIL.
inline constexpr uint32_t MaxContainerChunks = 4096;
inline bool CompleteContainer(std::span<const uint8_t> data) {
    if (data.size() < 32 || std::memcmp(data.data(), "DXBC", 4)) return false;
    auto word = [&](size_t offset) { uint32_t v; std::memcpy(&v, data.data()+offset, 4); return v; };
    if (word(24) != data.size()) return false;
    const uint32_t chunks = word(28);
    if (chunks > MaxContainerChunks || chunks > (data.size()-32)/4) return false;
    std::vector<std::pair<size_t,size_t>> ranges;
    ranges.reserve(chunks);
    for (uint32_t i=0; i<chunks; ++i) {
        const size_t offset = word(32+i*4);
        if (offset < 32 + size_t(chunks)*4 || offset % 4 || offset > data.size()-8 || word(offset+4) > data.size()-offset-8) return false;
        ranges.emplace_back(offset,offset+8+word(offset+4));
    }
    std::sort(ranges.begin(),ranges.end());
    for (size_t i=1;i<ranges.size();++i) if(ranges[i].first<ranges[i-1].second) return false;
    return chunks != 0;
}
// Validate complete SPIR-V instruction framing before handing a module to the driver.
inline bool CompleteSpirv(std::span<const uint8_t> data) {
    if (data.size() < 20 || data.size() % 4) return false;
    auto word = [&](size_t index) { uint32_t value; std::memcpy(&value,data.data()+index*4,4); return value; };
    if (word(0) != 0x07230203 || word(1) < 0x00010000 || word(1) > 0x00010600 || !word(3) || word(4)) return false;
    bool memoryModel=false, entryPoint=false;
    for (size_t index=5; index<data.size()/4;) {
        const uint32_t op=word(index), count=op>>16;
        if (!count || count>data.size()/4-index) return false;
        memoryModel |= (op&0xffff)==14 && count==3;
        entryPoint |= (op&0xffff)==15 && count>=4;
        index+=count;
    }
    return memoryModel && entryPoint;
}
inline bool CompleteBinary(std::span<const uint8_t> data, Format format) {
    if (format == Format::Spirv) return CompleteSpirv(data);
    if ((format != Format::Dxil && format != Format::Dxbc) || !CompleteContainer(data)) return false;
    auto word = [&](size_t offset) { uint32_t v; std::memcpy(&v,data.data()+offset,4); return v; };
    bool dxil=false, dxbc=false;
    for (uint32_t i=0; i<word(28); ++i) {
        const auto tag=word(word(32+i*4));
        dxil |= tag==0x4c495844; // DXIL
        dxbc |= tag==0x52444853 || tag==0x58454853; // SHDR or SHEX
    }
    return format==Format::Dxil ? dxil && !dxbc : dxbc && !dxil;
}
inline bool CompleteBinary(std::span<const uint8_t> data, bool spirv) {
    return CompleteBinary(data, spirv ? Format::Spirv : Format::Dxil);
}
}
