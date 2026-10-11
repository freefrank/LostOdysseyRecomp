#pragma once
#include "portable_shader_pack.h"
#include "cache.h"
#include "common_hlsl.h"
#include "resource_variant_identity.h"
#include "kernel/xex_identity.h"
#include <stdexcept>
namespace xenos::portable_pack {
// The executable prefix before import binding: XexLoader::UnboundIdentityPrefix()
// at runtime, the start of tools/xexdump output offline.
inline constexpr size_t RuntimeXexBytes = xex_identity::PrefixBytes;
// The one contract shared by the renderer, its export and LoShaderPackTool.
inline Digest RuntimeContract(std::span<const uint8_t> image,
    const cache::Identity& identity = cache::MakeIdentity(cache::Backend::Vulkan, ""),
    PackFormat format = PackFormat::Spirv) {
    if (image.size() != RuntimeXexBytes)
        throw std::runtime_error("portable shader contract needs the unbound executable prefix");
    return Contract(identity.translatorVersion, identity.options, identity.variant,
        ShaderCommonHlsl(cache::UsesUboConstants(identity)), resources::variants::DiscoveryIdentity, image, 1, format);
}
}
