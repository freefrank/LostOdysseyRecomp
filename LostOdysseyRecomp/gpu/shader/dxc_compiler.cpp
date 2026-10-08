#include "dxc_compiler.h"
#include <os/platform.h>
#include "cache.h"
#include "binary_cache.h"
#include "builtin_shader_store.h"
#include "resource_cpx_index_sha256.h"
#include <os/shader_log.h>
#include <os/user_paths.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <unknwn.h>
#include <objidl.h>
#include <dxcapi.h>
#else
#if !LO_PLATFORM_SWITCH
#include <dlfcn.h>
#endif
#include <type_traits>
#ifndef __EMULATE_UUID
#define __EMULATE_UUID 1
#endif
#include <WinAdapter.h>
#include <dxcapi.h>
#endif

namespace xenos
{
    namespace
    {
        // GUIDs from dxcapi.h, defined here so no import library is needed.
        const CLSID kClsidDxcCompiler = { 0x73e22d93, 0xe6ce, 0x47f3, { 0xb5, 0xbf, 0xf0, 0x66, 0x4f, 0x39, 0xc1, 0xb0 } };
        const CLSID kClsidDxcUtils = { 0x6245d6af, 0x66e0, 0x48fd, { 0x80, 0xb4, 0x4d, 0x27, 0x17, 0x96, 0x74, 0x8c } };

        DxcCreateInstanceProc g_createInstance = nullptr;
#ifdef _WIN32
        HMODULE g_module = nullptr;
#else
        void* g_module = nullptr;
        // The last dlopen error, reported with "not available" (e.g. a code
        // signature rejected by the hardened runtime's library validation).
        std::string g_loadError;
#endif
        std::atomic<uint64_t> g_calls{0}, g_succeeded{0}, g_rejected{0}, g_infrastructureFailed{0};
        std::once_flag g_loadOnce;

        // Host-shader stores (builtin_shader_store.h), one per binary format,
        // opened on first use. The first use also moves the old `builtin/`
        // folder into them and removes stores of older translator versions.
        struct BuiltinStores
        {
            std::mutex mutex;
            std::filesystem::path dir;
            bool cleaned = false;
            std::unique_ptr<cache::ShaderStore> stores[3];
            bool tried[3]{};
        };
        BuiltinStores& Builtins() { static BuiltinStores stores; return stores; }

        cache::ShaderStore* OpenBuiltinStoreLocked(BuiltinStores& s, cache::Format format)
        {
            const auto i = static_cast<size_t>(format);
            if (format == cache::Format::Dxbc) return nullptr;
            if (!s.tried[i]) {
                s.tried[i] = true;
                std::error_code ec;
                std::filesystem::create_directories(s.dir, ec);
                auto store = std::make_unique<cache::ShaderStore>();
                std::string error;
                const auto path = cache::BuiltinStorePath(s.dir, format);
                if (store->Open(path, format, &error)) s.stores[i] = std::move(store);
                else LOG_WARNING("shader cache: host shader store {} unavailable ({}); using per-shader files", path.string(), error);
            }
            return s.stores[i].get();
        }

