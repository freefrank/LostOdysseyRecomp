#pragma once
#include "portable_shader_pack.h"
#include "cache.h"
#include "common_hlsl.h"
#include "resource_variant_identity.h"
namespace xenos::portable_pack {
// xexdump emits a flat loaded image, not the encrypted on-disc XEX.
inline constexpr uint32_t RuntimeXexAddress = 0x82000000;
inline constexpr size_t RuntimeXexBytes = 0x185C60;
inline Digest RuntimeContract(std::span<const uint8_t> image,
    const cache::Identity& identity = cache::MakeIdentity(cache::Backend::Vulkan, "")) {
    return Contract(identity.translatorVersion, identity.options, identity.variant,
        kShaderCommonHlsl, resources::variants::DiscoveryIdentity, image);
}
}
