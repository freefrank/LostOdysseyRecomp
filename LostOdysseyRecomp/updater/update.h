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
    std::string sha256;
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
    std::string sha256;
    uint64_t size = 0;
};

struct Release
{
    std::string tag;
    std::vector<ReleaseAsset> assets;
};

std::optional<PackageManifest> ParsePackageManifest(std::string_view json, std::string &error);
std::optional<Release> ParseGitHubRelease(std::string_view json, std::string &error);
std::optional<ReleaseAsset> SelectAsset(const Release &release, std::string_view platform,
                                        std::string_view architecture, std::string &error);
bool IsSafePayloadPath(const std::filesystem::path &path, std::string &error);
std::string Sha256File(const std::filesystem::path &path, std::string &error);

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
                    const std::vector<std::wstring> &launchArguments, std::string &error);

struct ApplyPlan
{
    std::string version;
    std::filesystem::path installRoot;
    std::filesystem::path stageRoot;
    std::filesystem::path executable;
    std::vector<std::wstring> launchArguments;
    std::vector<FileEntry> files;
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
    std::vector<std::wstring> launchArguments;
    bool automaticUpdates = true;
    uint32_t uiLanguage = 0;
    std::string releaseApiUrl = "https://api.github.com/repos/freefrank/LostOdysseyRecomp/releases/latest";
};

StartupResult PrepareAtStartup(const StartupOptions &options);
std::filesystem::path CurrentExecutablePath();
std::vector<std::wstring> CurrentLaunchArguments();
std::wstring ApplyHelperArguments(const std::filesystem::path &planPath);
const char *StatusName(StartupStatus status);
} // namespace updater
