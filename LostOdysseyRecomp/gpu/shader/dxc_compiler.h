#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Runtime HLSL -> DXIL compilation through dxcompiler.dll (loaded dynamically
// from the Windows SDK or next to the executable).
namespace xenos
{
    struct CompiledShader
    {
        std::vector<uint8_t> dxil;
        std::string errors;
        bool ok = false;
    };

    // Returns false when dxcompiler.dll is unavailable.
    bool DxcAvailable();

    // profile: "vs_6_0" / "ps_6_0". debugInfo embeds source for PIX.
    CompiledShader CompileHlsl(const std::string& source, const char* entryPoint, const char* profile, bool debugInfo = false);
}
