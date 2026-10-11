#pragma once
#include "vertex_fetch_contract.h"
#include <string>
#include <string_view>

// Shared verbatim by translation and the offline/runtime pack contract.
namespace xenos {
// Translated SPIR-V shaders read the Xenos constant banks in one of two forms.
// Pointer: device addresses in push constants, read with vk::RawBufferLoad (the
// form every published pack uses). UBO: dynamic uniform buffers in set 5 with
// the D3D12 cbuffer layout; NVK keeps those in the Maxwell constant banks, where
// a read costs no memory load (the Switch). DXIL takes the cbuffers either way.
#define XE_POINTER_GUARD XE_CONSTANTS_FORM("#ifdef __spirv__", "#ifdef XE_POINTER_CONSTANTS")
#define XE_CONSTANTS_FORM(pointer, ubo) pointer
inline constexpr char kShaderCommonHlsl[] =
#include "common_hlsl_text.inc"
;
#undef XE_CONSTANTS_FORM
#define XE_CONSTANTS_FORM(pointer, ubo) ubo
inline constexpr char kShaderCommonHlslUbo[] =
#include "common_hlsl_text.inc"
;
#undef XE_CONSTANTS_FORM
#undef XE_POINTER_GUARD

inline std::string_view ShaderCommonHlsl(bool uboConstants) {
    return uboConstants ? kShaderCommonHlslUbo : kShaderCommonHlsl;
}

// Both stages share one pipeline layout: a pixel shader's bank is b2, in the
// prelude's space.
inline void UsePixelConstantRegister(std::string& hlsl) {
    constexpr std::string_view from = "cbuffer XeConstants : register(b0, ";
    if (const auto pos = hlsl.find(from); pos != std::string::npos)
        hlsl[pos + from.size() - 3] = '2';
}
}
