#pragma once

#include "cache.h"
#include "resource_cpx_index_sha256.h"
#include "resource_variant_identity.h"
#include "xenos_translator.h"
#include <atomic>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <set>
#include <sstream>
#include <stdexcept>

namespace xenos::startup_cache {
namespace fs = std::filesystem;
using Digest = resources::Sha256Digest;
inline constexpr uint32_t Schema = 2;
inline constexpr uint32_t MaxRecords = 100000;
inline constexpr uint32_t MaxRecordBytes = 32u << 20;
inline constexpr uint64_t Magic = 0x31454c444e42534cULL; // LSBNDLE1
inline std::span<const uint8_t> Bytes(std::string_view text) {
    return {reinterpret_cast<const uint8_t*>(text.data()), text.size()};
}
inline std::string Hex(std::span<const uint8_t> data) { return resources::Sha256Hex(resources::Sha256(data)); }

struct FileStamp { std::string name; uint64_t size = 0, modified = 0; };
inline std::vector<FileStamp> List(const fs::path& directory,
    const std::function<bool(const fs::path&)>& include) {
    std::vector<FileStamp> result;
#ifdef _WIN32
    WIN32_FIND_DATAW data{};
    auto pattern = fs::absolute(directory / L"*").wstring();
    if (!pattern.starts_with(L"\\\\?\\")) {
        if (pattern.starts_with(L"\\\\")) pattern = L"\\\\?\\UNC\\" + pattern.substr(2);
        else pattern = L"\\\\?\\" + pattern;
    }
    HANDLE search = FindFirstFileExW(pattern.c_str(), FindExInfoBasic, &data,
        FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
    if (search == INVALID_HANDLE_VALUE) {
        const auto error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) return result;
        throw std::runtime_error("cache directory enumeration failed: " + std::to_string(error));
    }
    struct Close { HANDLE handle; ~Close() { FindClose(handle); } } close{search};
    do {
        const fs::path name(data.cFileName);
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY || !include(name)) continue;
        // Reparse points cannot be certified by metadata for the link itself.
        if (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
            throw std::runtime_error("cache snapshot contains a reparse point");
        result.push_back({name.generic_string(), uint64_t(data.nFileSizeHigh) << 32 | data.nFileSizeLow,
            uint64_t(data.ftLastWriteTime.dwHighDateTime) << 32 | data.ftLastWriteTime.dwLowDateTime});
    } while (FindNextFileW(search, &data));
    if (GetLastError() != ERROR_NO_MORE_FILES) throw std::runtime_error("incomplete cache directory enumeration");
#else
    std::error_code ec;
    if (!fs::exists(directory, ec)) return result;
    for (const auto& file : fs::directory_iterator(directory)) {
        if (!file.is_regular_file() || !include(file.path().filename())) continue;
        if (file.is_symlink()) throw std::runtime_error("cache snapshot contains a symlink");
        result.push_back({file.path().filename().generic_string(), file.file_size(),
            uint64_t(file.last_write_time().time_since_epoch().count())});
    }
#endif
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.name < b.name; });
    return result;
}

