#include "update.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <fstream>
#include <set>

#include "../../tools/XenonRecomp/thirdparty/tomlplusplus/vendor/json.hpp"

namespace updater
{
namespace
{
using json = nlohmann::json;

bool HexDigest(std::string_view value)
{
    return value.size() == 64 && std::all_of(value.begin(), value.end(), [](unsigned char c) {
        return std::isxdigit(c) != 0;
    });
}

std::string Lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return value;
}

bool NumericIdentifier(std::string_view value)
{
    return !value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char c) { return std::isdigit(c); });
}

std::string PathUtf8(const std::filesystem::path &path)
{
    const auto value = path.generic_u8string();
    return std::string(reinterpret_cast<const char *>(value.data()), value.size());
}

std::filesystem::path PathFromUtf8(std::string_view value)
{
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t *>(value.data()), value.size()));
}

bool IsWithin(const std::filesystem::path &child, const std::filesystem::path &parent)
{
    std::error_code error;
    const auto normalizedChild = std::filesystem::weakly_canonical(child, error);
    if (error) return false;
    const auto normalizedParent = std::filesystem::weakly_canonical(parent, error);
    if (error) return false;
    auto childIt = normalizedChild.begin();
    for (auto parentIt = normalizedParent.begin(); parentIt != normalizedParent.end(); ++parentIt, ++childIt)
        if (childIt == normalizedChild.end() || *childIt != *parentIt) return false;
    return true;
}
} // namespace

std::optional<Version> ParseVersion(std::string_view text)
{
    if (!text.empty() && (text.front() == 'v' || text.front() == 'V')) text.remove_prefix(1);
    const auto plus = text.find('+');
    if (plus != std::string_view::npos) text = text.substr(0, plus);
    const auto dash = text.find('-');
    auto core = text.substr(0, dash);
    auto suffix = dash == std::string_view::npos ? std::string_view{} : text.substr(dash + 1);
    if (core.empty() || (!suffix.empty() && dash == std::string_view::npos)) return std::nullopt;
    Version result;
    size_t position = 0;
    while (position <= core.size())
    {
        const auto next = core.find('.', position);
        const auto token = core.substr(position, next == std::string_view::npos ? core.size() - position : next - position);
        if (token.empty() || (token.size() > 1 && token.front() == '0')) return std::nullopt;
        uint32_t number = 0;
        const auto parsed = std::from_chars(token.data(), token.data() + token.size(), number);
        if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size()) return std::nullopt;
        result.numbers.push_back(number);
        if (next == std::string_view::npos) break;
        position = next + 1;
    }
    if (result.numbers.size() < 2 || result.numbers.size() > 4) return std::nullopt;
    if (dash != std::string_view::npos)
    {
        if (suffix.empty()) return std::nullopt;
        position = 0;
        while (position <= suffix.size())
        {
            const auto next = suffix.find('.', position);
            auto token = suffix.substr(position, next == std::string_view::npos ? suffix.size() - position : next - position);
            if (token.empty() || !std::all_of(token.begin(), token.end(), [](unsigned char c) {
                    return std::isalnum(c) || c == '-';
                })) return std::nullopt;
            result.prerelease.emplace_back(token);
            if (next == std::string_view::npos) break;
            position = next + 1;
        }
    }
    return result;
}

int CompareVersions(const Version &left, const Version &right)
{
    const size_t count = std::max(left.numbers.size(), right.numbers.size());
    for (size_t i = 0; i < count; ++i)
    {
        const auto l = i < left.numbers.size() ? left.numbers[i] : 0;
        const auto r = i < right.numbers.size() ? right.numbers[i] : 0;
        if (l != r) return l < r ? -1 : 1;
    }
    if (left.prerelease.empty() != right.prerelease.empty()) return left.prerelease.empty() ? 1 : -1;
    for (size_t i = 0; i < std::min(left.prerelease.size(), right.prerelease.size()); ++i)
    {
        const auto &l = left.prerelease[i];
        const auto &r = right.prerelease[i];
        if (l == r) continue;
        const bool ln = NumericIdentifier(l), rn = NumericIdentifier(r);
        if (ln && rn)
        {
            if (l.size() != r.size()) return l.size() < r.size() ? -1 : 1;
            return l < r ? -1 : 1;
        }
        if (ln != rn) return ln ? -1 : 1;
        return l < r ? -1 : 1;
    }
    if (left.prerelease.size() == right.prerelease.size()) return 0;
    return left.prerelease.size() < right.prerelease.size() ? -1 : 1;
}

