#include "dxc_compiler.h"
#include "cache.h"
#include "binary_cache.h"
#include "resource_cpx_index_sha256.h"
#include <atomic>
#include <filesystem>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#include <unknwn.h>
#include <objidl.h>
#include <dxcapi.h>
#include <mutex>
#include <filesystem>
#include <algorithm>
#include <vector>

namespace xenos
{
    namespace
    {
        // GUIDs from dxcapi.h, defined here so no import library is needed.
        const CLSID kClsidDxcCompiler = { 0x73e22d93, 0xe6ce, 0x47f3, { 0xb5, 0xbf, 0xf0, 0x66, 0x4f, 0x39, 0xc1, 0xb0 } };
        const CLSID kClsidDxcUtils = { 0x6245d6af, 0x66e0, 0x48fd, { 0x80, 0xb4, 0x4d, 0x27, 0x17, 0x96, 0x74, 0x8c } };

        DxcCreateInstanceProc g_createInstance = nullptr;
        HMODULE g_module = nullptr;
        std::atomic<uint64_t> g_calls{0}, g_succeeded{0}, g_rejected{0}, g_infrastructureFailed{0};
        std::once_flag g_loadOnce;

        void LoadDxc()
        {
            HMODULE module = LoadLibraryW(L"dxcompiler.dll");
            if (!module)
            {
                // Allow custom installations without embedding workstation paths.
                wchar_t configuredPath[32768];
                DWORD length = GetEnvironmentVariableW(L"LO_DXC_PATH", configuredPath, 32768);
                if (length && length < 32768)
                    module = LoadLibraryW(configuredPath);
            }
            if (!module)
            {
                wchar_t programFiles[32768];
                DWORD length = GetEnvironmentVariableW(L"ProgramFiles(x86)", programFiles, 32768);
                std::vector<std::filesystem::path> candidates;
                if (length && length < 32768)
                {
                    const auto sdkBin = std::filesystem::path(programFiles) / L"Windows Kits" / L"10" / L"bin";
                    std::error_code error;
                    std::filesystem::directory_iterator entry(sdkBin, error), end;
                    while (!error && entry != end)
                    {
                        candidates.push_back(entry->path() / L"x64" / L"dxcompiler.dll");
                        entry.increment(error);
                    }
                }
                std::sort(candidates.rbegin(), candidates.rend());
                for (const auto& path : candidates)
                {
                    module = LoadLibraryW(path.c_str());
                    if (module)
                        break;
                }
            }
            if (module)
                g_module = module;
            if (module)
                g_createInstance = reinterpret_cast<DxcCreateInstanceProc>(GetProcAddress(module, "DxcCreateInstance"));
        }

        template<typename T>
        struct ComPtr
        {
            T* p = nullptr;
            ~ComPtr() { if (p) p->Release(); }
            T** operator&() { return &p; }
            T* operator->() { return p; }
            explicit operator bool() const { return p != nullptr; }
        };
    }

    bool DxcAvailable()
    {
        std::call_once(g_loadOnce, LoadDxc);
        return g_createInstance != nullptr;
    }

    const std::string& DxcIdentity()
    {
        static const std::string identity = []() -> std::string {
            if (!DxcAvailable()) return {};
            try {
                auto pathOf = [](HMODULE module) {
                    wchar_t path[32768];
                    const DWORD size = GetModuleFileNameW(module, path, 32768);
                    if (!size || size == 32768) throw std::runtime_error("DXC module path unavailable");
                    return std::filesystem::path(path);
                };
                auto hash = [](const std::filesystem::path& path) {
                    std::ifstream in(path, std::ios::binary | std::ios::ate);
                    const auto size = in.tellg();
                    if (size <= 0 || size > 128 * 1024 * 1024) throw std::runtime_error("DXC module read unavailable");
                    std::vector<uint8_t> bytes(static_cast<size_t>(size)); in.seekg(0);
                    if (!in.read(reinterpret_cast<char*>(bytes.data()), size)) throw std::runtime_error("DXC module read incomplete");
                    return resources::Sha256Hex(resources::Sha256(bytes));
                };
                const auto compiler = pathOf(g_module);
                // Retain the actual validator module, including an already
                // loaded one, so its identity cannot drift after certification.
                const auto loadedValidator = GetModuleHandleW(L"dxil.dll");
                const auto validatorPath = loadedValidator ? pathOf(loadedValidator) : compiler.parent_path() / "dxil.dll";
                static HMODULE retainedValidator = LoadLibraryW(validatorPath.c_str());
                if (!retainedValidator) return {};
                return hash(compiler) + ":" + hash(pathOf(retainedValidator));
            } catch (...) { return {}; }
        }();
        return identity;
    }

    DxcStatistics GetDxcStatistics()
    {
        return {g_calls.load(), g_succeeded.load(), g_rejected.load(), g_infrastructureFailed.load()};
    }