// A quick change detector, deliberately with the same size/mtime resource
// boundary as resources.manifest. It is not a content-integrity certificate for
// edits that deliberately preserve both values. Bundle contents are hashed below.
inline std::string Snapshot(const fs::path& game, const fs::path& cacheDir,
    bool spirv, std::string_view compiler, std::span<const uint8_t> xex,
    bool includeCompiled = true, bool includeSources = true, const cache::Identity* typed = nullptr,
    std::string_view discovery = resources::variants::DiscoveryIdentity) {
    std::ostringstream out;
    out << "startup-bundle=" << Schema << ";translator=" << (typed ? typed->translatorVersion : cache::Version)
        << ";backend=" << spirv << ";flags=lo-dxc-vulkan12-dx-layout-v1\n"
        << "compiler=" << compiler << "\nxex=" << Hex(xex) << '\n'
        << "discovery=" << discovery << '\n';
    if (typed) {
        if (!cache::ValidIdentity(*typed)) throw std::runtime_error("unsupported shader cache identity");
        // Frozen v0.4.17 contracts. Do not replace these with DefaultOptions:
        // a future compiler-argument change must invalidate existing bundles.
        constexpr std::string_view dxil = "main;vs/ps_6_0;HV2021;no-parentheses-equality;no-unused-value;all-resources-bound;O3;strip-debug;strip-reflect";
        constexpr std::string_view spv = "main;vs/ps_6_0;HV2021;no-parentheses-equality;no-unused-value;all-resources-bound;O3;strip-debug;spirv;vulkan1.2;dx-layout;vs-invert-y";
        const bool legacy = typed->translatorVersion == 21 && typed->variant == "guest-microcode" &&
            ((typed->backend == cache::Backend::D3D12 && typed->format == cache::Format::Dxil && typed->options == dxil) ||
             (typed->backend == cache::Backend::Vulkan && typed->format == cache::Format::Spirv && typed->options == spv));
        if (!legacy) out << "typed-identity=" << cache::IdentityKey(*typed) << '\n';
    }
    auto add = [&](const fs::path& directory, const auto& include) {
        out << fs::absolute(directory).lexically_normal().generic_string() << '\n';
        for (const auto& file : List(directory, include))
            out << file.name << '\t' << file.size << '\t' << file.modified << '\n';
    };
    std::vector<fs::path> roots{game};
    const auto name = game.filename().string();
    if (name == "disc1" || name == "disc2" || name == "disc3" || name == "disc4") {
        roots.clear();
        for (int disc = 1; disc <= 4; ++disc) {
            const auto root = game.parent_path() / ("disc" + std::to_string(disc));
            if (fs::is_directory(root)) roots.push_back(root);
        }
    }
    for (const auto& root : roots) add(root, [](const fs::path& p) {
        auto name = p.filename().string();
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        return name.ends_with(".fpd") || name == "lo.fpi" || name == "default.xex";
    });
    if(includeSources) add(cacheDir / "source", [](const fs::path& p) { return p.extension() == ".bin"; });
    if(includeSources) add(cacheDir, [&](const fs::path& p) {
        return (includeCompiled && (p.extension() == (typed ? cache::Extension(typed->format) : spirv ? ".spv" : ".dxil") ||
            p.extension() == ".failed")) || p.filename() == "resources.manifest";
    });
    return Hex(Bytes(out.str()));
}
inline std::string Snapshot(const fs::path& game, const fs::path& cacheDir,
    const cache::Identity& identity, std::span<const uint8_t> xex,
    bool includeCompiled = true, bool includeSources = true) {
    return Snapshot(game, cacheDir, identity.format == cache::Format::Spirv,
        identity.compiler, xex, includeCompiled, includeSources, &identity);
}

// Imported game assets are trusted on the normal startup path. Bind generated
// cache data to the current compiler/backend/translator and already-loaded XEX,
// without enumerating or re-reading the game or loose shader cache directories.
// Explicit full-scan/dump/retry controls bypass the bundle in the renderer.
inline std::string RuntimeIdentity(const fs::path& game, const cache::Identity& identity,
    std::span<const uint8_t> xex,
    std::string_view discovery = resources::variants::DiscoveryIdentity) {
    if (!cache::ValidIdentity(identity)) throw std::runtime_error("unsupported shader cache identity");
    std::ostringstream out;
    out << "trusted-import-bundle=" << Schema << '\n' << cache::IdentityKey(identity) << '\n'
        << fs::absolute(game).lexically_normal().generic_string() << '\n'
        << Hex(xex) << '\n' << discovery << '\n';
    return Hex(Bytes(out.str()));
}