bool IsSafePayloadPath(const std::filesystem::path &path, std::string &error)
{
    if (path.empty() || path.is_absolute() || path.has_root_name() || path.has_root_directory())
    {
        error = "payload path must be relative";
        return false;
    }
    for (const auto &component : path)
        if (component.empty() || component == "." || component == "..")
        {
            error = "payload path contains an unsafe component";
            return false;
        }
    const auto generic = PathUtf8(path);
    if (generic.find('\\') != std::string::npos || generic.find(':') != std::string::npos || generic.find('\0') != std::string::npos)
    {
        error = "payload path contains an unsafe character";
        return false;
    }
    auto first = Lower(PathUtf8(*path.begin()));
    static const std::set<std::string> mutableNames = {
        "save", "profile", "cache", "logs", "captures", "game", "settings.ini", "game-path.txt"
    };
    if (mutableNames.contains(first))
    {
        error = "payload attempts to replace user data: " + first;
        return false;
    }
    return true;
}

std::optional<PackageManifest> ParsePackageManifest(std::string_view text, std::string &error)
{
    try
    {
        const auto data = json::parse(text);
        if (!data.is_object() || !data.contains("version") || !data["version"].is_string() ||
            !data.contains("development_build") || !data["development_build"].is_boolean() ||
            !data.contains("files") || !data["files"].is_object())
        {
            error = "package manifest has an invalid schema";
            return std::nullopt;
        }
        PackageManifest result;
        result.version = data["version"].get<std::string>();
        result.developmentBuild = data["development_build"].get<bool>();
        std::set<std::string> seen;
        for (auto it = data["files"].begin(); it != data["files"].end(); ++it)
        {
            if (!it.value().is_string()) { error = "package manifest hash is not a string"; return std::nullopt; }
            FileEntry entry{PathFromUtf8(it.key()), Lower(it.value().get<std::string>())};
            std::string pathError;
            if (!IsSafePayloadPath(entry.path, pathError) || !HexDigest(entry.sha256))
            {
                error = pathError.empty() ? "package manifest has an invalid SHA256" : pathError;
                return std::nullopt;
            }
            const auto key = Lower(PathUtf8(entry.path));
            if (!seen.insert(key).second) { error = "package manifest contains duplicate paths"; return std::nullopt; }
            result.files.push_back(std::move(entry));
        }
        if (result.files.empty()) { error = "package manifest has no payload files"; return std::nullopt; }
        return result;
    }
    catch (const std::exception &exception)
    {
        error = std::string("package manifest JSON: ") + exception.what();
        return std::nullopt;
    }
}

std::optional<Release> ParseGitHubRelease(std::string_view text, std::string &error)
{
    try
    {
        const auto data = json::parse(text);
        if (!data.is_object() || !data.contains("tag_name") || !data["tag_name"].is_string() ||
            !data.contains("assets") || !data["assets"].is_array())
        {
            error = "GitHub release has an invalid schema";
            return std::nullopt;
        }
        Release result;
        result.tag = data["tag_name"].get<std::string>();
        if (!ParseVersion(result.tag)) { error = "GitHub release tag is not a supported version"; return std::nullopt; }
        for (const auto &asset : data["assets"])
        {
            if (!asset.is_object() || asset.value("state", "") != "uploaded" || !asset.contains("name") ||
                !asset["name"].is_string() || !asset.contains("browser_download_url") ||
                !asset["browser_download_url"].is_string() || !asset.contains("size") ||
                !asset["size"].is_number_unsigned() || !asset.contains("digest") || !asset["digest"].is_string()) continue;
            auto digest = asset["digest"].get<std::string>();
            if (!digest.starts_with("sha256:")) continue;
            digest = Lower(digest.substr(7));
            if (!HexDigest(digest)) continue;
            const auto url = asset["browser_download_url"].get<std::string>();
            if (!url.starts_with("https://github.com/freefrank/LostOdysseyRecomp/releases/download/")) continue;
            const auto size = asset["size"].get<uint64_t>();
            if (!size) continue;
            result.assets.push_back({asset["name"].get<std::string>(), url, digest, size});
        }
        return result;
    }
    catch (const std::exception &exception)
    {
        error = std::string("GitHub release JSON: ") + exception.what();
        return std::nullopt;
    }
}

std::optional<ReleaseAsset> SelectAsset(const Release &release, std::string_view platform,
                                        std::string_view architecture, std::string &error)
{
    std::string extension;
    if (platform == "windows") extension = ".zip";
    else if (platform == "macos") extension = ".zip";
    else if (platform == "linux") extension = ".AppImage";
    else { error = "unsupported update platform"; return std::nullopt; }
    const std::string expected = "LostOdysseyRecomp-" + std::string(platform) + "-" +
                                 std::string(architecture) + "-" + release.tag + extension;
    std::optional<ReleaseAsset> match;
    for (const auto &asset : release.assets)
        if (asset.name == expected)
        {
            if (match) { error = "release contains duplicate matching assets"; return std::nullopt; }
            match = asset;
        }
    if (!match) error = "release has no asset named " + expected;
    return match;
}

