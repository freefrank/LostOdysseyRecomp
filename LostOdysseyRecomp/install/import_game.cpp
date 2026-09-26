#if defined(LO_HAS_PCH) || defined(__has_include)
#if __has_include(<stdafx.h>) && defined(LO_RUNTIME_BUILD)
#include <stdafx.h>
#endif
#endif
#include "import_game.h"
#include "import_image.h"
#include "import_crypto.h"
#include "../os/user_paths.h"
#include "../settings/game_path.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <exception>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include "../../tools/XenonRecomp/thirdparty/tomlplusplus/vendor/json.hpp"
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <sstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace install
{
namespace
{
constexpr uint32_t MAX_DISCOVERY_DEPTH = 8;
constexpr uint32_t MAX_DISCOVERY_ENTRIES = 10000;
constexpr size_t MAX_CANDIDATES = 256;

std::string ProcessIdString()
{
#ifdef _WIN32
    return std::to_string(GetCurrentProcessId());
#else
    return std::to_string(static_cast<unsigned long long>(getpid()));
#endif
}

#ifdef LO_IMPORT_TESTING
std::map<uint32_t, std::string> g_testSha256Asia;
std::map<uint32_t, std::string> g_testSha256Europe;
std::map<uint32_t, std::string> g_testMd5Asia;
std::map<uint32_t, std::string> g_testMd5Europe;
std::string g_testDlcFailureFile;
std::string g_testDlcFailureStage;
std::string g_testDiscFailureFile;
std::string g_testDiscFailureStage;
std::string g_testPublishFailureSlot;
std::string g_testPublishFailureStage;
#endif

std::string ToLower(std::string_view str);

// Compare complete path components (a sibling such as disc10 is not disc1).
bool ContainsPath(const std::filesystem::path& parent, const std::filesystem::path& child)
{
    auto a = parent.begin(), b = child.begin();
    for (; a != parent.end() && b != child.end(); ++a, ++b)
    {
#ifdef _WIN32
        if (ToLower(a->string()) != ToLower(b->string())) return false;
#else
        if (*a != *b) return false;
#endif
    }
    return a == parent.end();
}

void RenameSlot(const std::filesystem::path& from, const std::filesystem::path& to,
                std::string_view slot, std::string_view stage)
{
#ifdef LO_IMPORT_TESTING
    if (slot == g_testPublishFailureSlot && stage == g_testPublishFailureStage)
        throw Error("Injected " + std::string(stage) + " failure: " + from.string());
#endif
    std::error_code ec;
    std::filesystem::rename(from, to, ec);
    if (ec) throw Error("Could not " + std::string(stage) + " " + from.string() + " -> " + to.string() + ": " + ec.message());
}

void CheckDiscOutput(std::ofstream& out, const std::filesystem::path& path, std::string_view stage)
{
#ifdef LO_IMPORT_TESTING
    if (path.filename().string() == g_testDiscFailureFile && stage == g_testDiscFailureStage)
        out.setstate(std::ios::badbit);
#endif
    if (!out) throw Error("Disc " + std::string(stage) + " failed: " + path.string());
}

void FinishDiscOutput(std::ofstream& out, const std::filesystem::path& path)
{
    CheckDiscOutput(out, path, "write");
    out.flush();
    CheckDiscOutput(out, path, "flush");
    out.close();
    CheckDiscOutput(out, path, "close");
}

void CheckDlcOutput(std::ofstream& out, const std::filesystem::path& path, std::string_view stage)
{
#ifdef LO_IMPORT_TESTING
    if (path.filename().string() == g_testDlcFailureFile && stage == g_testDlcFailureStage)
        out.setstate(std::ios::badbit);
#endif
    if (!out) throw Error("DLC " + std::string(stage) + " failed: " + path.string());
}

void FinishDlcOutput(std::ofstream& out, const std::filesystem::path& path)
{
    CheckDlcOutput(out, path, "write");
    out.flush();
    CheckDlcOutput(out, path, "flush");
    out.close();
    CheckDlcOutput(out, path, "close");
}

std::string ToLower(std::string_view str)
{
    std::string result(str);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

std::string ToUpper(std::string_view str)
{
    std::string result(str);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return result;
}

bool IsAsciiSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

std::string Trim(std::string value)
{
    if (value.size() >= 3 && static_cast<unsigned char>(value[0]) == 0xef &&
        static_cast<unsigned char>(value[1]) == 0xbb &&
        static_cast<unsigned char>(value[2]) == 0xbf)
    {
        value.erase(0, 3);
    }
    size_t begin = 0;
    while (begin < value.size() && IsAsciiSpace(value[begin]))
        ++begin;
    size_t end = value.size();
    while (end > begin && IsAsciiSpace(value[end - 1]))
        --end;
    return value.substr(begin, end - begin);
}

bool IsSymlinkOrReparse(const std::filesystem::path& path)
{
    std::error_code ec;
    if (std::filesystem::is_symlink(path, ec))
        return true;
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data))
    {
        if (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
            return true;
    }
#endif
    return false;
}

std::string Sha256Bytes(const void* data, size_t length)
{
    return crypto::Sha256Hex(data, length);
}

std::string Md5Bytes(const void* data, size_t length)
{
    return crypto::Md5Hex(data, length);
}

struct ExecutionInfo
{
    std::string title;
    std::string media;
    uint32_t version = 0;
    uint32_t base = 0;
    uint32_t disc = 0;
    uint32_t discs = 0;
};

ExecutionInfo ParseExecution(std::span<const uint8_t> data)
{
    if (data.size() < 24 || std::memcmp(data.data(), "XEX2", 4) != 0)
        throw Error("Invalid default.xex");

    auto readBeU32 = [&](size_t offset) -> uint32_t {
        return (static_cast<uint32_t>(data[offset]) << 24) |
               (static_cast<uint32_t>(data[offset + 1]) << 16) |
               (static_cast<uint32_t>(data[offset + 2]) << 8) |
               static_cast<uint32_t>(data[offset + 3]);
    };

    uint32_t count = readBeU32(20);
    if (count > 1024 || 24 + count * 8 > data.size())
        throw Error("Invalid XEX optional headers");

    size_t execOffset = static_cast<size_t>(-1);
    for (uint32_t i = 0; i < count; ++i)
    {
        uint32_t key = readBeU32(24 + i * 8);
        uint32_t val = readBeU32(24 + i * 8 + 4);
        if (key == 0x40006)
        {
            execOffset = val;
            break;
        }
    }

    if (execOffset == static_cast<size_t>(-1) || execOffset + 24 > data.size())
        throw Error("Missing XEX execution information");

    uint32_t media = readBeU32(execOffset);
    uint32_t version = readBeU32(execOffset + 4);
    uint32_t base = readBeU32(execOffset + 8);
    uint32_t title = readBeU32(execOffset + 12);
    uint8_t disc = data[execOffset + 18];
    uint8_t discs = data[execOffset + 19];

    char titleBuf[16]{};
    char mediaBuf[16]{};
    std::snprintf(titleBuf, sizeof(titleBuf), "%08X", title);
    std::snprintf(mediaBuf, sizeof(mediaBuf), "%08X", media);

    return {titleBuf, mediaBuf, version, base, disc, discs};
}

struct EditionEntry
{
    const char* id;
    const char* label;
    uint32_t version;
    uint32_t base;
    std::array<const char*, 5> media;
    std::array<const char*, 5> sha256;
};

constexpr EditionEntry EDITIONS_DATA[] = {
    {
        "asia",
        "Europe / Asia",
        4,
        4,
        {"", "39F7D748", "0EF8CEA8", "309E3386", "7B21A91D"},
        {"",
         "40c7dbb12cca03921d52cf4177a0f700cc4ae94ab730ab594940e2bdffd8ecf2",
         "a42a46b211b22e923ccaed7a313e59083adbdbfbbc62d04cb7c6848fd9717ec7",
         "0d7965a11fb9d102856e26c7fb462b888cb57a1fedc474378b9ae2cceccf6570",
         "893914d1bf334f06508b5d54fa20004ee642a53ffcc6137500c10655c3831916"}
    },
    {
        "usa-europe",
        "USA / Europe",
        3,
        3,
        {"", "368DE6DD", "1888BE4E", "6DD59D08", "0C0E80B5"},
        {"",
         "175ae53d109d480a83bebbd186e7b6871f7b03ce80af69ab388db2f747640de3",
         "1d8a78379349e4583957d34148d5dbf8a091955edb6c6877bc24bdc9c7b87541",
         "dd323967d7f4b99b48c00aa6a15a643c96df525e539669e9be49a876513b86f6",
         "9204ba8b91836853ae1e9f0dc49090abd5e63709935599c5551c23b28ecf48d4"}
    }
};

std::string GetExpectedSha256(const EditionEntry& ed, uint32_t disc)
{
    if (disc >= 1 && disc <= 4)
    {
#ifdef LO_IMPORT_TESTING
        bool isEu = (std::string_view(ed.id) == "usa-europe");
        const auto& overrideMap = isEu ? g_testSha256Europe : g_testSha256Asia;
        auto it = overrideMap.find(disc);
        if (it != overrideMap.end())
            return it->second;
#endif
        return ed.sha256[disc];
    }
    return {};
}

std::string GetExpectedMd5(const EditionEntry& ed, uint32_t disc)
{
#ifdef LO_IMPORT_TESTING
    if (disc >= 1 && disc <= 4)
    {
        bool isEu = (std::string_view(ed.id) == "usa-europe");
        const auto& overrideMap = isEu ? g_testMd5Europe : g_testMd5Asia;
        auto it = overrideMap.find(disc);
        if (it != overrideMap.end())
            return it->second;
    }
#endif
    return {};
}

std::pair<std::string, std::string> IdentifyDisc(const ExecutionInfo& info,
                                                 const std::string& sha256,
                                                 const std::string& md5)
{
    auto metadataMatches = [&](const EditionEntry& ed) {
        if (info.disc < 1 || info.disc > 4) return false;
        return ed.media[info.disc] == info.media &&
               info.version == ed.version &&
               info.base == ed.base;
    };

    for (const auto& ed : EDITIONS_DATA)
    {
        std::string expSha = GetExpectedSha256(ed, info.disc);
        if (!expSha.empty() && !sha256.empty() && expSha == sha256)
        {
            if (!metadataMatches(ed))
                throw Error("XEX hash and execution metadata identify different disc builds");
            return {ed.id, "sha256"};
        }

        std::string expMd5 = GetExpectedMd5(ed, info.disc);
        if (!expMd5.empty() && !md5.empty() && expMd5 == md5)
        {
            if (!metadataMatches(ed))
                throw Error("XEX hash and execution metadata identify different disc builds");
            return {ed.id, "md5"};
        }
    }
    return {"unknown", "none"};
}

std::string MetadataEdition(const ExecutionInfo& info)
{
    for (const auto& ed : EDITIONS_DATA)
    {
        if (info.disc >= 1 && info.disc <= 4)
        {
            if (ed.media[info.disc] == info.media &&
                info.version == ed.version &&
                info.base == ed.base)
            {
                return ed.id;
            }
        }
    }
    return {};
}

const EditionEntry* FindEditionEntry(std::string_view id)
{
    for (const auto& ed : EDITIONS_DATA)
    {
        if (ed.id == id)
            return &ed;
    }
    return nullptr;
}

// PrepareDisc parses and authenticates any source kind (Folder, ISO, GOD)
DiscInfo PrepareDisc(const std::filesystem::path& path,
                    std::optional<Kind> explicitKind,
                    std::vector<Entry>& outEntries,
                    bool validate,
                    const Cancelled& cancelled = {},
                    std::unique_ptr<ImageReader>* retainedReader = nullptr)
{
    if (cancelled && cancelled())
        throw Error("Source check cancelled", true);

    if (IsSymlinkOrReparse(path))
        throw Error("Links are not supported as game sources");

    Kind kind = Kind::Folder;
    if (explicitKind.has_value())
    {
        kind = *explicitKind;
    }
    else
    {
        std::string ext = ToLower(path.extension().string());
        if (ext == ".iso") kind = Kind::Iso;
        else if (ext == ".data") kind = Kind::God;
        else kind = Kind::Folder;
    }

    std::unique_ptr<ImageReader> imageReader;
    if (kind == Kind::Iso)
    {
        imageReader = std::make_unique<IsoImageReader>(path, cancelled);
        outEntries = imageReader->GetEntries();
    }
    else if (kind == Kind::God)
    {
        imageReader = std::make_unique<GodImageReader>(path, cancelled);
        outEntries = imageReader->GetEntries();
    }
    else
    {
        FolderSource folder(path);
        outEntries = folder.GetEntries(cancelled);
    }

    const Entry* xexEntry = nullptr;
    for (const auto& e : outEntries)
    {
        if (ToLower(e.name) == "default.xex")
        {
            xexEntry = &e;
            break;
        }
    }

    if (!xexEntry || xexEntry->size < 24 || xexEntry->size > 32 * 1024 * 1024)
        throw Error("Missing or invalid default.xex in game root");

    std::vector<uint8_t> xexBytes(static_cast<size_t>(xexEntry->size));
    if (imageReader)
    {
        imageReader->Read(xexEntry->offset, xexBytes.data(), xexBytes.size());
    }
    else
    {
        std::filesystem::path xexPath = path / xexEntry->name;
        std::ifstream xexFile(xexPath, std::ios::binary);
        if (!xexFile) throw Error("Could not read default.xex");
        xexFile.read(reinterpret_cast<char*>(xexBytes.data()), static_cast<std::streamsize>(xexBytes.size()));
        if (static_cast<size_t>(xexFile.gcount()) != xexBytes.size())
            throw Error("Truncated default.xex read");
    }

    ExecutionInfo exec = ParseExecution(xexBytes);
    std::string sha256 = Sha256Bytes(xexBytes.data(), xexBytes.size());
    std::string md5 = Md5Bytes(xexBytes.data(), xexBytes.size());
    auto [edition, identity] = IdentifyDisc(exec, sha256, md5);
    std::string metaEd = MetadataEdition(exec);

    if (validate)
    {
        if (exec.title != "4D5307FA")
            throw Error("Wrong game: Title ID " + exec.title + " (expected 4D5307FA)");
        if (exec.disc < 1 || exec.disc > 4 || exec.discs != 4)
            throw Error("Unsupported disc set: disc " + std::to_string(exec.disc) +
                        " of " + std::to_string(exec.discs) + " (expected 1-4 of 4)");
        if (edition == "unknown")
        {
            std::string resembles;
            if (!metaEd.empty())
            {
                const auto* ed = FindEditionEntry(metaEd);
                resembles = std::string(" Metadata resembles ") + (ed ? ed->label : metaEd.c_str()) +
                            ", but metadata alone cannot prove compatible guest code.";
            }
            throw Error("Unrecognized Lost Odyssey XEX: Media ID " + exec.media +
                        ", version " + std::to_string(exec.version) +
                        ", base " + std::to_string(exec.base) + "; MD5 " + md5 +
                        "; SHA256 " + sha256 + "." + resembles);
        }

        // Required disc files
        static const char* requiredNames[] = {
            "lo.fpd", "lo.fpi", "xenon_battle.fpd", "xenon_chr.fpd", "xenon_event.fpd",
            "xenon_field.fpd", "xenon_loc.fpd", "xenon_mov.fpd", "xenon_obj.fpd",
            "xenon_scr.fpd", "xenon_snd.fpd", "xenon_sys.fpd", "xenon_vfx.fpd", "xenon_world.fpd"
        };
        std::set<std::string> fileNames;
        for (const auto& e : outEntries)
            fileNames.insert(ToLower(e.name));

        for (const char* req : requiredNames)
        {
            if (fileNames.find(req) == fileNames.end())
                throw Error("Incomplete game folder: select the full disc, not only the XEX");
        }
    }

    uint64_t totalBytes = 0;
    for (const auto& e : outEntries) totalBytes += e.size;

    DiscInfo info;
    info.path = path;
    info.kind = kind;
    info.files = static_cast<uint32_t>(outEntries.size());
    info.bytes = totalBytes;
    info.disc = exec.disc;
    info.discs = exec.discs;
    info.version = exec.version;
    info.base = exec.base;
    info.title = exec.title;
    info.media = exec.media;
    info.edition = edition;
    info.identity = identity;
    info.metadataEdition = metaEd;
    info.sha256 = sha256;
    info.md5 = md5;
    if (retainedReader)
        *retainedReader = std::move(imageReader);
    return info;
}

// ----------------------------------------------------------------------------
// Discovery: Folder, default.xex, ISO, and GOD containers
// ----------------------------------------------------------------------------
struct DiscoveredSources
{
    // path -> (Format: Folder, ISO, GOD)
    std::vector<std::pair<std::filesystem::path, Kind>> candidates;
    std::vector<std::filesystem::path> packages;
    std::vector<std::filesystem::path> extractedPackages;
    std::vector<std::pair<std::filesystem::path, std::string>> rejected;
};

using json = nlohmann::json;
struct ExtractedDlcFile { std::string path; uint64_t size = 0; std::string sha256; };
struct ExtractedDlc
{
    DlcPackageInfo info;
    std::vector<ExtractedDlcFile> files;
    std::array<std::vector<uint8_t>, 3> sidecars;
};
constexpr std::array<const char*, 3> DlcSidecars{".lo-content", ".lo-dlc-header", ".lo-dlc.json"};

bool IsHexSha256(std::string_view value)
{
    if (value.size() != 64) return false;
    return std::all_of(value.begin(), value.end(), [](unsigned char c) { return std::isxdigit(c) != 0; });
}

ExtractedDlc ReadExtractedDlc(const std::filesystem::path& dir, const Cancelled& cancelled = {})
{
    auto checkCancelled = [&]() {
        if (cancelled && cancelled()) throw Error("DLC check cancelled", true);
    };
    checkCancelled();
    for (auto ancestor = std::filesystem::absolute(dir); !ancestor.empty(); ancestor = ancestor.parent_path())
    {
        if (IsSymlinkOrReparse(ancestor)) throw Error("Links are not supported as DLC sources");
        if (ancestor == ancestor.parent_path()) break;
    }
    ExtractedDlc package;
    for (size_t i = 0; i < DlcSidecars.size(); ++i)
    {
        auto path = dir / DlcSidecars[i];
        if (IsSymlinkOrReparse(path) || !std::filesystem::is_regular_file(path) || std::filesystem::file_size(path) > 1024 * 1024)
            throw Error("Missing or invalid extracted DLC sidecar");
        std::ifstream input(path, std::ios::binary);
        package.sidecars[i] = std::vector<uint8_t>(std::istreambuf_iterator<char>(input), {});
        if (!input.eof() && input.fail()) throw Error("Could not read extracted DLC sidecar");
    }
    const auto& manifest = package.sidecars[2];
    json data = json::parse(manifest.begin(), manifest.end(), nullptr, false);
    if (data.is_discarded() || !data.is_object()) throw Error("Invalid extracted DLC manifest");
    if (data.value("schema", 0) != 1 || data.value("title_id", "") != "4D5307FA") throw Error("Wrong extracted DLC metadata");
    auto contentId = data.value("content_id", "");
    auto sourceSha = data.value("source_sha256", "");
    if (contentId.size() != 40 || !std::all_of(contentId.begin(), contentId.end(), [](unsigned char c) { return std::isxdigit(c); }) || !IsHexSha256(sourceSha)) throw Error("Invalid extracted DLC identity");
    auto& info = package.info;
    info.path = dir; info.contentId = ToUpper(contentId); info.sourceSha256 = ToLower(sourceSha); info.format = "extracted";
    info.extractedManifestSha256 = crypto::Sha256Hex(manifest.data(), manifest.size());
    info.displayName = data.value("display_name", "");
    info.licenseMask = data.value("license_mask", uint32_t(0));
    const auto& header = package.sidecars[1];
    const auto& record = package.sidecars[0];
    auto ReadBeU32 = [](const uint8_t* p) -> uint32_t {
        return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
    };
    if (header.size() < 0x3ad || (std::memcmp(header.data(), "CON ", 4) && std::memcmp(header.data(), "LIVE", 4) && std::memcmp(header.data(), "PIRS", 4)) ||
        ReadBeU32(header.data() + 0x360) != 0x4D5307FA || ReadBeU32(header.data() + 0x344) != 2 ||
        ToUpper(crypto::HexString(header.data() + 0x32c, 20)) != info.contentId ||
        record.size() != 308 || ReadBeU32(record.data()) != 1 || ReadBeU32(record.data() + 4) != 2 ||
        std::string(record.begin() + 264, record.begin() + 304) != info.contentId)
        throw Error("Extracted DLC sidecars identify different content");
    auto& files = package.files;
    std::set<std::string> names;
    for (const auto& item : data.value("files", json::array()))
    {
        if (!item.is_object()) throw Error("Invalid extracted DLC file entry");
        checkCancelled();
        auto name = item.value("path", ""); auto rel = std::filesystem::u8path(name);
        if (name.empty() || !names.insert(ToLower(name)).second || rel.has_root_path() || rel.lexically_normal() != rel || name.find_first_of("\\:") != std::string::npos || name.find('\0') != std::string::npos)
            throw Error("Unsafe extracted DLC manifest path");
        for (const auto& component : rel)
            if (component.empty() || component.string().front() == '.') throw Error("Unsafe extracted DLC path component");
        auto file = dir / rel;
        for (auto ancestor = file; !ancestor.empty(); ancestor = ancestor.parent_path())
        {
            checkCancelled();
            if (IsSymlinkOrReparse(ancestor)) throw Error("Links are not supported as DLC payloads");
            if (ancestor == ancestor.parent_path()) break;
        }
        auto expected = item.value("size", uint64_t(0)); auto hash = item.value("sha256", "");
        if (!IsHexSha256(hash)) throw Error("Invalid extracted DLC file hash");
        std::error_code ec;
        if (IsSymlinkOrReparse(file) || !std::filesystem::is_regular_file(file, ec) || std::filesystem::file_size(file, ec) != expected)
            throw Error("Extracted DLC file is missing or has the wrong size: " + name);
        std::ifstream stream(file, std::ios::binary); crypto::Sha256 sha; std::array<uint8_t, 4096> buffer{}; uint64_t read = 0;
        if (!stream) throw Error("Could not open extracted DLC payload: " + name);
        while (stream.read(reinterpret_cast<char*>(buffer.data()), buffer.size()) || stream.gcount()) { checkCancelled(); auto n = stream.gcount(); sha.Update(buffer.data(), static_cast<size_t>(n)); read += static_cast<uint64_t>(n); }
        if (read != expected || ToLower(crypto::HexString(sha.Finalize())) != ToLower(hash)) throw Error("Extracted DLC file hash mismatch: " + name);
        files.push_back({name, expected, ToLower(hash)}); info.bytes += expected;
    }
    if (files.empty()) throw Error("Extracted DLC manifest has no files");
    for (const auto& sidecar : DlcSidecars) names.insert(sidecar);
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir))
    {
        checkCancelled();
        if (IsSymlinkOrReparse(entry.path())) throw Error("Links are not supported as DLC payloads");
        if (entry.is_regular_file() && !names.contains(ToLower(entry.path().lexically_relative(dir).generic_string())))
            throw Error("Unlisted file in extracted DLC directory");
    }
    info.files = static_cast<uint32_t>(files.size()); return package;
}