        cache::ShaderStore* BuiltinStore(const std::filesystem::path& dir, cache::Format format)
        {
            auto& s = Builtins();
            std::lock_guard lock(s.mutex);
            if (!s.cleaned) {
                s.cleaned = true;
                s.dir = dir;
                uint64_t oldBytes = 0;
                if (const auto removed = cache::RemoveOldBuiltinStores(dir, &oldBytes))
                    LOG_INFO("shader cache cleanup: removed {} host shader stores of older translator versions ({} bytes freed)", removed, oldBytes);
                const auto folder = dir / "builtin";
                std::error_code ec;
                if (std::filesystem::is_directory(folder, ec)) {
                    const auto moved = cache::MigrateBuiltinFolder(folder, [&](cache::Format f) -> cache::ShaderStore* {
                        auto* store = OpenBuiltinStoreLocked(s, f);
                        return store && store->Writable() ? store : nullptr;
                    });
                    LOG_INFO("shader cache cleanup: moved {} host shaders from {} into {}, discarded {} stale files, {} bytes of per-shader files freed; folder {}{}",
                        moved.moved, folder.string(), cache::BuiltinStorePath(dir, format).filename().string(), moved.discarded, moved.bytesFreed,
                        moved.folderRemoved ? "removed" : fmt::format("kept ({} files left)", moved.kept),
                        moved.error.empty() ? std::string{} : "; " + moved.error);
                }
            }
            return OpenBuiltinStoreLocked(s, format);
        }

#ifdef _WIN32
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
#else
        void LoadDxc()
        {
#if LO_PLATFORM_SWITCH
            // No dynamic libraries on Horizon and no DXC build for it: the
            // console renders from the prebuilt SPIR-V pack (docs/SWITCH.md).
            g_loadError = "not available on Nintendo Switch (install the Vulkan shader pack)";
            return;
#else
#if LO_PLATFORM_MACOS
            constexpr const char* kLibrary = "libdxcompiler.dylib";
#else
            constexpr const char* kLibrary = "libdxcompiler.so";
#endif
            std::vector<std::filesystem::path> candidates;
            candidates.push_back(kLibrary);
            if (const char* envPath = std::getenv("LO_DXC_PATH"); envPath && *envPath)
            {
                candidates.push_back(envPath);
            }
            std::error_code ec;
            const auto cwd = std::filesystem::current_path(ec);
            if (!ec)
            {
                candidates.push_back(cwd / kLibrary);
            }
#if LO_PLATFORM_MACOS
            // dyld resolves this beside the running executable.
            candidates.push_back(std::string("@executable_path/") + kLibrary);
            const std::filesystem::path relativeDxc = "tools/XenosRecomp/thirdparty/dxc-bin/lib/arm64/libdxcompiler.dylib";
#else
            char selfPath[4096]{};
            const ssize_t n = readlink("/proc/self/exe", selfPath, sizeof(selfPath) - 1);
            if (n > 0)
            {
                selfPath[n] = '\0';
                candidates.push_back(std::filesystem::path(selfPath).parent_path() / "libdxcompiler.so");
            }
#if defined(__linux__) && defined(__aarch64__)
            const std::filesystem::path relativeDxc = "tools/XenosRecomp/thirdparty/dxc-bin/lib/arm64/libdxcompiler.so";
#else
            const std::filesystem::path relativeDxc = "tools/XenosRecomp/thirdparty/dxc-bin/lib/x64/libdxcompiler.so";
#endif
#endif
            if (std::filesystem::exists(relativeDxc, ec))
            {
                candidates.push_back(relativeDxc);
            }

            void* module = nullptr;
            for (const auto& candidate : candidates)
            {
                if (candidate.empty())
                    continue;
                module = dlopen(candidate.c_str(), RTLD_NOW | RTLD_LOCAL);
                if (!module)
                    if (const char* error = dlerror()) g_loadError = error;
                if (module)
                {
                    auto proc = reinterpret_cast<DxcCreateInstanceProc>(dlsym(module, "DxcCreateInstance"));
                    if (proc)
                    {
                        g_module = module;
                        g_createInstance = proc;
                        break;
                    }
                    dlclose(module);
                    module = nullptr;
                }
            }
#endif // LO_PLATFORM_SWITCH
        }
#endif

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
#if LO_PLATFORM_SWITCH
        // No DXC on the console, but SPIR-V compiled on a PC by the pinned DXC
        // (tools/XenosRecomp/thirdparty/dxc-bin, v1.8) is valid here: the
        // PC Vulkan shader cache (builtin host shaders, local shader store,
        // startup bundle) copied to the SD card is keyed by this identity.
        static const std::string identity = LO_SWITCH_DXC_IDENTITY;
        return identity;
#else
        static const std::string identity = []() -> std::string {
            if (!DxcAvailable()) return {};
            // Compiler version is cache compatibility metadata. Do not reread
            // and hash entire compiler/validator libraries during game startup.
            ComPtr<IDxcCompiler3> compiler;
            ComPtr<IDxcVersionInfo> version;
            if (FAILED(g_createInstance(kClsidDxcCompiler, __uuidof(IDxcCompiler3), reinterpret_cast<void**>(&compiler))) ||
                FAILED(compiler->QueryInterface(__uuidof(IDxcVersionInfo), reinterpret_cast<void**>(&version))))
                return {};
            UINT32 major = 0, minor = 0;
            if (FAILED(version->GetVersion(&major, &minor))) return {};
            return "dxc-" + std::to_string(major) + "." + std::to_string(minor);
        }();
        return identity;
#endif
    }