    CompiledShader CompileHlsl(const std::string& source, const char* entryPoint, const char* profile, ShaderBinaryFormat format, bool debugInfo)
    {
        CompiledShader result;
        if (!DxcAvailable())
        {
            ++g_infrastructureFailed;
            result.errors = "dxcompiler.dll not available";
            return result;
        }

        ComPtr<IDxcUtils> utils;
        ComPtr<IDxcCompiler3> compiler;
        if (FAILED(g_createInstance(kClsidDxcUtils, __uuidof(IDxcUtils), reinterpret_cast<void**>(&utils))) ||
            FAILED(g_createInstance(kClsidDxcCompiler, __uuidof(IDxcCompiler3), reinterpret_cast<void**>(&compiler))))
        {
            result.errors = "DxcCreateInstance failed";
            ++g_infrastructureFailed;
            return result;
        }

        std::wstring entry(entryPoint, entryPoint + strlen(entryPoint));
        std::wstring target(profile, profile + strlen(profile));
        std::vector<LPCWSTR> args = {
            L"-E", entry.c_str(),
            L"-T", target.c_str(),
            L"-HV", L"2021",
            L"-Wno-parentheses-equality",
            L"-Wno-unused-value",
            L"-all-resources-bound",
        };
        if (format == ShaderBinaryFormat::Spirv)
        {
            args.insert(args.end(), {L"-spirv", L"-fspv-target-env=vulkan1.2", L"-fvk-use-dx-layout"});
            if (target.starts_with(L"vs_")) args.push_back(L"-fvk-invert-y");
        }
        if (debugInfo && format == ShaderBinaryFormat::Dxil)
        {
            args.push_back(L"-Zi");
            args.push_back(L"-Qembed_debug");
        }
        else
        {
            args.push_back(L"-O3");
            args.push_back(L"-Qstrip_debug");
            if (format == ShaderBinaryFormat::Dxil) args.push_back(L"-Qstrip_reflect");
        }

        DxcBuffer buffer{};
        buffer.Ptr = source.data();
        buffer.Size = source.size();
        buffer.Encoding = DXC_CP_UTF8;

        ComPtr<IDxcResult> compileResult;
        ++g_calls;
        HRESULT hr = compiler->Compile(&buffer, args.data(), uint32_t(args.size()), nullptr, __uuidof(IDxcResult), reinterpret_cast<void**>(&compileResult));
        if (FAILED(hr) || !compileResult)
        {
            result.errors = "IDxcCompiler3::Compile failed";
            ++g_infrastructureFailed;
            return result;
        }

        ComPtr<IDxcBlobUtf8> errors;
        compileResult->GetOutput(DXC_OUT_ERRORS, __uuidof(IDxcBlobUtf8), reinterpret_cast<void**>(&errors), nullptr);
        if (errors && errors->GetStringLength() > 0)
            result.errors.assign(errors->GetStringPointer(), errors->GetStringLength());

        HRESULT status = E_FAIL;
        compileResult->GetStatus(&status);
        if (FAILED(status)) {
            // Only ordinary source diagnostics are reusable. Internal compiler
            // and allocation failures are transient and must be retried.
            result.deterministicFailure = status != E_OUTOFMEMORY &&
                result.errors.find("error:") != std::string::npos &&
                result.errors.find("out of memory") == std::string::npos &&
                result.errors.find("internal compiler error") == std::string::npos;
            if (result.deterministicFailure) ++g_rejected;
            else ++g_infrastructureFailed;
            return result;
        }

        ComPtr<IDxcBlob> object;
        compileResult->GetOutput(DXC_OUT_OBJECT, __uuidof(IDxcBlob), reinterpret_cast<void**>(&object), nullptr);
        if (!object)
        {
            result.errors += "\nno object output";
            ++g_infrastructureFailed;
            return result;
        }
        auto* data = static_cast<const uint8_t*>(object->GetBufferPointer());
        result.bytecode.assign(data, data + object->GetBufferSize());
        result.ok = true;
        ++g_succeeded;
        return result;
    }
}
#else
namespace xenos
{
    bool DxcAvailable() { return false; }
    const std::string& DxcIdentity() { static const std::string empty; return empty; }
    DxcStatistics GetDxcStatistics() { return {}; }
    CompiledShader CompileHlsl(const std::string&, const char*, const char*, ShaderBinaryFormat, bool) { return {}; }
}
#endif

namespace xenos
{
    CompiledShader CompileHlsl(const std::string& source, const char* entry, const char* profile, bool debugInfo)
    {
        return CompileHlsl(source, entry, profile, ShaderBinaryFormat::Dxil, debugInfo);
    }

    CompiledShader CompileCachedHlsl(const std::string& source, const char* entry, const char* profile, ShaderBinaryFormat format)
    {
        const std::string key = source + '\0' + entry + '\0' + profile + "lo-dxc-vulkan12-dx-layout-v1";
        uint64_t hash = 0xcbf29ce484222325ull;
        for (uint8_t byte : key) { hash ^= byte; hash *= 0x100000001b3ull; }
        const bool spirv = format == ShaderBinaryFormat::Spirv;
        auto identity = cache::MakeIdentity(spirv ? cache::Backend::Vulkan : cache::Backend::D3D12, DxcIdentity());
        identity.variant = "builtin:" + std::to_string(std::strlen(entry)) + ":" + entry +
            ":" + std::to_string(std::strlen(profile)) + ":" + profile;
        const bool pixel = std::string_view(profile).starts_with("ps_");
        const char* configured = std::getenv("LO_SHADER_CACHE_DIR");
        const auto directory = std::filesystem::path(configured ? configured : "cache/shaders") / "builtin";
        const auto path = directory / cache::FileName(pixel, hash, identity);
        CompiledShader result;
        if (!configured || *configured) result.bytecode = cache::ReadBinary(path, pixel, hash, identity);
        if (!result.bytecode.empty()) { result.ok = true; return result; }
        result = CompileHlsl(source, entry, profile, format);
        if (result.ok && (!configured || *configured)) {
            std::error_code error;
            std::filesystem::create_directories(directory, error);
            if (!error) cache::WriteBinary(path, pixel, hash, identity, result.bytecode);
        }
        return result;
    }
}
