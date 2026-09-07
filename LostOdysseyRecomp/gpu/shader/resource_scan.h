#pragma once

#include "cache.h"
#include "resource_index.h"
#include "resource_cpx_index.h"
#include "resource_cpx_index_sha256.h"
#include "resource_fpi.h"
#include "cpx_decode.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <set>
#include <sstream>
#include <unordered_map>
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
inline void SaveSource(const fs::path& source, bool pixel, std::span<const uint8_t> code) {
    const auto filename = SourceName(pixel, code);
    // Avoid rewriting a valid source, but repair interrupted or corrupted writes.
    std::ifstream old(source/filename, std::ios::binary | std::ios::ate);
    if (old && old.tellg() == std::streamoff(code.size())) {
        std::vector<uint8_t> existing(code.size()); old.seekg(0);
        if (old.read(reinterpret_cast<char*>(existing.data()), existing.size()) &&
            std::equal(existing.begin(), existing.end(), code.begin())) return;
    }
    old.close();
    std::ofstream out(source/filename, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(code.data()), code.size()); out.close();
    if (!out) throw std::runtime_error("cannot write shader source");
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
    size_t cpxPackages = 0, decodedPackages = 0;
    uint64_t decodedBytes = 0;
    uint64_t cacheBytesRead = 0;
    size_t indexedPackages = 0, fallbackPackages = 0, duplicatePackages = 0;
};
// Package identity is checked by the caller against the entire encoded payload.
// Decode only independently coded blocks that intersect indexed shader bytes.
// Prepare and verify every entry before publishing any source from this package.
inline bool ExtractCpxIndexed(std::span<const uint8_t> encoded, const CpxIndexPackage& profile,
                              const fs::path& source, std::set<std::string>& names,
                              uint64_t& decodedBytes) {
    std::vector<uint8_t> decoded;
    try {
        cpx::Header header;
        if (!cpx::ReadHeader(encoded,header) || header.storedSize!=encoded.size() ||
            profile.storedSize!=encoded.size() || profile.decodedSize!=header.decodedSize ||
            header.headerSize>encoded.size() || ReadLE(encoded.data()+16)!=header.headerSize) return false;
        for (size_t i=0;i<header.blockCount;++i) {
            const size_t begin=ReadLE(encoded.data()+16+i*4);
            const size_t end=i+1<header.blockCount?ReadLE(encoded.data()+20+i*4):encoded.size();
            if (begin<header.headerSize || begin>=end || end>encoded.size() || end-begin<4) return false;
            const auto* block=encoded.data()+begin;
            const size_t count=std::min(cpx::kBlockSize,size_t(header.decodedSize)-i*cpx::kBlockSize);
            if ((size_t(block[2])|(size_t(block[3])<<8))+1!=count ||
                (block[0]==255 && (block[1]!=0 || end-begin-4<count))) return false;
        }
        std::vector<bool> needed(header.blockCount,false);
        for (const auto& entry:profile.entries) {
            if (entry.size<12 || entry.size>262144 || entry.size%12 || entry.offset>header.decodedSize ||
                entry.size>header.decodedSize-entry.offset) return false;
            for (size_t i=entry.offset/cpx::kBlockSize;i<=(entry.offset+entry.size-1)/cpx::kBlockSize;++i) needed[i]=true;
        }
        // Empty known packages require no decoded allocation or block execution.
        if (profile.entries.empty()) return true;
        decoded.resize(header.decodedSize);
        for (size_t i=0;i<header.blockCount;++i) {
            if (!needed[i]) continue;
            const size_t begin=ReadLE(encoded.data()+16+i*4);
            const size_t end=i+1<header.blockCount?ReadLE(encoded.data()+20+i*4):encoded.size();
            const size_t count=std::min(cpx::kBlockSize,size_t(header.decodedSize)-i*cpx::kBlockSize);
            cpx::detail::DecodeBlock(encoded.subspan(begin,end-begin),header.bitWidth,
                std::span(decoded).subspan(i*cpx::kBlockSize,count));
            decodedBytes+=count;
        }
        for (const auto& entry:profile.entries)
            if (Hash(std::span(decoded).subspan(entry.offset,entry.size))!=entry.hash) return false;
    } catch (const std::exception&) { return false; }
    for (const auto& entry:profile.entries) {
        const auto code=std::span(decoded).subspan(entry.offset,entry.size);
        const auto name=SourceName(entry.pixel,code);
        if (!names.contains(name)) { SaveSource(source,entry.pixel,code); names.insert(name); }
    }
    return true;
}
// Full digest of the small layout file, independent of archive payload size.
inline std::string FpiDigest(const fs::path& file, uint64_t& bytesRead) {
    std::ifstream in(file,std::ios::binary|std::ios::ate);
    const auto size=in.tellg();
    if (size<64 || size>1024*1024) throw std::runtime_error("invalid FPI digest size");
    std::vector<uint8_t> bytes(static_cast<size_t>(size)); in.seekg(0);
    if (!in.read(reinterpret_cast<char*>(bytes.data()),size)) throw std::runtime_error("short FPI digest read");
    bytesRead+=bytes.size(); return Sha256Hex(Sha256(bytes));
}
// Read only the known shader blocks. The layout binding is established separately
// using the full small FPI digest, archive name/size and exact extent location.
// Unread blocks (including known empty packages) are deliberately not validated.
inline bool ExtractCpxSparse(std::ifstream& in, uint64_t base, const CpxIndexPackage& profile,
                             const fs::path& source, std::set<std::string>& names,
                             uint64_t& bytesRead, uint64_t& decodedBytes) {
    if (profile.entries.empty()) return true;
    std::vector<std::vector<uint8_t>> blocks;
    std::vector<std::pair<bool,std::vector<uint8_t>>> prepared;
    try {
        auto read=[&](uint64_t offset,std::span<uint8_t> bytes) {
            if (offset>profile.storedSize || bytes.size()>profile.storedSize-offset)
                throw std::runtime_error("sparse CPX range");
            in.clear(); in.seekg(base+offset);
            in.read(reinterpret_cast<char*>(bytes.data()),bytes.size());
            bytesRead+=uint64_t(in.gcount());
            if (!in) throw std::runtime_error("short sparse CPX read");
        };
        std::vector<uint8_t> headerBytes(16); read(0,headerBytes);
        cpx::Header header;
        if (!cpx::ReadHeader(headerBytes,header) || header.storedSize!=profile.storedSize ||
            header.decodedSize!=profile.decodedSize || header.headerSize>profile.storedSize) return false;
        headerBytes.resize(header.headerSize); read(16,std::span(headerBytes).subspan(16));
        if (ReadLE(headerBytes.data()+16)!=header.headerSize) return false;
        blocks.resize(header.blockCount);
        std::vector<bool> needed(header.blockCount,false);
        for (const auto& entry:profile.entries) {
            if (entry.size<12 || entry.size>262144 || entry.size%12 || entry.offset>header.decodedSize ||
                entry.size>header.decodedSize-entry.offset) return false;
            for (size_t i=entry.offset/cpx::kBlockSize;i<=(entry.offset+entry.size-1)/cpx::kBlockSize;++i) needed[i]=true;
        }
        for (size_t i=0;i<header.blockCount;++i) {
            const size_t begin=ReadLE(headerBytes.data()+16+i*4);
            const size_t end=i+1<header.blockCount?ReadLE(headerBytes.data()+20+i*4):profile.storedSize;
            if (begin<header.headerSize || begin>=end || end>profile.storedSize || end-begin<4) return false;
            if (!needed[i]) continue;
            std::vector<uint8_t> encoded(end-begin); read(begin,encoded);
            const size_t count=std::min(cpx::kBlockSize,size_t(header.decodedSize)-i*cpx::kBlockSize);
            if ((size_t(encoded[2])|(size_t(encoded[3])<<8))+1!=count ||
                (encoded[0]==255 && (encoded[1]!=0 || encoded.size()-4<count))) return false;
            blocks[i].resize(count);
            cpx::detail::DecodeBlock(encoded,header.bitWidth,blocks[i]); decodedBytes+=count;
        }
        for (const auto& entry:profile.entries) {
            std::vector<uint8_t> code(entry.size);
            size_t copied=0;
            while (copied<code.size()) {
                const size_t offset=size_t(entry.offset)+copied, block=offset/cpx::kBlockSize, start=offset%cpx::kBlockSize;
                const size_t count=std::min(code.size()-copied,blocks[block].size()-start);
                std::copy_n(blocks[block].data()+start,count,code.data()+copied); copied+=count;
            }
            if (Hash(code)!=entry.hash) return false;
            prepared.emplace_back(entry.pixel,std::move(code));
        }
    } catch (const std::exception&) { return false; }
    for (const auto& [pixel,code]:prepared) {
        const auto name=SourceName(pixel,code);
        if (!names.contains(name)) { SaveSource(source,pixel,code); names.insert(name); }
    }
    return true;
}
inline std::vector<IndexEntry> ContainerLocations(std::span<const uint8_t> data) {
    std::vector<IndexEntry> entries;
    std::set<std::string> names;
    for (size_t i=0;i+36<=data.size();++i) {
        if (data[i]!=0x10 || data[i+1]!=0x2a || data[i+2]!=0x11) continue;
        bool pixel=false;
        const auto code=Microcode(data.subspan(i),pixel);
        if (!code.empty() && names.insert(SourceName(pixel,code)).second)
            entries.push_back({uint64_t(code.data()-data.data()),uint32_t(code.size()),Hash(code),pixel});
    }
    return entries;
}
inline void ExtractContainers(std::span<const uint8_t> data, size_t core, const fs::path& source,
                              std::set<std::string>& names) {
    for (size_t i=0; i<core && i+36<=data.size(); ++i) {
        if (data[i]!=0x10 || data[i+1]!=0x2a || data[i+2]!=0x11) continue;
        bool pixel=false;
        auto code=Microcode(data.subspan(i),pixel);
        if (!code.empty() && names.insert(SourceName(pixel,code)).second) SaveSource(source,pixel,code);
    }
}
inline Result Scan(const fs::path& root, const fs::path& cacheDir,
                   const std::function<void(const ScanProgress&)>& progress,
                   std::span<const IndexFile> index = builtin::files,
                   std::span<const CpxIndexPackage> cpxIndex = builtin::cpxPackages,
                   std::span<const CpxIndexArchive> archiveIndex = builtin::cpxArchives,
                   bool strict = false) {
    Result result;
    // Built-in location indices refer only to the built-in package array.
    if (archiveIndex.data()==std::span<const CpxIndexArchive>(builtin::cpxArchives).data() &&
        cpxIndex.data()!=std::span<const CpxIndexPackage>(builtin::cpxPackages).data()) archiveIndex={};
    try {
        std::vector<fs::path> roots{root}, files, indexes;
        const auto name = root.filename().string();
        // Development four-disc layout. A standalone --game directory scans itself only.
        if (name == "disc1" || name == "disc2" || name == "disc3" || name == "disc4") {
            roots.clear();
            for (int i=1; i<=4; ++i) {
                auto p = root.parent_path() / ("disc" + std::to_string(i));
                if (fs::is_directory(p)) roots.push_back(p);
            }
        }
        for (const auto& dir : roots) {
            if (fs::is_regular_file(dir/"LO.fpi")) indexes.push_back(dir/"LO.fpi");
            for (const auto& entry : fs::directory_iterator(dir))
                if (entry.is_regular_file() && entry.path().extension() == ".fpd") files.push_back(entry.path());
        }
        std::sort(files.begin(), files.end());
        if (files.empty()) { result.error = "no FPD game resources found"; return result; }
        std::ostringstream identity;
        identity << (strict ? "resource-scanner-v5-cpx-strict\n" : "resource-scanner-v5-cpx-direct\n");
        uint64_t totalBytes = 0;
        for (const auto& file : files) {
            const auto size = fs::file_size(file); totalBytes += size;
            identity << fs::absolute(file).generic_string() << '\t' << size << '\t'
                     << fs::last_write_time(file).time_since_epoch().count() << '\n';
        }
        // FPI changes also invalidate the discovery cache. Like FPD identity,
        // this is a size/mtime fingerprint, not a full game-integrity digest.
        for (const auto& file : indexes)
            identity << fs::absolute(file).generic_string() << '\t' << fs::file_size(file) << '\t'
                     << fs::last_write_time(file).time_since_epoch().count() << '\n';
        const auto source = cacheDir / "source";
        fs::create_directories(source);
        const auto manifest = cacheDir / "resources.manifest";
        const std::string prefix = identity.str() + "--sources--\n";
        std::ifstream old(manifest, std::ios::binary);
        std::string contents((std::istreambuf_iterator<char>(old)), {});
        old.close();
        if (!strict && contents.starts_with(prefix)) {
            const auto body=contents.substr(prefix.size());
            const auto footer=body.rfind("--complete--\t");
            const auto sourceList=body.substr(0,footer);
            const auto digest=Sha256Hex(Sha256(std::span(reinterpret_cast<const uint8_t*>(sourceList.data()),sourceList.size())));
            const bool complete=footer!=std::string::npos && body.substr(footer)=="--complete--\t"+digest+"\n";
            std::istringstream lines(sourceList);
            std::string filename; bool valid = true;
            const auto sourceCount=std::count(sourceList.begin(),sourceList.end(),'\n');
            progress({ScanStage::CacheValidation,0,uint64_t(sourceCount),ScanUnit::Files});
            if (!complete) valid=false;
            while (valid && std::getline(lines, filename)) {
                if (filename.size()!=23 || (!filename.starts_with("ps_") && !filename.starts_with("vs_")) ||
                    !filename.ends_with(".bin") || filename.find_first_not_of("0123456789abcdef",3) != 19) { valid=false; break; }
                std::ifstream in(source / filename, std::ios::binary | std::ios::ate);
                const auto size=in.tellg();
                if (size < 12 || size > 262144 || size % 12) { valid=false; break; }
                std::vector<uint8_t> bytes(static_cast<size_t>(size)); in.seekg(0);
                in.read(reinterpret_cast<char*>(bytes.data()), size);
                result.cacheBytesRead+=uint64_t(in.gcount());
                if (!in || SourceName(filename.starts_with("ps_"), bytes)!=filename) { valid=false; break; }
                ++result.shaders;
                progress({ScanStage::CacheValidation,result.shaders,uint64_t(sourceCount),ScanUnit::Files});
            }
            if (valid && result.shaders) { result.reused=true; return result; }
            result.shaders=0;
        }
        constexpr size_t chunk = 4*1024*1024, overlap = 65536+262144;
        std::vector<uint8_t> bytes(chunk+overlap);
        std::set<std::string> names;
        ResourceExtents extents;
        std::map<fs::path,std::string> fpiDigests;
        for (const auto& file : indexes) {
            if (!strict && !archiveIndex.empty()) fpiDigests.emplace(file.parent_path(),FpiDigest(file,result.bytesRead));
            auto parsed = ReadResourceExtents(file,&result.bytesRead);
            extents.merge(parsed);
        }
        std::set<Sha256Digest> seenPackages;
        std::unordered_map<std::string_view,const CpxIndexPackage*> packageIndex;
        for (const auto& profile:cpxIndex) packageIndex.emplace(profile.sha256,&profile);
        uint64_t completed=0;
        for (const auto& file : files) {
            const auto length=fs::file_size(file);
            if (ExtractIndexed(file, source, index, names, result.bytesRead)) {
                ++result.indexedFiles;
                completed += length;
                progress({ScanStage::IndexedExtraction,completed,totalBytes,ScanUnit::Bytes});
            } else {
                ++result.scannedFiles;
                std::ifstream in(file, std::ios::binary);
                if (!in) throw std::runtime_error("cannot read " + file.string());
                for (uint64_t base=0; base<length; base+=chunk) {
                    in.clear(); in.seekg(base);
                    const auto amount=std::min<uint64_t>(bytes.size(),length-base);
                    if (!in.read(reinterpret_cast<char*>(bytes.data()), amount)) throw std::runtime_error("short resource read");
                    result.bytesRead += amount;
                    const auto core=std::min<uint64_t>(chunk,length-base);
                    ExtractContainers(std::span<const uint8_t>(bytes.data(),amount),core,source,names);
                    progress({ScanStage::FallbackScan,completed+base+core,totalBytes,ScanUnit::Bytes});
                }
                completed+=length;
            }
        }
        // Known layouts use direct extent bindings. Strict mode and unknown layouts
        // retain full encoded identities and complete fallback discovery. A trusted
        // binding is not a certificate for same-size changes in unread content.
        size_t extentCount=0, extentDone=0;
        for (const auto& [file, records] : extents) extentCount+=records.size();
        std::vector<uint8_t> encoded, decoded;
        std::set<uint32_t> extractedProfiles;
        for (const auto& [file, records] : extents) {
            std::ifstream in(file,std::ios::binary);
            if (!in) throw std::runtime_error("cannot read CPX archive");
            const CpxIndexArchive* layout=nullptr;
            const auto fingerprint=fpiDigests.find(file.parent_path());
            if (!strict && fingerprint!=fpiDigests.end()) for (const auto& candidate:archiveIndex) {
                if (candidate.fpiSha256!=fingerprint->second || candidate.name!=file.filename().string() ||
                    candidate.size!=fs::file_size(file) || candidate.extents.size()!=records.size()) continue;
                bool matches=true;
                for (size_t i=0;i<records.size();++i)
                    if (candidate.extents[i].offset!=records[i].offset || candidate.extents[i].size!=records[i].size ||
                        (candidate.extents[i].package!=UINT32_MAX && candidate.extents[i].package>=cpxIndex.size())) {matches=false;break;}
                if (matches) {layout=&candidate;break;}
            }
            size_t recordIndex=0;
            for (const auto& record : records) {
                const auto binding=layout?&layout->extents[recordIndex]:nullptr;
                ++recordIndex;
                progress({ScanStage::IndexedExtraction,extentDone++,extentCount,ScanUnit::Entries});
                if (binding && binding->package==UINT32_MAX) continue;
                bool sparseFailed=false;
                if (binding) {
                    const auto& profile=cpxIndex[binding->package];
                    if (profile.storedSize==record.size) {
                        if (extractedProfiles.contains(binding->package)) {
                            ++result.cpxPackages; ++result.duplicatePackages; continue;
                        }
                        const auto before=result.decodedBytes;
                        if (ExtractCpxSparse(in,record.offset,profile,source,names,result.bytesRead,result.decodedBytes)) {
                            ++result.cpxPackages; ++result.indexedPackages;
                            if (before!=result.decodedBytes) ++result.decodedPackages;
                            extractedProfiles.insert(binding->package); continue;
                        }
                        sparseFailed=true;
                    }
                }
                if (record.size < 16) continue;
                uint8_t header[16]; in.clear(); in.seekg(record.offset);
                if (!in.read(reinterpret_cast<char*>(header),16)) throw std::runtime_error("short CPX header read");
                result.bytesRead+=16;
                if (header[0]!='c' || header[1]!='p' || header[2]!='x') {
                    if (binding) {
                        // The expected package was replaced with a naked resource.
                        // Scan the complete bounded extent instead of accepting it as empty.
                        if (record.size>cpx::kMaxStoredSize) throw std::runtime_error("invalid replaced CPX size");
                        encoded.resize(record.size); std::copy(std::begin(header),std::end(header),encoded.begin());
                        if (!in.read(reinterpret_cast<char*>(encoded.data()+16),encoded.size()-16))
                            throw std::runtime_error("short replaced CPX read");
                        result.bytesRead+=encoded.size()-16; ++result.fallbackPackages;
                        ExtractContainers(encoded,encoded.size(),source,names);
                    }
                    continue;
                }
                ++result.cpxPackages;
                if (record.size > 128u*1024*1024 || ReadLE(header+8)!=record.size ||
                    ReadLE(header+12)>cpx::kMaxDecodedSize) throw std::runtime_error("invalid CPX extent size");
                encoded.resize(record.size); std::copy(std::begin(header),std::end(header),encoded.begin());
                if (!in.read(reinterpret_cast<char*>(encoded.data()+16),encoded.size()-16)) throw std::runtime_error("short CPX payload read");
                result.bytesRead+=encoded.size()-16;
                const auto digest=Sha256(encoded);
                if (seenPackages.contains(digest)) { ++result.duplicatePackages; continue; }
                const auto matched=packageIndex.find(Sha256Hex(digest));
                const auto beforeDecoded=result.decodedBytes;
                if (!sparseFailed && matched!=packageIndex.end() && ExtractCpxIndexed(encoded,*matched->second,source,names,result.decodedBytes)) {
                    ++result.indexedPackages;
                } else {
                    ++result.fallbackPackages;
                    progress({ScanStage::FallbackScan,extentDone-1,extentCount,ScanUnit::Entries});
                    if (!cpx::Decode(encoded,decoded)) throw std::runtime_error("invalid CPX block stream in " + file.filename().string());
                    result.decodedBytes+=decoded.size();
                    ExtractContainers(decoded,decoded.size(),source,names);
                }
                if (beforeDecoded!=result.decodedBytes) ++result.decodedPackages;
                seenPackages.insert(digest);
            }
        }
        progress({ScanStage::IndexedExtraction,extentCount,extentCount,ScanUnit::Entries});
        result.shaders=names.size();
        if (names.empty()) { result.error="no supported shader containers found"; return result; }
        // Never mark an interrupted extraction complete. Sources are revalidated on reuse.
        const auto temp=cacheDir / "resources.manifest.tmp";
        std::ofstream out(temp,std::ios::binary | std::ios::trunc); out << prefix;
        std::string sourceList;
        for (const auto& filename : names) sourceList+=filename+'\n';
        out << sourceList << "--complete--\t" << Sha256Hex(Sha256(
            std::span(reinterpret_cast<const uint8_t*>(sourceList.data()),sourceList.size()))) << '\n';
        out.close(); if (!out) throw std::runtime_error("cannot write resource manifest");
        std::error_code ec; fs::remove(manifest,ec); fs::rename(temp,manifest);
    } catch (const std::exception& e) { result.error=e.what(); }
    return result;
}
}