struct Record {
    uint64_t hash = 0;
    TranslatedShader info;
    std::vector<uint8_t> binary;
    std::string failure; // A deterministic compiler rejection, never a module/IO failure.
};
struct Encoder {
    std::vector<uint8_t> bytes;
    void U32(uint32_t n) { for (int i=0;i<4;++i) bytes.push_back(uint8_t(n>>(8*i))); }
    void U64(uint64_t n) { U32(uint32_t(n)); U32(uint32_t(n>>32)); }
    void Blob(std::span<const uint8_t> b) {
        if (b.size() > MaxRecordBytes) throw std::runtime_error("startup cache record too large");
        U32(uint32_t(b.size())); bytes.insert(bytes.end(), b.begin(), b.end());
    }
    void Text(std::string_view s) { Blob(Bytes(s)); }
};
struct Decoder {
    std::span<const uint8_t> bytes;
    uint32_t U32() {
        if (bytes.size()<4) throw std::runtime_error("truncated startup cache record");
        uint32_t n=0; for (int i=0;i<4;++i) n|=uint32_t(bytes[i])<<(8*i);
        bytes=bytes.subspan(4); return n;
    }
    uint64_t U64() { const auto lo=U32(); return lo | uint64_t(U32())<<32; }
    std::span<const uint8_t> Blob() {
        const auto size=U32(); if (size>bytes.size()) throw std::runtime_error("invalid startup cache field length");
        const auto result=bytes.first(size); bytes=bytes.subspan(size); return result;
    }
    std::string Text() { const auto b=Blob(); return {reinterpret_cast<const char*>(b.data()),b.size()}; }
};
inline std::string Prelude(std::string_view common, bool pixel) {
    std::string result(common);
    if (pixel) {
        const std::string from="cbuffer XeConstants : register(b0, space0)";
        if (const auto pos=result.find(from);pos!=std::string::npos)
            result.replace(pos,from.size(),"cbuffer XeConstants : register(b2, space0)");
    }
    return result;
}
inline std::vector<uint8_t> Encode(const Record& r, std::string_view common) {
    Encoder e; const auto& i=r.info;
    e.U64(r.hash); e.U32(i.isPixelShader); e.U32(i.vertexFetchSlotsUsed);
    e.U64(i.vertexFetchSlotMask[0]); e.U64(i.vertexFetchSlotMask[1]); e.U32(i.textureSlotMask);
    e.Blob(i.textureDimension); e.U32(i.writesDepth); e.U32(i.colorTargetsWritten);
    e.U32(i.usesPointSize); e.U32(i.usesRelativeConstants);
    const auto prelude=Prelude(common,i.isPixelShader);
    const auto pos=prelude.empty() ? std::string::npos : i.hlsl.find(prelude);
    e.U32(pos!=std::string::npos);
    e.Text(pos==std::string::npos ? i.hlsl : i.hlsl.substr(0,pos));
    if(pos!=std::string::npos) e.Text(std::string_view(i.hlsl).substr(pos+prelude.size()));
    e.Text(i.errors); e.Text(r.failure); e.Blob(r.binary);
    return std::move(e.bytes);
}
inline Record Decode(std::span<const uint8_t> bytes, std::string_view common, cache::Format format,
    bool retainHlsl = true) {
    Decoder d{bytes}; Record r; auto& i=r.info;
    auto boolean=[&] { const auto n=d.U32(); if(n>1) throw std::runtime_error("invalid startup cache flag"); return n!=0; };
    r.hash=d.U64(); i.isPixelShader=boolean(); i.vertexFetchSlotsUsed=d.U32();
    i.vertexFetchSlotMask[0]=d.U64(); i.vertexFetchSlotMask[1]=d.U64(); i.textureSlotMask=d.U32();
    const auto dimensions=d.Blob(); if(dimensions.size()!=32) throw std::runtime_error("invalid texture metadata");
    std::copy(dimensions.begin(),dimensions.end(),i.textureDimension);
    i.writesDepth=boolean(); i.colorTargetsWritten=d.U32(); i.usesPointSize=boolean(); i.usesRelativeConstants=boolean();
    const bool sharedPrelude=boolean();
    const auto prefix=d.Blob();
    const auto suffix=sharedPrelude ? d.Blob() : std::span<const uint8_t>{};
    if (retainHlsl) {
        i.hlsl.assign(reinterpret_cast<const char*>(prefix.data()),prefix.size());
        if(sharedPrelude) {
            i.hlsl+=Prelude(common,i.isPixelShader);
            i.hlsl.append(reinterpret_cast<const char*>(suffix.data()),suffix.size());
        }
    }
    i.errors=d.Text(); r.failure=d.Text(); const auto binary=d.Blob();
    if(!d.bytes.empty() || !r.hash || i.colorTargetsWritten>15 || (r.failure.empty() && !cache::CompleteBinary(binary,format)) ||
        (!r.failure.empty() && !binary.empty())) throw std::runtime_error("invalid startup cache payload");
    r.binary.assign(binary.begin(),binary.end()); return r;
}
inline Record Decode(std::span<const uint8_t> bytes, std::string_view common, bool spirv) {
    return Decode(bytes, common, spirv ? cache::Format::Spirv : cache::Format::Dxil);
}
inline void WriteRaw(std::ostream& out, std::span<const uint8_t> b) {
    out.write(reinterpret_cast<const char*>(b.data()),b.size());
    if(!out) throw std::runtime_error("startup cache write failed");
}
inline void WriteNumber(std::ostream& out,uint64_t n) { Encoder e;e.U64(n);WriteRaw(out,e.bytes); }
inline uint64_t ReadNumber(std::istream& in) {
    std::array<uint8_t,8> b{}; if(!in.read(reinterpret_cast<char*>(b.data()),b.size())) throw std::runtime_error("truncated startup cache");
    Decoder d{b};return d.U64();
}
inline fs::path Temporary(const fs::path& target) {
    static std::atomic<uint64_t> sequence{0};
    return fs::path(target.wstring()+L".tmp-"+std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count())+L"-"+std::to_wstring(sequence++));
}
inline void Publish(const fs::path& temp,const fs::path& target) {
#ifdef _WIN32
    if(!MoveFileExW(temp.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("startup cache publish failed: "+std::to_string(GetLastError()));
#else
    fs::rename(temp,target);
#endif
}
class Writer {
    fs::path path,temp;
    std::ofstream out;
    uint64_t count=0;
    std::string common;
public:
    Writer(const fs::path& target,std::string_view commonText) : path(target),temp(Temporary(target)),out(temp,std::ios::binary),common(commonText) {
        WriteNumber(out,Magic);WriteNumber(out,Schema);WriteNumber(out,0);
        WriteRaw(out,std::array<uint8_t,64>{}); // Final input snapshot (hex).
        WriteNumber(out,common.size());WriteRaw(out,Bytes(common));
    }
    ~Writer() { out.close();std::error_code ec;fs::remove(temp,ec); }
    void Add(const Record& record) {
        if(++count>MaxRecords) throw std::runtime_error("too many startup cache records");
        const auto bytes=Encode(record,common);
        if(bytes.size()>MaxRecordBytes) throw std::runtime_error("startup cache record too large");
        // Reserved checksum slot preserves the existing bundle layout.
        WriteNumber(out,bytes.size());WriteRaw(out,Digest{});WriteRaw(out,bytes);
    }
    void Finish(std::string_view snapshot) {
        if(!count || snapshot.size()!=64) throw std::runtime_error("incomplete startup cache publication");
        WriteNumber(out,Magic);WriteRaw(out,Digest{});
        out.seekp(16);WriteNumber(out,count);WriteRaw(out,Bytes(snapshot));out.close();
        if(!out) throw std::runtime_error("startup cache close failed");
        Publish(temp,path);
    }
};
inline std::string ReadBundleIdentity(const fs::path& path) {
    std::error_code ec;
    if (!fs::is_regular_file(path, ec)) return {};
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    try {
        if (ReadNumber(in) != Magic || ReadNumber(in) != Schema) return {};
        const auto count = ReadNumber(in);
        if (!count || count > MaxRecords) return {};
        std::string identity(64, '\0');
        if (!in.read(identity.data(), identity.size())) return {};
        return identity;
    } catch (...) { return {}; }
}
using Progress = std::function<void(uint32_t done, uint32_t total)>;
struct LoadResult { bool ok=false;uint32_t records=0;uint64_t bytesRead=0;std::string reason; };
inline LoadResult Load(const fs::path& path,std::string_view snapshot,cache::Format format,
    const std::function<void(Record&&)>& consume,const std::function<void()>& pump={},
    const Progress& progress={}, bool retainHlsl=true) {
    LoadResult result;
    try {
        // pubsetbuf must precede open; the external buffer must outlive filebuf.
        std::vector<char> ioBuffer(256 << 10);
        std::ifstream in;
        in.rdbuf()->pubsetbuf(ioBuffer.data(), ioBuffer.size());
        in.open(path,std::ios::binary);if(!in) {result.reason="bundle missing";return result;}
        if(ReadNumber(in)!=Magic || ReadNumber(in)!=Schema) throw std::runtime_error("bundle format changed");
        const auto count=ReadNumber(in);if(!count || count>MaxRecords) throw std::runtime_error("invalid bundle record count");
        std::string identity(64,'\0');in.read(identity.data(),identity.size());
        if(!in || identity!=snapshot) throw std::runtime_error("resource/source/binary/compiler identity changed");
        const auto commonSize=ReadNumber(in);if(commonSize>MaxRecordBytes) throw std::runtime_error("invalid bundle prelude length");
        std::string common(size_t(commonSize),'\0');if(!in.read(common.data(),common.size())) throw std::runtime_error("truncated bundle prelude");

        std::set<std::pair<bool,uint64_t>> keys;

        if(progress) progress(0,uint32_t(count));
        if(pump) pump();
        auto lastPump=std::chrono::steady_clock::now();
        for(uint64_t index=0;index<count;++index) {
            const auto size=ReadNumber(in);if(size>MaxRecordBytes || size<32) throw std::runtime_error("invalid bundle record length");
            in.ignore(Digest{}.size());
            std::vector<uint8_t> bytes(static_cast<size_t>(size));
            if(!in.read(reinterpret_cast<char*>(bytes.data()),bytes.size())) throw std::runtime_error("truncated bundle record");
            result.bytesRead+=bytes.size();
            auto record=Decode(bytes,common,format,retainHlsl);
            if(!keys.emplace(record.info.isPixelShader,record.hash).second) throw std::runtime_error("duplicate bundle record");
            consume(std::move(record));
            const auto now=std::chrono::steady_clock::now();
            if(index+1==count || now-lastPump>=std::chrono::milliseconds(16)) {
                if(progress) progress(uint32_t(index+1),uint32_t(count));
                if(pump) pump();
                lastPump=now;
            }
        }

        if(ReadNumber(in)!=Magic) throw std::runtime_error("bundle completion marker missing");
        in.ignore(Digest{}.size());
        if(in.gcount()!=Digest{}.size() || !in || in.peek()!=std::char_traits<char>::eof())
            throw std::runtime_error("bundle completion marker truncated");

        result.ok=true;result.records=uint32_t(count);
    } catch(const std::exception& e) {result.reason=e.what();}
    return result;
}
inline LoadResult Load(const fs::path& path,std::string_view snapshot,bool spirv,
    const std::function<void(Record&&)>& consume,const std::function<void()>& pump={},
    const Progress& progress={}, bool retainHlsl=true) {
    return Load(path,snapshot,spirv ? cache::Format::Spirv : cache::Format::Dxil,consume,pump,progress,retainHlsl);
}
inline LoadResult LoadTransactional(const fs::path& path,std::string_view snapshot,bool spirv,
    const std::function<void(Record&&)>& consume,const std::function<void()>& rollback,
    const std::function<void()>& validateInputs,const std::function<void()>& pump={},
    const Progress& progress={}, bool retainHlsl=true) {
    auto result=Load(path,snapshot,spirv,consume,pump,progress,retainHlsl);
    if(result.ok) try {validateInputs();}
        catch(const std::exception& e) {result.ok=false;result.reason=e.what();}
    if(!result.ok) rollback();
    return result;
}
inline LoadResult LoadTransactional(const fs::path& path,std::string_view snapshot,const cache::Identity& identity,
    const std::function<void(Record&&)>& consume,const std::function<void()>& rollback,
    const std::function<void()>& validateInputs,const std::function<void()>& pump={},
    const Progress& progress={}, bool retainHlsl=true) {
    if (!cache::ValidIdentity(identity)) { rollback(); return {false,0,0,"unsupported shader cache identity"}; }
    auto result=Load(path,snapshot,identity.format,consume,pump,progress,retainHlsl);
    if(result.ok) try {validateInputs();}
        catch(const std::exception& e) {result.ok=false;result.reason=e.what();}
    if(!result.ok) rollback();
    return result;
}

// Negative results are separate from successful binaries. Only actual source
// compilation rejections may be remembered; infrastructure/device failures retry.
inline std::string FailureKey(std::string_view hlsl,std::string_view compiler,bool pixel,bool spirv) {
    const auto key=std::to_string(cache::Version)+":"+std::to_string(pixel)+":"+std::to_string(spirv)+
        ":lo-dxc-vulkan12-dx-layout-v1:"+std::string(compiler)+":"+Hex(Bytes(hlsl));
    return Hex(Bytes(key));
}
inline std::string FailureKey(std::string_view hlsl,const cache::Identity& identity,bool pixel) {
    if (!cache::ValidIdentity(identity)) return {};
    return Hex(Bytes(cache::IdentityKey(identity)+":"+std::to_string(pixel)+":"+Hex(Bytes(hlsl))));
}
inline std::string ReadFailure(const fs::path& path,std::string_view key) {
    try {
        std::ifstream in(path,std::ios::binary|std::ios::ate);const auto size=in.tellg();
        if(size<136 || size>8*1024*1024) return {};
        std::string bytes(size_t(size),'\0');in.seekg(0);if(!in.read(bytes.data(),bytes.size())) return {};
        if(bytes.substr(0,8)!="LOFAIL1\n" || bytes.substr(8,64)!=key) return {};
        return bytes.substr(136);
    } catch(...) {return {};}
}
inline bool WriteFailure(const fs::path& path,std::string_view key,std::string_view error,bool deterministic) {
    if(!deterministic || key.size()!=64 || error.empty() || error.size()>(8u<<20)-136) return false;
    const auto temp=Temporary(path);
    try {
        std::ofstream out(temp,std::ios::binary);out<<"LOFAIL1\n"<<key<<std::string(64,'0')<<error;out.close();
        if(!out) throw std::runtime_error("cannot write compiler failure record");
        Publish(temp,path);return true;
    } catch(...) {std::error_code ec;fs::remove(temp,ec);return false;}
}
}