DiscoveredSources DiscoverAllSources(const std::vector<std::filesystem::path>& paths,
                                    const Cancelled& cancelled = {})
{
    auto cancelCheck = [&]() {
        if (cancelled && cancelled())
            throw Error("Source check cancelled", true);
    };

    struct Task
    {
        std::filesystem::path path;
        uint32_t depth = 0;
        bool direct = true;
        bool insideDisc = false;
    };

    std::vector<Task> pending;
    for (const auto& p : paths)
    {
        std::error_code ec;
        pending.push_back({std::filesystem::absolute(p, ec).lexically_normal(), 0, true, false});
    }

    std::set<std::string> seen;
    std::map<std::filesystem::path, Kind> discs;
    std::set<std::filesystem::path> packages;
    std::vector<std::pair<std::filesystem::path, std::string>> rejected;

    size_t entriesCount = 0;

    while (!pending.empty())
    {
        cancelCheck();
        auto task = pending.back();
        pending.pop_back();

        std::string key = ToLower(task.path.generic_string());
        if (seen.count(key)) continue;
        seen.insert(key);

        if (IsSymlinkOrReparse(task.path))
        {
            if (task.direct)
                rejected.emplace_back(task.path, "Select a source without links or junctions");
            continue;
        }

        std::error_code ec;
        try
        {
            if (std::filesystem::is_directory(task.path, ec))
            {
                std::vector<std::filesystem::path> children;
                for (const auto& entry : std::filesystem::directory_iterator(task.path, ec))
                {
                    if (!ec) children.push_back(entry.path());
                }
                if (ec) continue;

                std::sort(children.begin(), children.end(), [](const auto& a, const auto& b) {
                    return ToLower(a.filename().string()) < ToLower(b.filename().string());
                });

                entriesCount += children.size();
                if (entriesCount > MAX_DISCOVERY_ENTRIES)
                    throw Error("Too many source entries; select a closer folder");

                bool hasDefaultXex = false;
                bool hasData0000 = false;
                bool hasDlcManifest = false;
                for (const auto& child : children)
                {
                    std::string lowerName = ToLower(child.filename().string());
                    if (lowerName == "default.xex") hasDefaultXex = true;
                    if (lowerName == "data0000") hasData0000 = true;
                    if (lowerName == ".lo-dlc.json") hasDlcManifest = true;
                }

                if (hasDlcManifest)
                {
                    packages.insert(task.path);
                    continue;
                }

                if (!task.insideDisc && hasDefaultXex)
                {
                    discs[task.path] = Kind::Folder;
                    task.insideDisc = true;
                }
                if (!task.insideDisc && hasData0000)
                {
                    discs[task.path] = Kind::God;
                    continue;
                }

                if (task.depth < MAX_DISCOVERY_DEPTH)
                {
                    for (auto it = children.rbegin(); it != children.rend(); ++it)
                    {
                        pending.push_back({*it, task.depth + 1, false, task.insideDisc});
                    }
                }
            }
            else if (std::filesystem::is_regular_file(task.path, ec))
            {
                std::ifstream stream(task.path, std::ios::binary);
                if (stream)
                {
                    std::vector<uint8_t> header(0x400);
                    stream.read(reinterpret_cast<char*>(header.data()), header.size());
                    size_t count = static_cast<size_t>(stream.gcount());

                    static const uint8_t CON_M[4] = {'C', 'O', 'N', ' '};
                    static const uint8_t LIVE_M[4] = {'L', 'I', 'V', 'E'};
                    static const uint8_t PIRS_M[4] = {'P', 'I', 'R', 'S'};
                    static const uint8_t XEX2_M[4] = {'X', 'E', 'X', '2'};

                    bool isStfs = (count >= 4 && (
                        std::memcmp(header.data(), CON_M, 4) == 0 ||
                        std::memcmp(header.data(), LIVE_M, 4) == 0 ||
                        std::memcmp(header.data(), PIRS_M, 4) == 0));

                    if (isStfs)
                    {
                        if (count < 0x3AD)
                            throw Error("Truncated Xbox 360 content header");

                        uint32_t contentType = (static_cast<uint32_t>(header[0x344]) << 24) |
                                               (static_cast<uint32_t>(header[0x345]) << 16) |
                                               (static_cast<uint32_t>(header[0x346]) << 8) |
                                               static_cast<uint32_t>(header[0x347]);
                        uint32_t volume = (static_cast<uint32_t>(header[0x3A9]) << 24) |
                                          (static_cast<uint32_t>(header[0x3AA]) << 16) |
                                          (static_cast<uint32_t>(header[0x3AB]) << 8) |
                                          static_cast<uint32_t>(header[0x3AC]);

                        if (volume == 0 && contentType == 2)
                        {
                            packages.insert(task.path);
                        }
                        else if (volume == 1 && (contentType == 0x4000 || contentType == 0x7000))
                        {
                            uint32_t title = (static_cast<uint32_t>(header[0x360]) << 24) |
                                             (static_cast<uint32_t>(header[0x361]) << 16) |
                                             (static_cast<uint32_t>(header[0x362]) << 8) |
                                             static_cast<uint32_t>(header[0x363]);
                            if (title != 0x4D5307FA)
                            {
                                char titleBuf[16];
                                std::snprintf(titleBuf, sizeof(titleBuf), "%08X", title);
                                throw Error("Wrong GOD game: Title ID " + std::string(titleBuf) + " (expected 4D5307FA)");
                            }
                            auto dataDir = task.path.parent_path() / (task.path.filename().string() + ".data");
                            if (!std::filesystem::is_directory(dataDir, ec) || IsSymlinkOrReparse(dataDir))
                                throw Error("GOD header needs its matching .data folder");
                            discs[dataDir] = Kind::God;
                        }
                        else
                        {
                            throw Error("Unsupported Xbox 360 content type; select game discs or Marketplace STFS DLC");
                        }
                    }
                    else if (!task.insideDisc)
                    {
                        if (count >= 4 && std::memcmp(header.data(), XEX2_M, 4) == 0)
                        {
                            discs[task.path.parent_path()] = Kind::Folder;
                        }
                        else if (task.direct || ToLower(task.path.extension().string()) == ".iso")
                        {
                            discs[task.path] = Kind::Iso;
                        }
                        else
                        {
                            // Probe standard XDVDFS offsets: 0x10000, 0x2090000, 0xFDA0000
                            static const uint64_t standardOffsets[] = {0x10000, 0x2090000, 0xFDA0000};
                            constexpr size_t SECTOR_SIZE = 2048;
                            std::vector<uint8_t> vd(SECTOR_SIZE);
                            static const char MAGIC_TAG[20] = {'M','I','C','R','O','S','O','F','T','*','X','B','O','X','*','M','E','D','I','A'};
                            for (uint64_t off : standardOffsets)
                            {
                                cancelCheck();
                                stream.seekg(off);
                                stream.read(reinterpret_cast<char*>(vd.data()), SECTOR_SIZE);
                                if (stream.gcount() == SECTOR_SIZE &&
                                    std::memcmp(vd.data(), MAGIC_TAG, 20) == 0 &&
                                    std::memcmp(vd.data() + 0x7EC, MAGIC_TAG, 20) == 0)
                                {
                                    discs[task.path] = Kind::Iso;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
            else if (task.direct)
            {
                rejected.emplace_back(task.path, "Source does not exist or is not a regular file/folder");
            }
        }
        catch (const Error& err)
        {
            if (err.cancelled()) throw;
            if (std::filesystem::is_directory(task.path, ec)) throw;
            rejected.emplace_back(task.path, err.what());
        }
        catch (const std::exception& ex)
        {
            rejected.emplace_back(task.path, ex.what());
        }

        if (discs.size() + packages.size() > MAX_CANDIDATES)
            throw Error("Too many content candidates; select a closer folder");
    }

    DiscoveredSources result;
    for (const auto& [p, k] : discs)
        result.candidates.emplace_back(p, k);

    std::sort(result.candidates.begin(), result.candidates.end(), [](const auto& a, const auto& b) {
        return ToLower(a.first.generic_string()) < ToLower(b.first.generic_string());
    });

    for (const auto& pkg : packages)
        result.packages.push_back(pkg);

    std::sort(result.packages.begin(), result.packages.end(), [](const auto& a, const auto& b) {
        return ToLower(a.generic_string()) < ToLower(b.generic_string());
    });

    result.rejected = std::move(rejected);
    return result;
}

// ----------------------------------------------------------------------------
// DLC Helpers: Content Record & Sidecar Manifests
// ----------------------------------------------------------------------------
std::vector<uint8_t> MakeContentRecord(const DlcPackageInfo& info)
{
    std::vector<uint8_t> data(308, 0);
    // struct.pack_into('>II', data, 0, 1, 2)
    data[0] = 0; data[1] = 0; data[2] = 0; data[3] = 1;
    data[4] = 0; data[5] = 0; data[6] = 0; data[7] = 2;

    // display_name encode utf-16-be up to 254 bytes
    std::vector<uint8_t> utf16;
    for (char c : info.displayName)
    {
        utf16.push_back(0);
        utf16.push_back(static_cast<uint8_t>(c));
    }
    if (utf16.size() > 254) utf16.resize(254);
    std::copy(utf16.begin(), utf16.end(), data.begin() + 8);

    // identity (contentId) ascii up to 40 bytes at offset 264
    std::string id = info.contentId;
    if (id.size() > 40) id.resize(40);
    std::copy(id.begin(), id.end(), data.begin() + 264);

    return data;
}

bool ExistingDlcPayloadMatches(const std::filesystem::path& target, StfsPackage& package)
{
    if (IsSymlinkOrReparse(target)) return false;
    std::set<std::string> expected{".lo-content", ".lo-dlc-header", ".lo-dlc.json"};
    std::array<uint8_t, 4096> original{}, installed{};
    for (const auto& entry : package.GetEntries())
    {
        auto path = target / std::filesystem::u8path(entry.path);
        for (auto ancestor = path; ancestor != target; ancestor = ancestor.parent_path())
            if (IsSymlinkOrReparse(ancestor)) return false;
        if (entry.isDirectory) continue;
        expected.insert(ToLower(entry.path));
        if (!std::filesystem::is_regular_file(path) || std::filesystem::file_size(path) != entry.size)
            return false;
        std::ifstream input(path, std::ios::binary);
        uint64_t remaining = entry.size;
        for (auto block : entry.blocks)
        {
            package.ReadBlock(block, original.data());
            const auto take = static_cast<size_t>(std::min<uint64_t>(remaining, original.size()));
            input.read(reinterpret_cast<char*>(installed.data()), take);
            if (input.gcount() != static_cast<std::streamsize>(take) ||
                !std::equal(original.begin(), original.begin() + take, installed.begin())) return false;
            remaining -= take;
        }
        if (remaining) return false;
    }
    std::set<std::string> actual;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(target))
    {
        if (IsSymlinkOrReparse(entry.path())) return false;
        if (entry.is_regular_file()) actual.insert(ToLower(entry.path().lexically_relative(target).generic_string()));
    }
    if (actual != expected) return false;
    std::ifstream content(target / ".lo-content", std::ios::binary);
    std::vector<uint8_t> record((std::istreambuf_iterator<char>(content)), {});
    return record == MakeContentRecord(package.GetInfo());
}

} // namespace

// ----------------------------------------------------------------------------
// Public API Implementations
// ----------------------------------------------------------------------------

#ifdef LO_IMPORT_TESTING
void SetTestSha256(uint32_t disc, std::string_view hex, bool europe)
{
    if (europe) g_testSha256Europe[disc] = std::string(hex);
    else g_testSha256Asia[disc] = std::string(hex);
}

void SetTestMd5(uint32_t disc, std::string_view hex, bool europe)
{
    if (europe) g_testMd5Europe[disc] = std::string(hex);
    else g_testMd5Asia[disc] = std::string(hex);
}

void ClearTestOverrides()
{
    g_testSha256Asia.clear();
    g_testSha256Europe.clear();
    g_testMd5Asia.clear();
    g_testMd5Europe.clear();
}

void SetTestDlcWriteFailure(std::string_view filename, std::string_view stage)
{
    g_testDlcFailureFile = filename;
    g_testDlcFailureStage = stage;
}

void SetTestDiscWriteFailure(std::string_view filename, std::string_view stage)
{
    g_testDiscFailureFile = filename;
    g_testDiscFailureStage = stage;
}

void SetTestPublishFailure(std::string_view slot, std::string_view stage)
{
    g_testPublishFailureSlot = slot;
    g_testPublishFailureStage = stage;
}
#endif

std::vector<std::filesystem::path> Discover(const std::filesystem::path& path)
{
    auto discovered = DiscoverAllSources({path});
    std::vector<std::filesystem::path> sources;
    for (const auto& [p, k] : discovered.candidates)
        sources.push_back(p);
    return sources;
}

ContentScan ScanContent(const std::vector<std::filesystem::path>& paths, const Cancelled& cancelled)
{
    auto discovered = DiscoverAllSources(paths, cancelled);
    ContentScan scanResult;
    scanResult.rejected = discovered.rejected;

    // 1. Scan game discs
    for (const auto& [candidate, kind] : discovered.candidates)
    {
        if (cancelled && cancelled())
            throw Error("Source check cancelled", true);

        try
        {
            std::vector<Entry> entries;
            DiscInfo info = PrepareDisc(candidate, kind, entries, true, cancelled);
            scanResult.discs.push_back(info);
        }
        catch (const Error& err)
        {
            if (err.cancelled()) throw;
            scanResult.rejected.emplace_back(candidate, err.what());
        }
        catch (const std::exception& ex)
        {
            scanResult.rejected.emplace_back(candidate, ex.what());
        }
    }

    // 2. Scan DLC packages
    std::map<std::string, std::string> seenDlcIdentities; // content_id -> source_sha256
    std::map<std::string, std::string> seenExtractedManifests;
    for (const auto& pkgPath : discovered.packages)
    {
        if (cancelled && cancelled())
            throw Error("Source check cancelled", true);

        try
        {
            if (std::filesystem::is_directory(pkgPath))
            {
                auto info = ReadExtractedDlc(pkgPath, cancelled).info;
                auto it = seenDlcIdentities.find(info.contentId);
                if (it != seenDlcIdentities.end())
                {
                    if (it->second != info.sourceSha256 || !seenExtractedManifests.contains(info.contentId) ||
                        seenExtractedManifests.at(info.contentId) != info.extractedManifestSha256)
                        throw Error("Different source packages have the same DLC content ID");
                    continue;
                }
                seenDlcIdentities[info.contentId] = info.sourceSha256;
                seenExtractedManifests[info.contentId] = info.extractedManifestSha256;
                scanResult.packages.push_back(std::move(info));
                continue;
            }
            StfsPackage stfs(pkgPath, cancelled);
            const auto& info = stfs.GetInfo();

            auto it = seenDlcIdentities.find(info.contentId);
            if (it != seenDlcIdentities.end())
            {
                if (it->second != info.sourceSha256 || seenExtractedManifests.contains(info.contentId))
                    throw Error("Different source packages have the same DLC content ID");
                continue;
            }
            seenDlcIdentities[info.contentId] = info.sourceSha256;
            scanResult.packages.push_back(info);
        }
        catch (const Error& err)
        {
            if (err.cancelled()) throw;
            scanResult.rejected.emplace_back(pkgPath, err.what());
        }
        catch (const std::exception& ex)
        {
            scanResult.rejected.emplace_back(pkgPath, ex.what());
        }
    }

    if (scanResult.discs.empty() && scanResult.packages.empty())
    {
        std::string details;
        for (size_t i = 0; i < std::min<size_t>(4, scanResult.rejected.size()); ++i)
        {
            if (!details.empty()) details += "; ";
            details += scanResult.rejected[i].first.filename().string() + ": " + scanResult.rejected[i].second;
        }
        std::string suffix = details.empty() ? "" : (" Details: " + details);
        throw Error("No supported Lost Odyssey discs or DLC found." + suffix);
    }

    // Check duplicate disc numbers
    std::map<uint32_t, std::vector<std::filesystem::path>> discMap;
    for (const auto& d : scanResult.discs)
        discMap[d.disc].push_back(d.path);

    for (const auto& [num, pathsFound] : discMap)
    {
        if (pathsFound.size() > 1)
        {
            std::string joined;
            for (size_t i = 0; i < std::min<size_t>(3, pathsFound.size()); ++i)
            {
                if (!joined.empty()) joined += ", ";
                joined += pathsFound[i].string();
            }
            throw Error("Found multiple sources for Disc " + std::to_string(num) +
                        ": " + joined + ". Select a closer folder.");
        }
    }

    // Check editions consistency
    std::set<std::string> editions;
    for (const auto& d : scanResult.discs)
        editions.insert(d.edition);
    if (editions.size() > 1)
        throw Error("Cannot mix Europe/Asia and USA/Europe discs in one installation");

    std::sort(scanResult.discs.begin(), scanResult.discs.end(),
              [](const auto& a, const auto& b) { return a.disc < b.disc; });

    return scanResult;
}

Scan ScanSource(const std::filesystem::path& path, const Cancelled& cancelled)
{
    ContentScan content = ScanContent(path, cancelled);
    Scan scanResult;
    scanResult.discs = std::move(content.discs);
    scanResult.packages = std::move(content.packages);
    scanResult.rejected = std::move(content.rejected);
    return scanResult;
}

// ----------------------------------------------------------------------------
// Transactional Installer (Discs and DLC)
// ----------------------------------------------------------------------------
static InstallResult ImportContentImpl(const ContentScan& selection,
                                const std::filesystem::path& destination,
                                const Progress& progress,
                                const Cancelled& cancelled,
                                bool replace,
                                const Commit& commit)
{
    auto checkCancelled = [&]() -> bool {
        return cancelled && cancelled();
    };
    auto reportProgress = [&](uint64_t done, uint64_t total, std::string_view label) {
        if (progress) progress(done, total, label);
    };

    InstallResult result;
    if (checkCancelled())
        throw Error("Import cancelled; original files were kept", true);

    std::error_code ec;
    auto dest = std::filesystem::absolute(destination, ec).lexically_normal();
    for (auto ancestor = dest; !ancestor.empty(); ancestor = ancestor.parent_path())
    {
        if (IsSymlinkOrReparse(ancestor))
            throw Error("Destination ancestors must not contain links or junctions");
        if (ancestor == ancestor.parent_path()) break;
    }
    result.destination = dest.string();

    // Legacy install callers may pass an installed disc directory. Reimport
    // always receives an explicit installation root, even when named disc1.
    auto gameRootDir = dest;
    std::string leafName = ToLower(gameRootDir.filename().string());
    if (!replace && (leafName == "disc1" || leafName == "disc2" || leafName == "disc3" || leafName == "disc4"))
    {
        if (std::filesystem::exists(gameRootDir / "default.xex", ec))
            gameRootDir = gameRootDir.parent_path();
    }
    dest = gameRootDir;
    result.destination = dest.string();

    if (replace && !selection.discs.empty() && std::filesystem::exists(dest / "default.xex", ec))
        throw Error("A flat default.xex installation needs a separate destination for disc reimport: " + dest.string());

    reportProgress(0, 0, "Checking selected source");

    // Check every source against the effective root, including ISO/STFS files and
    // the GOD header paired with its .data directory. Resolve existing aliases.
    const auto canonicalDest = std::filesystem::weakly_canonical(dest);
    auto checkSource = [&](const std::filesystem::path& path) {
        const auto canonicalSource = std::filesystem::weakly_canonical(path);
        if (ContainsPath(canonicalSource, canonicalDest) || ContainsPath(canonicalDest, canonicalSource))
            throw Error("Source and destination must be separate folders: " + path.string());
    };
    for (const auto& disc : selection.discs)
    {
        checkSource(disc.path);
        if (disc.kind == Kind::God)
        {
            const auto data = disc.path;
            const auto header = data.parent_path() / data.stem();
            checkSource(header);
        }
    }

    for (const auto& package : selection.packages)
        checkSource(package.path);
    std::filesystem::create_directories(dest, ec);
    if (ec) throw Error("Could not create destination: " + ec.message());

    // Destination directory link/junction check
    if (IsSymlinkOrReparse(dest))
        throw Error("Destination folder must not contain links or junctions");

    // .import.lock
    std::filesystem::path lockPath = dest / ".import.lock";
#ifdef _WIN32
    HANDLE lockHandle = INVALID_HANDLE_VALUE;
    lockHandle = CreateFileW(lockPath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                             CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (lockHandle == INVALID_HANDLE_VALUE)
    {
        throw Error("Another import owns this destination. If an earlier import crashed, "
                    "close all importers before removing .import.lock.");
    }
#else
    const int lockFd = open(lockPath.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (lockFd < 0 || flock(lockFd, LOCK_EX | LOCK_NB) != 0)
    {
        if (lockFd >= 0) close(lockFd);
        throw Error("Another import owns this destination. If an earlier import crashed, "
                    "close all importers before removing .import.lock.");
    }
#endif

    auto unlock = [&]() {
#ifdef _WIN32
        if (lockHandle != INVALID_HANDLE_VALUE)
        {
            CloseHandle(lockHandle);
            lockHandle = INVALID_HANDLE_VALUE;
        }
#else
        close(lockFd);
#endif
        std::error_code removeEc;
        std::filesystem::remove(lockPath, removeEc);
    };

    struct LockGuard
    {
        std::function<void()> fn;
        ~LockGuard() { if (fn) fn(); }
    } guard{unlock};

    if (selection.discs.empty() && selection.packages.empty())
        throw Error("Select at least one disc or DLC package");

    // Exclusive staging: never delete a pre-existing staging path (it may hold
    // the only copy of a previous transaction's failed rollback).
    std::filesystem::path stagingPath;
    for (unsigned attempt = 0; attempt < 100; ++attempt)
    {
        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        auto candidate = dest / (".import-staging-" + ProcessIdString() + "-" + std::to_string(nonce) + "-" + std::to_string(attempt));
        ec.clear();
        if (std::filesystem::create_directory(candidate, ec)) { stagingPath = candidate; break; }
        if (ec && ec != std::errc::file_exists) throw Error("Could not create import staging: " + ec.message());
    }
    if (stagingPath.empty()) throw Error("Could not reserve a unique import staging directory");

    // Only paths selected for replacement are entered in this publication list.
    struct Slot { std::filesystem::path target, staged, backup; bool old = false, published = false; };
    std::vector<Slot> slots;
    bool preserveStaging = false;
    auto cleanupStaging = [&]() {
        if (preserveStaging) return;
#ifdef LO_IMPORT_TESTING
        if (g_testPublishFailureSlot == "staging" && g_testPublishFailureStage == "cleanup")
        {
            result.warning += "Import staging cleanup failed; remove " + stagingPath.string() + ": injected failure";
            return;
        }
#endif
        std::error_code removeEc;
        std::filesystem::remove_all(stagingPath, removeEc);
        if (removeEc)
            result.warning += "Import staging cleanup failed; remove " + stagingPath.string() + ": " + removeEc.message();
    };

    try
    {
    // ------------------------------------------------------------------------
    // Phase 1: Install Discs
    // ------------------------------------------------------------------------
    if (!selection.discs.empty())
    {
        for (const auto& d : selection.discs)
        {
            if (!replace && std::filesystem::exists(dest / ("disc" + std::to_string(d.disc)), ec))
                throw Error("Disc " + std::to_string(d.disc) + " is already installed; existing files were kept");
        }

        std::string targetEdition = selection.discs[0].edition;
        for (uint32_t n = 1; n <= 4; ++n)
        {
            auto existing = dest / ("disc" + std::to_string(n));
            if (std::filesystem::exists(existing, ec) &&
                std::none_of(selection.discs.begin(), selection.discs.end(), [n](const DiscInfo& disc) { return disc.disc == n; }))
            {
                if (IsSymlinkOrReparse(existing)) throw Error("Disc destination is a link: " + existing.string());
                // Retained discs only need an edition-compatible XEX identity;
                // do not require a second complete scan of installed resources.
                const auto xexPath = existing / "default.xex";
                if (IsSymlinkOrReparse(xexPath) || !std::filesystem::is_regular_file(xexPath, ec) ||
                    std::filesystem::file_size(xexPath, ec) > 32 * 1024 * 1024)
                    throw Error("Retained disc XEX is missing or invalid: " + existing.string());
                std::ifstream xex(xexPath, std::ios::binary);
                std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(xex)), {});
                if (bytes.empty() || bytes.size() > 32 * 1024 * 1024) throw Error("Retained disc XEX is missing or invalid: " + existing.string());
                auto info = ParseExecution(bytes);
                if (info.title != "4D5307FA" || info.discs != 4)
                    throw Error("Retained disc XEX has incompatible title or disc count: " + existing.string());
                auto [edition, identity] = IdentifyDisc(info, Sha256Bytes(bytes.data(), bytes.size()), Md5Bytes(bytes.data(), bytes.size()));
                DiscInfo installed;
                installed.disc = info.disc; installed.edition = edition;
                if (installed.disc != n || installed.edition != targetEdition)
                    throw Error("Cannot mix Europe/Asia and USA/Europe discs in one installation");
            }
        }

        uint64_t totalDiscBytes = 0;
        struct LoadedDisc
        {
            DiscInfo info;
            std::unique_ptr<ImageReader> image;
            std::vector<Entry> entries;
        };
        std::vector<LoadedDisc> loadedDiscs;
        std::set<uint32_t> selectedNumbers;

        for (const auto& d : selection.discs)
        {
            LoadedDisc ld;
            ld.info = PrepareDisc(d.path, d.kind, ld.entries, true, checkCancelled, &ld.image);
            if (ld.info.disc != d.disc || ld.info.sha256 != d.sha256 ||
                ld.info.media != d.media || ld.info.edition != d.edition)
                throw Error("The selected disc identity changed after review; check the source again");
            if (ld.info.edition != targetEdition)
                throw Error("Cannot mix Europe/Asia and USA/Europe discs in one installation");
            if (!selectedNumbers.insert(ld.info.disc).second)
                throw Error("Duplicate selected disc number");
            for (const auto& e : ld.entries) totalDiscBytes += e.size;
            loadedDiscs.push_back(std::move(ld));
        }

        auto spaceInfo = std::filesystem::space(dest, ec);
        if (!ec && spaceInfo.free < totalDiscBytes + 64ULL * 1024 * 1024)
            throw Error("Not enough free space for the selected discs");

        uint64_t doneBytes = 0;
            for (const auto& ld : loadedDiscs)
            {
                if (checkCancelled())
                    throw Error("Import cancelled; original files were kept", true);

                std::filesystem::path targetDisc = stagingPath / ("disc" + std::to_string(ld.info.disc));
                std::filesystem::create_directories(targetDisc, ec);

                for (const auto& entry : ld.entries)
                {
                    std::filesystem::path dstFile = targetDisc / entry.name;
                    std::filesystem::create_directories(dstFile.parent_path(), ec);

                    std::ofstream out(dstFile, std::ios::binary | std::ios::trunc);
                    CheckDiscOutput(out, dstFile, "open");

                    std::vector<char> buffer(64 * 1024);
                    uint64_t remaining = entry.size;
                    uint64_t offset = 0;

                    std::unique_ptr<std::ifstream> inFolder;
                    if (!ld.image)
                    {
                        inFolder = std::make_unique<std::ifstream>(ld.info.path / entry.name, std::ios::binary);
                        if (!*inFolder) throw Error("Could not open source file: " + (ld.info.path / entry.name).string());
                    }

                    while (remaining > 0)
                    {
                        if (checkCancelled())
                            throw Error("Import cancelled; original files were kept", true);

                        size_t chunk = static_cast<size_t>(std::min<uint64_t>(buffer.size(), remaining));
                        if (ld.image)
                        {
                            ld.image->Read(entry.offset + offset, buffer.data(), chunk);
                        }
                        else
                        {
                            inFolder->read(buffer.data(), chunk);
                            if (static_cast<size_t>(inFolder->gcount()) != chunk)
                                throw Error("Source changed or was truncated during import");
                        }

                        out.write(buffer.data(), chunk);
                        CheckDiscOutput(out, dstFile, "write");

                        remaining -= chunk;
                        offset += chunk;
                        doneBytes += chunk;
                        reportProgress(doneBytes, totalDiscBytes, entry.name);
                    }
                    FinishDiscOutput(out, dstFile);
                }

                // Write import-info.json
                std::filesystem::path infoJsonPath = targetDisc / "import-info.json";
                std::ofstream infoJson(infoJsonPath);
                CheckDiscOutput(infoJson, infoJsonPath, "open");
                infoJson << "{\n"
                         << "  \"title\": \"" << ld.info.title << "\",\n"
                         << "  \"media\": \"" << ld.info.media << "\",\n"
                         << "  \"version\": " << ld.info.version << ",\n"
                         << "  \"base\": " << ld.info.base << ",\n"
                         << "  \"disc\": " << ld.info.disc << ",\n"
                         << "  \"discs\": " << ld.info.discs << ",\n"
                         << "  \"sha256\": \"" << ld.info.sha256 << "\",\n"
                         << "  \"md5\": \"" << ld.info.md5 << "\",\n"
                         << "  \"edition\": \"" << ld.info.edition << "\",\n"
                         << "  \"identity\": \"" << ld.info.identity << "\",\n"
                         << "  \"metadata_edition\": \"" << ld.info.metadataEdition << "\"\n"
                         << "}\n";
                FinishDiscOutput(infoJson, infoJsonPath);

                // Verify copied default.xex SHA256 matches
                std::filesystem::path copiedXex = targetDisc / "default.xex";
                std::ifstream copiedXexFile(copiedXex, std::ios::binary);
                std::vector<uint8_t> copiedXexBytes((std::istreambuf_iterator<char>(copiedXexFile)),
                                                    std::istreambuf_iterator<char>());
                std::string copiedHash = Sha256Bytes(copiedXexBytes.data(), copiedXexBytes.size());
                if (copiedHash != ld.info.sha256)
                    throw Error("The source XEX changed during import; please retry");
            }

            if (checkCancelled())
                throw Error("Import cancelled; original files were kept", true);

            for (const auto& ld : loadedDiscs)
            {
                auto name = "disc" + std::to_string(ld.info.disc);
                slots.push_back({dest / name, stagingPath / name, stagingPath / "old" / name});
                result.discs.push_back(static_cast<int>(ld.info.disc));
            }
            std::sort(result.discs.begin(), result.discs.end());
    }

    // ------------------------------------------------------------------------
    // Phase 2: Install DLC Packages
    // ------------------------------------------------------------------------
    if (!selection.packages.empty())
    {
        auto dlcRoot = dest / "dlc";
        if (IsSymlinkOrReparse(dlcRoot)) throw Error("DLC destination must not contain links or junctions");
        std::filesystem::create_directories(dlcRoot, ec);
        if (ec) throw Error("Could not create DLC directory: " + ec.message());

        struct DlcInstallPackage { DlcPackageInfo info; std::unique_ptr<StfsPackage> stfs; std::optional<ExtractedDlc> extracted; };
        std::vector<DlcInstallPackage> dlcPackages;
        std::set<std::string> selectedContentIds;
        for (const auto& pkgInfo : selection.packages)
        {
            if (checkCancelled())
                throw Error("DLC import cancelled; source files were kept", true);
            if (!selectedContentIds.insert(pkgInfo.contentId).second)
                throw Error("Duplicate selected DLC content ID: " + pkgInfo.contentId);

            DlcInstallPackage package{pkgInfo, nullptr, std::nullopt};
            if (std::filesystem::is_directory(pkgInfo.path))
            {
                package.extracted = ReadExtractedDlc(pkgInfo.path, checkCancelled);
                package.info = package.extracted->info;
                if (package.info.sourceSha256 != pkgInfo.sourceSha256 || package.info.contentId != pkgInfo.contentId ||
                    package.info.extractedManifestSha256 != pkgInfo.extractedManifestSha256)
                    throw Error("DLC source changed since review; check the source again");
            }
            else
            {
                package.stfs = std::make_unique<StfsPackage>(pkgInfo.path, checkCancelled);
                if (package.stfs->GetInfo().sourceSha256 != pkgInfo.sourceSha256 || package.stfs->GetInfo().contentId != pkgInfo.contentId)
                    throw Error("DLC source changed since review; check the source again");
            }

            auto targetDir = dlcRoot / pkgInfo.contentId;
            if (IsSymlinkOrReparse(targetDir)) throw Error("DLC destination is a link: " + targetDir.string());
            if (std::filesystem::exists(targetDir, ec))
            {
                if (replace) { dlcPackages.push_back(std::move(package)); continue; }
                if (package.extracted)
                {
                    auto existing = ReadExtractedDlc(targetDir, checkCancelled);
                    if (existing.sidecars == package.extracted->sidecars)
                    {
                        result.dlcUnchanged.push_back(pkgInfo.contentId);
                        continue;
                    }
                    throw Error("DLC already exists with different content; existing files were kept");
                }
                // Check if existing match
                std::filesystem::path headerPath = targetDir / ".lo-dlc-header";
                if (std::filesystem::exists(headerPath, ec))
                {
                    std::ifstream hdrIn(headerPath, std::ios::binary);
                    std::vector<uint8_t> existingHdr((std::istreambuf_iterator<char>(hdrIn)),
                                                     std::istreambuf_iterator<char>());
                    if (package.stfs && existingHdr == package.stfs->GetHeader() && ExistingDlcPayloadMatches(targetDir, *package.stfs))
                    {
                        result.dlcUnchanged.push_back(pkgInfo.contentId);
                        continue;
                    }
                }
                throw Error("DLC " + pkgInfo.contentId + " already exists with different content; existing files were kept");
            }

            dlcPackages.push_back(std::move(package));
        }

        if (!dlcPackages.empty())
        {
            uint64_t totalDlcBytes = 0;
            for (const auto& pkg : dlcPackages) totalDlcBytes += pkg.info.bytes;

            auto spaceInfo = std::filesystem::space(dest, ec);
            if (!ec && spaceInfo.free < totalDlcBytes + 16ULL * 1024 * 1024)
                throw Error("Not enough free space for the selected DLC");

            const auto dlcStagingPath = stagingPath / "dlc";
            std::filesystem::create_directories(dlcStagingPath);

            uint64_t dlcDone = 0;
                for (const auto& pkg : dlcPackages)
                {
                    const auto& info = pkg.info;
                    std::filesystem::path targetDir = dlcStagingPath / info.contentId;
                    std::filesystem::create_directories(targetDir, ec);

                    struct ManifestFile
                    {
                        std::string path;
                        uint32_t size = 0;
                        std::string sha256;
                    };
                    std::vector<ManifestFile> manifestFiles;

                    std::vector<uint8_t> blockBuf(4096);
                    std::vector<StfsPackage::StfsEntry> stfsEntries;
                    if (pkg.stfs) stfsEntries = pkg.stfs->GetEntries();
                    for (const auto& entry : stfsEntries)
                    {
                        if (checkCancelled())
                            throw Error("DLC import cancelled; source files were kept", true);

                        std::filesystem::path outPath = targetDir / entry.path;
                        if (entry.isDirectory)
                        {
                            std::filesystem::create_directories(outPath, ec);
                            continue;
                        }

                        std::filesystem::create_directories(outPath.parent_path(), ec);
                        std::ofstream out(outPath, std::ios::binary | std::ios::trunc);
                        CheckDlcOutput(out, outPath, "open");

                        crypto::Sha256 fileSha;
                        uint32_t remaining = entry.size;

                        for (uint32_t b : entry.blocks)
                        {
                            pkg.stfs->ReadBlock(b, blockBuf.data());
                            uint32_t take = std::min<uint32_t>(remaining, 4096);
                            if (take > 0)
                            {
                                out.write(reinterpret_cast<const char*>(blockBuf.data()), take);
                                CheckDlcOutput(out, outPath, "write");
                                fileSha.Update(blockBuf.data(), take);
                                remaining -= take;
                                dlcDone += take;
                                reportProgress(dlcDone, totalDlcBytes, entry.path);
                            }
                        }

                        FinishDlcOutput(out, outPath);
                        if (remaining != 0)
                            throw Error("DLC file chain is shorter than its declared size");

                        manifestFiles.push_back({entry.path, entry.size, crypto::HexString(fileSha.Finalize())});
                    }
                    if (pkg.extracted) for (const auto& entry : pkg.extracted->files)
                    {
                        if (checkCancelled()) throw Error("DLC import cancelled; source files were kept", true);
                        auto source = info.path / std::filesystem::u8path(entry.path), outPath = targetDir / std::filesystem::u8path(entry.path);
                        std::filesystem::create_directories(outPath.parent_path(), ec);
                        std::ifstream in(source, std::ios::binary); std::ofstream out(outPath, std::ios::binary | std::ios::trunc);
                        if (!in || !out) throw Error("Could not open extracted DLC payload for copying");
                        crypto::Sha256 sha; std::array<uint8_t, 4096> bytes{}; uint64_t total = 0;
                        while (in.read(reinterpret_cast<char*>(bytes.data()), bytes.size()) || in.gcount()) { auto n = in.gcount(); sha.Update(bytes.data(), static_cast<size_t>(n)); out.write(reinterpret_cast<char*>(bytes.data()), n); total += n; dlcDone += n; reportProgress(dlcDone, totalDlcBytes, entry.path); if (checkCancelled()) throw Error("DLC import cancelled; source files were kept", true); }
                        out.flush();
                        if (!out) throw Error("Extracted DLC write failed");
                        if (total != entry.size || ToLower(crypto::HexString(sha.Finalize())) != entry.sha256) throw Error("Extracted DLC changed during import");
                    }

                    if (pkg.extracted)
                    {
                        for (size_t i = 0; i < DlcSidecars.size(); ++i)
                        {
                            std::ofstream out(targetDir / DlcSidecars[i], std::ios::binary);
                            const auto& bytes = pkg.extracted->sidecars[i];
                            out.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
                            out.flush();
                            if (!out) throw Error("Extracted DLC sidecar write failed");
                        }
                        ReadExtractedDlc(targetDir, checkCancelled);
                        continue;
                    }

                    // Sidecars
                    // 1. .lo-content
                    std::filesystem::path contentPath = targetDir / ".lo-content";
                    std::ofstream contentOut(contentPath, std::ios::binary);
                    CheckDlcOutput(contentOut, contentPath, "open");
                    auto rec = MakeContentRecord(info);
                    contentOut.write(reinterpret_cast<const char*>(rec.data()), rec.size());
                    FinishDlcOutput(contentOut, contentPath);

                    // 2. .lo-dlc-header
                    std::filesystem::path headerPath = targetDir / ".lo-dlc-header";
                    std::ofstream headerOut(headerPath, std::ios::binary);
                    CheckDlcOutput(headerOut, headerPath, "open");
                    if (pkg.stfs) headerOut.write(reinterpret_cast<const char*>(pkg.stfs->GetHeader().data()), pkg.stfs->GetHeader().size());
                    else { std::ifstream in(info.path / ".lo-dlc-header", std::ios::binary); headerOut << in.rdbuf(); }
                    FinishDlcOutput(headerOut, headerPath);

                    // 3. .lo-dlc.json
                    std::filesystem::path jsonPath = targetDir / ".lo-dlc.json";
                    std::ofstream jsonOut(jsonPath);
                    CheckDlcOutput(jsonOut, jsonPath, "open");
                    jsonOut << "{\n"
                            << "  \"schema\": 1,\n"
                            << "  \"title_id\": \"4D5307FA\",\n"
                            << "  \"content_id\": \"" << info.contentId << "\",\n"
                            << "  \"source_sha256\": \"" << info.sourceSha256 << "\",\n"
                            << "  \"display_name\": \"" << info.displayName << "\",\n"
                            << "  \"license_mask\": " << info.licenseMask << ",\n"
                            << "  \"files\": [\n";
                    for (size_t i = 0; i < manifestFiles.size(); ++i)
                    {
                        jsonOut << "    {\"path\": \"" << manifestFiles[i].path << "\", \"size\": "
                                << manifestFiles[i].size << ", \"sha256\": \"" << manifestFiles[i].sha256 << "\"}";
                        if (i + 1 < manifestFiles.size()) jsonOut << ",";
                        jsonOut << "\n";
                    }
                    jsonOut << "  ]\n}\n";
                    FinishDlcOutput(jsonOut, jsonPath);
                }

                if (checkCancelled())
                    throw Error("DLC import cancelled; source files were kept", true);

                for (const auto& pkg : dlcPackages)
                {
                    const auto& id = pkg.info.contentId;
                    slots.push_back({dlcRoot / id, dlcStagingPath / id, stagingPath / "old" / "dlc" / id});
                    result.dlcImported.push_back(id);
                }
        }
    }

    // One commit for all selected discs and DLC; reverse every rename on failure.
    const auto dataRoot = os::user_paths::UsePortableLayout() ? std::filesystem::current_path() : os::user_paths::DataDir();
    const std::array protectedRoots{
        std::filesystem::weakly_canonical(os::user_paths::ProfileDir()),
        std::filesystem::weakly_canonical(dataRoot / "save"),
        std::filesystem::weakly_canonical(dataRoot / "cache")};
    for (const auto& slot : slots)
    {
        const auto target = std::filesystem::weakly_canonical(slot.target);
        for (const auto& root : protectedRoots)
            if (ContainsPath(target, root) || ContainsPath(root, target))
                throw Error("Import would replace a profile, save or cache root: " + root.string());
    }
    if (checkCancelled()) throw Error("Import cancelled; original files were kept", true);
    try
    {
        for (auto& slot : slots)
        {
            if (IsSymlinkOrReparse(slot.target)) throw Error("Import target is a link: " + slot.target.string());
            std::filesystem::create_directories(slot.target.parent_path());
            if (std::filesystem::exists(slot.target))
            {
                if (!replace) throw Error("Import target already exists: " + slot.target.string());
                std::filesystem::create_directories(slot.backup.parent_path());
                RenameSlot(slot.target, slot.backup, slot.target.filename().string(), "backup");
                slot.old = true;
            }
            RenameSlot(slot.staged, slot.target, slot.target.filename().string(), "publish");
            slot.published = true;
        }
        if (checkCancelled()) throw Error("Import cancelled; original files were kept", true);
        if (commit)
        {
            if (std::filesystem::is_regular_file(dest / "default.xex") ||
                std::filesystem::is_regular_file(dest / "disc1" / "default.xex"))
                commit(result);
            else
                result.warning += "Destination has no Disc 1; game path was not updated. ";
        }
    }
    catch (...)
    {
        const auto failure = std::current_exception();
        std::string rollbackErrors;
        for (auto it = slots.rbegin(); it != slots.rend(); ++it)
        {
            auto& slot = *it;
            try
            {
                if (slot.published) RenameSlot(slot.target, slot.staged, slot.target.filename().string(), "rollback-new");
                // Never replace a live new slot if moving it away failed.
                if (slot.old && std::filesystem::exists(slot.target))
                    throw Error("Cannot restore old slot while new slot remains at " + slot.target.string());
                if (slot.old) RenameSlot(slot.backup, slot.target, slot.target.filename().string(), "rollback-old");
            }
            catch (const std::exception& error)
            {
                preserveStaging = true;
                rollbackErrors += " [" + std::string(error.what()) + "; old backup: " + slot.backup.string() + "]";
            }
        }
        if (!rollbackErrors.empty())
        {
            std::string reason = "Non-standard publication failure";
            try { std::rethrow_exception(failure); }
            catch (const std::exception& error) { reason = error.what(); }
            catch (...) {}
            throw Error(reason + "; rollback incomplete, keep staging " + stagingPath.string() + ":" + rollbackErrors);
        }
        std::rethrow_exception(failure);
    }

    } // preparation and publication
    catch (...)
    {
        cleanupStaging();
        throw;
    }
    // The callback is the commit point. Cleanup problems must not turn a
    // committed import into a failure that prompts the user to retry.
    cleanupStaging();
    try { reportProgress(1, 1, "Import complete"); }
    catch (const std::exception& error) { result.warning += " Progress reporting failed after commit: " + std::string(error.what()); }
    catch (...) { result.warning += " Progress reporting failed after commit."; }
    return result;
}

InstallResult InstallContent(const ContentScan& selection,
                             const std::filesystem::path& destination,
                             const Progress& progress,
                             const Cancelled& cancelled)
{
    return ImportContentImpl(selection, destination, progress, cancelled, false, {});
}

InstallResult ReimportContent(const ContentScan& selection,
                              const std::filesystem::path& destination,
                              const Progress& progress,
                              const Cancelled& cancelled,
                              const Commit& commit)
{
    return ImportContentImpl(selection, destination, progress, cancelled, true, commit);
}

std::vector<int> InstallDiscs(const std::filesystem::path& source,
                              const std::filesystem::path& gameDir,
                              const Progress* progressPtr,
                              const Cancelled* cancelledPtr)
{
    ContentScan scan = ScanContent(source, cancelledPtr ? *cancelledPtr : Cancelled{});
    // Remove DLC packages if any so this call installs only discs
    scan.packages.clear();

    InstallResult res = InstallContent(scan, gameDir,
                                       progressPtr ? *progressPtr : Progress{},
                                       cancelledPtr ? *cancelledPtr : Cancelled{});
    if (res.cancelled)
        throw Error("Import cancelled; original files were kept", true);
    if (!res.error.empty())
        throw Error(res.error);

    return res.discs;
}

std::filesystem::path DefaultGameDirectory(const std::filesystem::path& executableDirectory)
{
    std::error_code ec;
    auto absExe = std::filesystem::absolute(executableDirectory, ec).lexically_normal();
    return settings::game_path::DefaultGameRoot(absExe);
}

bool WriteGamePath(const std::filesystem::path& executableDirectory,
                   const std::filesystem::path& gameDirectory,
                   std::string& error)
{
    std::error_code ec;
    auto absExe = std::filesystem::absolute(executableDirectory, ec).lexically_normal();
    auto configPath = os::user_paths::UsePortableLayout()
        ? absExe / "game-path.txt"
        : os::user_paths::ConfigDir() / "game-path.txt";

    std::filesystem::create_directories(configPath.parent_path(), ec);
    if (ec)
    {
        error = "Could not create config directory for game-path.txt";
        return false;
    }

    const auto temporary = configPath.parent_path() / "game-path.txt.tmp";
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        if (!out)
        {
            error = "Could not open game-path.txt for writing";
            return false;
        }

        auto u8str = gameDirectory.generic_u8string();
        out.write(reinterpret_cast<const char*>(u8str.data()), u8str.size());
        out.put('\n');
        out.flush();
        if (!out)
        {
            error = "Could not write to game-path.txt";
            return false;
        }
    }

#ifdef _WIN32
    if (!MoveFileExW(temporary.wstring().c_str(), configPath.wstring().c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        error = "Could not commit game-path.txt";
        return false;
    }
#else
    std::filesystem::rename(temporary, configPath, ec);
    if (ec)
    {
        error = "Could not commit game-path.txt";
        return false;
    }
#endif
    return true;
}

} // namespace install
