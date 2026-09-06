#pragma once

#include "cache.h"
#include "resource_index.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <set>
#include <sstream>
#include <vector>

namespace xenos::resources {
namespace fs = std::filesystem;
inline uint32_t ReadBE(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}
inline uint64_t Hash(std::span<const uint8_t> bytes, uint64_t h = 0xcbf29ce484222325ULL) {
    for (auto b : bytes) h = (h ^ b) * 0x100000001b3ULL;
    return h;
}
inline std::string SourceName(bool pixel, std::span<const uint8_t> bytes) {
    return cache::FileName(pixel, Hash(bytes)).substr(0, 19) + ".bin";
}
// SDK container layout also documented in tools/XenosRecomp/XenosRecomp/shader.h.
// Validate both the container framing and the embedded constant-table stage.
inline std::span<const uint8_t> Microcode(std::span<const uint8_t> data, bool& pixel) {
    if (data.size() < 36) return {};
    const auto flags = ReadBE(data.data());
    if (flags != 0x102a1100 && flags != 0x102a1101) return {};
    const auto virt = ReadBE(data.data()+4), phys = ReadBE(data.data()+8);
    const auto ct = ReadBE(data.data()+16), shader = ReadBE(data.data()+24);
    if (virt < 36 || virt > 65536 || phys < 12 || phys > 262144 ||
        uint64_t(virt)+phys > data.size() || ct < 36 || ct > virt-16 ||
        shader < 36 || shader > virt-24) return {};
    pixel = (flags & 1) == 0;
    if (ReadBE(data.data()+ct+12) != (pixel ? 0xffff0300u : 0xfffe0300u)) return {};
    const auto offset = ReadBE(data.data()+shader), size = ReadBE(data.data()+shader+4);
    if (size < 12 || size % 12 || uint64_t(offset)+size > phys) return {};
    return data.subspan(virt+offset, size);
}
// Bounded probes identify known resource layouts; every indexed shader is also
// hashed before any indexed outputs for this file are published. These probes
// are not a whole-file integrity check for unrelated game assets.
inline uint64_t Fingerprint(std::ifstream& in, uint64_t length, uint64_t& bytesRead) {
    const uint64_t sample = std::min<uint64_t>(length, 4096);
    const uint64_t offsets[] = {0, length/4, length/2, length-sample};
    std::vector<uint8_t> bytes(sample);
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (auto offset : offsets) {
        const auto amount = std::min<uint64_t>(sample, length-offset);
        in.clear(); in.seekg(offset);
        if (!in.read(reinterpret_cast<char*>(bytes.data()), amount)) throw std::runtime_error("short resource probe");
        bytesRead += amount;
        hash = Hash(std::span(bytes).first(amount), hash);
    }
    return hash;
}
inline bool ExtractIndexed(const fs::path& file, const fs::path& source,
                           std::span<const IndexFile> index, std::set<std::string>& names,
                           uint64_t& bytesRead) {
    const auto length = fs::file_size(file);
    const auto filename = file.filename().string();
    bool probed = false;
    uint64_t fingerprint = 0;
    std::ifstream in(file, std::ios::binary);
    for (const auto& profile : index) {
        if (profile.name != filename || profile.size != length) continue;
        if (!probed) { fingerprint = Fingerprint(in, length, bytesRead); probed = true; }
        if (profile.fingerprint != fingerprint) continue;
        std::vector<std::pair<std::string, std::vector<uint8_t>>> prepared;
        bool valid = true;
        for (const auto& entry : profile.entries) {
            if (entry.size < 12 || entry.size > 262144 || entry.size % 12 ||
                entry.offset > length || entry.size > length-entry.offset) { valid=false; break; }
            std::vector<uint8_t> code(entry.size);
            in.clear(); in.seekg(entry.offset);
            if (!in.read(reinterpret_cast<char*>(code.data()), code.size())) { valid=false; break; }
            bytesRead += code.size();
            if (Hash(code) != entry.hash) { valid=false; break; }
            auto name = SourceName(entry.pixel, code);
            if (!names.contains(name)) prepared.emplace_back(std::move(name), std::move(code));
        }
        if (!valid) continue;
        for (const auto& [name, code] : prepared) {
            std::ofstream out(source / name, std::ios::binary | std::ios::trunc);
            out.write(reinterpret_cast<const char*>(code.data()), code.size()); out.close();
            if (!out) throw std::runtime_error("cannot write indexed shader");
            names.insert(name);
        }
        return true; // Includes verified resources containing no shader containers.
    }
    return false;
}
struct Result {
    size_t shaders = 0;
    bool reused = false;
    std::string error;
    size_t indexedFiles = 0, scannedFiles = 0;
    uint64_t bytesRead = 0;
};
inline Result Scan(const fs::path& root, const fs::path& cacheDir,
                   const std::function<void(uint32_t,uint32_t)>& progress,
                   std::span<const IndexFile> index = builtin::files) {
    Result result;
    try {
        std::vector<fs::path> roots{root}, files;
        const auto name = root.filename().string();
        // Development four-disc layout. A standalone --game directory scans itself only.
        if (name == "disc1" || name == "disc2" || name == "disc3" || name == "disc4") {
            roots.clear();
            for (int i=1; i<=4; ++i) {
                auto p = root.parent_path() / ("disc" + std::to_string(i));
                if (fs::is_directory(p)) roots.push_back(p);
            }
        }
        for (const auto& dir : roots)
            for (const auto& entry : fs::directory_iterator(dir))
                if (entry.is_regular_file() && entry.path().extension() == ".fpd") files.push_back(entry.path());
        std::sort(files.begin(), files.end());
        if (files.empty()) { result.error = "no FPD game resources found"; return result; }
        std::ostringstream identity;
        identity << "resource-scanner-v2\n";
        uint64_t totalBytes = 0;
        for (const auto& file : files) {
            const auto size = fs::file_size(file); totalBytes += size;
            identity << fs::absolute(file).generic_string() << '\t' << size << '\t'
                     << fs::last_write_time(file).time_since_epoch().count() << '\n';
        }
        const auto source = cacheDir / "source";
        fs::create_directories(source);
        const auto manifest = cacheDir / "resources.manifest";
        const std::string prefix = identity.str() + "--sources--\n";
        std::ifstream old(manifest, std::ios::binary);
        std::string contents((std::istreambuf_iterator<char>(old)), {});
        old.close();
        if (contents.starts_with(prefix)) {
            std::istringstream lines(contents.substr(prefix.size()));
            std::string filename; bool valid = true;
            while (std::getline(lines, filename)) {
                if (filename.size()!=23 || (!filename.starts_with("ps_") && !filename.starts_with("vs_")) ||
                    !filename.ends_with(".bin") || filename.find_first_not_of("0123456789abcdef",3) != 19) { valid=false; break; }
                std::ifstream in(source / filename, std::ios::binary | std::ios::ate);
                const auto size=in.tellg();
                if (size < 12 || size > 262144 || size % 12) { valid=false; break; }
                std::vector<uint8_t> bytes(static_cast<size_t>(size)); in.seekg(0);
                if (!in.read(reinterpret_cast<char*>(bytes.data()), size) || SourceName(filename.starts_with("ps_"), bytes)!=filename) { valid=false; break; }
                ++result.shaders;
            }
            if (valid && result.shaders) { result.reused=true; return result; }
            result.shaders=0;
        }
        constexpr size_t chunk = 4*1024*1024, overlap = 65536+262144;
        std::vector<uint8_t> bytes(chunk+overlap);
        std::set<std::string> names;
        uint64_t completed=0;
        for (const auto& file : files) {
            const auto length=fs::file_size(file);
            if (ExtractIndexed(file, source, index, names, result.bytesRead)) {
                ++result.indexedFiles;
                completed += length;
                progress(uint32_t(completed/1048576),uint32_t((totalBytes+1048575)/1048576));
                continue;
            }
            ++result.scannedFiles;
            std::ifstream in(file, std::ios::binary);
            if (!in) throw std::runtime_error("cannot read " + file.string());
            for (uint64_t base=0; base<length; base+=chunk) {
                in.clear(); in.seekg(base);
                const auto amount=std::min<uint64_t>(bytes.size(),length-base);
                if (!in.read(reinterpret_cast<char*>(bytes.data()), amount)) throw std::runtime_error("short resource read");
                result.bytesRead += amount;
                const auto core=std::min<uint64_t>(chunk,length-base);
                for (size_t i=0; i<core && i+36<=amount; ++i) {
                    if (bytes[i]!=0x10 || bytes[i+1]!=0x2a || bytes[i+2]!=0x11) continue;
                    bool pixel=false;
                    auto code=Microcode(std::span<const uint8_t>(bytes.data()+i,amount-i),pixel);
                    if (code.empty()) continue;
                    const auto filename=SourceName(pixel,code);
                    if (names.insert(filename).second) {
                        std::ofstream out(source / filename,std::ios::binary | std::ios::trunc);
                        out.write(reinterpret_cast<const char*>(code.data()),code.size()); out.close();
                        if (!out) throw std::runtime_error("cannot write extracted shader");
                    }
                }
                progress(uint32_t((completed+base+core)/1048576),uint32_t((totalBytes+1048575)/1048576));
            }
            completed+=length;
        }
        result.shaders=names.size();
        if (names.empty()) { result.error="no supported shader containers found"; return result; }
        // Never mark an interrupted extraction complete. Sources are revalidated on reuse.
        const auto temp=cacheDir / "resources.manifest.tmp";
        std::ofstream out(temp,std::ios::binary | std::ios::trunc); out << prefix;
        for (const auto& filename : names) out << filename << '\n';
        out.close(); if (!out) throw std::runtime_error("cannot write resource manifest");
        std::error_code ec; fs::remove(manifest,ec); fs::rename(temp,manifest);
    } catch (const std::exception& e) { result.error=e.what(); }
    return result;
}
}
