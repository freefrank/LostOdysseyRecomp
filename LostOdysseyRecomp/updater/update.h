#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace updater
{
struct Version
{
    std::vector<uint32_t> numbers;
    std::vector<std::string> prerelease;
};

std::optional<Version> ParseVersion(std::string_view text);
int CompareVersions(const Version &left, const Version &right);
bool ShouldUpdateToLatest(const Version &current, const Version &latest);

struct FileEntry
{
    std::filesystem::path path;
};

struct PackageManifest
{
    std::string version;
    bool developmentBuild = true;
    std::vector<FileEntry> files;
};

struct ReleaseAsset
{
    std::string name;
    std::string url;
    uint64_t size = 0;
};

struct Release
{
    std::string tag;
    std::string changelogEnglish;
    std::string changelogChinese;
    std::vector<ReleaseAsset> assets;
};

std::optional<PackageManifest> ParsePackageManifest(std::string_view json, std::string &error);
std::optional<Release> ParseGitHubRelease(std::string_view json, std::string &error);
std::string_view ReleaseChangelog(const Release &release, uint32_t uiLanguage);
std::optional<ReleaseAsset> SelectAsset(const Release &release, std::string_view platform,
                                        std::string_view architecture, std::string &error);
bool IsSafePayloadPath(const std::filesystem::path &path, std::string &error);

struct StagedUpdate
{
    std::string version;
    std::filesystem::path installRoot;
    std::filesystem::path operationRoot;
    std::filesystem::path stageRoot;
    std::filesystem::path planPath;
    std::filesystem::path runnerPath;
    std::vector<FileEntry> files;
};

bool StageArchive(const std::filesystem::path &archive, const std::filesystem::path &operationRoot,
                  std::string_view expectedVersion, StagedUpdate &update, std::string &error);
bool WriteApplyPlan(const StagedUpdate &update, const std::filesystem::path &executable,
                    const std::vector<std::wstring> &launchArguments, std::string &error,
                    bool launchAfterApply = false);

struct ApplyPlan
{
    std::string version;
    std::filesystem::path installRoot;
    std::filesystem::path stageRoot;
    std::filesystem::path executable;
    std::vector<std::wstring> launchArguments;
    std::vector<FileEntry> files;
    bool launchAfterApply = false;
};

std::optional<ApplyPlan> ReadApplyPlan(const std::filesystem::path &path, std::string &error);
bool ValidateApplyPlan(const ApplyPlan &plan, std::string &error);

struct ApplyOptions
{
    // Fault injection used only by the host fixture. Zero disables it.
    size_t failAfterReplacements = 0;
};

bool ApplyWithRollback(const ApplyPlan &plan, const ApplyOptions &options, std::string &error);
bool RollbackInstalledFiles(const ApplyPlan &plan, std::string &error);

enum class StartupStatus
{
    Disabled,
    UnmanagedBuild,
    ExternalUpdateAvailable,
    CurrentPackageMismatch,
    Offline,
    UpToDate,
    NoCompatibleAsset,
    InvalidRelease,
    DownloadFailed,
    IntegrityFailed,
    Cancelled,
    Ready
};

struct StartupResult
{
    StartupStatus status = StartupStatus::UnmanagedBuild;
    std::string detail;
    std::optional<StagedUpdate> update;
};

struct StartupOptions
{
    std::string currentVersion;
    std::filesystem::path installRoot;
    std::filesystem::path executable;
    // Standalone updater copies its own running helper into the apply runner.
    std::filesystem::path runnerSource;
    std::vector<std::wstring> launchArguments;
    bool automaticUpdates = true;
    uint32_t uiLanguage = 0;
    std::string releaseApiUrl = "https://api.github.com/repos/freefrank/LostOdysseyRecomp/releases/latest";
    bool (*confirmUpdate)(std::string_view version, std::string_view changelog, uint32_t uiLanguage) = nullptr;
};

struct StartupPreferences
{
    bool automaticUpdates = true;
    uint32_t uiLanguage = 0;
};

StartupPreferences ReadStartupPreferences(const std::filesystem::path &settingsPath);

StartupResult PrepareAtStartup(const StartupOptions &options);
std::filesystem::path CurrentExecutablePath();
std::vector<std::wstring> CurrentLaunchArguments();
std::wstring ApplyHelperArguments(const std::filesystem::path &planPath);
const char *StatusName(StartupStatus status);
} // namespace updater
