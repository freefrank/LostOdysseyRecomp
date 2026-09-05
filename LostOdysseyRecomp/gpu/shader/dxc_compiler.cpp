#include "dxc_compiler.h"

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

    CompiledShader CompileHlsl(const std::string& source, const char* entryPoint, const char* profile, bool debugInfo)
    {
        CompiledShader result;
        if (!DxcAvailable())
        {
            result.errors = "dxcompiler.dll not available";
            return result;
        }

        ComPtr<IDxcUtils> utils;
        ComPtr<IDxcCompiler3> compiler;
        if (FAILED(g_createInstance(kClsidDxcUtils, __uuidof(IDxcUtils), reinterpret_cast<void**>(&utils))) ||
            FAILED(g_createInstance(kClsidDxcCompiler, __uuidof(IDxcCompiler3), reinterpret_cast<void**>(&compiler))))
        {
            result.errors = "DxcCreateInstance failed";
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
        if (debugInfo)
        {
            args.push_back(L"-Zi");
            args.push_back(L"-Qembed_debug");
        }
        else
        {
            args.push_back(L"-O3");
            args.push_back(L"-Qstrip_debug");
            args.push_back(L"-Qstrip_reflect");
        }

        DxcBuffer buffer{};
        buffer.Ptr = source.data();
        buffer.Size = source.size();
        buffer.Encoding = DXC_CP_UTF8;

        ComPtr<IDxcResult> compileResult;
        HRESULT hr = compiler->Compile(&buffer, args.data(), uint32_t(args.size()), nullptr, __uuidof(IDxcResult), reinterpret_cast<void**>(&compileResult));
        if (FAILED(hr) || !compileResult)
        {
            result.errors = "IDxcCompiler3::Compile failed";
            return result;
        }

        ComPtr<IDxcBlobUtf8> errors;
        compileResult->GetOutput(DXC_OUT_ERRORS, __uuidof(IDxcBlobUtf8), reinterpret_cast<void**>(&errors), nullptr);
        if (errors && errors->GetStringLength() > 0)
            result.errors.assign(errors->GetStringPointer(), errors->GetStringLength());

        HRESULT status = E_FAIL;
        compileResult->GetStatus(&status);
        if (FAILED(status))
            return result;

        ComPtr<IDxcBlob> object;
        compileResult->GetOutput(DXC_OUT_OBJECT, __uuidof(IDxcBlob), reinterpret_cast<void**>(&object), nullptr);
        if (!object)
        {
            result.errors += "\nno object output";
            return result;
        }
        auto* data = static_cast<const uint8_t*>(object->GetBufferPointer());
        result.dxil.assign(data, data + object->GetBufferSize());
        result.ok = true;
        return result;
    }
}
#else
namespace xenos
{
    bool DxcAvailable() { return false; }
    CompiledShader CompileHlsl(const std::string&, const char*, const char*, bool) { return {}; }
}
#endif
