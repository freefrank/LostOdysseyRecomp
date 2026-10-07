#include "shader_pack_index.h"
#include "update.h"

#include <algorithm>
#include <fstream>
#include <set>
#include <utility>

#include <os/json.h>

namespace updater::shader_pack
{
namespace
{
constexpr std::string_view Renderers[] = {"vulkan", "d3d12", "metal", CorpusRenderer};

bool KnownRenderer(std::string_view renderer)
{
    return std::find(std::begin(Renderers), std::end(Renderers), renderer) != std::end(Renderers);
}

std::string Trim(std::string text)
{
    while (!text.empty() && (text.back() == '\r' || text.back() == ' ' || text.back() == '\t')) text.pop_back();
    const auto start = text.find_first_not_of(" \t");
    return start == std::string::npos ? std::string{} : text.substr(start);
}
} // namespace

bool LowerHex64(std::string_view text)
{
    return text.size() == 64 && std::all_of(text.begin(), text.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}

bool SafeAssetName(std::string_view name)
{
    if (name.empty() || name.size() > 128 || name.front() == '.' || name.find("..") != std::string_view::npos)
        return false;
    return std::all_of(name.begin(), name.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
               c == '.' || c == '-' || c == '_';
    });
}

std::optional<std::vector<IndexEntry>> ParseIndex(std::string_view text, std::string &error)
{
    try
    {
        const auto data = nlohmann::json::parse(text);
        if (!data.is_object() || !data.contains("schema") || !data["schema"].is_number_unsigned() ||
            !data.contains("packs") || !data["packs"].is_array())
        {
            error = "shader pack index has an invalid schema";
            return std::nullopt;
        }
        if (data["schema"].get<uint64_t>() != 1)
        {
            error = "shader pack index schema " + std::to_string(data["schema"].get<uint64_t>()) + " is not supported";
            return std::nullopt;
        }
        std::vector<IndexEntry> entries;
        std::set<std::pair<std::string, std::string>> seen;
        for (const auto &pack : data["packs"])
        {
            if (!pack.is_object() || !pack.contains("renderer") || !pack["renderer"].is_string())
            {
                error = "shader pack index entry has no renderer";
                return std::nullopt;
            }
            IndexEntry entry;
            entry.renderer = pack["renderer"].get<std::string>();
            // Entries for renderers this runtime does not know are left for newer runtimes.
            if (!KnownRenderer(entry.renderer)) continue;
            const auto text = [&pack](const char *key) {
                return pack.contains(key) && pack[key].is_string() ? pack[key].get<std::string>() : std::string{};
            };
            entry.contract = text("contract");
            entry.file = text("file");
            entry.sha256 = text("sha256");
            if (pack.contains("size") && pack["size"].is_number_unsigned()) entry.size = pack["size"].get<uint64_t>();
            if (!LowerHex64(entry.contract) || !LowerHex64(entry.sha256) || !SafeAssetName(entry.file) ||
                !entry.size || entry.size > MaxPackBytes)
            {
                error = "shader pack index entry for " + entry.renderer + " is malformed";
                return std::nullopt;
            }
            if (!seen.emplace(entry.renderer, entry.contract).second)
            {
                error = "shader pack index lists " + entry.renderer + " contract " + entry.contract + " twice";
                return std::nullopt;
            }
            entries.push_back(std::move(entry));
        }
        return entries;
    }
    catch (const std::exception &exception)
    {
        error = std::string("shader pack index JSON: ") + exception.what();
        return std::nullopt;
    }
}

const IndexEntry *Select(const std::vector<IndexEntry> &entries, std::string_view renderer,
                         std::string_view contract)
{
    for (const auto &entry : entries)
        if (entry.renderer == renderer && entry.contract == contract) return &entry;
    return nullptr;
}

std::string IndexUrl(const char *overrideUrl)
{
    if (overrideUrl && *overrideUrl) return overrideUrl;
    return std::string("https://github.com/") + kReleaseRepository + "/releases/download/" +
           std::string(ReleaseTag) + "/" + std::string(IndexFileName);
}

std::string AssetUrl(std::string_view indexUrl, std::string_view file)
{
    const auto slash = indexUrl.rfind('/');
    return std::string(indexUrl.substr(0, slash == std::string_view::npos ? 0 : slash + 1)) + std::string(file);
}

bool Declined(const std::filesystem::path &record, std::string_view contract)
{
    std::ifstream input(record);
    std::string line;
    while (std::getline(input, line))
        if (Trim(line) == contract) return true;
    return false;
}

bool RecordDecline(const std::filesystem::path &record, std::string_view contract, std::string &error)
{
    if (Declined(record, contract)) return true;
    std::error_code filesystemError;
    if (record.has_parent_path()) std::filesystem::create_directories(record.parent_path(), filesystemError);
    std::ofstream output(record, std::ios::app);
    output << contract << '\n';
    output.flush();
    if (!output)
    {
        error = "could not record the declined shader pack";
        return false;
    }
    return true;
}
}