bool WriteApplyPlan(const StagedUpdate &update, const std::filesystem::path &executable,
                    const std::vector<std::wstring> &launchArguments, std::string &error)
{
    try
    {
        json data{{"schema", 1}, {"version", update.version}, {"install_root", PathUtf8(update.installRoot)},
                  {"stage_root", PathUtf8(update.stageRoot)}, {"executable", PathUtf8(executable)},
                  {"launch_arguments", json::array()}, {"files", json::object()}};
        for (const auto &argument : launchArguments)
        {
            const auto utf8 = std::filesystem::path(argument).u8string();
            data["launch_arguments"].push_back(std::string(reinterpret_cast<const char *>(utf8.data()), utf8.size()));
        }
        for (const auto &file : update.files) data["files"][PathUtf8(file.path)] = file.sha256;
        std::ofstream output(update.planPath, std::ios::binary | std::ios::trunc);
        output << data.dump(2);
        output.flush();
        if (!output) { error = "could not write apply plan"; return false; }
        return true;
    }
    catch (const std::exception &exception)
    {
        error = std::string("could not create apply plan: ") + exception.what();
        return false;
    }
}

std::optional<ApplyPlan> ReadApplyPlan(const std::filesystem::path &path, std::string &error)
{
    try
    {
        std::ifstream input(path, std::ios::binary);
        if (!input) { error = "could not open apply plan"; return std::nullopt; }
        const auto data = json::parse(input);
        if (!data.is_object() || data.value("schema", 0) != 1 || !data.contains("version") ||
            !data["version"].is_string() || !data.contains("install_root") || !data["install_root"].is_string() ||
            !data.contains("stage_root") || !data["stage_root"].is_string() || !data.contains("executable") ||
            !data["executable"].is_string() || !data.contains("launch_arguments") || !data["launch_arguments"].is_array() ||
            !data.contains("files") || !data["files"].is_object())
        {
            error = "apply plan has an invalid schema";
            return std::nullopt;
        }
        ApplyPlan result;
        result.version = data["version"].get<std::string>();
        result.installRoot = PathFromUtf8(data["install_root"].get<std::string>());
        result.stageRoot = PathFromUtf8(data["stage_root"].get<std::string>());
        result.executable = PathFromUtf8(data["executable"].get<std::string>());
        for (const auto &argument : data["launch_arguments"])
        {
            if (!argument.is_string()) { error = "apply plan argument is not a string"; return std::nullopt; }
            result.launchArguments.push_back(PathFromUtf8(argument.get<std::string>()).wstring());
        }
        for (auto it = data["files"].begin(); it != data["files"].end(); ++it)
        {
            if (!it.value().is_string()) { error = "apply plan hash is not a string"; return std::nullopt; }
            result.files.push_back({PathFromUtf8(it.key()), Lower(it.value().get<std::string>())});
        }
        return result;
    }
    catch (const std::exception &exception)
    {
        error = std::string("apply plan JSON: ") + exception.what();
        return std::nullopt;
    }
}

bool ValidateApplyPlan(const ApplyPlan &plan, std::string &error)
{
    if (!ParseVersion(plan.version)) { error = "apply plan version is invalid"; return false; }
    if (!plan.installRoot.is_absolute() || !plan.stageRoot.is_absolute() || !plan.executable.is_absolute())
    {
        error = "apply plan paths must be absolute";
        return false;
    }
    if (!IsWithin(plan.stageRoot, plan.installRoot / ".update") || !IsWithin(plan.executable, plan.installRoot))
    {
        error = "apply plan paths escape the install root";
        return false;
    }
    if (plan.files.empty()) { error = "apply plan has no files"; return false; }
    std::set<std::string> seen;
    for (const auto &file : plan.files)
    {
        std::string pathError;
        if (!IsSafePayloadPath(file.path, pathError) || !HexDigest(file.sha256))
        {
            error = pathError.empty() ? "apply plan has an invalid SHA256" : pathError;
            return false;
        }
        if (!seen.insert(Lower(PathUtf8(file.path))).second) { error = "apply plan contains duplicate paths"; return false; }
        const auto staged = plan.stageRoot / file.path;
        if (!std::filesystem::is_regular_file(staged) || Sha256File(staged, error) != file.sha256)
        {
            if (error.empty()) error = "staged payload hash mismatch: " + PathUtf8(file.path);
            return false;
        }
    }
    return true;
}

const char *StatusName(StartupStatus status)
{
    switch (status)
    {
    case StartupStatus::Disabled: return "disabled";
    case StartupStatus::UnmanagedBuild: return "unmanaged-build";
    case StartupStatus::CurrentPackageMismatch: return "current-package-mismatch";
    case StartupStatus::Offline: return "offline";
    case StartupStatus::UpToDate: return "up-to-date";
    case StartupStatus::NoCompatibleAsset: return "no-compatible-asset";
    case StartupStatus::InvalidRelease: return "invalid-release";
    case StartupStatus::DownloadFailed: return "download-failed";
    case StartupStatus::IntegrityFailed: return "integrity-failed";
    case StartupStatus::Cancelled: return "cancelled";
    case StartupStatus::Ready: return "ready";
    }
    return "unknown";
}
} // namespace updater