    DxcStatistics GetDxcStatistics()
    {
        return {g_calls.load(), g_succeeded.load(), g_rejected.load(), g_infrastructureFailed.load()};
    }

    static CompiledShader CompileHlslImpl(const std::string& source, const char* entryPoint, const char* profile, ShaderBinaryFormat format, bool debugInfo)
    {
        CompiledShader result;
        if (!DxcAvailable())
        {
            ++g_infrastructureFailed;
#ifdef _WIN32
            result.errors = "dxcompiler.dll not available";
#else
            result.errors = "dxcompiler library not available" + (g_loadError.empty() ? "" : ": " + g_loadError);
#endif
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

namespace xenos
{
    CompiledShader CompileHlsl(const std::string& source, const char* entry, const char* profile, ShaderBinaryFormat format, bool debugInfo)
    {
        const auto start = std::chrono::steady_clock::now();
        auto result = CompileHlslImpl(source, entry, profile, format, debugInfo);
        // This is compilation-time provenance, not a guest shader or cache key.
        // Do not add content hashing to the cache-hit/per-draw path.
        if (os::shaderlog::Current().IsOpen())
        {
            const auto hash = resources::Sha256Hex(resources::Sha256(
                std::span(reinterpret_cast<const uint8_t*>(source.data()), source.size())));
            os::shaderlog::Log(result.ok ? LogType::Info : LogType::Error,
                result.ok ? "compile-success" : result.deterministicFailure ? "compile-rejected" : "compile-failed",
                os::shaderlog::HashNamespace::HlslSourceSha256,
                "source={} profile={} entry={} format={} debug={} bytes={} elapsed_ms={:.3f}\n{}", hash, profile, entry,
                format == ShaderBinaryFormat::Spirv ? "spirv" : "dxil", debugInfo, result.bytecode.size(),
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count(), result.errors);
        }
        return result;
    }

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
        const bool cacheEnabled = !configured || *configured;
        const auto cacheDir = configured ? std::filesystem::path(configured) : (os::user_paths::UsePortableLayout() ? std::filesystem::path("cache/shaders") : os::user_paths::DataDir() / "cache/shaders");
        const auto directory = cacheDir / "builtin";
        const auto path = directory / cache::FileName(pixel, hash, identity);
        auto* store = cacheEnabled && cache::ValidIdentity(identity) ? BuiltinStore(cacheDir, identity.format) : nullptr;
        const auto digest = store ? cache::BuiltinDigest(identity) : cache::InputDigest{};
        CompiledShader result;
        if (store) {
            result.bytecode = store->Find(pixel, hash, digest);
            // A second instance cannot move the folder; it still reads it.
            if (result.bytecode.empty() && !store->Writable())
                result.bytecode = cache::ReadBinary(path, pixel, hash, identity);
        } else if (cacheEnabled) result.bytecode = cache::ReadBinary(path, pixel, hash, identity);
        if (!result.bytecode.empty()) {
            result.ok = true;
            if (os::shaderlog::Current().IsOpen())
                SHADER_LOG_INFO("cache-hit", BuiltinKeyFnv, "builtin key={:016x} profile={} entry={} format={} bytes={}",
                    hash, profile, entry, spirv ? "spirv" : "dxil", result.bytecode.size());
            return result;
        }
        if (os::shaderlog::Current().IsOpen())
            SHADER_LOG_INFO("cache-miss", BuiltinKeyFnv, "builtin key={:016x} profile={} entry={} format={}",
                hash, profile, entry, spirv ? "spirv" : "dxil");
        result = CompileHlsl(source, entry, profile, format);
        if (result.ok && store) {
            if (store->Writable()) store->Add(pixel, hash, digest, result.bytecode);
        } else if (result.ok && cacheEnabled) {
            std::error_code error;
            std::filesystem::create_directories(directory, error);
            if (!error) cache::WriteBinary(path, pixel, hash, identity, result.bytecode);
        }
        return result;
    }
}
