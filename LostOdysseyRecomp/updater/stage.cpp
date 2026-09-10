#include "update.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <set>

#include "../../tools/XenosRecomp/thirdparty/smol-v/testing/external/miniz/miniz.c"

namespace updater
{
namespace
{
std::string PathUtf8(const std::filesystem::path &path)
{
    const auto value = path.generic_u8string();
    return std::string(reinterpret_cast<const char *>(value.data()), value.size());
}

std::filesystem::path PathFromUtf8(std::string_view value)
{
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t *>(value.data()), value.size()));
}

std::string Lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return value;
}

bool ReadFile(const std::filesystem::path &path, std::vector<unsigned char> &bytes, std::string &error)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) { error = "could not open update archive"; return false; }
    const auto length = input.tellg();
    if (length <= 0 || uint64_t(length) > 2ull * 1024 * 1024 * 1024)
    {
        error = "update archive has an invalid size";
        return false;
    }
    bytes.resize(size_t(length));
    input.seekg(0);
    input.read(reinterpret_cast<char *>(bytes.data()), std::streamsize(bytes.size()));
    if (!input) { error = "could not read update archive"; return false; }
    return true;
}

bool WriteExtracted(mz_zip_archive &archive, mz_uint index, const std::filesystem::path &destination,
                    std::string &error)
{
    size_t size = 0;
    void *memory = mz_zip_reader_extract_to_heap(&archive, index, &size, 0);
    if (!memory) { error = "ZIP entry extraction or CRC validation failed"; return false; }
    std::error_code filesystemError;
    std::filesystem::create_directories(destination.parent_path(), filesystemError);
    if (filesystemError) { mz_free(memory); error = "could not create staging directory"; return false; }
    std::ofstream output(destination, std::ios::binary | std::ios::trunc);
    if (size) output.write(static_cast<const char *>(memory), std::streamsize(size));
    mz_free(memory);
    output.flush();
    if (!output) { error = "could not write staged payload"; return false; }
    return true;
}
} // namespace

bool StageArchive(const std::filesystem::path &archivePath, const std::filesystem::path &operationRoot,
                  std::string_view expectedVersion, StagedUpdate &update, std::string &error)
{
    std::vector<unsigned char> archiveBytes;
    if (!ReadFile(archivePath, archiveBytes, error)) return false;
    mz_zip_archive archive{};
    if (!mz_zip_reader_init_mem(&archive, archiveBytes.data(), archiveBytes.size(), 0))
    {
        error = "download is not a supported ZIP archive";
        return false;
    }
    struct EndArchive { mz_zip_archive *archive; ~EndArchive() { mz_zip_reader_end(archive); } } end{&archive};
    const auto count = mz_zip_reader_get_num_files(&archive);
    if (!count || count > 4096) { error = "ZIP entry count is outside the supported range"; return false; }
    std::map<std::string, mz_uint> entries;
    std::string root;
    mz_uint manifestIndex = UINT32_MAX;
    std::string manifestArchivePath;
    for (mz_uint index = 0; index < count; ++index)
    {
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&archive, index, &stat)) { error = "could not read ZIP metadata"; return false; }
        std::string name(stat.m_filename);
        std::replace(name.begin(), name.end(), '\\', '/');
        while (name.ends_with('/')) name.pop_back();
        if (name.empty()) continue;
        const auto slash = name.find('/');
        const bool directory = mz_zip_reader_is_file_a_directory(&archive, index);
        if ((!directory && slash == std::string::npos) || name.starts_with('/') || name.find(':') != std::string::npos)
        {
            error = "ZIP must contain one package root directory";
            return false;
        }
        if (root.empty()) root = name.substr(0, slash);
        if (root == "." || root == "..") { error = "ZIP contains an unsafe package root"; return false; }
        if (name.substr(0, slash) != root) { error = "ZIP contains multiple package roots"; return false; }
        // shutil.make_archive (used by release packaging) emits the root itself.
        if (directory && slash == std::string::npos) continue;
        const auto relative = name.substr(slash + 1);
        if (relative.empty() || mz_zip_reader_is_file_a_directory(&archive, index)) continue;
        const auto relativePath = PathFromUtf8(relative);
        std::string pathError;
        if (relative != "manifest.json" && !IsSafePayloadPath(relativePath, pathError))
        {
            error = pathError;
            return false;
        }
        const auto key = Lower(relative);
        if (!entries.emplace(key, index).second) { error = "ZIP contains duplicate payload paths"; return false; }
        if (key == "manifest.json") { manifestIndex = index; manifestArchivePath = name; }
    }
    if (manifestIndex == UINT32_MAX) { error = "ZIP does not contain a root manifest.json"; return false; }
    size_t manifestSize = 0;
    void *manifestMemory = mz_zip_reader_extract_to_heap(&archive, manifestIndex, &manifestSize, 0);
    if (!manifestMemory || manifestSize > 4 * 1024 * 1024)
    {
        if (manifestMemory) mz_free(manifestMemory);
        error = "could not read bounded package manifest";
        return false;
    }
    std::string manifestText(static_cast<const char *>(manifestMemory), manifestSize);
    mz_free(manifestMemory);
    auto manifest = ParsePackageManifest(manifestText, error);
    if (!manifest || manifest->developmentBuild || !ParseVersion(manifest->version) ||
        CompareVersions(*ParseVersion(manifest->version), *ParseVersion(expectedVersion)) != 0)
    {
        if (error.empty()) error = "archive manifest version/build does not match the selected release";
        return false;
    }
    if (entries.size() != manifest->files.size() + 1)
    {
        error = "ZIP contents do not exactly match manifest allowlist";
        return false;
    }
    const auto stageRoot = operationRoot / "stage";
    std::error_code filesystemError;
    std::filesystem::create_directories(stageRoot, filesystemError);
    if (filesystemError) { error = "could not create update staging root"; return false; }
    for (const auto &file : manifest->files)
    {
        const auto key = Lower(PathUtf8(file.path));
        const auto found = entries.find(key);
        if (found == entries.end()) { error = "manifest payload is absent from ZIP: " + key; return false; }
        const auto destination = stageRoot / file.path;
        if (!WriteExtracted(archive, found->second, destination, error)) return false;
        std::string hashError;
        if (Sha256File(destination, hashError) != file.sha256)
        {
            error = hashError.empty() ? "staged payload SHA256 mismatch: " + key : hashError;
            return false;
        }
    }
    update.version = manifest->version;
    update.operationRoot = operationRoot;
    update.stageRoot = stageRoot;
    update.planPath = operationRoot / "apply-plan.json";
    update.runnerPath = operationRoot / "LostOdysseyUpdater-runner.exe";
    update.files = std::move(manifest->files);
    return true;
}
} // namespace updater
