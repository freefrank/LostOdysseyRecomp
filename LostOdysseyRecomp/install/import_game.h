#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace install
{
class Error : public std::runtime_error
{
public:
    explicit Error(std::string message, bool cancelled = false)
        : std::runtime_error(message), cancelled_(cancelled) {}
    bool cancelled() const { return cancelled_; }

private:
    bool cancelled_ = false;
};

enum class Kind { Folder, Iso, God };

struct DiscInfo
{
    std::filesystem::path path;
    Kind kind = Kind::Folder;
    uint32_t files = 0;
    uint64_t bytes = 0;
    uint32_t disc = 0;
    uint32_t discs = 4;
    uint32_t version = 0;
    uint32_t base = 0;
    std::string title;
    std::string media;
    std::string edition;
    std::string identity;
    std::string metadataEdition;
    std::string sha256;
    std::string md5;
};

struct DlcPackageInfo
{
    std::filesystem::path path;
    std::string contentId;
    std::string displayName;
    uint32_t licenseMask = 0;
    std::string format;
    std::string sourceSha256;
    std::string extractedManifestSha256;
    uint32_t files = 0;
    uint64_t bytes = 0;
};

struct Scan
{
    std::vector<DiscInfo> discs;
    std::vector<DlcPackageInfo> packages;
    std::vector<std::pair<std::filesystem::path, std::string>> rejected;
};

struct ContentScan
{
    std::vector<DiscInfo> discs;
    std::vector<DlcPackageInfo> packages;
    std::vector<std::pair<std::filesystem::path, std::string>> rejected;
};

struct InstallResult
{
    std::vector<int> discs;
    std::vector<std::string> dlcImported;
    std::vector<std::string> dlcUnchanged;
    std::string destination;
    std::string warning;
    std::string error;
    bool cancelled = false;
};

using Progress = std::function<void(uint64_t done, uint64_t total, std::string_view label)>;
using Cancelled = std::function<bool()>;
// Called after all selected slots are published but before old slots are deleted.
// Throw on path-persistence failure to restore every selected old slot.
using Commit = std::function<void(const InstallResult&)>;

std::vector<std::filesystem::path> Discover(const std::filesystem::path& path);
Scan ScanSource(const std::filesystem::path& path, const Cancelled& cancelled = {});

// Comprehensive discovery and scanning for both Discs and STFS DLC packages
ContentScan ScanContent(const std::vector<std::filesystem::path>& paths, const Cancelled& cancelled = {});
inline ContentScan ScanContent(const std::filesystem::path& path, const Cancelled& cancelled = {})
{
    return ScanContent(std::vector<std::filesystem::path>{path}, cancelled);
}

std::vector<int> InstallDiscs(const std::filesystem::path& source,
                              const std::filesystem::path& gameDir,
                              const Progress* progress,
                              const Cancelled* cancelled);
inline std::vector<int> InstallDiscs(const std::filesystem::path& source,
                                     const std::filesystem::path& gameDir,
                                     const Progress& progress = {},
                                     const Cancelled& cancelled = {})
{
    return InstallDiscs(source, gameDir, progress ? &progress : nullptr, cancelled ? &cancelled : nullptr);
}

// Integrated install for scanned discs and DLC packages
InstallResult InstallContent(const ContentScan& selection,
                              const std::filesystem::path& destination,
                              const Progress& progress = {},
                              const Cancelled& cancelled = {});

// Replace only selected discN and dlc/<contentId> slots. `destination` must be
// the exact installation root, even if its own name is discN. A root with
// a flat default.xex requires a separate destination when replacing discs.
// Persist result.destination in commit; failure rolls back before returning.
// commit is skipped (with a warning) when a DLC-only destination has no bootable
// default.xex or disc1/default.xex; do not make such a new root the game default.
InstallResult ReimportContent(const ContentScan& selection,
                              const std::filesystem::path& destination,
                              const Progress& progress = {},
                              const Cancelled& cancelled = {},
                              const Commit& commit = {});

std::filesystem::path DefaultGameDirectory(const std::filesystem::path& executableDirectory);
bool WriteGamePath(const std::filesystem::path& executableDirectory,
                   const std::filesystem::path& gameDirectory,
                   std::string& error);

#ifdef LO_IMPORT_TESTING
void SetTestSha256(uint32_t disc, std::string_view hex, bool europe);
void SetTestMd5(uint32_t disc, std::string_view hex, bool europe);
void ClearTestOverrides();
void SetTestDlcWriteFailure(std::string_view filename, std::string_view stage);
void SetTestDiscWriteFailure(std::string_view filename, std::string_view stage);
void SetTestPublishFailure(std::string_view slot, std::string_view stage);
#endif
}
