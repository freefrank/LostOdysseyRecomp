#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Runtime HLSL -> DXIL compilation through dxcompiler.dll (loaded dynamically
// from the Windows SDK or next to the executable).
namespace xenos
{
    enum class ShaderBinaryFormat { Dxil, Spirv };
    struct CompiledShader
    {
        std::vector<uint8_t> bytecode;
        std::string errors;
        bool ok = false;
        bool deterministicFailure = false;
    };

    // Returns false when dxcompiler.dll is unavailable.
    bool DxcAvailable();
    // Content identity of the loaded compiler and its validator companion.
    // Empty means it could not be certified; persistent failure reuse is disabled.
    const std::string& DxcIdentity();
    struct DxcStatistics { uint64_t calls, succeeded, rejected, infrastructureFailed; };
    DxcStatistics GetDxcStatistics();

    // profile: "vs_6_0" / "ps_6_0". debugInfo embeds source for PIX.
    CompiledShader CompileHlsl(const std::string& source, const char* entryPoint, const char* profile, bool debugInfo = false);
    CompiledShader CompileHlsl(const std::string& source, const char* entryPoint, const char* profile,
        ShaderBinaryFormat format, bool debugInfo = false);
    CompiledShader CompileCachedHlsl(const std::string& source, const char* entryPoint, const char* profile,
        ShaderBinaryFormat format);
}
