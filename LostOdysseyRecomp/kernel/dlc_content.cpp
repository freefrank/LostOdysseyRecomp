#include <stdafx.h>
#include "dlc_content.h"
#include <kernel/io/file_system.h>
#include "../../tools/XenonRecomp/thirdparty/tomlplusplus/vendor/json.hpp"
#include <optional>

namespace DlcContent
{
namespace
{
using json = nlohmann::json;
constexpr uintmax_t kMaxMetadata = 16 * 1024 * 1024;

std::string Lower(std::string text)
{
    for (char& c : text) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return text;
}

bool Hex(std::string_view value, size_t length, bool upper)
{
    return value.size() == length && std::all_of(value.begin(), value.end(), [upper](char c) {
        return (c >= '0' && c <= '9') || (upper ? (c >= 'A' && c <= 'F') : (c >= 'a' && c <= 'f'));
    });
}

bool Plain(const std::filesystem::path& path)
{
    std::error_code ec;
    const auto status = std::filesystem::symlink_status(path, ec);
    if (ec || !std::filesystem::exists(status) || std::filesystem::is_symlink(status)) return false;
#ifdef _WIN32
    const auto attrs = GetFileAttributesW(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
#endif
    return true;
}

bool Regular(const std::filesystem::path& path)
{
    std::error_code ec;
    return Plain(path) && std::filesystem::is_regular_file(path, ec) && !ec;
}

std::optional<std::string> ReadSmall(const std::filesystem::path& path, uintmax_t limit)
{
    std::error_code ec;
    if (!Regular(path)) return {};
    const auto size = std::filesystem::file_size(path, ec);
    if (ec || size > limit) return {};
    std::string bytes(static_cast<size_t>(size), '\0');
    std::ifstream input(path, std::ios::binary);
    if (!input.read(bytes.data(), bytes.size())) return {};
    return bytes;
}

bool SafeComponent(std::string_view name)
{
    if (name.empty() || name == "." || name == ".." || name.back() == ' ' || name.back() == '.') return false;
    for (unsigned char c : name) if (c < 32 || std::string_view("<>:\"/\\|?*").find(c) != std::string_view::npos) return false;
    const auto stem = Lower(std::string(name.substr(0, name.find('.'))));
    if (stem == "con" || stem == "prn" || stem == "aux" || stem == "nul") return false;
    return !(stem.size() == 4 && (stem.starts_with("com") || stem.starts_with("lpt")) && stem[3] >= '1' && stem[3] <= '9');
}

bool PayloadPath(const std::filesystem::path& root, std::string_view relative, std::filesystem::path& out)
{
    if (relative.empty() || relative.size() > 4096 || relative.find('\\') != std::string_view::npos) return false;
    out = root;
    size_t start = 0;
    while (start < relative.size())
    {
        const auto end = relative.find('/', start);
        const auto part = relative.substr(start, end == std::string_view::npos ? relative.size() - start : end - start);
        if (!SafeComponent(part) || Lower(std::string(part)).starts_with(".lo-")) return false;
        out /= std::filesystem::path(std::u8string_view(reinterpret_cast<const char8_t*>(part.data()), part.size()));
        if (!Plain(out)) return false;
        if (end == std::string_view::npos) return Regular(out);
        std::error_code ec;
        if (!std::filesystem::is_directory(out, ec) || ec) return false;
        start = end + 1;
    }
    return false;
}

uint32_t ReadBE32(const std::string& bytes, size_t offset)
{
    return (uint32_t(uint8_t(bytes[offset])) << 24) | (uint32_t(uint8_t(bytes[offset + 1])) << 16) |
        (uint32_t(uint8_t(bytes[offset + 2])) << 8) | uint8_t(bytes[offset + 3]);
}

std::optional<Installed> ReadPackage(const std::filesystem::path& root)
{
    const auto id = FileSystem::PathUtf8(root.filename());
    if (!Hex(id, 40, true) || !Plain(root)) return {};
    const auto content = ReadSmall(root / ".lo-content", sizeof(XCONTENT_DATA));
    const auto metadata = ReadSmall(root / ".lo-dlc.json", kMaxMetadata);
    const auto header = ReadSmall(root / ".lo-dlc-header", kMaxMetadata);
    if (!content || content->size() != sizeof(XCONTENT_DATA) || !metadata || !header || header->size() < 0x364) return {};
    const std::string_view magic(header->data(), 4);
    if ((magic != "CON " && magic != "LIVE" && magic != "PIRS") ||
        ReadBE32(*header, 0x344) != 2 || ReadBE32(*header, 0x360) != 0x4D5307FA) return {};
    constexpr char hex[] = "0123456789ABCDEF";
    for (size_t i = 0; i < 20; ++i)
    {
        const auto byte = uint8_t((*header)[0x32C + i]);
        if (id[i * 2] != hex[byte >> 4] || id[i * 2 + 1] != hex[byte & 15]) return {};
    }
    Installed result;
    memcpy(&result.data, content->data(), sizeof(result.data));
    if (result.data.DeviceID != 1 || result.data.dwContentType != XCONTENTTYPE_DLC ||
        !memchr(result.data.szFileName, 0, sizeof(result.data.szFileName)) || id != result.data.szFileName) return {};
    const auto data = json::parse(*metadata);
    if (data.at("schema") != 1 || data.at("title_id") != "4D5307FA" || data.at("content_id") != id ||
        !Hex(data.at("source_sha256").get<std::string>(), 64, false) || !data.at("license_mask").is_number_unsigned()) return {};
    const auto license = data.at("license_mask").get<uint64_t>();
    if (license > UINT32_MAX) return {};
    result.licenseMask = uint32_t(license);
    const auto display = data.at("display_name").get<std::string>();
    const auto utf16 = std::filesystem::path(std::u8string_view(reinterpret_cast<const char8_t*>(display.data()), display.size())).u16string();
    if (utf16.size() >= XCONTENT_MAX_DISPLAYNAME || utf16.find(u'\0') != std::u16string::npos) return {};
    for (size_t i = 0; i < XCONTENT_MAX_DISPLAYNAME; ++i)
        if (result.data.szDisplayName[i] != (i < utf16.size() ? uint16_t(utf16[i]) : 0)) return {};
    const auto& files = data.at("files");
    if (!files.is_array() || files.empty() || files.size() > 100000) return {};
    std::set<std::string> names;
    for (const auto& file : files)
    {
        const auto relative = file.at("path").get<std::string>();
        if (!names.insert(Lower(relative)).second || !file.at("size").is_number_unsigned() ||
            !Hex(file.at("sha256").get<std::string>(), 64, false)) return {};
        std::filesystem::path path;
        if (!PayloadPath(root, relative, path)) return {};
        std::error_code ec;
        if (std::filesystem::file_size(path, ec) != file.at("size").get<uint64_t>() || ec) return {};
    }
    result.root = root;
    return result;
}
}

std::filesystem::path SharedRoot(const std::filesystem::path& gamePath)
{
    const auto name = Lower(FileSystem::PathUtf8(gamePath.filename()));
    if (name.size() == 5 && name.starts_with("disc") && name[4] >= '1' && name[4] <= '4' && Regular(gamePath / "default.xex"))
        return gamePath.parent_path();
    return gamePath;
}

std::vector<Installed> Discover(const std::filesystem::path& gamePath)
{
    std::vector<Installed> result;
    const auto shared = SharedRoot(gamePath);
    const auto dlc = shared / "dlc";
    if (!Plain(shared) || !Plain(dlc)) return result;
    std::error_code ec;
    size_t count = 0;
    for (std::filesystem::directory_iterator it(dlc, ec), end; !ec && it != end; it.increment(ec))
    {
        if (++count > 4096) break;
        try { if (auto item = ReadPackage(it->path())) result.push_back(std::move(*item)); }
        catch (const std::exception&) { /* Invalid or incomplete imports remain invisible. */ }
    }
    std::sort(result.begin(), result.end(), [](const Installed& a, const Installed& b) { return strcmp(a.data.szFileName, b.data.szFileName) < 0; });
    return result;
}
}
